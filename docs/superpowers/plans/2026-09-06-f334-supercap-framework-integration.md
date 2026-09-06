# F334 Supercapacitor Framework Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Connect the NSEonLu F407 chassis framework to the independent F334 supercapacitor controller over its native CAN protocol and feed a safe, energy-aware total power budget into the existing four-M3508 limiter.

**Architecture:** Keep protocol encoding and decoding pure and host-testable, keep CAN lifetime/watchdog behavior in the existing `super_cap` module, and add a pure `chassis_power_budget` policy between supercapacitor status and the DJI power group. `ChassisTask()` sends referee data at 200 Hz and updates the motor budget; the existing motor power model remains the sole per-motor limiter.

**Tech Stack:** C11, STM32F407 HAL/BxCAN, CMSIS-RTOS/FreeRTOS, existing daemon service, existing DJI motor and power-model modules, GNU Make, host GCC tests.

## Global Constraints

- Preserve the independent F334 ADC, HRTIM, PID, DCDC, and protection loops.
- Use standard CAN ID `0x061` for F407-to-F334 commands and `0x051` for F334-to-F407 status; both frames have DLC 8.
- Encode integers and IEEE-754 floats explicitly as little-endian bytes; do not cast CAN buffers to packed structs.
- Send the original referee power limit and original referee buffer energy to F334, never the boosted motor budget.
- Treat a status stream as offline after 200 ms without a valid frame.
- Allow boost only when the capacitor is online, output-enabled, error-free, and above 10% energy.
- Use 10% and 30% energy breakpoints, a 100 W boost ceiling, immediate downward changes, and a 200 W/s upward slew limit.
- Preserve the existing power model and its 0.95 safety factor; do not apply the old attenuation field a second time.
- A capacitor fault removes boost but does not directly stop the chassis motors.
- Preserve the user's existing uncommitted edits in `application/chassis/chassis.c` and `application/robot_def.h`; never stage those lines unless the integration task changes the same file deliberately, and keep the expanded Kp comment and current bench-mode value intact.
- Do not add a second DJI motor send path, a second power feedback controller, multi-capacitor support, or a versioned multi-frame protocol.

---

## File Structure

- `modules/super_cap/super_cap_protocol.h`: wire constants, protocol-level command/status types, pure codec signatures.
- `modules/super_cap/super_cap_protocol.c`: explicit little-endian command encoder and status decoder.
- `modules/super_cap/super_cap.h`: runtime instance, online status, typed public driver API.
- `modules/super_cap/super_cap.c`: CAN registration, callback, daemon, snapshot, and command transmission.
- `modules/algorithm/chassis_power_budget.h`: budget configuration, input/output/state types and public policy API.
- `modules/algorithm/chassis_power_budget.c`: pure energy mapping, fault fallback, and slew limiting.
- `application/chassis/chassis.c`: 200 Hz orchestration only; no protocol byte manipulation and no per-motor power math.
- `modules/motor/DJImotor/dji_motor.h/.c`: rename the public total-budget setter without changing the algorithm.
- `tests/super_cap_protocol/`: host codec tests.
- `tests/chassis_power_budget/`: host policy tests.
- `docs/supercap-can-protocol.md`: operator-facing wire contract and CAN-analyzer examples.

---

### Task 1: Add the pure supercapacitor CAN codec

**Files:**
- Create: `modules/super_cap/super_cap_protocol.h`
- Create: `modules/super_cap/super_cap_protocol.c`
- Create: `tests/super_cap_protocol/test_super_cap_protocol.c`
- Create: `tests/super_cap_protocol/Makefile`
- Modify: `Makefile:142-150`

**Interfaces:**
- Consumes: an application command or raw 8-byte F334 status frame.
- Produces: `bool SuperCapProtocolEncodeCommand(const SuperCapCommand_s *, uint8_t frame[8])` and `bool SuperCapProtocolDecodeStatus(const uint8_t frame[8], uint8_t dlc, SuperCapStatus_s *)`.

