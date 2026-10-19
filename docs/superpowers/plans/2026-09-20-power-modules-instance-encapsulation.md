# Power Modules Instance Encapsulation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move the power-meter, super-capacitor, and chassis-power runtime instance pointers into their owning module `.c` files while preserving current control behavior.

**Architecture:** Each public instance type remains in its owning header. Each implementation owns one file-static pointer, allocates it during first initialization, and exposes singleton APIs so `chassis.c` never stores or passes the module instance. The pure `power_model` math API remains stateless; only the DJI chassis-power integration state changes from a file-local value to a module-owned pointer.

**Tech Stack:** C11, STM32 HAL, FreeRTOS critical sections, YueLu CAN/daemon modules, GNU Make host tests, ARM GCC firmware build.

## Global Constraints

- Preserve CAN protocols, power-model coefficients, control periods, PID values, and power-budget behavior.
- Keep user changes in `.vscode/launch.json`, `application/chassis/chassis.c`, `application/cmd/robot_cmd.c`, `application/robot_def.h`, and `modules/algorithm/chassis_power_budget.h` unless a directly overlapping line must change.
- Do not add multi-instance registries or deinitialization APIs.
- Do not expose a production-only reset function for tests.
- All public snapshot operations retain their existing critical sections.

---

### Task 1: Encapsulate the power-meter instance pointer

**Files:**
- Modify: `tests/power_meter/test_power_meter.c`
- Modify: `modules/power_meter/power_meter.h`
- Modify: `modules/power_meter/power_meter.c`

**Interfaces:**
- Consumes: `PowerMeter_Init_Config_s`, `PowerMeterStatus_s`, `CANRegister()`, `DaemonRegister()`.
- Produces: `bool PowerMeterInit(const PowerMeter_Init_Config_s *config)` and `bool PowerMeterGetStatus(PowerMeterStatus_s *status)`.

- [ ] **Step 1: Change the host test to the singleton contract**

Add uninitialized checks before the suite's one successful initialization, then use captured `CANInstance.id` only for diagnostic assertions:

```c
ASSERT_FALSE(PowerMeterGetStatus(&status));
ASSERT_FALSE(PowerMeterInit(NULL));
ASSERT_TRUE(PowerMeterInit(&config));
ASSERT_FALSE(PowerMeterInit(&config));
ASSERT_TRUE(PowerMeterGetStatus(&status));

PowerMeterInstance *meter =
    (PowerMeterInstance *)captured_can_config.id;
ASSERT_TRUE(meter != NULL);
ASSERT_EQ_U32(1U, meter->received_frame_count);
```

Keep one initialization per test executable and reset only fake counters/buffers that do not invalidate the registered module instance.

- [ ] **Step 2: Run the power-meter test and verify RED**

Run: `mingw32-make -C tests/power_meter clean test`

Expected: compilation fails because the old header returns `PowerMeterInstance *` and `PowerMeterGetStatus()` still requires an instance argument.

- [ ] **Step 3: Implement the module-owned pointer**

At the top of `power_meter.c`, add:

```c
static PowerMeterInstance *power_meter;
```

Update initialization to validate `config` and `power_meter == NULL`, allocate a local candidate, fully register it, then publish it only on success:

```c
bool PowerMeterInit(const PowerMeter_Init_Config_s *config)
{
    if (config == NULL || power_meter != NULL)
        return false;

    PowerMeterInstance *instance = malloc(sizeof(*instance));
    if (instance == NULL)
        return false;
    memset(instance, 0, sizeof(*instance));
    /* existing daemon and CAN setup */
    power_meter = instance;
    return true;
}
```

Update the getter to use the static pointer and retain the critical section:

```c
bool PowerMeterGetStatus(PowerMeterStatus_s *status)
{
    if (power_meter == NULL || status == NULL || power_meter->daemon == NULL)
        return false;
    taskENTER_CRITICAL();
    *status = power_meter->status;
    status->online = PowerMeterIsFresh(power_meter, HAL_GetTick());
    taskEXIT_CRITICAL();
    return true;
}
```

