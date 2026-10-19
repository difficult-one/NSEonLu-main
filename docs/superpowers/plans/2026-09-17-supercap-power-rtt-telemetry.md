# Super-Capacitor Power RTT Telemetry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Append the super-capacitor module's latest measured chassis power to every existing 100 Hz chassis-power RTT CSV row.

**Architecture:** Keep the pure CSV conversion in `power_model.c` and pass the measured watt value into the formatter as an extra argument. The chassis application forwards the latest `SuperCapStatus_s.chassis_power_w` through a narrow DJI chassis-power setter; the motor task stores the latest value and appends it during its existing coherent RTT emission point.

**Tech Stack:** C11, STM32F407, FreeRTOS/CMSIS-RTOS, SEGGER RTT, MinGW host tests, GNU Arm Embedded firmware build.

## Global Constraints

- Preserve the existing first 18 CSV columns and append only `p_cap_cw` as column 19.
- Convert watts to centiwatts with the existing saturating integer conversion.
- Do not add an online/valid field or special offline representation.
- Before the first super-capacitor frame, print zero; after a later disconnect, retain the last decoded power.
- Do not feed measured power into budgeting, allocation, limiting, or motor control.
- Keep RTT output at nominal 100 Hz.
- Do not overwrite unrelated uncommitted changes in `application/cmd/robot_cmd.c` or `application/robot_def.h`.

---

## File Map

- `modules/algorithm/power_model.h`: extend the pure CSV formatter interface with measured chassis power.
- `modules/algorithm/power_model.c`: append measured chassis power as centiwatts.
- `tests/power_model/test_power_model.c`: lock the 19-column output, sign preservation, non-finite normalization, saturation, and truncation behavior.
- `modules/motor/DJImotor/dji_motor.h`: declare the latest-measurement setter.
- `modules/motor/DJImotor/dji_motor.c`: store the latest measurement and add the header/data column at the existing RTT emission point.
- `application/chassis/chassis.c`: forward `SuperCapStatus_s.chassis_power_w` from the already-read status snapshot.

---

### Task 1: Extend the pure CSV formatter

**Files:**
- Modify: `tests/power_model/test_power_model.c`
- Modify: `modules/algorithm/power_model.h`
- Modify: `modules/algorithm/power_model.c`

**Interfaces:**
- Consumes: `ChassisPowerTelemetry_s`, `uint32_t tick_ms`, and a measured chassis power in watts.
- Produces: `size_t ChassisPowerTelemetryFormatCsv(const ChassisPowerTelemetry_s *telemetry, uint32_t tick_ms, float measured_chassis_power_w, char *buffer, size_t buffer_size)`.

- [ ] **Step 1: Write failing tests for the appended measured-power column**

Update `TestTelemetryFormatsOneCompleteCsvRow()` to pass `12.34f` and require the hand-checked final field `1234`:

```c
ASSERT_TRUE(ChassisPowerTelemetryFormatCsv(&telemetry,
                                           123U,
                                           12.34f,
                                           row,
                                           sizeof(row)) > 0U);
ASSERT_STREQ("123,2000,2100,2200,2300,1000,1100,1200,1300,"
             "1010,1111,1212,1313,1000,1100,1200,1300,4600,1234\r\n",
             row);
```

Add focused tests whose literal expectations catch a lost sign, missing finite check, or missing saturation:

```c
static void TestTelemetryCsvPreservesNegativeMeasuredPower(void)
{
    ChassisPowerTelemetry_s telemetry = {0};
    char row[CHASSIS_POWER_TELEMETRY_CSV_MAX_LENGTH];

    ASSERT_TRUE(ChassisPowerTelemetryFormatCsv(&telemetry,
                                               1U,
                                               -1.25f,
                                               row,
                                               sizeof(row)) > 0U);
    ASSERT_STREQ("1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-125\r\n", row);
}

static void TestTelemetryCsvNormalizesNonfiniteMeasuredPower(void)
{
    ChassisPowerTelemetry_s telemetry = {0};
    char row[CHASSIS_POWER_TELEMETRY_CSV_MAX_LENGTH];

    ASSERT_TRUE(ChassisPowerTelemetryFormatCsv(&telemetry,
                                               1U,
                                               NAN,
                                               row,
                                               sizeof(row)) > 0U);
    ASSERT_STREQ("1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\r\n", row);
}

static void TestTelemetryCsvSaturatesMeasuredPower(void)
{
    ChassisPowerTelemetry_s telemetry = {0};
    char row[CHASSIS_POWER_TELEMETRY_CSV_MAX_LENGTH];

    ASSERT_TRUE(ChassisPowerTelemetryFormatCsv(&telemetry,
                                               1U,
                                               FLT_MAX,
                                               row,
                                               sizeof(row)) > 0U);
    ASSERT_STREQ("1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2147483647\r\n", row);
}
```

Update the truncation and existing saturation calls to supply `0.0f`. Register all new tests in `main()`.

- [ ] **Step 2: Run the host test and verify RED**

Run:

```powershell
mingw32-make -C tests/power_model clean test
```

Expected: compilation fails because `ChassisPowerTelemetryFormatCsv()` still has the old four-argument signature.

- [ ] **Step 3: Implement the minimal formatter change**

