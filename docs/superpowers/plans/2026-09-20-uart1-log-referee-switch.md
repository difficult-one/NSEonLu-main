# USART1 Log/Referee Compile-Time Switch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add one compile-time selector that assigns USART1 either to the referee/UI stack or to a non-blocking UART log stream, with optional simultaneous RTT output.

**Architecture:** A board-level header derives mutually exclusive ownership and routing flags from `UART1_USE`. A focused `bsp_uart_log` transport owns USART1 reconfiguration, a 4096-byte DMA ring, and the TX-complete callback; the existing `bsp_log` API formats and routes messages without changing callers. Application initialization uses the same derived flags to omit referee registration and UI task creation whenever USART1 belongs to logging.

**Tech Stack:** STM32F407 HAL UART/DMA, C11, SEGGER RTT, FreeRTOS/CMSIS-RTOS, GNU Make, host-side GCC tests.

## Global Constraints

- `UART1_USE_REFEREE` means USART1 referee/UI enabled, logging through RTT only, at the generated 115200 8N1 setting.
- `UART1_USE_LOG` means USART1 referee/UI disabled, logging through USART1 only, at 115200 8N1.
- `UART1_USE_LOG_RTT` means USART1 referee/UI disabled, logging through both USART1 and RTT, at 115200 8N1.
- The initial default is `UART1_USE_LOG`; changing one definition to `UART1_USE_REFEREE` restores the on-robot configuration.
- USART1 logging and `VISION_USE_UART` are an invalid combination and must fail compilation.
- UART writes never block; a message that does not fit is dropped atomically and counted.
- The UART DMA ring is exactly 4096 bytes; formatted-log storage is exactly 384 bytes.
- UART copies of formatted messages omit ANSI/SEGGER color sequences; RTT copies retain them.
- Raw writes, including chassis-power CSV, remain byte-for-byte unchanged.
- Do not change the established 100 Hz chassis-power CSV schema, values, or timing.
- Preserve the user's existing uncommitted `YAW_CHASSIS_ALIGN_ECD` change in `application/robot_def.h`; do not stage that file.

---

## File Structure

- Create `Inc/board_config.h`: single USART1 selector, validation, and derived ownership/routing flags.
- Create `bsp/log/bsp_uart_log.h`: narrow UART-log transport and diagnostics interface.
- Create `bsp/log/bsp_uart_log.c`: baud reconfiguration, DMA ring, drop/error counters, and HAL TX callback.
- Modify `bsp/log/bsp_log.c`: transport-neutral routing, bounded formatting, and UART ANSI removal.
- Modify `bsp/log/bsp_log.h`: documented return semantics and `DISABLE_LOG_SYSTEM` default.
- Modify `application/chassis/chassis.c`: compile out USART1 referee registration in log modes.
- Modify `application/robot_task.h`: compile out UI task creation in log modes.
- Modify `modules/master_machine/master_process.c`: reject USART1 vision together with USART1 logging.
- Modify `Makefile`: compile the UART transport and accept `UART1_USE=<0|1|2>` build overrides.
- Modify `CMakeLists.txt`: accept the same optional `UART1_USE` cache override; recursive source discovery already includes the new `.c` file.
- Modify `bsp/log/bsp_log.md`: switching, wiring, baud, Serial Monitor, and bandwidth instructions.
- Create `tests/uart1_config/test_uart1_config.c` and `tests/uart1_config/Makefile`: compile-time mode table tests.
- Create `tests/uart_log/test_uart_log.c`, `tests/uart_log/Makefile`, and HAL stubs: host tests for the DMA ring.
- Modify `tests/bsp_log/test_bsp_log.c`, `tests/bsp_log/Makefile`, and its stubs: three-mode routing and formatting tests.

---

### Task 1: Define and Test the Single USART1 Ownership Selector

**Files:**
- Create: `Inc/board_config.h`
- Create: `tests/uart1_config/test_uart1_config.c`
- Create: `tests/uart1_config/Makefile`

