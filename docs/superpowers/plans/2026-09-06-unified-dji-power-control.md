# Unified DJI Chassis Power Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the duplicated legacy chassis motor driver and power algorithm with the new six-coefficient power model hooked into the single shared DJI motor control/send path.

**Architecture:** Keep `power_model.c` hardware-independent and operate on four-element arrays. `dji_motor.c` computes every DJI motor's PID output first, applies the power model only to the registered four-motor chassis group, then packs and sends every DJI command through its existing shared CAN sender.

**Tech Stack:** C11, STM32F407 HAL, FreeRTOS/CMSIS-RTOS, Arm GNU Toolchain 13.3, MinGW GCC host tests, GNU Make.

## Global Constraints

- All DJI motors use only `DJIMotorInit()` and `DJIMotorControl()` after the migration.
- Delete `modules/motor/power_control.c` and `modules/motor/power_control.h`; do not retain a legacy switch or fallback implementation.
- The power-managed group contains exactly four distinct M3508 instances.
- Power-model speed is rpm; shared DJI speed-loop feedback remains degrees per second.
- Power-model math uses `float`, `fabsf()`, `sqrtf()`, fixed arrays, and no dynamic allocation.
- Invalid chassis power inputs fail safe to zero current for the four chassis motors only.
- Non-chassis DJI motor PID behavior and CAN grouping must remain unchanged.
- Initial supercapacitor attenuation is exactly `1.0f`.
- Initial real-vehicle validation uses safety factor `0.90f` to `0.95f`; `0.98f` is enabled only after measured model error is acceptable.

---

## File Structure

- `modules/algorithm/power_model.h`: hardware-independent data types and function contracts.
- `modules/algorithm/power_model.c`: validation, prediction, allocation, quadratic limiting, and four-motor orchestration.
- `tests/power_model/test_power_model.c`: host-side behavioral and edge-case tests.
- `tests/power_model/Makefile`: isolated MinGW/GCC test build.
- `modules/motor/DJImotor/dji_motor.h`: chassis power-group configuration, diagnostics, and public registration/update APIs.
- `modules/motor/DJImotor/dji_motor.c`: unified PID staging, chassis hook, fail-safe checks, and shared CAN packing.
- `application/chassis/chassis.c`: unified motor registration, M3508 model configuration, speed-unit conversion, and referee budget updates.
- `modules/motor/motor_task.c`: remove the second motor control call.
- `modules/super_cap/super_cap.h`: declare the already-implemented `SuperCapGet()` accessor.
- `Makefile`: remove the legacy source and add the new algorithm source.
- `modules/motor/power_control.c` and `modules/motor/power_control.h`: delete after all call sites move.
- `modules/motor/power_control.md`: document runtime data flow, units, tuning, and safety behavior.

---

### Task 1: Implement and test the hardware-independent power model

**Files:**

- Create: `modules/algorithm/power_model.h`
- Create: `modules/algorithm/power_model.c`
- Create: `tests/power_model/test_power_model.c`
- Create: `tests/power_model/Makefile`

**Interfaces:**

- Consumes: four desired current commands, four speed values in rpm, four speed errors in rpm, four model configurations, one algorithm configuration, referee power limit, and attenuation.
- Produces: `bool PowerModelApply(...)`, `float PowerModelPredict(...)`, and a complete `ChassisPowerOutput_s` diagnostic result.

- [ ] **Step 1: Create the public types and signatures**

Write `modules/algorithm/power_model.h` with this contract:

```c
#ifndef POWER_MODEL_H
#define POWER_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#define POWER_MODEL_MOTOR_COUNT 4U

typedef enum
{
    POWER_MODEL_STATUS_OK = 0,
    POWER_MODEL_STATUS_INVALID_ARGUMENT,
    POWER_MODEL_STATUS_INVALID_CONFIG,
    POWER_MODEL_STATUS_INVALID_BUDGET,
    POWER_MODEL_STATUS_NUMERIC_ERROR,
} PowerModelStatus_e;

typedef struct
{
    float k0;
    float k1;
    float k2;
    float k3;
    float k4;
    float k5;
    float current_conversion;
} MotorPowerModelConfig_s;

typedef struct
{
    float safety_factor;
    float small_error_threshold_rpm;
    float reserved_power_threshold_w;
    float per_motor_reserved_power_w;
    float max_current_command;
} ChassisPowerAlgorithmConfig_s;

typedef struct
{
    float desired_current[POWER_MODEL_MOTOR_COUNT];
    float speed_rpm[POWER_MODEL_MOTOR_COUNT];
    float speed_error_rpm[POWER_MODEL_MOTOR_COUNT];
    float referee_power_limit_w;
    float attenuation;
} ChassisPowerInput_s;

typedef struct
{
    float effective_limit_w;
    float predicted_unlimited_power_w[POWER_MODEL_MOTOR_COUNT];
    float allocated_power_w[POWER_MODEL_MOTOR_COUNT];
    float current_scale[POWER_MODEL_MOTOR_COUNT];
    float limited_current[POWER_MODEL_MOTOR_COUNT];
    float predicted_limited_power_w[POWER_MODEL_MOTOR_COUNT];
    PowerModelStatus_e status;
} ChassisPowerOutput_s;

bool PowerModelConfigIsValid(const MotorPowerModelConfig_s *config);
bool ChassisPowerAlgorithmConfigIsValid(const ChassisPowerAlgorithmConfig_s *config);
float PowerModelPredict(const MotorPowerModelConfig_s *config,
                        float current_command,
                        float speed_rpm);
bool PowerModelApply(const MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT],
                     const ChassisPowerAlgorithmConfig_s *config,
                     const ChassisPowerInput_s *input,
                     ChassisPowerOutput_s *output);

#endif
```

- [ ] **Step 2: Write failing tests for prediction and allocation**

Create `tests/power_model/test_power_model.c` with a small assertion harness and cases that verify:

```c
static const MotorPowerModelConfig_s m3508 = {
    .k0 = 0.65213f,
    .k1 = -0.15659f,
    .k2 = 0.00041660f,
    .k3 = 0.00235415f,
    .k4 = 0.20022f,
    .k5 = 1.08e-7f,
    .current_conversion = 1000.0f,
};

static const ChassisPowerAlgorithmConfig_s algorithm = {
    .safety_factor = 0.98f,
    .small_error_threshold_rpm = 500.0f,
    .reserved_power_threshold_w = 54.0f,
    .per_motor_reserved_power_w = 8.0f,
    .max_current_command = 15000.0f,
};

static void test_prediction_matches_polynomial(void)
{
    const float actual = PowerModelPredict(&m3508, 5000.0f, 3000.0f);
    const float expected = 0.65213f - 0.15659f * 5.0f
                         + 0.00041660f * 3000.0f
                         + 0.00235415f * 5.0f * 3000.0f
                         + 0.20022f * 25.0f
                         + 1.08e-7f * 9000000.0f;
    ASSERT_NEAR(expected, actual, 1.0e-4f);
}

static void test_small_error_evenly_splits_budget(void)
{
    ChassisPowerInput_s input = valid_input();
    ChassisPowerOutput_s output;
    input.referee_power_limit_w = 100.0f;
    input.speed_error_rpm[0] = 100.0f;
    input.speed_error_rpm[1] = 100.0f;
    input.speed_error_rpm[2] = 100.0f;
    input.speed_error_rpm[3] = 100.0f;
    ASSERT_TRUE(PowerModelApply(models, &algorithm, &input, &output));
    ASSERT_NEAR(24.5f, output.allocated_power_w[0], 1.0e-4f);
    ASSERT_NEAR(98.0f, sum4(output.allocated_power_w), 1.0e-4f);
}

static void test_large_error_uses_reserved_power(void)
{
    ChassisPowerInput_s input = valid_input();
    ChassisPowerOutput_s output;
    input.referee_power_limit_w = 100.0f;
    input.speed_error_rpm[0] = 700.0f;
    input.speed_error_rpm[1] = 100.0f;
    input.speed_error_rpm[2] = 100.0f;
    input.speed_error_rpm[3] = 100.0f;
    ASSERT_TRUE(PowerModelApply(models, &algorithm, &input, &output));
    ASSERT_NEAR(54.2f, output.allocated_power_w[0], 1.0e-3f);
    ASSERT_NEAR(14.6f, output.allocated_power_w[1], 1.0e-3f);
    ASSERT_NEAR(98.0f, sum4(output.allocated_power_w), 1.0e-4f);
}
```

