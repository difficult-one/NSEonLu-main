# Chassis Power Calibration RTT Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the chassis-power RTT CSV so all six M3508 power-model coefficients can be fitted from rows whose power-meter timing and four-motor output validity are explicit.

**Architecture:** Keep the existing 100 Hz telemetry path and its first 18 values. Add a four-bit motor output mask to the pure power telemetry snapshot, expose the power meter's last receive tick, and carry a coherent `{power, online, tick}` measurement snapshot through the DJI chassis-power instance into a 22-column RTT row.

**Tech Stack:** C11, STM32 HAL/FreeRTOS, SEGGER RTT through `bsp_log`, GNU Make host tests, ARM GCC firmware build.

## Global Constraints

- The final CSV has exactly 22 columns in the order approved by `docs/superpowers/specs/2026-09-20-chassis-power-calibration-rtt-design.md`.
- Existing columns 1 through 18 retain their current order, scaling, rounding, and saturation behavior.
- Column 19 is named `p_meter_cw`; columns 20 through 22 are `meter_online`, `meter_tick_ms`, and `motor_enabled_mask`.
- RTT remains nominally 100 Hz and writes only complete rows.
- Do not change power-model coefficients, power allocation, outer-loop behavior, control periods, or CAN protocols.
- Implement each behavior test-first and preserve unrelated working-tree changes.

---

## File Structure

- `modules/algorithm/power_model.h/.c`: own the structured four-motor telemetry and construct its output-enable mask.
- `modules/power_meter/power_meter.h/.c`: expose the last valid CAN receive tick in the public status snapshot.
- `modules/telemetry/rtt_telemetry.h/.c`: own the fixed 22-column CSV schema and formatting.
- `modules/motor/DJImotor/dji_motor.h/.c`: own the coherent cross-task measured-power snapshot and supply it to RTT.
- `application/chassis/chassis.c`: map the power-meter status snapshot into the DJI chassis-power observation API.
- `tests/power_model/test_power_model.c`: verify motor-mask construction.
- `tests/power_meter/test_power_meter.c`: verify last-receive-tick status behavior.
- `tests/rtt_telemetry/test_rtt_telemetry.c`: verify the exact schema and row conversion.
- `tests/dji_chassis_power_rtt/`: verify the complete measured-power tuple reaches one emitted motor RTT row.

---

### Task 1: Add the four-motor output mask to structured telemetry

**Files:**
- Modify: `tests/power_model/test_power_model.c`
- Modify: `modules/algorithm/power_model.h`
- Modify: `modules/algorithm/power_model.c`

**Interfaces:**
- Consumes: existing `output_enabled[POWER_MODEL_MOTOR_COUNT]` passed to `ChassisPowerTelemetryBuild()`.
- Produces: `uint8_t ChassisPowerTelemetry_s.motor_enabled_mask`, with bit `i` set when `output_enabled[i]` is true.

- [ ] **Step 1: Write failing mask assertions**

Add `#include <stdint.h>` to `power_model.h` only when production is implemented. First insert these assertions in the named existing tests:

```c
/* At the end of TestTelemetryBuildsCoherentSnapshot(), whose input is
 * {true, true, true, true}. */
ASSERT_EQ_INT(0x0F, telemetry.motor_enabled_mask);

/* At the end of TestTelemetryClearsCommandsDisabledByFinalOutputGate(),
 * whose input is {true, false, true, true}. */
ASSERT_EQ_INT(0x0D, telemetry.motor_enabled_mask);
```

In `TestTelemetryRejectsInvalidPowerAndClearsOutput()`, initialize the telemetry mask to `0x0F` before the call and assert that failure clears it to zero:

```c
telemetry.motor_enabled_mask = 0x0FU;
ASSERT_FALSE(ChassisPowerTelemetryBuild(&input, &power, output_enabled, &telemetry));
ASSERT_EQ_INT(0, telemetry.motor_enabled_mask);
```