**Interfaces:**
- Produces: `UART1_USE`, `UART1_REFEREE_ENABLED`, `UART1_LOG_ENABLED`, and `UART1_RTT_ENABLED`, each usable in `#if` expressions.
- Consumes: optional compiler definition `-DUART1_USE=0`, `1`, or `2`.

- [ ] **Step 1: Write the compile-time mode-table test**

Create `tests/uart1_config/test_uart1_config.c`:

```c
#include "board_config.h"

#ifndef EXPECT_REFEREE
#error EXPECT_REFEREE is required
#endif
#ifndef EXPECT_UART_LOG
#error EXPECT_UART_LOG is required
#endif
#ifndef EXPECT_RTT
#error EXPECT_RTT is required
#endif

_Static_assert(UART1_REFEREE_ENABLED == EXPECT_REFEREE, "referee flag mismatch");
_Static_assert(UART1_LOG_ENABLED == EXPECT_UART_LOG, "UART log flag mismatch");
_Static_assert(UART1_RTT_ENABLED == EXPECT_RTT, "RTT flag mismatch");

int main(void) { return 0; }
```

Create a Makefile that builds the same source three times with these definitions:

```make
CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -I../../Inc

.PHONY: test clean

test: referee.exe uart.exe uart_rtt.exe

referee.exe: test_uart1_config.c
	$(CC) $(CFLAGS) -DUART1_USE=0 -DEXPECT_REFEREE=1 -DEXPECT_UART_LOG=0 -DEXPECT_RTT=1 $< -o $@

uart.exe: test_uart1_config.c
	$(CC) $(CFLAGS) -DUART1_USE=1 -DEXPECT_REFEREE=0 -DEXPECT_UART_LOG=1 -DEXPECT_RTT=0 $< -o $@

uart_rtt.exe: test_uart1_config.c
	$(CC) $(CFLAGS) -DUART1_USE=2 -DEXPECT_REFEREE=0 -DEXPECT_UART_LOG=1 -DEXPECT_RTT=1 $< -o $@

clean:
	-if exist *.exe del /Q *.exe
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `mingw32-make -C tests/uart1_config clean test`

Expected: FAIL because `board_config.h` does not exist.

- [ ] **Step 3: Add the board configuration header**

Create `Inc/board_config.h` with this exact mode derivation:

```c
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#define UART1_USE_REFEREE 0
#define UART1_USE_LOG     1
#define UART1_USE_LOG_RTT 2

#ifndef UART1_USE
#define UART1_USE UART1_USE_LOG
#endif

#if UART1_USE != UART1_USE_REFEREE && \
    UART1_USE != UART1_USE_LOG && \
    UART1_USE != UART1_USE_LOG_RTT
#error UART1_USE must be UART1_USE_REFEREE, UART1_USE_LOG, or UART1_USE_LOG_RTT
#endif

#define UART1_REFEREE_ENABLED (UART1_USE == UART1_USE_REFEREE)
#define UART1_LOG_ENABLED     (UART1_USE != UART1_USE_REFEREE)
#define UART1_RTT_ENABLED     (UART1_USE != UART1_USE_LOG)

#if UART1_LOG_ENABLED && defined(VISION_USE_UART)
#error VISION_USE_UART cannot share USART1 with UART logging
#endif

