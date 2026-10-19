# Chassis Power RTT Telemetry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Capture one coherent four-motor chassis power telemetry snapshot at 1 kHz and emit it as integer CSV over J-Link RTT at 100 Hz.

**Architecture:** Add a pure telemetry builder beside the power model so its field mapping and total can be host-tested. Store the resulting snapshot in the existing DJI chassis-power state immediately after each model calculation, then decimate RTT output inside the same motor-control path so a row cannot mix two control cycles.

**Tech Stack:** C11, STM32F407, FreeRTOS/CMSIS-RTOS, SEGGER RTT, existing host-side GCC/Make tests.

## Global Constraints

- Include only four limited current commands, four measured speeds, four target speeds, four limited predicted powers, and their predicted total.
- Do not include super-capacitor status or measured power-meter power.
- Refresh the telemetry snapshot from the nominal 1 kHz DJI motor-control path.
- Emit RTT CSV at nominal 100 Hz, one row per ten motor-control invocations.
- Do not change power budgeting, current limiting, or motor output behavior.
- Do not use RTT `%f`; print integer command units, integer rpm, and centiwatts.

---

### Task 1: Pure chassis power telemetry builder

**Files:**
- Modify: `modules/algorithm/power_model.h`
- Modify: `modules/algorithm/power_model.c`
- Test: `tests/power_model/test_power_model.c`

**Interfaces:**
- Consumes: `ChassisPowerInput_s`, `ChassisPowerOutput_s`, and `POWER_MODEL_MOTOR_COUNT`.
- Produces: `ChassisPowerTelemetry_s` and `bool ChassisPowerTelemetryBuild(const ChassisPowerInput_s *input, const ChassisPowerOutput_s *power, ChassisPowerTelemetry_s *telemetry)`.

- [ ] **Step 1: Write failing tests for valid and invalid telemetry construction**

Add tests that call the not-yet-defined interface with four distinct values. The valid test must assert that each `current_command` equals `limited_current`, each `actual_speed_rpm` equals `speed_rpm`, each `target_speed_rpm` equals `speed_rpm + speed_error_rpm`, each `predicted_power_w` equals `predicted_limited_power_w`, and `predicted_total_power_w` equals the four-power sum. The invalid test must pass a non-OK model status and assert that the call returns false and zeroes the complete telemetry object.

```c
static void TestTelemetryBuildsCoherentSnapshot(void)
{
    ChassisPowerInput_s input = ValidInput();
    ChassisPowerOutput_s power = {.status = POWER_MODEL_STATUS_OK};
    ChassisPowerTelemetry_s telemetry;
    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
    {
        input.speed_rpm[i] = 1000.0f + 100.0f * (float)i;
        input.speed_error_rpm[i] = 10.0f + (float)i;
        power.limited_current[i] = 2000.0f + 100.0f * (float)i;
        power.predicted_limited_power_w[i] = 10.0f + (float)i;
    }

    ASSERT_TRUE(ChassisPowerTelemetryBuild(&input, &power, &telemetry));
    ASSERT_NEAR(46.0f, telemetry.predicted_total_power_w, 1.0e-6f);
    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
    {
        ASSERT_NEAR(power.limited_current[i], telemetry.current_command[i], 1.0e-6f);
        ASSERT_NEAR(input.speed_rpm[i], telemetry.actual_speed_rpm[i], 1.0e-6f);
        ASSERT_NEAR(input.speed_rpm[i] + input.speed_error_rpm[i],
                    telemetry.target_speed_rpm[i], 1.0e-6f);
        ASSERT_NEAR(power.predicted_limited_power_w[i], telemetry.predicted_power_w[i], 1.0e-6f);
    }
}

static void TestTelemetryRejectsInvalidPowerAndClearsOutput(void)
{
    ChassisPowerInput_s input = ValidInput();
    ChassisPowerOutput_s power = {.status = POWER_MODEL_STATUS_INVALID_BUDGET};
    ChassisPowerTelemetry_s telemetry;
    memset(&telemetry, 0xA5, sizeof(telemetry));

    ASSERT_FALSE(ChassisPowerTelemetryBuild(&input, &power, &telemetry));
    const unsigned char *bytes = (const unsigned char *)&telemetry;
    for (size_t i = 0; i < sizeof(telemetry); ++i)
        ASSERT_TRUE(bytes[i] == 0U);
}
```

- [ ] **Step 2: Run the power-model test and verify RED**

Run:

```powershell
mingw32-make -C tests/power_model clean test
```

Expected: compilation fails because `ChassisPowerTelemetry_s` and `ChassisPowerTelemetryBuild` do not exist.

- [ ] **Step 3: Define the telemetry type and implement the minimal builder**

Add the five-field structure to `power_model.h`. Implement the builder in `power_model.c` so it clears output first, rejects null pointers, rejects non-OK status or non-finite source values, copies the four coherent fields, and accumulates a finite predicted total.