- [ ] **Step 2: Run the power-model test and verify RED**

Run:

```powershell
make -C tests/power_model clean test
```

Expected: compilation fails because `ChassisPowerTelemetry_s` has no `motor_enabled_mask` member.

- [ ] **Step 3: Implement the minimal mask construction**

In `modules/algorithm/power_model.h`, include `<stdint.h>` and append:

```c
uint8_t motor_enabled_mask;
```

to `ChassisPowerTelemetry_s`.

In the successful loop in `ChassisPowerTelemetryBuild()`, set the bit after validating the row inputs:

```c
if (output_enabled[i])
    telemetry->motor_enabled_mask |= (uint8_t)(1U << i);
```

Keep the existing initial `memset()`, which guarantees a zero mask for rejected telemetry.

- [ ] **Step 4: Run the power-model test and verify GREEN**

Run:

```powershell
make -C tests/power_model clean test
```

Expected: `PASS: power model tests`.

- [ ] **Step 5: Commit Task 1**

```powershell
git add -- modules/algorithm/power_model.h modules/algorithm/power_model.c tests/power_model/test_power_model.c
git commit -m "feat: expose chassis motor output mask"
```

---

### Task 2: Expose the power meter's last valid receive tick

**Files:**
- Modify: `tests/power_meter/test_power_meter.c`
- Modify: `modules/power_meter/power_meter.h`
- Modify: `modules/power_meter/power_meter.c`

**Interfaces:**
- Consumes: existing private `PowerMeterInstance.last_rx_tick_ms` updated only for successfully decoded CAN frames.
- Produces: `PowerMeterStatus_s.last_rx_tick_ms`; it is zero before the first valid frame and retains the last valid tick after rejection or timeout.

- [ ] **Step 1: Write failing public-status tick assertions**

Add the following assertions to existing tests:

```c
/* TestStartsOfflineAndRejectsNullArguments */
ASSERT_EQ_U32(0U, status.last_rx_tick_ms);

/* TestValidFrameUpdatesSnapshotAndDiagnostics */
ASSERT_EQ_U32(1234U, status.last_rx_tick_ms);

/* TestBadLengthDoesNotOverwriteOrFeedDaemon */
ASSERT_EQ_U32(status_before.last_rx_tick_ms, status_after.last_rx_tick_ms);

```

In `TestOfflineCallbackClearsOnlyOnlineState()`, replace the statements from
`ResetMeterState();` through the existing `PowerMeterGetStatus()` call with this
exact sequence, then retain the existing power assertion:

```c
fake_tick_ms = 42U;
DeliverFrame(frame, 8U);
fake_tick_ms = 142U;
fake_daemon_instance.temp_count = 0U;
captured_daemon_config.callback(captured_daemon_config.owner_id);
ASSERT_TRUE(PowerMeterGetStatus(&status));
ASSERT_FALSE(status.online);
ASSERT_EQ_U32(42U, status.last_rx_tick_ms);
```

- [ ] **Step 2: Run the power-meter test and verify RED**

Run:

```powershell
make -C tests/power_meter clean test
```

Expected: compilation fails because `PowerMeterStatus_s` has no `last_rx_tick_ms` member.

- [ ] **Step 3: Implement the public tick snapshot**

Append this field to `PowerMeterStatus_s` in `power_meter.h`:

```c
uint32_t last_rx_tick_ms;
```

Keep the private instance field as the freshness source. In `PowerMeterGetStatus()`, while still inside the existing critical section, copy it into the returned status:

```c
*status = power_meter->status;
status->last_rx_tick_ms = power_meter->last_rx_tick_ms;
status->online = PowerMeterIsFresh(power_meter, HAL_GetTick());
```

Do not update the public tick for rejected frames and do not clear it when the meter becomes offline.

- [ ] **Step 4: Run the power-meter test and verify GREEN**

Run:

```powershell
make -C tests/power_meter clean test
```

Expected: `PASS: power meter module tests` for the default, `can_failure`, and `custom_timeout` invocations.