- [ ] **Step 1: Create the protocol header**

```c
#ifndef SUPER_CAP_PROTOCOL_H
#define SUPER_CAP_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#define SUPERCAP_COMMAND_CAN_ID 0x061U
#define SUPERCAP_STATUS_CAN_ID 0x051U
#define SUPERCAP_CAN_DLC 8U

#define SUPERCAP_ERROR_UNDER_VOLTAGE 0x01U
#define SUPERCAP_ERROR_OVER_VOLTAGE 0x02U
#define SUPERCAP_ERROR_BUCK_BOOST 0x04U
#define SUPERCAP_ERROR_SHORT_CIRCUIT 0x08U
#define SUPERCAP_ERROR_HIGH_TEMPERATURE 0x10U
#define SUPERCAP_ERROR_NO_POWER_INPUT 0x20U
#define SUPERCAP_ERROR_CAPACITOR 0x40U
#define SUPERCAP_OUTPUT_DISABLED_MASK 0x80U
#define SUPERCAP_ERROR_MASK 0x7FU

typedef struct
{
    bool enable_dcdc;
    uint16_t referee_power_limit_w;
    uint16_t referee_buffer_energy_j;
} SuperCapCommand_s;

typedef struct
{
    bool online;
    bool output_enabled;
    uint8_t error_code;
    float chassis_power_w;
    uint16_t available_power_limit_w;
    float energy_ratio;
} SuperCapStatus_s;

bool SuperCapProtocolEncodeCommand(const SuperCapCommand_s *command,
                                   uint8_t frame[SUPERCAP_CAN_DLC]);
bool SuperCapProtocolDecodeStatus(const uint8_t frame[SUPERCAP_CAN_DLC],
                                  uint8_t dlc,
                                  SuperCapStatus_s *status);

#endif
```

- [ ] **Step 2: Write codec tests before the implementation**

Create a small assertion harness matching `tests/power_model/test_power_model.c`. Add these exact cases:

```c
static void TestEncodeKnownCommand(void)
{
    const SuperCapCommand_s command = {
        .enable_dcdc = true,
        .referee_power_limit_w = 100U,
        .referee_buffer_energy_j = 50U,
    };
    const uint8_t expected[8] = {0x01, 0x64, 0x00, 0x32, 0x00, 0, 0, 0};
    uint8_t actual[8] = {0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5};

    ASSERT_TRUE(SuperCapProtocolEncodeCommand(&command, actual));
    ASSERT_BYTES(expected, actual, 8U);
}

static void TestDecodeKnownStatus(void)
{
    const uint8_t frame[8] = {0x00, 0x00, 0x00, 0xF1, 0x42, 0xB4, 0x00, 0x80};
    SuperCapStatus_s status = {0};

    ASSERT_TRUE(SuperCapProtocolDecodeStatus(frame, 8U, &status));
    ASSERT_TRUE(status.online);
    ASSERT_TRUE(status.output_enabled);
    ASSERT_EQ_INT(0, status.error_code);
    ASSERT_NEAR(120.5f, status.chassis_power_w, 0.001f);
    ASSERT_EQ_INT(180, status.available_power_limit_w);
    ASSERT_NEAR(128.0f / 255.0f, status.energy_ratio, 0.0001f);
}

static void TestDecodeDisabledAndErrors(void)
{
    const uint8_t frame[8] = {0xC5, 0, 0, 0, 0, 0x64, 0, 0};
    SuperCapStatus_s status = {0};

    ASSERT_TRUE(SuperCapProtocolDecodeStatus(frame, 8U, &status));
    ASSERT_FALSE(status.output_enabled);
    ASSERT_EQ_INT(0x45, status.error_code);
}

static void TestRejectsBadLengthAndNan(void)
{
    const uint8_t finite_frame[8] = {0};
    const uint8_t nan_frame[8] = {0, 0, 0, 0xC0, 0x7F, 0, 0, 0};
    SuperCapStatus_s status = {0};

    ASSERT_FALSE(SuperCapProtocolDecodeStatus(finite_frame, 7U, &status));
    ASSERT_FALSE(SuperCapProtocolDecodeStatus(nan_frame, 8U, &status));
    ASSERT_FALSE(SuperCapProtocolDecodeStatus(NULL, 8U, &status));
    ASSERT_FALSE(SuperCapProtocolDecodeStatus(finite_frame, 8U, NULL));
}
```

