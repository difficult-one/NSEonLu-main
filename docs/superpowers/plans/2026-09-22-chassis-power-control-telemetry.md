# Chassis Power Control Telemetry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the 22-column calibration-oriented chassis-power CSV with the approved 17-column control-loop diagnostic CSV.

**Architecture:** `ChassisTask()` publishes one atomic outer-loop telemetry structure into the existing singleton DJI chassis-power instance. `DJIMotorControl()` combines that coherent shared snapshot with the current cycle's local power-model output and motor telemetry, then the pure RTT formatter derives aggregate power, current-scale, limiter-mask, and meter-age fields before writing one row through the existing `BSPLogWrite*` path.

**Tech Stack:** C11, FreeRTOS critical sections, existing host tests built with GCC/Make, STM32 ARM GNU Make build.

## Global Constraints

- The output schema is exactly: `tick,nominal_budget_cw,target_power_cw,corrected_budget_cw,effective_limit_cw,p_unlimited_cw,p_limited_cw,p_meter_cw,p_filtered_cw,error_cw,correction_cw,min_scale_pm,limited_mask,outer_mode,meter_online,meter_age_ms,motor_enabled_mask`.
- Do not change the power model, budget algorithm, outer-loop algorithm, PID settings, CAN output, UART/RTT transport, queue sizing, or 100 Hz telemetry divider.
- Preserve the user's unrelated working-tree changes; touch only the files listed in this plan.
- Follow strict TDD: change tests first, run them and confirm the expected failure, then write the minimum production change.
- Keep CSV writes as one complete non-blocking `BSPLogWrite` call.

---

### Task 1: Define and format the 17-column control-loop CSV

**Files:**
- Modify: `modules/telemetry/rtt_telemetry.h`
- Modify: `modules/telemetry/rtt_telemetry.c`
- Test: `tests/rtt_telemetry/test_rtt_telemetry.c`

**Interfaces:**
- Produces: `ChassisPowerControlTelemetry_s` with the approved seven outer-loop fields.
- Produces: expanded `RTTTelemetryFormatChassisPowerCsv()` and `RTTTelemetryWriteChassisPowerRow()` inputs: `ChassisPowerOutput_s`, `ChassisPowerTelemetry_s`, `ChassisPowerControlTelemetry_s`, tick, and meter tuple.
- Consumes: existing `ChassisPowerOutput_s.status`, `effective_limit_w`, `predicted_unlimited_power_w[]`, and `current_scale[]`; existing `ChassisPowerTelemetry_s.predicted_total_power_w` and `motor_enabled_mask`.

- [ ] **Step 1: Rewrite formatter tests for the approved schema**

In `tests/rtt_telemetry/test_rtt_telemetry.c`, replace the old 22-column expectations. The representative test uses:

```c
const ChassisPowerOutput_s power = {
    .effective_limit_w = 28.5f,
    .predicted_unlimited_power_w = {10.0f, 20.0f, 30.0f, 40.0f},
    .current_scale = {1.0f, 0.75f, 0.5f, 1.0f},
    .status = POWER_MODEL_STATUS_OK,
};
const ChassisPowerTelemetry_s motor = {
    .predicted_total_power_w = 25.25f,
    .motor_enabled_mask = 0x0FU,
};
const ChassisPowerControlTelemetry_s control = {
    .nominal_budget_w = 30.0f,
    .target_power_w = 28.5f,
    .corrected_budget_w = 30.0f,
    .filtered_power_w = 28.0f,
    .error_w = 0.5f,
    .correction_w = 0.0f,
    .outer_mode = 2U,
};
```

Assert the exact header and row:

```text
tick,nominal_budget_cw,target_power_cw,corrected_budget_cw,effective_limit_cw,p_unlimited_cw,p_limited_cw,p_meter_cw,p_filtered_cw,error_cw,correction_cw,min_scale_pm,limited_mask,outer_mode,meter_online,meter_age_ms,motor_enabled_mask\r\n
123,3000,2850,3000,2850,10000,2525,2900,2800,50,0,500,6,2,1,3,15\r\n
```

Add focused tests for:

- valid unscaled output: `min_scale_pm=1000`, `limited_mask=0`;
- invalid model status: `effective_limit`, `p_unlimited`, `min_scale_pm`, and `limited_mask` are zero;
- `meter_tick_ms==0`: `meter_age_ms=0`;
- unsigned tick wrap: `tick_ms=2`, `meter_tick_ms=UINT32_MAX-2` gives age `5`;
- negative/nonfinite/saturated power conversion;
- null inputs and insufficient buffer return zero without emitting a partial row.