Also include cases for low-budget proportional allocation, attenuation clamping, and current hard clamping.

- [ ] **Step 3: Add the host test Makefile and verify the tests fail**

Write `tests/power_model/Makefile`:

```make
CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_power_model.exe
SOURCE := test_power_model.c ../../modules/algorithm/power_model.c
INCLUDES := -I../../modules/algorithm

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -lm -o $(TARGET)

test: $(TARGET)
	./$(TARGET)

clean:
	$(RM) $(TARGET)
```

Run: `make -C tests/power_model test`

Expected: FAIL because `power_model.c` has not implemented the declared functions.

- [ ] **Step 4: Implement validation, prediction, allocation, and limiting**

Implement `modules/algorithm/power_model.c` with these rules:

```c
static float ClampFloat(float value, float minimum, float maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

float PowerModelPredict(const MotorPowerModelConfig_s *config,
                        float current_command,
                        float speed_rpm)
{
    if (!PowerModelConfigIsValid(config) || !isfinite(current_command) || !isfinite(speed_rpm))
        return NAN;

    const float current = fabsf(current_command / config->current_conversion);
    const float speed = fabsf(speed_rpm);
    return config->k0 + config->k1 * current + config->k2 * speed
         + config->k3 * current * speed + config->k4 * current * current
         + config->k5 * speed * speed;
}
```

For power allocation, compute `effective_limit_w = referee_power_limit_w * clamp(attenuation, 0, 1) * safety_factor`. Use equal shares when total absolute error is at most `small_error_threshold_rpm`, proportional shares below `reserved_power_threshold_w`, and `per_motor_reserved_power_w` plus proportional remainder at or above it.

For an over-budget motor, solve:

```c
const float current = fabsf(desired_current / model->current_conversion);
const float speed = fabsf(speed_rpm);
const float a = model->k4 * current * current;
const float b = (model->k1 + model->k3 * speed) * current;
const float c = model->k0 + model->k2 * speed
              + model->k5 * speed * speed - power_limit_w;
```

Handle `fabsf(a) < 1.0e-6f` as a linear equation and actually multiply the desired current by the resulting scale. For a quadratic, clamp a discriminant in `[-1.0e-5f, 0]` to zero, reject more-negative discriminants, and select the largest finite root in `[0, 1]`. If no valid root exists, use scale `0`. Recompute limited predicted power and reject non-finite results.

- [ ] **Step 5: Add edge-case tests**

Add explicit cases for:

```c
test_no_limit_keeps_current();
test_over_limit_scales_current();
test_linear_branch_scales_current();
test_negative_discriminant_zeros_current();
test_invalid_budget_fails_safe();
test_nan_input_fails_safe();
test_invalid_conversion_rejected();
test_each_allocation_is_non_negative();
test_allocation_sum_never_exceeds_budget();
```

The linear-branch fixture must use `.k4 = 0.0f`, a positive linear coefficient, and an expected scale strictly between zero and one so it catches the source C++ bug.

- [ ] **Step 6: Run host tests**

Run: `make -C tests/power_model clean test`

Expected: all tests print `PASS` and the process exits with code `0`.

- [ ] **Step 7: Commit the pure algorithm**

```bash
git add modules/algorithm/power_model.c modules/algorithm/power_model.h tests/power_model
git commit -m "feat: add testable chassis power model"
```

---

### Task 2: Add the chassis power group to the shared DJI driver

**Files:**

- Modify: `modules/motor/DJImotor/dji_motor.h`
- Modify: `modules/motor/DJImotor/dji_motor.c`

**Interfaces:**

- Consumes: `PowerModelApply()` from Task 1 and four existing `DJIMotorInstance *` values.
- Produces: `DJIChassisPowerRegister()`, `DJIChassisPowerSetLimit()`, `DJIChassisPowerSetAttenuation()`, and `DJIChassisPowerGetState()`.

- [ ] **Step 1: Add driver-facing group types and APIs**

After `DJIMotorInstance` is defined in `dji_motor.h`, add:

```c
#include "power_model.h"

typedef struct
{
    DJIMotorInstance *motors[POWER_MODEL_MOTOR_COUNT];
    MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT];
    ChassisPowerAlgorithmConfig_s algorithm;
} DJIChassisPowerConfig_s;

typedef struct
{
    bool registered;
    ChassisPowerOutput_s power;
} DJIChassisPowerState_s;

bool DJIChassisPowerRegister(const DJIChassisPowerConfig_s *config);
void DJIChassisPowerSetLimit(float referee_power_limit_w);
void DJIChassisPowerSetAttenuation(float attenuation);
const DJIChassisPowerState_s *DJIChassisPowerGetState(void);
```

- [ ] **Step 2: Add private group state and registration validation**

In `dji_motor.c`, add one static group:

```c
typedef struct
{
    bool registered;
    uint8_t motor_indices[POWER_MODEL_MOTOR_COUNT];
    MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT];
    ChassisPowerAlgorithmConfig_s algorithm;
    ChassisPowerInput_s input;
    DJIChassisPowerState_s state;
} DJIChassisPowerGroup_s;

static DJIChassisPowerGroup_s chassis_power_group = {
    .input.attenuation = 1.0f,
};
```

`DJIChassisPowerRegister()` must reject a null config, repeated registration, null motor pointers, duplicate motor pointers, non-M3508 motors, motors absent from `dji_motor_instance[]`, and values rejected by `PowerModelConfigIsValid()` or `ChassisPowerAlgorithmConfigIsValid()`. Resolve and store each motor's `dji_motor_instance[]` index once during registration.

- [ ] **Step 3: Split `DJIMotorControl()` into compute, apply, and send phases**

Introduce fixed working arrays:

```c
static float motor_output[DJI_MOTOR_CNT];
static float speed_reference_aps[DJI_MOTOR_CNT];
static float speed_feedback_aps[DJI_MOTOR_CNT];
```

During the first loop, preserve the existing PID order. Immediately before each enabled speed-loop `PIDCalculate()`, store its `pid_ref` and `pid_measure`; after all loops and feedback reversal, store the resulting `pid_ref` in `motor_output[i]`. Do not pack or transmit CAN data in this loop.

Apply the power hook once:

```c
static void ApplyChassisPowerGroup(void)
{
    if (!chassis_power_group.registered)
        return;

    for (size_t slot = 0; slot < POWER_MODEL_MOTOR_COUNT; ++slot)
    {
        const uint8_t index = chassis_power_group.motor_indices[slot];
        chassis_power_group.input.desired_current[slot] = motor_output[index];
        chassis_power_group.input.speed_rpm[slot] = speed_feedback_aps[index] / 6.0f;
        chassis_power_group.input.speed_error_rpm[slot] =
            (speed_reference_aps[index] - speed_feedback_aps[index]) / 6.0f;
    }

    const bool ok = PowerModelApply(chassis_power_group.models,
                                    &chassis_power_group.algorithm,
                                    &chassis_power_group.input,
                                    &chassis_power_group.state.power);

    for (size_t slot = 0; slot < POWER_MODEL_MOTOR_COUNT; ++slot)
    {
        const uint8_t index = chassis_power_group.motor_indices[slot];
        DJIMotorInstance *motor = dji_motor_instance[index];
        motor_output[index] = ok && DaemonIsOnline(motor->daemon)
                            ? chassis_power_group.state.power.limited_current[slot]
                            : 0.0f;
    }
}
```

The second loop clamps finite outputs to the motor command range, converts them to `int16_t`, applies `MOTOR_STOP`, and packs the existing sender buffers. The final sender loop remains unchanged.

- [ ] **Step 4: Add compile-time and runtime safety checks**

Use `_Static_assert(POWER_MODEL_MOTOR_COUNT == 4U, "DJI chassis power group must contain four motors");`. Ensure a non-finite ordinary motor output is sent as zero. Preserve existing CAN grouping and ID collision behavior.

- [ ] **Step 5: Run host tests and compile the driver translation unit**

Run:

```bash
make -C tests/power_model clean test
make -C . build/dji_motor.o
```

Expected: host tests pass and `dji_motor.c` compiles with `-Werror`.

- [ ] **Step 6: Commit the shared-driver hook**

```bash
git add modules/motor/DJImotor/dji_motor.c modules/motor/DJImotor/dji_motor.h
git commit -m "feat: hook chassis power management into DJI control"
```