Also assert that null command/output pointers return `false`, reserved command bytes are zero, and a negative finite chassis power decodes successfully.

- [ ] **Step 3: Add the host test Makefile and verify the tests fail**

```make
CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_super_cap_protocol.exe
SOURCE := test_super_cap_protocol.c ../../modules/super_cap/super_cap_protocol.c
INCLUDES := -I../../modules/super_cap

.PHONY: all test clean
all: $(TARGET)
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -lm -o $(TARGET)
test: $(TARGET)
	./$(TARGET)
clean:
	-if exist $(TARGET) del /Q $(TARGET)
```

Run: `make -C tests/super_cap_protocol clean test`

Expected: FAIL because `super_cap_protocol.c` has no codec implementation.

- [ ] **Step 4: Implement explicit little-endian encoding and decoding**

Use helpers with these exact semantics:

```c
static uint16_t ReadU16LE(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static void WriteU16LE(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static float ReadF32LE(const uint8_t *bytes)
{
    const uint32_t bits = (uint32_t)bytes[0] |
                          ((uint32_t)bytes[1] << 8) |
                          ((uint32_t)bytes[2] << 16) |
                          ((uint32_t)bytes[3] << 24);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}
```

`SuperCapProtocolEncodeCommand()` must zero all eight bytes, set only bit0 in byte 0, and write fields at byte offsets 1 and 3. `SuperCapProtocolDecodeStatus()` must reject non-8 DLC and non-finite `chassis_power_w`, set `online = true`, split byte 0 into the output-disabled bit and lower-seven error bits, and normalize byte 7 by `255.0f`.

- [ ] **Step 5: Run the protocol tests**

Run: `make -C tests/super_cap_protocol clean test`

Expected: `All super capacitor protocol tests passed.`

- [ ] **Step 6: Add the codec source to the firmware build**

Add this line immediately before `modules/super_cap/super_cap.c` in `C_SOURCES`:

```make
modules/super_cap/super_cap_protocol.c \
```

Run: `make build/super_cap_protocol.o`

Expected: the ARM object compiles without warnings.

- [ ] **Step 7: Commit the codec**

```bash
git add modules/super_cap/super_cap_protocol.h modules/super_cap/super_cap_protocol.c tests/super_cap_protocol Makefile
git commit -m "feat: add F334 supercap CAN codec"
```

---

### Task 2: Replace the legacy supercapacitor runtime driver

**Files:**
- Modify: `modules/super_cap/super_cap.h`
- Modify: `modules/super_cap/super_cap.c`

**Interfaces:**
- Consumes: codec functions from Task 1, `CANRegister()`, `CANTransmit()`, `CANSetDLC()`, and daemon APIs.
- Produces: `SuperCapInit()`, `SuperCapSendCommand()`, and `SuperCapGetStatus()` for `ChassisTask()`.

- [ ] **Step 1: Replace the legacy public header**

Make `super_cap.h` include `daemon.h` and `super_cap_protocol.h`. Define the runtime types and signatures exactly as follows:

```c
typedef struct
{
    CANInstance *can_ins;
    DaemonInstance *daemon;
    SuperCapStatus_s status;
} SuperCapInstance;

typedef struct
{
    CAN_Init_Config_s can_config;
    uint16_t offline_reload_count;
} SuperCap_Init_Config_s;

SuperCapInstance *SuperCapInit(const SuperCap_Init_Config_s *config);
bool SuperCapSendCommand(SuperCapInstance *instance,
                         const SuperCapCommand_s *command);
bool SuperCapGetStatus(SuperCapInstance *instance,
                       SuperCapStatus_s *status);
```