- [ ] **Step 2: Run the formatter test and verify RED**

Run:

```powershell
make -C tests/rtt_telemetry clean test
```

Expected: compilation fails because `ChassisPowerControlTelemetry_s` and the new function signatures do not exist, or exact-string assertions fail against the old 22-column format. The failure must be attributable to the requested schema change.

- [ ] **Step 3: Add the telemetry structure and new formatter signatures**

Add to `modules/telemetry/rtt_telemetry.h`:

```c
typedef struct
{
    float nominal_budget_w;
    float target_power_w;
    float corrected_budget_w;
    float filtered_power_w;
    float error_w;
    float correction_w;
    uint8_t outer_mode;
} ChassisPowerControlTelemetry_s;
```

Change the formatter/writer signatures to accept:

```c
const ChassisPowerOutput_s *power,
const ChassisPowerTelemetry_s *motor,
const ChassisPowerControlTelemetry_s *control,
uint32_t tick_ms,
float measured_chassis_power_w,
bool meter_online,
uint32_t meter_tick_ms
```

Keep buffer and buffer-size parameters only on the pure formatter.

- [ ] **Step 4: Implement the minimum 17-column formatter**

In `modules/telemetry/rtt_telemetry.c`:

- replace the static header with the exact approved header;
- sum four `predicted_unlimited_power_w` values only when `power->status == POWER_MODEL_STATUS_OK`;
- for a valid model, calculate `min_scale_pm` from the minimum clamped finite scale and set bit `i` when `current_scale[i] < 1.0f`;
- for an invalid model, output zero for effective limit, unlimited power, minimum scale, and limited mask;
- calculate meter age as `meter_tick_ms == 0U ? 0U : tick_ms - meter_tick_ms`;
- retain signed centiwatt rounding, `int32_t` saturation, invalid-input rejection, and whole-row truncation rejection.

Use one `snprintf` with 17 conversions. Do not allocate memory or add a second output mode.

- [ ] **Step 5: Run the formatter test and verify GREEN**

Run:

```powershell
make -C tests/rtt_telemetry clean test
```

Expected: `PASS: RTT telemetry tests` with no compiler warnings.

- [ ] **Step 6: Commit Task 1**

```powershell
git add modules/telemetry/rtt_telemetry.h modules/telemetry/rtt_telemetry.c tests/rtt_telemetry/test_rtt_telemetry.c
git commit -m "feat: format power control telemetry"
```

---

### Task 2: Publish an atomic outer-loop snapshot into the motor telemetry row

**Files:**
- Modify: `modules/motor/DJImotor/dji_motor.h`
- Modify: `modules/motor/DJImotor/dji_motor.c`
- Modify: `application/chassis/chassis.c`
- Test: `tests/dji_chassis_power_rtt/test_dji_chassis_power_rtt.c`

**Interfaces:**
- Consumes: `ChassisPowerControlTelemetry_s` from Task 1.
- Produces: `void DJIChassisPowerSetControlTelemetry(const ChassisPowerControlTelemetry_s *telemetry)`.
- Produces: one RTT/UART row that combines the latest atomic outer-loop snapshot with one local motor-control-cycle result.

- [ ] **Step 1: Write the failing DJI integration tests**

Update `tests/dji_chassis_power_rtt/test_dji_chassis_power_rtt.c` for the new formatter contract. Add a test that calls:

```c
const ChassisPowerControlTelemetry_s control = {
    .nominal_budget_w = 30.0f,
    .target_power_w = 28.5f,
    .corrected_budget_w = 27.0f,
    .filtered_power_w = 29.0f,
    .error_w = -0.5f,
    .correction_w = -3.0f,
    .outer_mode = 2U,
};
DJIChassisPowerSetControlTelemetry(&control);
DJIChassisPowerSetMeasuredPower(12.34f, true, 77U);
EmitOneRow();
```

With the existing test fixture's 100 W effective limit and zero model coefficients, assert:

```text
123,3000,2850,2700,10000,0,0,1234,2900,-50,-300,1000,0,2,1,46,15\r\n
```

Retain and update tests proving that an offline meter preserves its last value/tick, a nonfinite meter value becomes zero, and exactly one row is emitted after `CHASSIS_POWER_RTT_DIVIDER` motor cycles. Add a null-control-pointer test proving the setter leaves the prior complete snapshot unchanged.

- [ ] **Step 2: Run the DJI integration test and verify RED**

Run:

```powershell
make -C tests/dji_chassis_power_rtt clean test
```