- [ ] **Step 4: Run the power-meter test and verify GREEN**

Run: `mingw32-make -C tests/power_meter clean test`

Expected: `PASS: power meter module tests`.

- [ ] **Step 5: Commit the isolated power-meter change**

```bash
git add modules/power_meter/power_meter.h modules/power_meter/power_meter.c tests/power_meter/test_power_meter.c
git commit -m "refactor: encapsulate power meter instance"
```

### Task 2: Encapsulate the super-capacitor instance pointer

**Files:**
- Create: `tests/super_cap/Makefile`
- Create: `tests/super_cap/test_super_cap.c`
- Create: `tests/super_cap/stubs/FreeRTOS.h`
- Create: `tests/super_cap/stubs/task.h`
- Create: `tests/super_cap/stubs/bsp_can.h`
- Create: `tests/super_cap/stubs/daemon.h`
- Modify: `modules/super_cap/super_cap.h`
- Modify: `modules/super_cap/super_cap.c`

**Interfaces:**
- Consumes: `SuperCap_Init_Config_s`, `SuperCapCommand_s`, `SuperCapStatus_s`, `CANRegister()`, `CANTransmit()`, `DaemonRegister()`.
- Produces: `bool SuperCapInit(const SuperCap_Init_Config_s *config)`, `bool SuperCapSendCommand(const SuperCapCommand_s *command)`, and `bool SuperCapGetStatus(SuperCapStatus_s *status)`.

- [ ] **Step 1: Add a host test for the singleton behavior**

Use the same narrow HAL/CAN/daemon fakes as the power-meter module and test behavior through the public API:

```c
SuperCapStatus_s status = {0};
const SuperCapCommand_s command = {
    .enable_dcdc = true,
    .referee_power_limit_w = 100U,
    .referee_buffer_energy_j = 50U,
};

ASSERT_FALSE(SuperCapGetStatus(&status));
ASSERT_FALSE(SuperCapSendCommand(&command));
ASSERT_FALSE(SuperCapInit(NULL));
ASSERT_TRUE(SuperCapInit(&config));
ASSERT_FALSE(SuperCapInit(&config));
ASSERT_TRUE(SuperCapSendCommand(&command));
ASSERT_TRUE(SuperCapGetStatus(&status));
```

Deliver a valid `0x051` frame through the captured CAN callback and assert the public status values, online transition, daemon reload, and exact encoded command bytes.

- [ ] **Step 2: Run the super-cap test and verify RED**

Run: `mingw32-make -C tests/super_cap clean test`

Expected: compilation fails because the old API requires a `SuperCapInstance *` parameter and returns an instance pointer.

- [ ] **Step 3: Implement the module-owned pointer**

At the top of `super_cap.c`, add:

```c
static SuperCapInstance *cap;
```

Allocate and fully initialize a local candidate in `SuperCapInit()`, publish it to `cap` only after successful dependency registration, and reject repeated initialization. Change send/get functions to validate and use `cap` internally while preserving callback ownership through `CANInstance.id`.

- [ ] **Step 4: Run the super-cap tests and verify GREEN**

Run:

```bash
mingw32-make -C tests/super_cap clean test
mingw32-make -C tests/super_cap_protocol clean test
```

Expected: both module and protocol suites pass.

- [ ] **Step 5: Commit the isolated super-cap change**

```bash
git add modules/super_cap/super_cap.h modules/super_cap/super_cap.c tests/super_cap
git commit -m "refactor: encapsulate super capacitor instance"
```

### Task 3: Convert the DJI chassis-power state to a module-owned pointer

**Files:**
- Modify: `modules/motor/DJImotor/dji_motor.h`
- Modify: `modules/motor/DJImotor/dji_motor.c`

**Interfaces:**
- Consumes: `DJIChassisPowerConfig_s`, `PowerModelApply()`, `ChassisPowerTelemetryBuild()`.
- Produces: public `DJIChassisPowerInstance_s` type and unchanged singleton control/snapshot functions.

- [ ] **Step 1: Record the green characterization baseline**

Run:

```bash
mingw32-make -C tests/power_model clean test
make -j4
```

Expected: the existing mathematical behavior tests pass and the current value-backed DJI integration compiles. This task is an internal ownership refactor with no intended algorithm change, so it is protected by characterization tests and the integration compiler rather than a source-layout assertion.

- [ ] **Step 2: Move the structure type and allocate the instance on registration**

Move the existing private fields into `dji_motor.h`:

```c
typedef struct
{
    bool registered;
    uint8_t motor_indices[POWER_MODEL_MOTOR_COUNT];
    MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT];
    ChassisPowerAlgorithmConfig_s algorithm;
    ChassisPowerInput_s input;
    DJIChassisPowerState_s state;
    volatile float measured_chassis_power_w;
} DJIChassisPowerInstance_s;
```

Replace the file-static value with:

```c
static DJIChassisPowerInstance_s *chassis_power;
```

In `DJIChassisPowerRegister()`, validate the complete configuration before allocation, allocate and zero a candidate, fill it, then assign `chassis_power = instance`. Update all member access to `->`. Before registration, setters are no-ops, `DJIChassisPowerGetState()` returns `NULL`, `DJIChassisPowerGetSnapshot()` returns `false`, and the motor loop skips application.

- [ ] **Step 3: Run power algorithm tests and ARM compile**

Run:

```bash
mingw32-make -C tests/power_model clean test
make -j4
```

Expected: power-model tests pass and firmware compiles without a null dereference or type error.

- [ ] **Step 4: Commit the isolated chassis-power instance change**

```bash
git add modules/motor/DJImotor/dji_motor.h modules/motor/DJImotor/dji_motor.c tests
git commit -m "refactor: encapsulate chassis power instance"
```

### Task 4: Remove application-owned module pointers and run regression verification

**Files:**
- Modify: `application/chassis/chassis.c`

**Interfaces:**
- Consumes: the singleton APIs from Tasks 1 and 2.
- Produces: a chassis application with no `PowerMeterInstance *` or `SuperCapInstance *` storage.

- [ ] **Step 1: Update application calls before implementation and verify RED**

Delete the two file-static application pointers and change calls to:

```c
(void)SuperCapInit(&cap_conf);
(void)PowerMeterInit(&power_meter_config);
(void)SuperCapSendCommand(&link_output.command);
(void)SuperCapGetStatus(&cap_status);
if (PowerMeterGetStatus(&meter_status))
    meter_valid = meter_status.online;
```

Run: `make -j4`

Expected before Tasks 1 and 2 are present: signature errors. When executing sequentially after Tasks 1 and 2, use `git show HEAD^` or the recorded RED results from those tests as the required failing-interface evidence; do not revert working production code.

- [ ] **Step 2: Finish the application integration**

Preserve all surrounding user changes and only update the pointer declarations, initialization assignments, null checks, and API argument lists directly required by the new module ownership.

- [ ] **Step 3: Run all relevant host regression tests**

Run each existing relevant suite:

```bash
mingw32-make -C tests/power_meter clean test
mingw32-make -C tests/power_meter_protocol clean test
mingw32-make -C tests/super_cap clean test
mingw32-make -C tests/super_cap_protocol clean test
mingw32-make -C tests/power_model clean test
mingw32-make -C tests/chassis_power_budget clean test
mingw32-make -C tests/chassis_power_outer_loop clean test
mingw32-make -C tests/chassis_power_feedback clean test
```

Expected: every suite prints `PASS`.

- [ ] **Step 4: Build both power-meter configurations**

Run:

```bash
make -j4
make -j4 POWER_METER_ENABLE=0
```

Both builds must complete without new errors or warnings attributable to this change.

- [ ] **Step 5: Review the diff and commit integration**

Run:

```bash
git diff --check
git diff -- modules/power_meter modules/super_cap modules/motor/DJImotor application/chassis/chassis.c tests
```

Confirm every changed line traces to instance ownership, API migration, or its tests, then commit:

```bash
git add application/chassis/chassis.c
git commit -m "refactor: keep power module instances in module layer"
```