Remove `SuperCap_Msg_s`, the global-instance contract, `SuperCapSend(uint8_t *)`, and the by-value `SuperCapGet()` API.

- [ ] **Step 2: Compile to expose every old caller**

Run: `make build/super_cap.o`

Expected: FAIL in `super_cap.c` because it still defines the removed legacy types and functions.

- [ ] **Step 3: Implement instance-bound reception and daemon handling**

Implement the callback using `_instance->id`:

```c
static void SuperCapOffline(void *owner)
{
    SuperCapInstance *instance = (SuperCapInstance *)owner;
    if (instance != NULL)
        instance->status.online = false;
}

static void SuperCapRxCallback(CANInstance *can_instance)
{
    SuperCapInstance *instance = (SuperCapInstance *)can_instance->id;
    SuperCapStatus_s decoded;

    if (instance == NULL ||
        !SuperCapProtocolDecodeStatus(can_instance->rx_buff,
                                      can_instance->rx_len,
                                      &decoded))
        return;

    instance->status = decoded;
    DaemonReload(instance->daemon);
}
```

In `SuperCapInit()`:

- reject a null config;
- allocate and zero one `SuperCapInstance`;
- copy `config->can_config` to a local mutable configuration;
- set callback to `SuperCapRxCallback` and `id` to the allocated instance;
- register CAN and set DLC to 8;
- register a daemon with owner set to the instance, callback `SuperCapOffline`, reload count `offline_reload_count == 0 ? 20 : offline_reload_count`, and the same initial count;
- return `NULL` if allocation or either registration fails.

- [ ] **Step 4: Implement typed command transmission and atomic snapshots**

```c
bool SuperCapSendCommand(SuperCapInstance *instance,
                         const SuperCapCommand_s *command)
{
    if (instance == NULL || instance->can_ins == NULL ||
        !SuperCapProtocolEncodeCommand(command, instance->can_ins->tx_buff))
        return false;
    return CANTransmit(instance->can_ins, 1.0f) != 0U;
}

bool SuperCapGetStatus(SuperCapInstance *instance,
                       SuperCapStatus_s *status)
{
    if (instance == NULL || status == NULL || instance->daemon == NULL)
        return false;

    taskENTER_CRITICAL();
    *status = instance->status;
    status->online = DaemonIsOnline(instance->daemon) != 0U;
    taskEXIT_CRITICAL();
    return true;
}
```

Include `FreeRTOS.h` and `task.h` for the short task-side snapshot critical section. Do not enter a task critical section from the CAN ISR callback.

- [ ] **Step 5: Compile the driver object**

Run: `make build/super_cap.o`

Expected: PASS with no warnings.

- [ ] **Step 6: Commit the runtime driver**

```bash
git add modules/super_cap/super_cap.h modules/super_cap/super_cap.c
git commit -m "feat: replace legacy supercap driver"
```

---

### Task 3: Add the energy-aware chassis budget policy

**Files:**
- Create: `modules/algorithm/chassis_power_budget.h`
- Create: `modules/algorithm/chassis_power_budget.c`
- Create: `tests/chassis_power_budget/test_chassis_power_budget.c`
- Create: `tests/chassis_power_budget/Makefile`
- Modify: `Makefile:141-143`

**Interfaces:**
- Consumes: referee limit, capacitor status fields, elapsed seconds, and previous applied budget.
- Produces: a finite total motor budget plus `OFFLINE`, `DEGRADED`, or `READY` diagnostics.

- [ ] **Step 1: Create the policy header**

