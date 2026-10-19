# Portable Referee and Supercapacitor Modules Design

## Purpose

Create a shareable package containing exactly two independent firmware modules:

- `portable_modules/modules/referee`
- `portable_modules/modules/super_cap`

The package is intended for RoboMaster A-board projects but must remain usable on
other microcontrollers. The existing firmware, BSP, application code, generated
CubeMX files, and build files remain unchanged.

## Scope

### Referee module

The referee module is an isolated port of the current implementation, not a new
protocol invented from scratch. It preserves the complete capability already
implemented by the current project:

- accept arbitrary UART receive chunks;
- recover frames across split packets and multiple frames in one packet;
- validate frame-header CRC8 and frame CRC16;
- decode every command currently handled by the project's referee implementation;
- retain the latest decoded data and link diagnostics;
- report online/offline state from the time of the last valid frame;
- build and send UI delete, graph, and text packets;
- build and send robot-to-robot interactive packets;
- expose the chassis power limit, buffer energy, and power-output state used by
  the supercapacitor control path.

This work ports the protocol version already present in the repository. Updating
the protocol to a different competition-year specification is outside scope.

### Supercapacitor module

The supercapacitor module is likewise ported from the current implementation and
preserves its classic-CAN protocol:

- transmit eight-byte command frames on default standard ID `0x061`;
- receive eight-byte status frames on default standard ID `0x051`;
- decode output state, fault bits, chassis power, available power, and energy;
- reject frames with an invalid ID, DLC, or non-finite floating-point power;
- report online/offline state from the time of the last valid status frame;
- permit overriding the two CAN IDs in configuration.

### Exclusions

- No existing source file will be edited, moved, or deleted.
- The new package will not be added to the existing Makefile or CMake build.
- No board-specific UART/CAN initialization source will be included.
- No automatic UI task, FreeRTOS task, or power-control application will be
  included.
- No third shared adapter module will be created.

## Architecture

Both modules are portable C11 libraries. They do not include STM32 HAL headers,
FreeRTOS headers, the current BSP, daemon services, logging facilities, or
`robot_def.h`. They do not allocate memory. The application owns each instance
and supplies the small hardware-dependent operations through configuration
callbacks declared in that module's public header.

The callbacks keep board-specific code in the A-board application:

- the referee module receives a UART transmit callback, a millisecond-clock
  callback, and optional critical-section callbacks;
- the supercapacitor module receives a classic-CAN transmit callback, a
  millisecond-clock callback, and optional critical-section callbacks.

Callback context pointers are opaque `void *` values. This permits an A-board
project to pass `UART_HandleTypeDef *`, `CAN_HandleTypeDef *`, or its own driver
object without exposing those types to either module.

## Package Layout

```text
portable_modules/
└── modules/
    ├── referee/
    │   ├── referee.h
    │   ├── referee.c
    │   ├── referee_protocol.h
    │   ├── referee_crc.h
    │   ├── referee_crc.c
    │   ├── referee_ui.h
    │   ├── referee_ui.c
    │   ├── README.md
    │   └── tests/
    │       ├── Makefile
    │       └── test_referee.c
    └── super_cap/
        ├── super_cap.h
        ├── super_cap.c
        ├── README.md
        └── tests/
            ├── Makefile
            └── test_super_cap.c
```

Tests and instructions live inside their owning module, so the deliverable still
contains only the two requested module directories.

## Referee Public Interface

The public API is instance-based and uses caller-owned storage:

```c
typedef bool (*RefereeUartSendFn)(void *context,
                                  const uint8_t *data,
                                  size_t length);
typedef uint32_t (*RefereeNowMsFn)(void *context);
typedef void (*RefereeCriticalFn)(void *context);

typedef struct {
    RefereeUartSendFn uart_send;
    RefereeNowMsFn now_ms;
    RefereeCriticalFn enter_critical;
    RefereeCriticalFn exit_critical;
    void *context;
    uint32_t offline_timeout_ms;
} RefereeConfig;

bool RefereeInit(Referee *instance, const RefereeConfig *config);
bool RefereeFeed(Referee *instance, const uint8_t *data, size_t length);
bool RefereeGetData(Referee *instance, RefereeData *data);
bool RefereeGetPower(Referee *instance, RefereePower *power);
bool RefereeGetDiagnostics(Referee *instance,
                           RefereeDiagnostics *diagnostics);
bool RefereeIsOnline(Referee *instance);
```

`RefereeFeed` is called with the byte count reported by an A-board UART DMA/idle
callback. It handles arbitrary chunk boundaries. A CRC-valid frame refreshes the
link timestamp. A known command with an invalid payload size increments a payload
length error counter and does not overwrite the last valid value.