#endif
```

- [ ] **Step 4: Verify all valid and invalid configurations**

Run: `mingw32-make -C tests/uart1_config clean test`

Expected: PASS with three executables created.

Run:

```powershell
gcc -std=c11 -Wall -Wextra -Werror -pedantic -IInc -DUART1_USE=99 -c tests/uart1_config/test_uart1_config.c
```

Expected: FAIL with `UART1_USE must be ...`; remove any emitted object if the compiler unexpectedly creates one.

Run:

```powershell
gcc -std=c11 -Wall -Wextra -Werror -pedantic -IInc -DUART1_USE=1 -DVISION_USE_UART -DEXPECT_REFEREE=0 -DEXPECT_UART_LOG=1 -DEXPECT_RTT=0 -c tests/uart1_config/test_uart1_config.c
```

Expected: FAIL with `VISION_USE_UART cannot share USART1 with UART logging`.

- [ ] **Step 5: Commit the selector and its test**

```powershell
git add Inc/board_config.h tests/uart1_config
git commit -m "feat: define exclusive USART1 roles"
```

---

### Task 2: Implement the Non-Blocking USART1 DMA Log Transport

**Files:**
- Create: `bsp/log/bsp_uart_log.h`
- Create: `bsp/log/bsp_uart_log.c`
- Create: `tests/uart_log/test_uart_log.c`
- Create: `tests/uart_log/Makefile`
- Create: `tests/uart_log/stubs/main.h`
- Create: `tests/uart_log/stubs/usart.h`

**Interfaces:**
- Consumes: `huart1`, `HAL_UART_Init`, `HAL_UART_Transmit_DMA`, IRQ save/restore intrinsics, and `UART1_LOG_ENABLED`.
- Produces: `void BSPUARTLogInit(void)`, `unsigned BSPUARTLogWrite(const void *data, unsigned length)`, and `void BSPUARTLogGetDiagnostics(BSPUARTLogDiagnostics_s *out)`; the HAL callback remains an implementation detail.

- [ ] **Step 1: Define the transport contract and HAL stubs**

Create `bsp/log/bsp_uart_log.h`:

```c
#ifndef BSP_UART_LOG_H
#define BSP_UART_LOG_H

#include <stdint.h>

typedef struct
{
    uint32_t dropped_messages;
    uint32_t tx_errors;
} BSPUARTLogDiagnostics_s;

void BSPUARTLogInit(void);
unsigned BSPUARTLogWrite(const void *data, unsigned length);
void BSPUARTLogGetDiagnostics(BSPUARTLogDiagnostics_s *out);

#endif
```

The host `main.h` stub must define the minimal `HAL_StatusTypeDef`, `HAL_OK`,
UART constants, `UART_InitTypeDef`, `UART_HandleTypeDef`, and declarations for
the three HAL calls/intrinsics. The `usart.h` stub declares `extern
UART_HandleTypeDef huart1;`.

- [ ] **Step 2: Write failing DMA-ring tests**

In `tests/uart_log/test_uart_log.c`, provide capture stubs and independent test
functions for these exact behaviors:

```c
static void TestInitSelects115200AndResetsDiagnostics(void);
static void TestFirstWriteStartsDmaAndPreservesBytes(void);
static void TestBusyWriteQueuesWithoutRestartingDma(void);
static void TestCompletionDrainsQueuedAndWrappedSegmentsInOrder(void);
static void TestOversizeOrFullQueueDropsWholeMessage(void);
static void TestForeignUartCompletionIsIgnored(void);
static void TestHalInitAndDmaStartFailuresAreCounted(void);
```

Make the stub `HAL_UART_Transmit_DMA` record the pointer and length without
copying immediately; each test copies those bytes before invoking
`HAL_UART_TxCpltCallback(&huart1)`. This verifies that DMA always references
stable ring storage rather than caller-owned memory.

Create `tests/uart_log/Makefile` compiling `test_uart_log.c` with
`../../bsp/log/bsp_uart_log.c`, `-DUART1_USE=1`, `-Istubs`, `-I../../Inc`, and
`-I../../bsp/log`.

- [ ] **Step 3: Run the transport test to verify it fails**

Run: `mingw32-make -C tests/uart_log clean test`

Expected: FAIL at link time because the transport functions are not implemented.

- [ ] **Step 4: Implement the minimal ring and initialization**

In `bsp_uart_log.c`, define a 4096-byte byte ring, `head`, `tail`, the current
`dma_length`, and diagnostics. Preserve one unused byte to distinguish full
from empty. `BSPUARTLogWrite` must:

```c
if (data == NULL || length == 0U)
    return 0U;
if (length > FreeBytes())
{
    ++diagnostics.dropped_messages;
    return 0U;
}
CopyIntoRing(data, length);
if (dma_length == 0U)
    StartNextContiguousDmaSegment();