```c
#ifndef CHASSIS_POWER_BUDGET_H
#define CHASSIS_POWER_BUDGET_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    CHASSIS_POWER_BUDGET_OFFLINE = 0,
    CHASSIS_POWER_BUDGET_DEGRADED,
    CHASSIS_POWER_BUDGET_READY,
} ChassisPowerBudgetMode_e;

typedef struct
{
    float empty_energy_ratio;
    float full_energy_ratio;
    float max_boost_w;
    float rise_rate_w_per_s;
} ChassisPowerBudgetConfig_s;

typedef struct
{
    float referee_limit_w;
    bool cap_online;
    bool cap_output_enabled;
    uint8_t cap_error_code;
    float cap_energy_ratio;
    float cap_reported_limit_w;
} ChassisPowerBudgetInput_s;

typedef struct
{
    float applied_budget_w;
    ChassisPowerBudgetMode_e mode;
} ChassisPowerBudgetState_s;

typedef struct
{
    float energy_factor;
    float target_budget_w;
    float applied_budget_w;
    ChassisPowerBudgetMode_e mode;
} ChassisPowerBudgetOutput_s;

bool ChassisPowerBudgetConfigIsValid(const ChassisPowerBudgetConfig_s *config);
void ChassisPowerBudgetReset(ChassisPowerBudgetState_s *state,
                             float initial_budget_w);
bool ChassisPowerBudgetUpdate(const ChassisPowerBudgetConfig_s *config,
                              const ChassisPowerBudgetInput_s *input,
                              float dt_s,
                              ChassisPowerBudgetState_s *state,
                              ChassisPowerBudgetOutput_s *output);

#endif
```

- [ ] **Step 2: Write the policy tests first**

Use this exact baseline configuration and helper input:

```c
static const ChassisPowerBudgetConfig_s config = {
    .empty_energy_ratio = 0.10f,
    .full_energy_ratio = 0.30f,
    .max_boost_w = 100.0f,
    .rise_rate_w_per_s = 200.0f,
};

static ChassisPowerBudgetInput_s ReadyInput(float energy)
{
    const ChassisPowerBudgetInput_s input = {
        .referee_limit_w = 100.0f,
        .cap_online = true,
        .cap_output_enabled = true,
        .cap_error_code = 0,
        .cap_energy_ratio = energy,
        .cap_reported_limit_w = 180.0f,
    };
    return input;
}
```

Add exact assertions for:

```c
// Reset to 100 W before each independent energy case and use dt=1.0 s.
energy=0.05f -> mode DEGRADED, factor 0.0f, target 100.0f, applied 100.0f
energy=0.20f -> mode READY,    factor 0.5f, target 140.0f, applied 140.0f
energy=0.50f -> mode READY,    factor 1.0f, target 180.0f, applied 180.0f

// Boost ceiling.
referee=100.0f, reported=400.0f, energy=1.0f -> target 200.0f

// Rise slew.
state.applied=100.0f, target=180.0f, dt=0.005f -> applied 101.0f

// Immediate fall.
state.applied=180.0f, cap_online=false -> mode OFFLINE, applied 100.0f
state.applied=180.0f, output_enabled=false -> mode DEGRADED, applied 100.0f
state.applied=180.0f, error_code=each bit 0x01 through 0x40 -> applied 100.0f

// Invalid base inputs.
referee=0, referee=-1, referee=NAN, dt=-1, dt=NAN -> return false and zero output
```

- [ ] **Step 3: Run the tests to verify failure**

Run: `make -C tests/chassis_power_budget clean test`

Expected: FAIL because the policy functions are not implemented.

- [ ] **Step 4: Implement the minimum policy**

The implementation order must be:

1. validate pointers, finite positive configuration fields, `empty < full <= 1`, finite positive referee limit, and finite nonnegative `dt_s`;
2. choose `OFFLINE` when `cap_online == false`;
3. choose `DEGRADED` when output is disabled, an error bit is set, energy/report fields are non-finite, energy is outside `[0, 1]`, reported power is negative, or energy is at/below the empty threshold;
4. otherwise choose `READY` and calculate the clamped linear energy factor;
5. clamp reported boost to `[0, max_boost_w]`;
6. make the applied budget at least the current referee limit immediately, normalizing a non-finite or negative prior state to the referee limit;
7. apply upward slew `rise_rate_w_per_s * dt_s` without passing the target, but apply every downward transition immediately;
8. reset output and state to zero and return `false` for invalid base inputs.