Expected: compilation fails because `DJIChassisPowerSetControlTelemetry()` and instance storage are missing.

- [ ] **Step 3: Store and copy the control snapshot atomically**

In `dji_motor.h`:

- include `rtt_telemetry.h` so the shared structure has one definition;
- add `ChassisPowerControlTelemetry_s control_telemetry` to `DJIChassisPowerInstance_s`;
- declare `DJIChassisPowerSetControlTelemetry()` and document that it affects diagnostics only.

In `dji_motor.c`, implement:

```c
void DJIChassisPowerSetControlTelemetry(
    const ChassisPowerControlTelemetry_s *telemetry)
{
    if (chassis_power == NULL || telemetry == NULL)
        return;

    taskENTER_CRITICAL();
    chassis_power->control_telemetry = *telemetry;
    taskEXIT_CRITICAL();
}
```

In `ApplyChassisPowerGroup()`, copy `control_telemetry` with the meter tuple inside the existing critical section and pass the local `power_output`, local motor telemetry, copied control telemetry, and meter tuple to the new writer.

- [ ] **Step 4: Publish outer-loop values from `ChassisTask()`**

In `UpdateChassisPowerBudget()`, after the outer-loop result is finalized, construct:

```c
const ChassisPowerControlTelemetry_s control_telemetry = {
    .nominal_budget_w = chassis_budget_output.applied_budget_w,
    .target_power_w = chassis_budget_output.applied_budget_w *
                      CHASSIS_POWER_SAFETY_FACTOR,
    .corrected_budget_w = chassis_outer_loop_output.corrected_budget_w,
    .filtered_power_w = chassis_outer_loop_output.filtered_power_w,
    .error_w = chassis_outer_loop_output.error_w,
    .correction_w = chassis_outer_loop_output.correction_w,
    .outer_mode = (uint8_t)chassis_outer_loop_output.mode,
};
```

Call the setter before setting the model budget. In the `POWER_METER_ENABLE == 0` branch, publish the nominal budget, the safety-factor target, corrected budget equal to the applied budget, zero filter/error/correction, and `CHASSIS_POWER_OUTER_OFFLINE`.

Do not change any value passed to `DJIChassisPowerSetBudget()`.

- [ ] **Step 5: Run the DJI integration test and verify GREEN**

Run:

```powershell
make -C tests/dji_chassis_power_rtt clean test
```

Expected: `PASS: DJI chassis-power RTT integration tests` with no compiler warnings.

- [ ] **Step 6: Run focused algorithm regressions**

Run:

```powershell
make -C tests/power_model clean test
make -C tests/chassis_power_outer_loop clean test
```

Expected: both suites pass unchanged.

- [ ] **Step 7: Commit Task 2**

```powershell
git add modules/motor/DJImotor/dji_motor.h modules/motor/DJImotor/dji_motor.c application/chassis/chassis.c tests/dji_chassis_power_rtt/test_dji_chassis_power_rtt.c
git commit -m "feat: publish power loop telemetry"
```

---

### Task 3: Verify the complete firmware-facing change

**Files:**
- Verify only; do not add generated binaries or unrelated cleanup.

**Interfaces:**
- Consumes: completed formatter and atomic publication path from Tasks 1 and 2.
- Produces: test and build evidence that the schema change is isolated to diagnostics.

- [ ] **Step 1: Run all directly affected host tests from a clean state**

```powershell
make -C tests/rtt_telemetry clean test
make -C tests/dji_chassis_power_rtt clean test
make -C tests/power_model clean test
make -C tests/chassis_power_outer_loop clean test
```

Expected: all four print `PASS` and return exit code zero.

- [ ] **Step 2: Run the full ARM build**

```powershell
make -j4
```

Expected: the STM32 firmware links successfully with no new warnings or undefined references related to telemetry.

- [ ] **Step 3: Inspect the scoped diff and repository state**

```powershell
git diff --check HEAD~2..HEAD
git status --short
```

Confirm that generated `.exe`, object, map, and firmware files are not staged, and distinguish the user's pre-existing working-tree changes from this implementation.

- [ ] **Step 4: Request code review and resolve findings**

Provide the reviewer with the approved design document, this implementation plan, the two implementation commit SHAs, and the exact test/build results. Fix every Critical or Important finding, rerun the affected tests, and keep any correction limited to the approved telemetry scope.

- [ ] **Step 5: Final verification after review**

Rerun the four host tests and `make -j4`. Record the final schema, test results, build result, and any remaining hardware-only validation requirement for the handoff.
