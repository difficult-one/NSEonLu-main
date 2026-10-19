# Chassis Power Double-Loop Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a measured-power outer loop that safely corrects the existing four-motor model budget while preserving the current 1 kHz allocation and current-limiting inner loop.

**Architecture:** A hardware-independent `chassis_power_outer_loop` module runs at the 200 Hz chassis cadence and emits a bounded corrected budget. The existing `PowerModelApply()` remains the 1 kHz inner loop; `chassis.c` only gathers a power-meter snapshot and previous model diagnostics, calls the outer-loop module, and forwards its result.

**Tech Stack:** C11, STM32F407/FreeRTOS, host GCC tests with `-Wall -Wextra -Werror -pedantic`, GNU Make.

## Global Constraints

- Preserve the existing M3508 coefficients, four-wheel allocation rules, supercapacitor budget policy, and CAN protocols.
- Use `0.95f` as one shared safety factor for both the model configuration and the measured-power target.
- Initial outer-loop settings are `Kp=0.5`, `Ki=1.0/s`, `tau=0.030s`, activation margin `5W`, deadband `1W`, positive correction ceiling `8W`, and positive/recovery slew `20W/s`.
- Positive correction is allowed only near the power limit and in a forward-consuming operating point.
- Negative correction takes effect immediately; positive correction and release of a negative correction are slew-limited.
- With the meter disabled at compile time, forward the nominal budget unchanged.
- Keep the existing 19-column RTT CSV schema; only replace its measured-power source with the power meter.
- Preserve all unrelated working-tree edits, including current comments and bench configuration.

---

## File Structure

- Create `modules/algorithm/chassis_power_outer_loop.h`: public configuration, state, input, output, mode enum, validation, reset, and update API.
- Create `modules/algorithm/chassis_power_outer_loop.c`: measurement conditioning, state transitions, bounded PI, anti-windup, and fail-safe behavior.
- Create `tests/chassis_power_outer_loop/test_chassis_power_outer_loop.c`: host behavior tests using the real algorithm.
- Create `tests/chassis_power_outer_loop/Makefile`: strict host build and test target.
- Modify `application/chassis/chassis.c`: outer-loop configuration/state, meter snapshot, model-demand summary, corrected-budget wiring, and measured telemetry source.
- Modify `Makefile`: compile the new algorithm into ARM firmware.

---

### Task 1: Outer-loop API, validation, filtering, and inactive states

**Files:**
- Create: `modules/algorithm/chassis_power_outer_loop.h`
- Create: `modules/algorithm/chassis_power_outer_loop.c`
- Create: `tests/chassis_power_outer_loop/test_chassis_power_outer_loop.c`
- Create: `tests/chassis_power_outer_loop/Makefile`

**Interfaces:**
- Consumes: scalar powers in watts, elapsed time in seconds, and validity/mode booleans.
- Produces: `ChassisPowerOuterLoopReset()` and `ChassisPowerOuterLoopUpdate()` with the exact public types below.

- [ ] **Step 1: Create the strict host-test Makefile**

```make
CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_chassis_power_outer_loop.exe
SOURCE := test_chassis_power_outer_loop.c ../../modules/algorithm/chassis_power_outer_loop.c
INCLUDES := -I../../modules/algorithm

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -lm -o $(TARGET)

test: $(TARGET)
	./$(TARGET)

clean:
	-if exist $(TARGET) del /Q $(TARGET)
```

- [ ] **Step 2: Write the public-contract tests first**

Add the repository's existing `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ_INT`, and `ASSERT_NEAR` macros, then define literal fixtures:

```c
static const ChassisPowerOuterLoopConfig_s valid_config = {
    .kp = 0.5f,
    .ki = 1.0f,
    .measurement_lpf_tau_s = 0.030f,
    .activation_margin_w = 5.0f,
    .error_deadband_w = 1.0f,
    .max_positive_correction_w = 8.0f,
    .positive_slew_rate_w_per_s = 20.0f,
    .budget_drop_reset_w = 1.0f,
};

static ChassisPowerOuterLoopInput_s BaseInput(void)
{
    const ChassisPowerOuterLoopInput_s input = {
        .nominal_budget_w = 60.0f,
        .target_power_w = 57.0f,
        .measured_power_w = 20.0f,
        .predicted_unlimited_power_w = 20.0f,
        .meter_online = true,
        .allow_positive_correction = true,
        .control_enabled = true,
    };
    return input;
}
```