Use `fminf`, `fmaxf`, and `isfinite`; do not add HAL, CAN, RTOS, or heap dependencies.

`ChassisPowerBudgetReset()` must store `initial_budget_w` only when it is finite and nonnegative; otherwise it stores `0.0f` and sets mode to `OFFLINE`.

- [ ] **Step 5: Add and run the policy Makefile**

Use the same flags as the existing power-model tests:

```make
CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_chassis_power_budget.exe
SOURCE := test_chassis_power_budget.c ../../modules/algorithm/chassis_power_budget.c
INCLUDES := -I../../modules/algorithm
```

Run: `make -C tests/chassis_power_budget clean test`

Expected: `All chassis power budget tests passed.`

- [ ] **Step 6: Add the policy source to firmware and compile it**

Add after `modules/algorithm/power_model.c`:

```make
modules/algorithm/chassis_power_budget.c \
```

Run: `make build/chassis_power_budget.o`

Expected: PASS without warnings.

- [ ] **Step 7: Commit the policy**

```bash
git add modules/algorithm/chassis_power_budget.h modules/algorithm/chassis_power_budget.c tests/chassis_power_budget Makefile
git commit -m "feat: add supercap chassis power budget"
```

---

### Task 4: Rename the DJI total-power setter to budget semantics

**Files:**
- Modify: `modules/motor/DJImotor/dji_motor.h:149-157`
- Modify: `modules/motor/DJImotor/dji_motor.c:303-311`
- Modify: `application/chassis/chassis.c:233-237`

**Interfaces:**
- Consumes: a total available chassis budget in watts.
- Produces: `void DJIChassisPowerSetBudget(float total_budget_w)`.

- [ ] **Step 1: Rename the declaration, definition, parameter, and current caller**

```c
void DJIChassisPowerSetBudget(float total_budget_w);
```

Implementation:

```c
void DJIChassisPowerSetBudget(float total_budget_w)
{
    chassis_power_group.input.referee_power_limit_w = total_budget_w;
}
```

Keep `DJIChassisPowerSetAttenuation()` unchanged and initialized to `1.0f`.

- [ ] **Step 2: Prove the old symbol is gone**

Run: `rg -n "DJIChassisPowerSetLimit" application modules`

Expected: no matches.

- [ ] **Step 3: Compile the affected objects**

Run: `make build/dji_motor.o build/chassis.o`

Expected: `dji_motor.o` passes. `chassis.o` may remain blocked by the Task 2 API change until Task 5 is applied; there must be no missing `DJIChassisPowerSetLimit` reference.

- [ ] **Step 4: Commit only the symbol rename**

Preserve the user's expanded Kp comment in `chassis.c` when staging.

```bash
git add modules/motor/DJImotor/dji_motor.h modules/motor/DJImotor/dji_motor.c
git add -p application/chassis/chassis.c
git commit -m "refactor: name DJI power input as budget"
```

---

### Task 5: Connect referee data, F334 status, and the budget in ChassisTask

**Files:**
- Modify: `application/chassis/chassis.c:14-25,45-61,70-173,217-250`

**Interfaces:**
- Consumes: `SuperCapSendCommand()`, `SuperCapGetStatus()`, `ChassisPowerBudgetUpdate()`, referee state, and DWT elapsed time.
- Produces: 200 Hz F334 commands and `DJIChassisPowerSetBudget(applied_budget_w)` calls.

- [ ] **Step 1: Add fixed policy state and configuration**

Include `chassis_power_budget.h`. Add:

```c
static ChassisPowerBudgetState_s chassis_budget_state;
static ChassisPowerBudgetOutput_s chassis_budget_output;
static uint32_t chassis_budget_dwt_count;

static const ChassisPowerBudgetConfig_s chassis_budget_config = {
    .empty_energy_ratio = 0.10f,
    .full_energy_ratio = 0.30f,
    .max_boost_w = 100.0f,
    .rise_rate_w_per_s = 200.0f,
};
```