- [ ] **Step 5: Commit Task 2**

```powershell
git add -- modules/power_meter/power_meter.h modules/power_meter/power_meter.c tests/power_meter/test_power_meter.c
git commit -m "feat: expose power meter sample tick"
```

---

### Task 3: Change the RTT formatter to the fixed 22-column schema

**Files:**
- Modify: `tests/rtt_telemetry/test_rtt_telemetry.c`
- Modify: `modules/telemetry/rtt_telemetry.h`
- Modify: `modules/telemetry/rtt_telemetry.c`

**Interfaces:**
- Consumes: `ChassisPowerTelemetry_s`, motor snapshot tick, measured watts, meter-online flag, and meter receive tick.
- Produces:

```c
size_t RTTTelemetryFormatChassisPowerCsv(
    const ChassisPowerTelemetry_s *telemetry,
    uint32_t tick_ms,
    float measured_chassis_power_w,
    bool meter_online,
    uint32_t meter_tick_ms,
    char *buffer,
    size_t buffer_size);

void RTTTelemetryWriteChassisPowerRow(
    const ChassisPowerTelemetry_s *telemetry,
    uint32_t tick_ms,
    float measured_chassis_power_w,
    bool meter_online,
    uint32_t meter_tick_ms);
```

- [ ] **Step 1: Write the failing exact-schema tests**

Change the expected header to:

```c
"tick,i_lf,i_rf,i_lb,i_rb,rpm_lf,rpm_rf,rpm_lb,rpm_rb,"
"target_lf,target_rf,target_lb,target_rb,p_lf_cw,p_rf_cw,"
"p_lb_cw,p_rb_cw,p_total_cw,p_meter_cw,meter_online,"
"meter_tick_ms,motor_enabled_mask\r\n"
```

Set `.motor_enabled_mask = 0x0FU` in the representative telemetry and call the wished-for API:

```c
ASSERT_TRUE(RTTTelemetryFormatChassisPowerCsv(
                &telemetry, 123U, 12.34f, true, 120U,
                row, sizeof(row)) > 0U);
ASSERT_STREQ(
    "123,2000,2100,2200,2300,1000,1100,1200,1300,"
    "1010,1111,1212,1313,1000,1100,1200,1300,4600,1234,1,120,15\r\n",
    row);
```

Update every formatter and writer call in this test file with `false, 0U` unless
that test explicitly exercises an online meter. Update the existing expected
rows as follows:

```c
/* TestNegativeMeasuredPowerPreservesSign */
"1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-125,0,0,0\r\n"

/* TestNonfiniteMeasuredPowerBecomesZero */
"1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\r\n"

/* TestMeasuredPowerSaturates */
"1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2147483647,0,0,0\r\n"

/* TestPredictedPowerSaturates */
"1,0,0,0,0,0,0,0,0,0,0,0,0,2147483647,0,0,0,0,0,0,0,0\r\n"

/* TestRowWriterSendsOneCompleteRow */
"7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,150,0,0,0\r\n"
```

Add a focused online-normalization test:

```c
static void TestMeterOnlinePrintsAsOne(void)
{
    const ChassisPowerTelemetry_s telemetry = {.motor_enabled_mask = 5U};
    char row[RTT_TELEMETRY_CHASSIS_POWER_CSV_MAX_LENGTH];

    ASSERT_TRUE(RTTTelemetryFormatChassisPowerCsv(
                    &telemetry, 9U, 2.5f, true, 7U,
                    row, sizeof(row)) > 0U);
    ASSERT_STREQ(
        "9,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,250,1,7,5\r\n",
        row);
}
```

Register the new test in `main()`.

- [ ] **Step 2: Run the RTT test and verify RED**

Run:

```powershell
make -C tests/rtt_telemetry clean test
```

Expected: compilation fails because the formatter and writer do not accept meter validity/tick arguments and the old schema has only 19 columns.

- [ ] **Step 3: Implement the minimal 22-column formatter**