Write tests whose named production breaks are:

```c
static void TestResetClearsEveryStateField(void);
static void TestOfflineMeterPassesNominalBudgetFromReset(void);
static void TestFirstOnlineSampleSeedsFilterWithoutZeroRamp(void);
static void TestLowDemandStaysStandbyAndDoesNotAccumulateCorrection(void);
static void TestDisabledControlResetsStateAndOutputsZeroBudget(void);
static void TestInvalidBudgetFailsClosed(void);
static void TestInvalidConfigFailsClosed(void);
```

Use these hand-derived assertions for the representative cases:

```c
ASSERT_EQ_INT(CHASSIS_POWER_OUTER_OFFLINE, output.mode);
ASSERT_NEAR(60.0f, output.corrected_budget_w, 1.0e-5f);

ASSERT_EQ_INT(CHASSIS_POWER_OUTER_STANDBY, output.mode);
ASSERT_NEAR(20.0f, output.filtered_power_w, 1.0e-5f);
ASSERT_NEAR(0.0f, output.correction_w, 1.0e-5f);

input.control_enabled = false;
ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                        &state, &output));
ASSERT_NEAR(0.0f, output.corrected_budget_w, 1.0e-5f);
ASSERT_FALSE(state.measurement_initialized);
```

- [ ] **Step 3: Run the new test and verify RED**

Run:

```powershell
make -C tests/chassis_power_outer_loop test
```

Expected: compilation fails because `chassis_power_outer_loop.h` and its API do not exist. This proves the tests require the new production boundary.

- [ ] **Step 4: Add the exact public API**

Create `chassis_power_outer_loop.h`:

```c
#ifndef CHASSIS_POWER_OUTER_LOOP_H
#define CHASSIS_POWER_OUTER_LOOP_H

#include <stdbool.h>

typedef enum
{
    CHASSIS_POWER_OUTER_OFFLINE = 0,
    CHASSIS_POWER_OUTER_STANDBY,
    CHASSIS_POWER_OUTER_ACTIVE,
} ChassisPowerOuterLoopMode_e;

typedef struct
{
    float kp;
    float ki;
    float measurement_lpf_tau_s;
    float activation_margin_w;
    float error_deadband_w;
    float max_positive_correction_w;
    float positive_slew_rate_w_per_s;
    float budget_drop_reset_w;
} ChassisPowerOuterLoopConfig_s;

typedef struct
{
    float filtered_power_w;
    float integral_w;
    float correction_w;
    float previous_nominal_budget_w;
    bool measurement_initialized;
} ChassisPowerOuterLoopState_s;

typedef struct
{
    float nominal_budget_w;
    float target_power_w;
    float measured_power_w;
    float predicted_unlimited_power_w;
    bool meter_online;
    bool allow_positive_correction;
    bool control_enabled;
} ChassisPowerOuterLoopInput_s;

typedef struct
{
    float corrected_budget_w;
    float correction_w;
    float filtered_power_w;
    float error_w;
    ChassisPowerOuterLoopMode_e mode;
} ChassisPowerOuterLoopOutput_s;

bool ChassisPowerOuterLoopConfigIsValid(
    const ChassisPowerOuterLoopConfig_s *config);
void ChassisPowerOuterLoopReset(ChassisPowerOuterLoopState_s *state);
bool ChassisPowerOuterLoopUpdate(
    const ChassisPowerOuterLoopConfig_s *config,
    const ChassisPowerOuterLoopInput_s *input,
    float dt_s,
    ChassisPowerOuterLoopState_s *state,
    ChassisPowerOuterLoopOutput_s *output);

#endif
```

- [ ] **Step 5: Implement validation, reset, filtering, and inactive behavior only**

In `chassis_power_outer_loop.c`, add `ClampFloat()`, `MoveTowardZero()`, and `ClearOutput()` as private helpers. Validate that every configuration field is finite, all thresholds/rates are nonnegative, `measurement_lpf_tau_s`, `max_positive_correction_w`, and `positive_slew_rate_w_per_s` are strictly positive, and `kp`/`ki` are nonnegative.