- [ ] **Step 2: Replace the old CAN IDs and initialize runtime state**

Use:

```c
SuperCap_Init_Config_s cap_conf = {
    .can_config = {
        .can_handle = &hcan2,
        .tx_id = SUPERCAP_COMMAND_CAN_ID,
        .rx_id = SUPERCAP_STATUS_CAN_ID,
    },
    .offline_reload_count = 20U,
};
cap = SuperCapInit(&cap_conf);
ChassisPowerBudgetReset(&chassis_budget_state, 0.0f);
DWT_GetDeltaT(&chassis_budget_dwt_count);
```

If `cap == NULL`, leave it null; the periodic path must continue with an offline status and referee-only budget.

- [ ] **Step 3: Add bounded referee-to-wire conversion**

Include `<math.h>` and add a file-local helper that returns 0 for non-finite/nonpositive inputs and saturates above `UINT16_MAX`:

```c
static uint16_t PowerValueToU16(float value)
{
    if (!isfinite(value) || value <= 0.0f)
        return 0U;
    if (value >= (float)UINT16_MAX)
        return UINT16_MAX;
    return (uint16_t)(value + 0.5f);
}
```

- [ ] **Step 4: Add one orchestration function called by ChassisTask**

Implement `UpdateChassisPowerBudget()` with this data order:

```c
static void UpdateChassisPowerBudget(void)
{
    float bench_limit_w = 0.0f;
#if CHASSIS_POWER_BENCH_TEST
    bench_limit_w = CHASSIS_POWER_BENCH_LIMIT_W;
#endif

    const float referee_limit_w =
        (float)referee_data->GameRobotState.chassis_power_limit;
    const float selected_limit_w =
        PowerModelSelectLimit(referee_limit_w, bench_limit_w);
    const bool referee_valid = isfinite(referee_limit_w) && referee_limit_w > 0.0f;
    const bool bench_valid = !referee_valid && bench_limit_w > 0.0f;
    const bool enable_dcdc = (referee_valid || bench_valid) &&
                             (bench_valid || referee_data->GameRobotState.power_management_chassis_output) &&
                             chassis_cmd_recv.chassis_mode != CHASSIS_ZERO_FORCE;

    const SuperCapCommand_s command = {
        .enable_dcdc = enable_dcdc,
        .referee_power_limit_w = PowerValueToU16(selected_limit_w),
        .referee_buffer_energy_j =
            PowerValueToU16((float)referee_data->PowerHeatData.buffer_energy),
    };
    if (cap != NULL)
        (void)SuperCapSendCommand(cap, &command);

    SuperCapStatus_s cap_status = {0};
    if (cap != NULL)
        (void)SuperCapGetStatus(cap, &cap_status);

    const ChassisPowerBudgetInput_s input = {
        .referee_limit_w = selected_limit_w,
        .cap_online = cap_status.online,
        .cap_output_enabled = cap_status.output_enabled,
        .cap_error_code = cap_status.error_code,
        .cap_energy_ratio = cap_status.energy_ratio,
        .cap_reported_limit_w = (float)cap_status.available_power_limit_w,
    };
    const float dt_s = DWT_GetDeltaT(&chassis_budget_dwt_count);
    if (!ChassisPowerBudgetUpdate(&chassis_budget_config,
                                  &input,
                                  dt_s,
                                  &chassis_budget_state,
                                  &chassis_budget_output))
        chassis_budget_output.applied_budget_w = 0.0f;

    DJIChassisPowerSetBudget(chassis_budget_output.applied_budget_w);
}
```

Call it once in `ChassisTask()` after receiving `chassis_cmd_recv` and before the motor stop/enable branch. Remove the old inline `bench_power_limit_w`, `PowerModelSelectLimit()`, and setter block.

- [ ] **Step 5: Check that the wire command does not contain boosted power**

Review the function and verify the command uses `selected_limit_w`, while only the DJI setter uses `chassis_budget_output.applied_budget_w`.