return length;
```

Save PRIMASK, disable IRQs, update ring state/start DMA, then restore the prior
PRIMASK. Never enable interrupts if they were already disabled by the caller.
On `HAL_UART_Transmit_DMA` failure, increment `tx_errors`, discard queued bytes
by setting `tail = head`, and leave `dma_length` zero so the next message can
retry cleanly.

`BSPUARTLogInit` clears all state, sets `huart1.Init.BaudRate = 115200U`, and
calls `HAL_UART_Init(&huart1)`. A failed init increments `tx_errors`.

The implementation's private TX-completion handler ignores handles other than
`&huart1`; otherwise it advances `tail` by `dma_length`, clears `dma_length`,
and starts the next contiguous ring segment. Define `HAL_UART_TxCpltCallback`
as the single HAL hook that delegates to this private handler.

- [ ] **Step 5: Run transport tests and inspect static storage**

Run: `mingw32-make -C tests/uart_log clean test`

Expected: PASS for all seven behaviors.

Run: `rg -n "malloc|HAL_UART_Transmit\(" bsp/log/bsp_uart_log.c`

Expected: no matches; the implementation uses static storage and DMA only.

- [ ] **Step 6: Commit the DMA transport**

```powershell
git add bsp/log/bsp_uart_log.c bsp/log/bsp_uart_log.h tests/uart_log
git commit -m "feat: add nonblocking USART1 log transport"
```

---

### Task 3: Route the Existing BSP Log API Across RTT and USART1

**Files:**
- Modify: `bsp/log/bsp_log.c`
- Modify: `bsp/log/bsp_log.h`
- Modify: `tests/bsp_log/test_bsp_log.c`
- Modify: `tests/bsp_log/Makefile`
- Modify: `tests/bsp_log/stubs/SEGGER_RTT.h`

**Interfaces:**
- Consumes: the Task 1 derived flags and Task 2 `BSPUARTLog*` functions.
- Produces: unchanged call-site APIs `BSPLogInit`, `BSPLogWrite`, `BSPLogWriteString`, `BSPLogClear`, and `PrintLog`.

- [ ] **Step 1: Extend BSP-log captures for all three modes**

Add UART capture implementations of `BSPUARTLogInit` and `BSPUARTLogWrite` to
`test_bsp_log.c`. Build three test executables from the same test using
`-DUART1_USE=0`, `1`, and `2`. In each binary, compile assertions selected by
`UART1_RTT_ENABLED` and `UART1_LOG_ENABLED`.

Add these tests:

```c
static void TestInitStartsOnlyConfiguredTransports(void);
static void TestRawWriteRoutesExactEmbeddedNullBytes(void);
static void TestStringWriteRoutesToConfiguredDestinations(void);
static void TestFormattedWriteKeepsRttColorAndStripsUartColor(void);
static void TestFormattedWriteRejectsTruncationWithoutPartialOutput(void);
static void TestConfiguredDestinationFailureReturnsZero(void);
```

Use the input `"\x1B[92mI:value=%d\r\n\x1B[0m"` with `7`. RTT must capture
the escape sequences; UART must capture exactly `"I:value=7\r\n"`.

- [ ] **Step 2: Run the routing tests to verify failure**

Run: `mingw32-make -C tests/bsp_log clean test`

Expected: FAIL because current code always initializes and writes RTT and never calls the UART capture.

- [ ] **Step 3: Implement bounded formatting and mode routing**

Update `bsp_log.c` to include `board_config.h` and `bsp_uart_log.h`. Use a static
384-byte format buffer protected by GCC atomic builtins:

```c
if (__atomic_test_and_set(&format_busy, __ATOMIC_ACQUIRE))
    return 0;
written = vsnprintf(format_buffer, sizeof(format_buffer), fmt, args);
if (written < 0 || (size_t)written >= sizeof(format_buffer))
    result = -1;
else
    result = RouteFormattedMessage(format_buffer, (unsigned)written);