Implement these exact state rules:

```c
if (!input->control_enabled)
{
    ChassisPowerOuterLoopReset(state);
    output->mode = CHASSIS_POWER_OUTER_STANDBY;
    return true;
}

if (!input->meter_online || !isfinite(input->measured_power_w))
{
    state->measurement_initialized = false;
    state->integral_w = 0.0f;
    if (state->correction_w > 0.0f)
        state->correction_w = 0.0f;
    else
        state->correction_w = MoveTowardZero(
            state->correction_w,
            config->positive_slew_rate_w_per_s * dt_s);
    output->mode = CHASSIS_POWER_OUTER_OFFLINE;
}
else
{
    if (!state->measurement_initialized)
    {
        state->filtered_power_w = input->measured_power_w;
        state->measurement_initialized = true;
    }
    else
    {
        const float alpha = dt_s /
                            (config->measurement_lpf_tau_s + dt_s);
        state->filtered_power_w +=
            alpha * (input->measured_power_w - state->filtered_power_w);
    }

    const bool demand_near_limit =
        input->predicted_unlimited_power_w >=
            input->target_power_w - config->activation_margin_w ||
        state->filtered_power_w >=
            input->target_power_w - config->activation_margin_w ||
        input->measured_power_w > input->nominal_budget_w;

    if (!demand_near_limit)
    {
        state->integral_w = 0.0f;
        if (state->correction_w > 0.0f)
            state->correction_w = 0.0f;
        else
            state->correction_w = MoveTowardZero(
                state->correction_w,
                config->positive_slew_rate_w_per_s * dt_s);
        output->mode = CHASSIS_POWER_OUTER_STANDBY;
    }
}
```

Until Task 2, the active branch keeps correction at zero and reports `ACTIVE`; this is the minimal implementation required for Task 1's inactive-state tests and contains no speculative control behavior.

Always publish:

```c
state->previous_nominal_budget_w = input->nominal_budget_w;
output->correction_w = state->correction_w;
output->filtered_power_w = state->filtered_power_w;
output->corrected_budget_w = ClampFloat(
    input->nominal_budget_w + state->correction_w,
    0.0f,
    input->nominal_budget_w + config->max_positive_correction_w);
```

Invalid pointers, invalid config, nonpositive/nonfinite budget or target, or negative/nonfinite predicted power must clear state/output, set mode `OFFLINE`, and return `false`. Task 2 adds the conservative state-preserving policy for an invalid `dt_s` after that behavior has its own failing test.

- [ ] **Step 6: Run Task 1 tests and verify GREEN**

Run:

```powershell
make -C tests/chassis_power_outer_loop clean test
```

Expected: `PASS: chassis power outer-loop tests` with no compiler warnings.

- [ ] **Step 7: Commit Task 1**

```powershell
git add modules/algorithm/chassis_power_outer_loop.h modules/algorithm/chassis_power_outer_loop.c tests/chassis_power_outer_loop
git commit -m "feat: add chassis power outer-loop state machine"
```

---

### Task 2: Bounded PI correction and safety behavior

**Files:**
- Modify: `tests/chassis_power_outer_loop/test_chassis_power_outer_loop.c`
- Modify: `modules/algorithm/chassis_power_outer_loop.c`

**Interfaces:**
- Consumes: the Task 1 API unchanged.
- Produces: active negative/positive correction, hard-overlimit response, anti-windup, braking inhibition, and budget-drop reset.

- [ ] **Step 1: Add one failing behavior test at a time**

Add and run each test separately before implementing its behavior:

```c
static void TestOverPowerImmediatelyReducesBudget(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);
    input.measured_power_w = 62.0f;
    input.predicted_unlimited_power_w = 70.0f;

    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_EQ_INT(CHASSIS_POWER_OUTER_ACTIVE, output.mode);
    ASSERT_NEAR(-2.525f, output.correction_w, 1.0e-4f);
    ASSERT_NEAR(57.475f, output.corrected_budget_w, 1.0e-4f);
}

static void TestPositiveCorrectionIsSlewLimited(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);
    input.measured_power_w = 52.0f;
    input.predicted_unlimited_power_w = 70.0f;

    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_NEAR(0.1f, output.correction_w, 1.0e-5f);
    ASSERT_NEAR(60.1f, output.corrected_budget_w, 1.0e-5f);
}
```