```c
bool ChassisPowerTelemetryBuild(const ChassisPowerInput_s *input,
                                const ChassisPowerOutput_s *power,
                                ChassisPowerTelemetry_s *telemetry)
{
    if (telemetry == NULL)
        return false;
    memset(telemetry, 0, sizeof(*telemetry));
    if (input == NULL || power == NULL || power->status != POWER_MODEL_STATUS_OK)
        return false;

    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
    {
        const float target_speed_rpm = input->speed_rpm[i] + input->speed_error_rpm[i];
        if (!isfinite(power->limited_current[i]) || !isfinite(input->speed_rpm[i]) ||
            !isfinite(target_speed_rpm) || !isfinite(power->predicted_limited_power_w[i]))
        {
            memset(telemetry, 0, sizeof(*telemetry));
            return false;
        }
        telemetry->current_command[i] = power->limited_current[i];
        telemetry->actual_speed_rpm[i] = input->speed_rpm[i];
        telemetry->target_speed_rpm[i] = target_speed_rpm;
        telemetry->predicted_power_w[i] = power->predicted_limited_power_w[i];
        telemetry->predicted_total_power_w += power->predicted_limited_power_w[i];
    }

    if (!isfinite(telemetry->predicted_total_power_w))
    {
        memset(telemetry, 0, sizeof(*telemetry));
        return false;
    }
    return true;
}
```

- [ ] **Step 4: Run the power-model test and verify GREEN**

Run:

```powershell
mingw32-make -C tests/power_model clean test
```

Expected: `PASS: power model tests` with no compiler warnings.

- [ ] **Step 5: Commit the pure telemetry builder**

```powershell
git add modules/algorithm/power_model.h modules/algorithm/power_model.c tests/power_model/test_power_model.c
git commit -m "feat: build chassis power telemetry snapshot"
```

### Task 2: Integrate 1 kHz telemetry and 100 Hz RTT CSV

**Files:**
- Modify: `modules/motor/DJImotor/dji_motor.h`
- Modify: `modules/motor/DJImotor/dji_motor.c`

**Interfaces:**
- Consumes: `ChassisPowerTelemetryBuild(...)`, `SEGGER_RTT_printf(...)`, and the existing `DJIChassisPowerState_s`.
- Produces: `DJIChassisPowerState_s.telemetry`, updated after every model invocation, plus the RTT header and data stream.

- [ ] **Step 1: Expose telemetry through the existing state and call the builder**

Add `ChassisPowerTelemetry_s telemetry;` to `DJIChassisPowerState_s`. Immediately after `PowerModelApply`, call:

```c
(void)ChassisPowerTelemetryBuild(&chassis_power_group.input,
                                 &chassis_power_group.state.power,
                                 &chassis_power_group.state.telemetry);
```

This preserves existing control behavior because telemetry is derived only after the model output is available.

- [ ] **Step 2: Add integer conversion and a 10:1 RTT divider**

Add private helpers in `dji_motor.c` that round finite floats to `int32_t`, scale watts to centiwatts, and print the 18 CSV columns with `SEGGER_RTT_printf`. Print the header once when the chassis power group registers. Invoke the row printer every tenth `ApplyChassisPowerGroup()` call.

```c
#define CHASSIS_POWER_RTT_DIVIDER 10U

static int32_t RoundTelemetryValue(float value)
{
    return (int32_t)(value >= 0.0f ? value + 0.5f : value - 0.5f);
}
```

The data row must use this order:

```text
tick,i_lf,i_rf,i_lb,i_rb,rpm_lf,rpm_rf,rpm_lb,rpm_rb,target_lf,target_rf,target_lb,target_rb,p_lf_cw,p_rf_cw,p_lb_cw,p_rb_cw,p_total_cw
```

- [ ] **Step 3: Build the complete firmware**

Run:

```powershell
mingw32-make -j4
```

Expected: the firmware links successfully without new warnings or errors.

- [ ] **Step 4: Run the related host regression suites**

Run:

```powershell
mingw32-make -C tests/power_model clean test
mingw32-make -C tests/chassis_power_feedback clean test
```

Expected: both suites print `PASS`.

- [ ] **Step 5: Inspect the final diff and commit the integration**

Run `git diff --check` and verify that unrelated existing changes in `application/robot_def.h`, `application/chassis/chassis.c`, and `modules/power_meter/power_meter.c` were not overwritten. Then commit only the telemetry integration files:

```powershell
git add modules/motor/DJImotor/dji_motor.h modules/motor/DJImotor/dji_motor.c
git commit -m "feat: stream chassis power telemetry over RTT"
```

### Task 3: Hardware RTT verification

**Files:**
- No source changes expected.

**Interfaces:**
- Consumes: the built ELF, J-Link SWD connection, and RTT channel 0.
- Produces: evidence that the header appears once, data rows arrive near 100 Hz, and the control loop remains responsive.

- [ ] **Step 1: Flash and start the RTT client with safely raised wheels**

Use the existing J-Link build/debug configuration and start `.vscode/tasks.json` task for `JLinkRTTClient.exe` after the target is running.

- [ ] **Step 2: Verify field order and rate**

Confirm one header followed by rows containing exactly 18 comma-separated integers. Compare successive `tick` values; the expected interval is approximately 10 ms, with small RTOS scheduling jitter.

- [ ] **Step 3: Verify values under a low-power command**

With the bench limit kept at 60 W and wheels safely raised, confirm that target and actual rpm move consistently, current commands remain within `[-15000, 15000]`, each predicted power is finite, and `p_total_cw` equals the four centiwatt columns within integer-rounding error.