__atomic_clear(&format_busy, __ATOMIC_RELEASE);
return result;
```

`RouteFormattedMessage` writes the original bytes to RTT when enabled. Before
the UART write, compact the same owned buffer in place, removing CSI sequences
that begin with ESC `[` and end at a byte in the ASCII range `0x40..0x7E`.
Return the original formatted length only if every configured destination
accepted its entire intended copy; return zero on a transport rejection and
`-1` on formatting failure/truncation.

`BSPLogWrite` performs no ANSI filtering and returns the minimum accepted byte
count across configured destinations, which is zero if either enabled
destination rejects the complete raw message. `BSPLogWriteString` validates
non-null input and calls `BSPLogWrite(text, strlen(text))`.

Initialize only enabled transports in `BSPLogInit`. Add this compatibility
default to `bsp_log.h` before its `#if DISABLE_LOG_SYSTEM` block:

```c
#ifndef DISABLE_LOG_SYSTEM
#define DISABLE_LOG_SYSTEM 0
#endif
```

- [ ] **Step 4: Run BSP log and telemetry regression tests**

Run:

```powershell
mingw32-make -C tests/bsp_log clean test
mingw32-make -C tests/rtt_telemetry clean test
```

Expected: all three BSP-log mode binaries pass; existing telemetry tests pass unchanged.

- [ ] **Step 5: Commit central routing**

```powershell
git add bsp/log/bsp_log.c bsp/log/bsp_log.h tests/bsp_log
git commit -m "feat: route logs by USART1 role"
```

---

### Task 4: Enforce USART1 Ownership in Application Initialization

**Files:**
- Modify: `application/chassis/chassis.c`
- Modify: `application/robot_task.h`
- Modify: `modules/master_machine/master_process.c`
- Modify: `Makefile`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `UART1_REFEREE_ENABLED` and `UART1_LOG_ENABLED` from `board_config.h`.
- Produces: no USART1 referee registration/UI task in log builds; including `board_config.h` after `robot_def.h` activates the Task 1 vision collision check.

- [ ] **Step 1: Add failing source-level ownership checks**

Before edits, run these inspections and save the observed matches in the task notes:

```powershell
rg -n "UITaskInit\(&huart1|osThreadDef\(uitask|VisionInit\(&huart1" application modules
rg -n "bsp_uart_log.c|UART1_USE" Makefile CMakeLists.txt
```

Expected: referee/UI calls are unconditional, and neither build system exposes the new transport/configuration.

- [ ] **Step 2: Guard referee registration and UI task creation**

Include `board_config.h` where needed and guard the complete statements:

```c
#if UART1_REFEREE_ENABLED
    (void)UITaskInit(&huart1, &ui_data);
#endif
```

```c
#if UART1_REFEREE_ENABLED
    osThreadDef(uitask, StartUITASK, osPriorityNormal, 0, 512);
    uiTaskHandle = osThreadCreate(osThread(uitask), NULL);
#endif
```

The UI entry function may remain compiled because it has no runtime path in log
builds. Do not fabricate or initialize referee snapshots in log mode; the
existing invalid snapshot and bench fallback behavior must remain intact.

- [ ] **Step 3: Activate the vision-UART collision check**

In `master_process.c`, include the board configuration after `robot_def.h` so
the Task 1 check sees the selected vision transport:

```c
#include "robot_def.h"
#include "board_config.h"
```

This check belongs in the module source so `application/robot_def.h` remains
untouched and the user's uncommitted yaw calibration cannot enter a task commit.

- [ ] **Step 4: Add build-system integration and overrides**

Add `bsp/log/bsp_uart_log.c` immediately after `bsp/log/bsp_log.c` in the root
Makefile source list. Add:

```make
ifneq ($(strip $(UART1_USE)),)
C_DEFS += -DUART1_USE=$(UART1_USE)
endif
```

In `CMakeLists.txt`, after the existing definitions, add:

```cmake
if(DEFINED UART1_USE)
    add_compile_definitions(UART1_USE=${UART1_USE})
endif()
```

- [ ] **Step 5: Build all three ownership modes**

Use separate build directories so object files cannot leak definitions between modes:

```powershell
mingw32-make -j4 BUILD_DIR=build/uart1_referee UART1_USE=0
mingw32-make -j4 BUILD_DIR=build/uart1_log UART1_USE=1
mingw32-make -j4 BUILD_DIR=build/uart1_log_rtt UART1_USE=2
```

Expected: all three produce `basic_framework.elf`, `.hex`, and `.bin` without warnings.

The negative vision collision was already compiled in Task 1 with
`-DVISION_USE_UART`; do not edit `application/robot_def.h` to repeat it here.

- [ ] **Step 6: Verify ownership guards and commit integration**

Run:

```powershell
rg -n -C 2 "UART1_REFEREE_ENABLED|UART1_LOG_ENABLED" application modules/master_machine
git diff -- application/robot_def.h
```

Expected: both referee operations are guarded, the vision conflict is explicit,
and `robot_def.h` still contains only the user's pre-existing yaw calibration diff.

Stage only the named implementation files:

```powershell
git add application/chassis/chassis.c application/robot_task.h modules/master_machine/master_process.c Makefile CMakeLists.txt
git commit -m "feat: switch USART1 ownership at build time"
```

---

### Task 5: Document Serial Monitor Setup and Run the Final Matrix

**Files:**
- Modify: `bsp/log/bsp_log.md`

**Interfaces:**
- Consumes: all completed configuration and transport behavior.
- Produces: operator instructions for switching, wiring, monitoring, and restoring referee mode.

- [ ] **Step 1: Add exact operator instructions**

Update `bsp_log.md` to state:

- edit `UART1_USE` in `Inc/board_config.h` or pass `UART1_USE=0|1|2` to Make;
- `0` is referee + RTT, `1` is UART log, and `2` is UART log + RTT;
- connect C-board USART1 TX (PA9) to DAPLink UART RX and connect GND;
- select the DAPLink serial port in VSCode Serial Monitor;
- configure `115200 baud`, `8 data bits`, `no parity`, `1 stop bit`, and `no flow control`;
- USART1 RX is unnecessary for one-way logging;
- setting `UART1_USE_REFEREE` restores referee/UI at 115200 and keeps logs on RTT;
- the root Makefile currently defines `DISABLE_LOG_SYSTEM`, so structured
  `BSPLogWrite*` telemetry still appears while `LOGINFO`, `LOGWARNING`, and
  `LOGERROR` remain compiled out unless that build definition is removed.

- [ ] **Step 2: Run every affected host test**

```powershell
mingw32-make -C tests/uart1_config clean test
mingw32-make -C tests/uart_log clean test
mingw32-make -C tests/bsp_log clean test
mingw32-make -C tests/rtt_telemetry clean test
mingw32-make -C tests/dji_chassis_power_rtt clean test
mingw32-make -C tests/referee_integration clean test
```

Expected: every target prints its PASS line and exits zero.

- [ ] **Step 3: Run final firmware and static checks**

```powershell
mingw32-make -j4 BUILD_DIR=build/uart1_referee UART1_USE=0
mingw32-make -j4 BUILD_DIR=build/uart1_log UART1_USE=1
mingw32-make -j4 BUILD_DIR=build/uart1_log_rtt UART1_USE=2
rg -n "HAL_UART_Transmit\(" bsp/log
rg -n "SEGGER_RTT_[A-Za-z0-9_]+\s*\(" bsp modules application Src
git diff --check
git status --short
```

Expected:

- three firmware builds succeed;
- no blocking UART transmit exists in `bsp/log`;
- project-owned SEGGER calls remain confined to `bsp/log/bsp_log.c`;
- no whitespace errors exist;
- `application/robot_def.h` remains the only unrelated dirty user file.

- [ ] **Step 4: Perform the hardware smoke test**

Build/download `UART1_USE=1`, open the DAPLink serial port at 115200 8N1, and
verify that the CSV header appears once and rows continue without malformed or
interleaved bytes. Observe that motor-control delay warnings do not increase.
Then build/download `UART1_USE=0`, reconnect the referee system, and verify that
referee data/UI resume while RTT continues to show the same log stream.

- [ ] **Step 5: Commit documentation and final verification state**

```powershell
git add bsp/log/bsp_log.md
git commit -m "docs: explain USART1 Serial Monitor logging"
```

Record the exact host-test, firmware-build, and hardware-smoke results in the
final handoff. If hardware is unavailable, explicitly mark only the two physical
checks as pending; do not imply they ran.