The module contains no delay. The A-board application is responsible for
scheduling UI transmissions within referee-system rate limits.

UI and robot-interaction transmit functions take a `Referee *`, build the frame
in module-owned instance storage, update the sequence number, and invoke the
configured UART callback. Graph construction functions remain transport-free.
Robot-interaction transmit and receive data use an explicit pointer and length
within the protocol's maximum payload instead of the current hard-coded
five-byte application type.

## Supercapacitor Public Interface

```c
typedef bool (*SuperCapCanSendFn)(void *context,
                                  uint32_t standard_id,
                                  const uint8_t *data,
                                  uint8_t dlc);
typedef uint32_t (*SuperCapNowMsFn)(void *context);
typedef void (*SuperCapCriticalFn)(void *context);

typedef struct {
    SuperCapCanSendFn can_send;
    SuperCapNowMsFn now_ms;
    SuperCapCriticalFn enter_critical;
    SuperCapCriticalFn exit_critical;
    void *context;
    uint32_t command_id;
    uint32_t status_id;
    uint32_t offline_timeout_ms;
} SuperCapConfig;

bool SuperCapInit(SuperCap *instance, const SuperCapConfig *config);
bool SuperCapReceive(SuperCap *instance,
                     uint32_t standard_id,
                     const uint8_t *data,
                     uint8_t dlc);
bool SuperCapSendCommand(SuperCap *instance,
                         const SuperCapCommand *command);
bool SuperCapGetStatus(SuperCap *instance, SuperCapStatus *status);
bool SuperCapGetDiagnostics(SuperCap *instance,
                            SuperCapDiagnostics *diagnostics);
```

`SuperCapReceive` is called from the A-board CAN receive path. Only a valid frame
with the configured status ID refreshes the link timestamp. `SuperCapGetStatus`
computes `online` from the configured time callback and timeout, eliminating the
current daemon dependency.

## Concurrency and Ownership

All storage is contained in caller-owned module instances; the implementation
has no global singleton and no heap use. This permits multiple instances and
deterministic embedded memory usage.

When receive processing can interrupt a task that reads status or transmits,
the caller supplies matching `enter_critical` and `exit_critical` callbacks. Both
must be supplied together or both omitted. If omitted, the caller guarantees that
module APIs are not called concurrently. The callbacks are never invoked around
the hardware transmit callback, avoiding long interrupt-disabled sections.

## Validation and Error Handling

Initialization rejects missing mandatory send/time callbacks, mismatched critical
callbacks, zero instance pointers, and CAN IDs outside the 11-bit standard range.
Public functions reject null pointers and invalid lengths without modifying the
last valid state. Hardware transmit failures are returned to the caller as
`false` and counted in diagnostics.

All wire frames are encoded and decoded byte-by-byte. Public decoded structures
are normal C structures and do not depend on compiler packing or bit-field layout.
Compile-time constants define frame and payload bounds.

## A-board Integration Contract

The A-board owner performs only these board-specific actions:

1. Configure referee UART as `115200`, 8 data bits, no parity, one stop bit, with
   DMA/idle reception if desired.
2. Forward each received UART byte chunk to `RefereeFeed`.
3. Implement the referee UART send and millisecond-clock callbacks.
4. Configure classic CAN and a filter for the configured supercapacitor status ID.
5. Forward matching CAN frames to `SuperCapReceive`.
6. Implement the CAN send and millisecond-clock callbacks.

The README in each module will contain a minimal STM32 HAL example showing these
calls, but the example remains documentation rather than a compiled adapter.

## Testing and Acceptance Criteria

Each module has an independent host-side C11 test executable and Makefile. Tests
use fake clock and transmit callbacks, not STM32 or RTOS stubs.

The referee tests cover initialization validation, split/coalesced/noisy input,
CRC failures, all supported command payloads, bad payload lengths, timeout and
wraparound behavior, sequence handling, UI frame generation, arbitrary-length
robot interaction, failed transmission, and optional critical callbacks.

The supercapacitor tests cover exact command bytes, valid status decoding, status
ID and DLC rejection, non-finite power rejection, timeout and wraparound behavior,
custom CAN IDs, failed transmission, diagnostics, and optional critical callbacks.

Acceptance requires:

- both host test suites pass with `-std=c11 -Wall -Wextra -Werror -pedantic`;
- no new module source includes project-local, STM32 HAL, or RTOS headers;
- an include-dependency scan finds only standard headers and files from the same
  module;
- Git diff shows no modification to any file that existed before this work;
- the current working-tree changes in `Src/usart.c`, `application/chassis/chassis.c`,
  `basic_framework.ioc`, and `modules/referee/rm_referee.c` remain byte-for-byte
  untouched.
