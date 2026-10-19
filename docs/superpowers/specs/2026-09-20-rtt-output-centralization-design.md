# RTT Output Centralization Design

## Goal

Make every project-owned RTT output easy to find and keep SEGGER RTT details out
of business modules. Generic logs continue to use `bsp/log`, while all structured
telemetry definitions live in one clearly named `modules/telemetry` module.

The existing chassis-power stream must keep the same 19-column CSV schema, data,
and nominal 100 Hz output rate.

## Architecture

RTT output is split into two intentionally narrow layers:

1. `bsp/log/bsp_log.c` is the only project-owned source file that calls
   `SEGGER_RTT_*`. It owns initialization, formatted logging, raw byte writes,
   string writes, and terminal clearing.
2. `modules/telemetry/rtt_telemetry.c` owns every structured RTT stream's header,
   row formatting, and output buffer. It uses only the public `bsp_log` API and
   may depend on data types from lower-level algorithm modules.

Business modules may decide *when* to emit telemetry and supply the current data,
but they must not contain RTT headers, CSV formatting, output buffers, or direct
SEGGER calls. New structured RTT output is added to `rtt_telemetry.c/.h` so there
is one predictable place to find it.

## Components and Interfaces

### BSP log transport

`bsp_log.h/.c` retains `BSPLogInit`, `PrintLog`, the existing `LOG*` macros, and
`Float2Str`. It adds small raw/string output functions used by telemetry. The
macros are changed to call these wrappers instead of expanding to
`SEGGER_RTT_*`, and terminal clearing is similarly routed through a function.

No application, algorithm, motor, or telemetry source directly includes or
calls the SEGGER RTT API after this change.

### RTT telemetry module

`modules/telemetry/rtt_telemetry.h/.c` provides the chassis-power telemetry
entry points:

- write the fixed chassis-power CSV header;
- format a chassis-power snapshot into the established 19-column CSV row;
- write that complete row through `bsp_log`.

The formatter moves out of `power_model.c/.h` because CSV is an observation and
transport concern, not part of the power calculation. The telemetry module
accepts `ChassisPowerTelemetry_s`, a tick value, and measured chassis power in
watts. It keeps the existing integer conversion, centiwatt scaling, saturation,
and line ending behavior unchanged.

### DJI motor integration

`dji_motor.c` retains the 10:1 scheduling divider because that timing belongs to
the 1 kHz motor-control path. Registration calls the telemetry header function.
Every tenth control invocation calls the chassis-power row function with the
coherent snapshot, current tick, and latest measured power.

The motor module no longer owns an RTT buffer or any print helper, header text,
CSV formatter call, or `SEGGER_RTT_*` call.

## Data Flow

1. The power model produces the same coherent four-motor telemetry snapshot.
2. The DJI motor control path counts ten 1 kHz invocations.
3. On the tenth invocation it passes the snapshot and observation values to
   `RTTTelemetryWriteChassisPowerRow`.
4. The telemetry module formats one complete CSV row in its private buffer.
5. The BSP log transport writes the completed bytes to RTT buffer zero.

At chassis-power registration, the same path writes the fixed header once.

## Error Handling

The RTT path remains non-blocking and observational. A formatting failure or a
full RTT buffer must not change power control or motor output. If formatting
returns zero, the row is skipped. The BSP raw-write wrapper returns the number of
bytes accepted so callers can inspect it in future, but the current telemetry
path deliberately ignores short writes to preserve existing behavior.

Null telemetry input and insufficient output capacity are rejected by the pure
formatter without writing a partial CSV row.

## Testing

Host tests move with the formatter into a dedicated RTT telemetry test target.
They verify:

- the exact fixed header and its 19 columns;
- one representative row preserves every field and field order;
- finite, negative, non-finite, and saturating power conversions retain current
  behavior;
- null input and a too-small buffer return zero without exposing a partial row.

Integration verification additionally checks that:

- no project source outside `bsp/log/bsp_log.c` calls `SEGGER_RTT_*`;
- no chassis-power header or CSV formatting remains in `dji_motor.c` or
  `power_model.c`;
- all host tests pass;
- the ARM firmware builds without new warnings.

## Completion Criteria

- All project-owned direct SEGGER RTT calls are confined to `bsp_log.c`.
- All structured RTT headers and row formats are in `rtt_telemetry.c`.
- `dji_motor.c` contains only emission timing and telemetry API calls.
- Existing log macros keep their source-level interface.
- Chassis-power RTT output remains 19 columns at nominal 100 Hz with unchanged
  values and ordering.
- Host tests and the full firmware build pass.