In `rtt_telemetry.h`, include `<stdbool.h>` and change:

```c
#define RTT_TELEMETRY_CHASSIS_POWER_CSV_MAX_LENGTH 320U
```

Update both declarations to the interfaces above.

In `rtt_telemetry.c`, replace the header with the exact 22-column text and extend the format string with:

```c
",%u,%lu,%u\r\n"
```

after `p_meter_cw`. Supply arguments in this exact order:

```c
meter_online ? 1U : 0U,
(unsigned long)meter_tick_ms,
(unsigned)(telemetry->motor_enabled_mask & 0x0FU)
```

Update `RTTTelemetryWriteChassisPowerRow()` to forward the new arguments without changing the one-buffer, one-write behavior.

- [ ] **Step 4: Run the RTT test and verify GREEN**

Run:

```powershell
make -C tests/rtt_telemetry clean test
```

Expected: `PASS: RTT telemetry tests` with no warnings.

- [ ] **Step 5: Commit Task 3**

```powershell
git add -- modules/telemetry/rtt_telemetry.h modules/telemetry/rtt_telemetry.c tests/rtt_telemetry/test_rtt_telemetry.c
git commit -m "feat: add calibration metadata to RTT"
```

---

### Task 4: Carry one coherent power-meter snapshot into the motor RTT path

**Files:**
- Modify: `modules/motor/DJImotor/dji_motor.h`
- Modify: `modules/motor/DJImotor/dji_motor.c`
- Modify: `application/chassis/chassis.c`

**Interfaces:**
- Consumes: `PowerMeterStatus_s.power_w`, `.online`, and `.last_rx_tick_ms` from one `PowerMeterGetStatus()` call.
- Produces:

```c
void DJIChassisPowerSetMeasuredPower(float measured_power_w,
                                     bool meter_online,
                                     uint32_t meter_tick_ms);
```

- [ ] **Step 1: Change the application calls first and verify the firmware is RED**

Under `#if POWER_METER_ENABLE` in `UpdateChassisPowerBudget()`, replace the one-argument setter call with:

```c
DJIChassisPowerSetMeasuredPower(
    meter_status.power_w,
    meter_valid,
    meter_status.last_rx_tick_ms);
```

Under `#else`, use the explicit disabled snapshot:

```c
DJIChassisPowerSetMeasuredPower(0.0f, false, 0U);
```

Run:

```powershell
make -j4 POWER_METER_ENABLE=1
```

Expected: compilation fails because `DJIChassisPowerSetMeasuredPower()` still takes one argument.

- [ ] **Step 2: Implement coherent storage in the DJI chassis-power instance**

Replace `volatile float measured_chassis_power_w` in `DJIChassisPowerInstance_s` with:

```c
volatile float measured_chassis_power_w;
volatile bool power_meter_online;
volatile uint32_t power_meter_tick_ms;
```

Update the public declaration and implementation to the three-argument interface. Sanitize only non-finite power; preserve the online flag and receive tick:

```c
void DJIChassisPowerSetMeasuredPower(float measured_power_w,
                                     bool meter_online,
                                     uint32_t meter_tick_ms)
{
    if (chassis_power == NULL)
        return;

    taskENTER_CRITICAL();
    chassis_power->measured_chassis_power_w =
        isfinite(measured_power_w) ? measured_power_w : 0.0f;
    chassis_power->power_meter_online = meter_online;
    chassis_power->power_meter_tick_ms = meter_tick_ms;
    taskEXIT_CRITICAL();
}
```

In `ApplyChassisPowerGroup()`, declare local measurement values. Copy the three instance fields inside the same critical section that publishes `state.power` and `state.telemetry`, then pass the local values to `RTTTelemetryWriteChassisPowerRow()`:

```c
float measured_chassis_power_w;
bool meter_online;
uint32_t meter_tick_ms;

taskENTER_CRITICAL();
chassis_power->state.power = power_output;
chassis_power->state.telemetry = telemetry;
measured_chassis_power_w = chassis_power->measured_chassis_power_w;
meter_online = chassis_power->power_meter_online;
meter_tick_ms = chassis_power->power_meter_tick_ms;
taskEXIT_CRITICAL();
```

Use:

```c
RTTTelemetryWriteChassisPowerRow(
    &telemetry,
    HAL_GetTick(),
    measured_chassis_power_w,
    meter_online,
    meter_tick_ms);
```

- [ ] **Step 3: Build both feature configurations**

Run:

```powershell
make clean
make -j4 POWER_METER_ENABLE=1
make clean
make -j4 POWER_METER_ENABLE=0
```

Expected: both complete firmware builds compile and link without new warnings or errors.

- [ ] **Step 4: Commit Task 4**

```powershell
git add -- application/chassis/chassis.c modules/motor/DJImotor/dji_motor.h modules/motor/DJImotor/dji_motor.c
git commit -m "feat: forward coherent power meter telemetry"
```

---

### Task 5: Full regression and schema audit

**Files:**
- Verify only; modify earlier files only if a verification failure directly identifies a defect in this feature.

**Interfaces:**
- Consumes: completed 22-column RTT implementation.
- Produces: passing host suites, two passing ARM configurations, and a clean schema audit.

- [ ] **Step 1: Run focused and adjacent host tests**

```powershell
make -C tests/power_model clean test
make -C tests/power_meter clean test
make -C tests/rtt_telemetry clean test
make -C tests/dji_chassis_power_rtt clean test
make -C tests/chassis_power_outer_loop clean test
make -C tests/chassis_power_budget clean test
make -C tests/power_meter_protocol clean test
make -C tests/chassis_power_link clean test
```

Expected: every command passes without warnings. `ChassisPowerFeedback*` is
covered by `tests/power_model/test_power_model.c`; there is no standalone
`tests/chassis_power_feedback/Makefile`.

- [ ] **Step 2: Rebuild both firmware configurations from clean outputs**

Run:

```powershell
make clean
make -j4 POWER_METER_ENABLE=1
make clean
make -j4 POWER_METER_ENABLE=0
```

Expected: both firmware images compile and link successfully.

- [ ] **Step 3: Audit the fixed schema and scope**

Run:

```powershell
rg -n "p_cap_cw|p_meter_cw|meter_online|meter_tick_ms|motor_enabled_mask" application modules tests
git diff --check 4b0135d..HEAD
git status --short
```

Expected:

- no chassis RTT header still uses `p_cap_cw`;
- the new fields appear only in the intended status, telemetry, formatter, integration, and test paths;
- no whitespace errors;
- no unrelated files changed.

- [ ] **Step 4: Inspect the cumulative diff**

```powershell
git diff --stat 4b0135d..HEAD
git diff 4b0135d..HEAD -- application/chassis/chassis.c modules/algorithm/power_model.h modules/algorithm/power_model.c modules/power_meter/power_meter.h modules/power_meter/power_meter.c modules/telemetry/rtt_telemetry.h modules/telemetry/rtt_telemetry.c modules/motor/DJImotor/dji_motor.h modules/motor/DJImotor/dji_motor.c tests/power_model/test_power_model.c tests/power_meter/test_power_meter.c tests/rtt_telemetry/test_rtt_telemetry.c
```

Confirm every changed production line traces to the approved RTT calibration design.

- [ ] **Step 5: Perform the hardware acceptance check**

With wheels safely raised and bench mode intentionally enabled, capture RTT and verify:

```text
column_count == 22
meter_online == 1
motor_enabled_mask == 15
tick - meter_tick_ms remains within the expected power-meter update interval
```

Stop the motors and verify the mask becomes `0`; disconnect the power meter and verify `meter_online` becomes `0` while `meter_tick_ms` retains the last valid receive tick. This hardware step is required before using the CSV for coefficient fitting but does not block completion of host-verifiable code work.