---

### Task 3: Move the chassis to the shared DJI driver

**Files:**

- Modify: `application/chassis/chassis.c`
- Modify: `modules/super_cap/super_cap.h`

**Interfaces:**

- Consumes: the four Task 2 registration/update APIs.
- Produces: one fully configured four-M3508 power group and four degree-per-second speed references.

- [ ] **Step 1: Replace power-control includes and initialization**

Remove `#include "power_control.h"`; `dji_motor.h` is already available through the motor types used by the file, but include it explicitly for ownership clarity.

Replace all four `PowerControlInit()` calls with `DJIMotorInit()`. After the fourth motor initializes, register this configuration:

```c
static const MotorPowerModelConfig_s m3508_power_model = {
    .k0 = 0.65213f,
    .k1 = -0.15659f,
    .k2 = 0.00041660f,
    .k3 = 0.00235415f,
    .k4 = 0.20022f,
    .k5 = 1.08e-7f,
    .current_conversion = 1000.0f,
};

DJIChassisPowerConfig_s power_config = {
    .motors = {motor_lf, motor_rf, motor_lb, motor_rb},
    .models = {
        m3508_power_model,
        m3508_power_model,
        m3508_power_model,
        m3508_power_model,
    },
    .algorithm = {
        .safety_factor = 0.95f,
        .small_error_threshold_rpm = 500.0f,
        .reserved_power_threshold_w = 54.0f,
        .per_motor_reserved_power_w = 8.0f,
        .max_current_command = 15000.0f,
    },
};

if (!DJIChassisPowerRegister(&power_config))
{
    DJIMotorStop(motor_lf);
    DJIMotorStop(motor_rf);
    DJIMotorStop(motor_lb);
    DJIMotorStop(motor_rb);
}
DJIChassisPowerSetAttenuation(1.0f);
```

- [ ] **Step 2: Preserve speed-loop behavior across the unit change**

Change chassis speed PID `Kp` from `4.5f` to `0.75f`. Keep `Ki` and `Kd` at zero. Interpret `vt_lf`, `vt_rf`, `vt_lb`, and `vt_rb` as rpm, and set shared-driver references in degrees per second:

```c
DJIMotorSetRef(motor_lf, vt_lf * 6.0f);
DJIMotorSetRef(motor_rf, vt_rf * 6.0f);
DJIMotorSetRef(motor_lb, vt_lb * 6.0f);
DJIMotorSetRef(motor_rb, vt_rb * 6.0f);
```

This also fixes the existing missing left-front call.

- [ ] **Step 3: Replace the legacy budget update**

Replace `SetPowerLimit(...)` in `ChassisTask()` with:

```c
DJIChassisPowerSetLimit((float)referee_data->GameRobotState.chassis_power_limit);
```

Do not derive attenuation from `cap_msg.vol`, `cap_msg.current`, or `cap_msg.power` in this task.

- [ ] **Step 4: Declare the existing supercapacitor getter**

Add to `modules/super_cap/super_cap.h`:

```c
SuperCap_Msg_s SuperCapGet(SuperCapInstance *instance);
```

- [ ] **Step 5: Compile the changed application units**

Run:

```bash
make -C . build/chassis.o build/super_cap.o
```

Expected: both translation units compile without warnings.

- [ ] **Step 6: Commit the chassis migration**

```bash
git add application/chassis/chassis.c modules/super_cap/super_cap.h
git commit -m "feat: migrate chassis motors to shared DJI driver"
```

---

### Task 4: Delete the legacy power-control path and update builds

**Files:**

- Delete: `modules/motor/power_control.c`
- Delete: `modules/motor/power_control.h`
- Modify: `modules/motor/motor_task.c`
- Modify: `Makefile`

**Interfaces:**

- Consumes: `DJIMotorControl()` with its Task 2 power hook.
- Produces: a single DJI control invocation and no remaining legacy symbols.

- [ ] **Step 1: Remove the second task call**

In `motor_task.c`, remove:

```c
#include "power_control.h"
PowerControl();
```

Keep the existing single `DJIMotorControl();` call.

- [ ] **Step 2: Update the hand-written Makefile**

Replace:

```make
modules/motor/power_control.c \
```

