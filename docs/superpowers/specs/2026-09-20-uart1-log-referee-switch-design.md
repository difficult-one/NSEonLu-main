# USART1 Log/Referee Compile-Time Switch Design

## Goal

Allow the C board's USART1 connector to serve either the referee system or the
project log stream, selected by one compile-time setting. In log mode, the same
messages currently sent through the centralized RTT API must be observable in
VSCode Serial Monitor through the DAPLink UART bridge without blocking the motor
control loop. Switching back to referee mode must restore the current referee
and UI behavior and keep logs available over RTT.

## Configuration

Define one public USART1-use selector alongside the existing robot build
configuration:

```c
#define UART1_USE_REFEREE 0
#define UART1_USE_LOG     1
#define UART1_USE_LOG_RTT 2

#define UART1_USE UART1_USE_LOG
```

The modes have these exact meanings:

| `UART1_USE` value | USART1 owner | Log destination | Referee/UI |
| --- | --- | --- | --- |
| `UART1_USE_REFEREE` | referee module | RTT only | enabled |
| `UART1_USE_LOG` | log transport | USART1 only | disabled |
| `UART1_USE_LOG_RTT` | log transport | USART1 and RTT | disabled |

The build rejects every other value. This single selector prevents an invalid
configuration in which USART1 is assigned independently to both logging and the
referee protocol.

The initial project setting is `UART1_USE_LOG` for bench debugging. Changing
only that line to `UART1_USE_REFEREE` restores the on-robot configuration.

## Architecture

### Central log routing

`bsp/log` remains the only transport owner. `PrintLog`, `BSPLogWrite`,
`BSPLogWriteString`, and the existing `LOG*` macros keep their public
interfaces. They route complete formatted messages according to `UART1_USE`:

- referee mode calls the existing SEGGER RTT functions;
- log mode enqueues the bytes for USART1;
- combined log mode enqueues USART1 bytes and also writes them to RTT.

Business modules and `modules/telemetry/rtt_telemetry.c` do not call UART or
SEGGER APIs directly. Although the telemetry module retains its established
name for compatibility, its writes pass through the transport-neutral
`bsp_log` API.

Formatted logging uses one 384-byte internal formatting buffer guarded by a
non-blocking ownership flag before routing the message. A concurrent formatted
log is dropped instead of corrupting that buffer. Raw CSV writes remain
byte-for-byte unchanged. For both UART-capable modes, the UART copy of a
formatted message has SEGGER/ANSI color-control sequences removed; the RTT copy
in combined mode retains them. Serial Monitor therefore receives readable text
and CSV data.

### Non-blocking USART1 transport

USART1 logging uses its existing TX DMA channel and a private 4096-byte transmit
ring owned by `bsp/log`. A write copies bytes into the ring and starts DMA if
idle. The USART1 transmit-complete callback advances the ring and starts the
next contiguous segment. Queue manipulation is protected by short critical
sections so task and interrupt contexts cannot corrupt its indices.

No log call waits for bytes to leave the UART. If the queue lacks room, the
new message is dropped as one unit and an internal drop counter is incremented;
motor control must never be delayed by diagnostic output. The RTT destination
continues to use its existing non-blocking behavior.

In a USART1 logging mode, BSP log initialization reconfigures USART1 to
115200 baud, 8 data bits, no parity, 1 stop bit, and no flow control. Referee
mode retains the generated 115200-baud USART1 configuration. VSCode Serial
Monitor must use the matching `115200 8N1` settings.

### Referee and UI ownership

In referee mode, chassis initialization continues to call
`UITaskInit(&huart1, &ui_data)`, and OS initialization creates the UI task.

In either log mode, chassis initialization does not register the referee module
on USART1 and OS initialization does not create the UI task. This second guard
is required because the current UI task dereferences referee state initialized
by `UITaskInit`. Referee power snapshots remain invalid, allowing the existing
bench-power fallback policy to decide whether chassis operation is permitted.
No fake referee data is introduced.

The current `VISION_USE_VCP` configuration remains unchanged. Its
`VisionInit(&huart1)` argument is unused by the VCP implementation, so it does
not claim USART1. A compile-time check rejects `VISION_USE_UART` together with
either USART1 logging mode.

## Hardware and Data Flow

For log modes, connect C-board USART1 TX (MCU PA9) to the DAPLink UART RX and
connect the grounds. USART1 RX is not required for one-way logging. The DAPLink
UART bridge appears as a serial port selected by VSCode Serial Monitor.

The output path is:

1. Existing code calls `PrintLog`, a `LOG*` macro, or `BSPLogWrite*`.
2. `bsp/log` formats at most one complete message and selects the configured
   destination or destinations.
3. RTT output is accepted by SEGGER RTT when enabled.
4. UART output is copied into the DMA queue when enabled.
5. USART1 TX DMA drains the queue through the DAPLink UART bridge.
6. VSCode Serial Monitor displays the text or CSV rows.

## Error Handling

- Invalid selector values fail compilation.
- USART1 logging combined with `VISION_USE_UART` fails compilation.
- Referee/UI initialization and task creation cannot occur in a USART1 logging
  build.
- A full UART queue drops the new message atomically instead of blocking or
  emitting a partial CSV row.
- UART initialization or DMA-start failure increments a diagnostic counter and
  leaves control behavior unchanged.
- The existing referee mode and RTT path remain the safe fallback.

## Testing and Verification

Host tests use HAL and RTT stubs to verify:

- each configuration selects the intended destination or destinations;
- formatted, string, and raw writes preserve their bytes and lengths;
- a full queue drops a whole message and records the drop;
- DMA completion drains wrapped queue segments in order;
- invalid configurations are rejected by compile-time checks.

Integration checks verify:

- log builds contain no USART1 referee registration or UI task creation;
- referee builds retain both operations and route logs only to RTT;
- the existing RTT telemetry and BSP log host tests still pass;
- the ARM firmware builds in referee, UART-only, and UART-plus-RTT modes;
- at 115200 baud, Serial Monitor receives the existing 100 Hz chassis-power
  CSV stream without control-loop blocking.

## Completion Criteria

- One macro switches between referee, UART log, and UART-plus-RTT log modes.
- USART1 has exactly one owner in every valid build.
- All existing project logging APIs work without call-site changes.
- UART output is DMA-driven and never blocks the control loop.
- Referee mode behaves as before and preserves RTT logging.
- UART log modes are readable in VSCode Serial Monitor at 115200 8N1.