Change the declaration and definition to:

```c
size_t ChassisPowerTelemetryFormatCsv(const ChassisPowerTelemetry_s *telemetry,
                                      uint32_t tick_ms,
                                      float measured_chassis_power_w,
                                      char *buffer,
                                      size_t buffer_size);
```

Append one `,%ld` field before `\r\n` and pass:

```c
(long)PowerToCentiwatts(measured_chassis_power_w)
```

Retain the existing `snprintf()` truncation check and reuse `PowerToCentiwatts()` so negative, non-finite, and overflowing values follow the same rules as predicted power.

- [ ] **Step 4: Run the host test and verify GREEN**

Run:

```powershell
mingw32-make -C tests/power_model clean test
```

Expected: `PASS: power model tests` with no warnings.

- [ ] **Step 5: Commit the pure formatting change**

```powershell
git add modules/algorithm/power_model.h modules/algorithm/power_model.c tests/power_model/test_power_model.c
git commit -m "feat: append measured chassis power to RTT CSV"
```

---

### Task 2: Feed the super-capacitor measurement into RTT

**Files:**
- Modify: `modules/motor/DJImotor/dji_motor.h`
- Modify: `modules/motor/DJImotor/dji_motor.c`
- Modify: `application/chassis/chassis.c`

**Interfaces:**
- Consumes: the latest `SuperCapStatus_s.chassis_power_w` snapshot and the Task 1 formatter signature.
- Produces: `void DJIChassisPowerSetMeasuredPower(float measured_power_w)` and a 19-column RTT row ending in `p_cap_cw`.

- [ ] **Step 1: Add the narrow measurement interface**

Declare in `dji_motor.h`:

```c
/** @brief 更新仅用于 RTT 观测的底盘实测功率，单位 W */
void DJIChassisPowerSetMeasuredPower(float measured_power_w);
```

Add an aligned latest-value field to the private `DJIChassisPowerGroup_s`:

```c
volatile float measured_chassis_power_w;
```

Implement the setter in `dji_motor.c`:

```c
void DJIChassisPowerSetMeasuredPower(float measured_power_w)
{
    chassis_power_group.measured_chassis_power_w =
        isfinite(measured_power_w) ? measured_power_w : 0.0f;
}
```

The aligned 32-bit volatile value is a latest-sample observation shared between the 200 Hz chassis task and 1 kHz motor task; it does not participate in control state.

- [ ] **Step 2: Forward the existing super-capacitor status snapshot**

Immediately after `SuperCapGetStatus()` in `UpdateChassisPowerBudget()`, add:

```c
DJIChassisPowerSetMeasuredPower(cap_status.chassis_power_w);
```

Because `cap_status` starts zeroed and the super-capacitor module retains its last decoded fields when it goes offline, this implements the specified startup and disconnect behavior without another validity branch.

- [ ] **Step 3: Append the header and data value**

Extend the registration-time header to end with:

```text
p_total_cw,p_cap_cw\r\n
```

Pass `chassis_power_group.measured_chassis_power_w` into `ChassisPowerTelemetryFormatCsv()` from `PrintChassisPowerTelemetry()`. Do not change `CHASSIS_POWER_RTT_DIVIDER` or the control path.

- [ ] **Step 4: Build the complete firmware**

Run:

```powershell
mingw32-make -j4
```

Expected: the firmware compiles and links without new warnings or errors.

- [ ] **Step 5: Run focused and related regression tests**

Run:

```powershell
mingw32-make -C tests/power_model clean test
mingw32-make -C tests/super_cap_protocol clean test
mingw32-make -C tests/chassis_power_budget clean test
```

Expected: every suite prints `PASS`.

- [ ] **Step 6: Inspect only the intended diff**

Run:

```powershell
git diff --check
git status --short
git diff -- modules/algorithm/power_model.h modules/algorithm/power_model.c tests/power_model/test_power_model.c modules/motor/DJImotor/dji_motor.h modules/motor/DJImotor/dji_motor.c application/chassis/chassis.c
```

Verify that the pre-existing changes in `application/cmd/robot_cmd.c` and `application/robot_def.h` remain untouched.

- [ ] **Step 7: Commit the integration**

```powershell
git add modules/motor/DJImotor/dji_motor.h modules/motor/DJImotor/dji_motor.c application/chassis/chassis.c
git commit -m "feat: log supercap chassis power over RTT"
```

---

### Task 3: Hardware verification

**Files:**
- No source changes expected.

**Interfaces:**
- Consumes: the built firmware, a connected super-capacitor board, and RTT channel 0.
- Produces: evidence that predicted and measured power appear in the same 100 Hz row.

- [ ] **Step 1: Flash with the wheels safely raised**

Use the existing J-Link/OpenOCD workflow and start the RTT client after the target is running.

- [ ] **Step 2: Verify the row format**

Confirm that the header appears once and each data row contains exactly 19 comma-separated integers. Successive `tick` values should normally differ by about 10 ms.

- [ ] **Step 3: Verify the measured-power column**

Compare `p_cap_cw / 100` with the super-capacitor board's own displayed or diagnostic chassis power. Confirm that stopping and restarting the motors changes `p_cap_cw` while the existing prediction columns remain intact.