Run: `rg -n "referee_power_limit_w|applied_budget_w|SuperCapSendCommand|DJIChassisPowerSetBudget" application/chassis/chassis.c`

Expected: the two values have separate call sites matching the rule above.

- [ ] **Step 6: Compile the integration**

Run:

```bash
make build/chassis.o build/dji_motor.o build/super_cap.o build/super_cap_protocol.o build/chassis_power_budget.o
```

Expected: every object passes without warnings.

- [ ] **Step 7: Run all host tests**

```bash
make -C tests/power_model clean test
make -C tests/super_cap_protocol clean test
make -C tests/chassis_power_budget clean test
```

Expected: all three suites pass.

- [ ] **Step 8: Commit the chassis connection**

Before staging, inspect `git diff -- application/chassis/chassis.c application/robot_def.h`. Preserve the user-owned Kp comment and bench-mode value; do not stage `application/robot_def.h`.

```bash
git add -p application/chassis/chassis.c
git commit -m "feat: connect supercap budget to chassis"
```

---

### Task 6: Document and verify the complete F407 integration

**Files:**
- Create: `docs/supercap-can-protocol.md`
- Verify: all files from Tasks 1-5

**Interfaces:**
- Consumes: completed codec, driver, budget policy, and chassis integration.
- Produces: an operator contract and a build/test evidence record.

- [ ] **Step 1: Write the operator protocol document**

Document both 8-byte tables, the known command example `01 64 00 32 00 00 00 00`, the status error bits, little-endian float handling, 200 Hz F407 command rate, initial 1 kHz F334 feedback rate, and 200 ms offline threshold. Include this fallback statement verbatim:

```text
F334 offline, disabled, low-energy, or faulted: remove supercapacitor boost immediately and retain only the valid referee/bench base budget.
```

- [ ] **Step 2: Run the incomplete-text and old-protocol scan**

```bash
rg -n "0x301|0x302|SuperCap_Msg_s|SuperCapSend\(|SuperCapGet\(|DJIChassisPowerSetLimit" application modules tests docs/supercap-can-protocol.md
```

Expected: no active-code matches. Historical design documents may still describe the old state and are not edited.

- [ ] **Step 3: Run every host test from a clean state**

```bash
make -C tests/power_model clean test
make -C tests/super_cap_protocol clean test
make -C tests/chassis_power_budget clean test
```

Expected: all suites pass under `-Wall -Wextra -Werror -pedantic`.

- [ ] **Step 4: Run the F407 firmware build**

Run: `make -j8`

Expected: the ELF is produced with no new compile errors, duplicate symbols, or unresolved references.

- [ ] **Step 5: Inspect the final diff and user-owned changes**

```bash
git status --short
git diff --check
git diff 65782ad..HEAD -- application modules tests docs/supercap-can-protocol.md Makefile
```

Expected: integration changes are scoped to the planned files; `application/robot_def.h` remains an unstaged user modification; the expanded Kp comment remains present.

- [ ] **Step 6: Commit documentation**

```bash
git add docs/supercap-can-protocol.md
git commit -m "docs: document F334 supercap CAN protocol"
```

- [ ] **Step 7: Perform the bench acceptance sequence**

With wheels lifted and the existing `40 W` bench mode intentionally enabled:

1. Verify F407 sends ID `0x061`, DLC 8, every 5 ms.
2. Verify F334 sends ID `0x051`, DLC 8, initially every 1 ms.
3. Confirm command bytes show `40 W` as `28 00` and the measured buffer value in bytes 3-4.
4. Confirm a valid F334 status changes budget mode from `OFFLINE` to `READY` only when enabled, error-free, and above 10% energy.
5. Unplug CAN and confirm the budget returns to 40 W within 200 ms.
6. Inject each error bit and confirm boost is removed without a direct `DJIMotorStop()` call.
7. Record actual chassis power, reported available limit, energy ratio, target budget, applied budget, four raw currents, and four limited currents.

Do not proceed to loaded driving until all seven observations pass.