Then add literal-result tests for:

```c
static void TestDeadbandProducesNoCorrection(void);          /* 57.5 W -> 0 W correction */
static void TestPositiveCorrectionStopsAtEightWatts(void);   /* repeated active updates -> +8 W */
static void TestLargeOverPowerCanReduceBudgetToZero(void);   /* 200 W -> 0 W budget */
static void TestRawHardLimitBypassesFilteredMeasurement(void);/* seed 40 W, then raw 65 W */
static void TestBrakingCannotCreatePositiveCorrection(void); /* allow_positive=false */
static void TestOfflineDropsPositiveCorrectionImmediately(void);
static void TestOfflineReleasesNegativeCorrectionAtPointOneWattPerTick(void);
static void TestBudgetDropClearsPositiveState(void);          /* 60 W -> 50 W */
static void TestSaturationDoesNotWindIntegralFurther(void);
static void TestErrorCanDriveControllerOutOfSaturation(void);
static void TestInvalidDeltaTimeFailsWithoutPositiveOutput(void);
```

For every new test, run:

```powershell
make -C tests/chassis_power_outer_loop test
```

Expected before its production change: FAIL on the named numeric behavior, not a compile or fixture error.

- [ ] **Step 2: Implement the active PI calculation**

Replace Task 1's active pass-through branch with:

```c
output->mode = CHASSIS_POWER_OUTER_ACTIVE;
float control_power_w = state->filtered_power_w;
if (input->measured_power_w > input->nominal_budget_w &&
    input->measured_power_w > control_power_w)
    control_power_w = input->measured_power_w;

float error_w = input->target_power_w - control_power_w;
if (fabsf(error_w) <= config->error_deadband_w)
    error_w = 0.0f;
output->error_w = error_w;

const float lower_limit_w = -input->nominal_budget_w;
const float upper_limit_w = config->max_positive_correction_w;
float integral_candidate_w = ClampFloat(
    state->integral_w + config->ki * error_w * dt_s,
    lower_limit_w,
    upper_limit_w);
float candidate_w = config->kp * error_w + integral_candidate_w;

if ((!input->allow_positive_correction && candidate_w > 0.0f) ||
    (candidate_w >= upper_limit_w && error_w > 0.0f) ||
    (candidate_w <= lower_limit_w && error_w < 0.0f))
{
    integral_candidate_w = state->integral_w;
    candidate_w = config->kp * error_w + integral_candidate_w;
}

if (!input->allow_positive_correction && candidate_w > 0.0f)
    candidate_w = 0.0f;

state->integral_w = ClampFloat(integral_candidate_w,
                               lower_limit_w,
                               upper_limit_w);
candidate_w = ClampFloat(candidate_w, lower_limit_w, upper_limit_w);

if (candidate_w > state->correction_w)
{
    const float maximum_increase_w =
        config->positive_slew_rate_w_per_s * dt_s;
    state->correction_w = fminf(candidate_w,
                                state->correction_w + maximum_increase_w);
}
else
{
    state->correction_w = candidate_w;
}
```

- [ ] **Step 3: Implement budget-drop and invalid-`dt` policy**

Before filtering and PI evaluation, if the previous budget is initialized and the new nominal budget is lower by more than `budget_drop_reset_w`, clear positive state only:

```c
if (state->previous_nominal_budget_w > 0.0f &&
    input->nominal_budget_w <
        state->previous_nominal_budget_w - config->budget_drop_reset_w)
{
    if (state->integral_w > 0.0f)
        state->integral_w = 0.0f;
    if (state->correction_w > 0.0f)
        state->correction_w = 0.0f;
}
```

Treat nonpositive or nonfinite `dt_s` as an invalid update: clear positive integral/correction, preserve a finite negative correction without moving it, publish a clamped conservative budget, set mode `OFFLINE`, and return `false`. This test protects against a scheduler stall or corrupt timing value creating a positive jump.

- [ ] **Step 4: Run the complete outer-loop suite**

Run:

```powershell
make -C tests/chassis_power_outer_loop clean test
```