with:

```make
modules/algorithm/power_model.c \
```

No CMake source-list edit is needed because CMake recursively collects `modules/*.c`.

- [ ] **Step 3: Delete the legacy implementation**

Delete `modules/motor/power_control.c` and `modules/motor/power_control.h` after all references have moved.

- [ ] **Step 4: Prove no legacy symbols remain**

Run:

```bash
rg -n "PowerControlInit|PowerControl\(|SetPowerLimit|power_control\.h|power_control\.c|TORQUE_COEF|POWER_COEF|initial_give_power" modules application Makefile CMakeLists.txt
```

Expected: no matches.

- [ ] **Step 5: Run a clean firmware build**

Run:

```bash
make clean
make -j4
```

Expected: `build/basic_framework.elf`, `.hex`, and `.bin` are produced with no warnings or errors.

- [ ] **Step 6: Commit legacy removal**

```bash
git add modules/motor/motor_task.c modules/motor/power_control.c modules/motor/power_control.h Makefile
git commit -m "refactor: remove duplicated chassis motor driver"
```

---

### Task 5: Document, audit, and verify the complete migration

**Files:**

- Create: `modules/motor/power_control.md`
- Modify: `docs/superpowers/specs/2026-09-06-power-control-dji-hook-design.md`

**Interfaces:**

- Consumes: final API and measured build output from Tasks 1–4.
- Produces: maintainers' integration guide and final verification evidence.

- [ ] **Step 1: Write the module guide**

Document these exact facts in `modules/motor/power_control.md`:

- all DJI motors use `DJIMotorInit()` and `DJIMotorControl()`;
- the chassis registers one four-M3508 power group;
- application speed targets are converted from rpm to degrees per second using `× 6`;
- model inputs convert degrees per second back to rpm using `÷ 6`;
- current commands use DJI raw units divided by `1000` for the model;
- initial coefficients and allocation thresholds;
- invalid power input zeros chassis current only;
- attenuation remains `1.0f` until a real energy protocol exists;
- real-vehicle fitting and safety-factor procedure.

- [ ] **Step 2: Run the full verification suite**

Run:

```bash
make -C tests/power_model clean test
make clean
make -j4
arm-none-eabi-size build/basic_framework.elf
git diff --check
```

Expected: host tests pass, firmware builds, size is reported, and `git diff --check` is silent.

- [ ] **Step 3: Audit architectural invariants**

Run:

```bash
rg -n "DJIMotorControl\(" modules application Src
rg -n "DJIMotorInit\(" application
rg -n "DJIChassisPower" modules application
rg -n "power_control\.(c|h)" . -g '!docs/superpowers/**'
```

Verify exactly one runtime `DJIMotorControl()` call exists, the chassis uses `DJIMotorInit()`, the four-motor group is registered once, and no source/build reference to the deleted legacy files remains.

- [ ] **Step 4: Inspect the final diff for non-chassis regressions**

Compare the pre- and post-refactor control order for angle loop, speed loop, current loop, feedforward, motor reverse, feedback reverse, stop behavior, CAN grouping, and sender enable flags. Confirm every ordinary DJI motor still receives the same output for the same state.

- [ ] **Step 5: Commit documentation and verification notes**

```bash
git add modules/motor/power_control.md docs/superpowers/specs/2026-09-06-power-control-dji-hook-design.md
git commit -m "docs: document unified chassis power control"
```

---

## Real-Vehicle Acceptance Sequence

After software verification, do not immediately use `0.98f` on the ground. Use the following controlled sequence:

1. Start with wheels lifted and safety factor `0.90f`; inspect `DJIChassisPowerGetState()` in Ozone.
2. Confirm all four unlimited predicted powers respond to current and rpm with the correct wheel mapping.
3. Confirm limited predicted power does not exceed each allocated value except when zero-current rotating loss itself exceeds that allocation.
4. Run low-speed straight motion, lateral motion, acceleration, braking, and translation plus spin.
5. Log referee `chassis_power`, configured limit, predicted total, allocation total, raw currents, and limited currents.
6. Refit `k0` through `k5` if prediction error is systematic across operating points.
7. Raise safety factor from `0.90f` toward `0.98f` only after measured overshoot stays within the team's accepted buffer margin.