Expected: all outer-loop tests pass with pristine compiler output.

- [ ] **Step 5: Perform the mutation check**

Temporarily verify that each mutation causes at least one named test failure, then revert the mutation before continuing:

- reverse the error sign;
- remove the raw-overlimit branch;
- remove the positive slew limit;
- allow integration while saturated;
- retain positive correction on meter loss;
- skip budget-drop reset.

Run the suite after reverting all mutations and confirm it is green.

- [ ] **Step 6: Commit Task 2**

```powershell
git add modules/algorithm/chassis_power_outer_loop.c tests/chassis_power_outer_loop/test_chassis_power_outer_loop.c
git commit -m "feat: close measured chassis power outer loop"
```

---

### Task 3: Wire the measured outer loop into the chassis

**Files:**
- Modify: `application/chassis/chassis.c`
- Modify: `Makefile`

**Interfaces:**
- Consumes: `PowerMeterGetStatus()`, `DJIChassisPowerGetState()`, `ChassisPowerOuterLoopUpdate()`, and `chassis_budget_output.applied_budget_w`.
- Produces: corrected calls to `DJIChassisPowerSetBudget()` and real-meter calls to `DJIChassisPowerSetMeasuredPower()`.

- [ ] **Step 1: Add the new source to the ARM build and record the integration baseline**

Add directly after `modules/algorithm/chassis_power_budget.c`:

```make
modules/algorithm/chassis_power_outer_loop.c \
```

Run:

```powershell
make -j4
```

Expected at this point: the algorithm compiles, but no chassis behavior has changed. Record the successful baseline so the next integration failure is attributable to `chassis.c`.

- [ ] **Step 2: Add shared configuration and state without disturbing user edits**

In `chassis.c`, include `chassis_power_outer_loop.h`, then add:

```c
#define CHASSIS_POWER_SAFETY_FACTOR 0.95f

static ChassisPowerOuterLoopState_s chassis_outer_loop_state;
static ChassisPowerOuterLoopOutput_s chassis_outer_loop_output;

static const ChassisPowerOuterLoopConfig_s chassis_outer_loop_config = {
    .kp = 0.5f,
    .ki = 1.0f,
    .measurement_lpf_tau_s = 0.030f,
    .activation_margin_w = 5.0f,
    .error_deadband_w = 1.0f,
    .max_positive_correction_w = 8.0f,
    .positive_slew_rate_w_per_s = 20.0f,
    .budget_drop_reset_w = 1.0f,
};
```

Replace only the existing `.safety_factor = 0.95f` initializer with:

```c
.safety_factor = CHASSIS_POWER_SAFETY_FACTOR,
```

After `ChassisPowerBudgetReset()`, call:

```c
ChassisPowerOuterLoopReset(&chassis_outer_loop_state);
```

Preserve the user's comments on `chassis_budget_state` and `chassis_budget_output` verbatim.

- [ ] **Step 3: Gather meter and previous-model diagnostics once per chassis cycle**

After `ChassisPowerBudgetUpdate()`, build the inputs with the exact rules below. Keep the meter type and every consumer of it inside the compile-time guard so the disabled build never needs `power_meter.h`:

```c
#if POWER_METER_ENABLE
PowerMeterStatus_s meter_status = {0};
bool meter_valid = false;
if (power_meter != NULL && PowerMeterGetStatus(power_meter, &meter_status))
    meter_valid = meter_status.online;

float predicted_unlimited_power_w = 0.0f;
float motoring_indicator = 0.0f;
const DJIChassisPowerState_s *power_state = DJIChassisPowerGetState();
if (power_state != NULL && power_state->registered &&
    power_state->power.status == POWER_MODEL_STATUS_OK)
{
    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
    {
        predicted_unlimited_power_w +=
            power_state->power.predicted_unlimited_power_w[i];
        motoring_indicator +=
            power_state->telemetry.current_command[i] *
            power_state->telemetry.actual_speed_rpm[i];
    }
}
#endif
```

This uses existing diagnostic state and adds no second current-control path.

- [ ] **Step 4: Call the outer loop and forward its result**

Under `#if POWER_METER_ENABLE`, add:

```c
const ChassisPowerOuterLoopInput_s outer_input = {
    .nominal_budget_w = chassis_budget_output.applied_budget_w,
    .target_power_w = chassis_budget_output.applied_budget_w *
                      CHASSIS_POWER_SAFETY_FACTOR,
    .measured_power_w = meter_status.power_w,
    .predicted_unlimited_power_w = predicted_unlimited_power_w,
    .meter_online = meter_valid,
    .allow_positive_correction =
        meter_valid && meter_status.power_w >= 0.0f &&
        motoring_indicator >= 0.0f,
    .control_enabled =
        chassis_power_ready &&
        chassis_cmd_recv.chassis_mode != CHASSIS_ZERO_FORCE,
};

if (!ChassisPowerOuterLoopUpdate(&chassis_outer_loop_config,
                                 &outer_input,
                                 dt_s,
                                 &chassis_outer_loop_state,
                                 &chassis_outer_loop_output))
    chassis_outer_loop_output.corrected_budget_w = 0.0f;

DJIChassisPowerSetMeasuredPower(
    meter_valid ? meter_status.power_w : 0.0f);
DJIChassisPowerSetBudget(chassis_outer_loop_output.corrected_budget_w);
```

Under `#else`, retain model-only behavior:

```c
DJIChassisPowerSetMeasuredPower(0.0f);
DJIChassisPowerSetBudget(chassis_budget_output.applied_budget_w);
```

Remove the earlier `DJIChassisPowerSetMeasuredPower(cap_status.chassis_power_w)` call. Do not change the supercapacitor command or budget input.

- [ ] **Step 5: Build with the meter enabled**

Run:

```powershell
make clean
make -j4 POWER_METER_ENABLE=1
```

Expected: firmware compiles and links with no warnings or errors.

- [ ] **Step 6: Build the compile-time fallback**

Run:

```powershell
make clean
make -j4 POWER_METER_ENABLE=0
```

Expected: firmware compiles and links; there are no references to `power_meter` outside guarded code, and nominal model-only budgeting remains available.

- [ ] **Step 7: Run all directly related host regressions**

Run:

```powershell
make -C tests/chassis_power_outer_loop clean test
make -C tests/power_model clean test
make -C tests/chassis_power_budget clean test
make -C tests/power_meter clean test
make -C tests/power_meter_protocol clean test
make -C tests/chassis_power_feedback clean test
```

Expected: every suite prints its `PASS` line and exits zero.

- [ ] **Step 8: Inspect only intended integration changes**

Run:

```powershell
git diff --check
git diff -- application/chassis/chassis.c Makefile modules/algorithm/chassis_power_outer_loop.c modules/algorithm/chassis_power_outer_loop.h tests/chassis_power_outer_loop
git status --short
```

Confirm:

- the 19-field CSV format string is unchanged;
- M3508 coefficients and allocation parameters are unchanged;
- the supercapacitor command still uses `link_output.command`;
- user edits in `.vscode/launch.json`, `application/cmd/robot_cmd.c`, `application/robot_def.h`, and existing `chassis.c` comments remain untouched;
- no generated `.exe`, `.o`, or build output is staged.

- [ ] **Step 9: Commit Task 3 selectively**

```powershell
git add Makefile modules/algorithm/chassis_power_outer_loop.c modules/algorithm/chassis_power_outer_loop.h tests/chassis_power_outer_loop
git add -p application/chassis/chassis.c
git diff --cached --check
git commit -m "feat: integrate chassis measured-power outer loop"
```

If `application/chassis/chassis.c` contains pre-existing user hunks, stage only the outer-loop hunks with `git add -p`; do not stage or rewrite the user's comment-only changes.

---

## Final Verification

- [ ] All outer-loop tests were observed failing for the intended missing behavior before implementation.
- [ ] All host tests pass with warnings treated as errors.
- [ ] ARM firmware builds with `POWER_METER_ENABLE=1` and `POWER_METER_ENABLE=0`.
- [ ] The final diff contains no model coefficient, allocation policy, protocol, or RTT schema change.
- [ ] `git status --short` still shows the user's unrelated working-tree changes as uncommitted.
- [ ] On hardware, start with `Ki=0`, confirm correction direction, then restore `Ki=1.0f` and follow the staged bench procedure in the design document.
