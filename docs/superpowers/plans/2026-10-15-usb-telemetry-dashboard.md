# USB Telemetry Dashboard Implementation Plan

> **For agentic workers:** Execute the firmware and browser tasks against the same contract, with TDD for protocol and lifecycle behavior, then request independent code review. Steps use checkbox syntax for tracking.

**Goal:** Deliver 50 Hz C-board telemetry over USB CDC and a local browser dashboard.

**Architecture:** Firmware takes short coherent per-source snapshots and formats bounded CSV in a low-priority task. Static USB buffers remain owned until completion. A buildless Web Serial dashboard reads arbitrary chunks, displays valid data, retains 60 seconds, and exports CSV.

**Tech Stack:** STM32F407 / GCC / FreeRTOS / STM32 USB CDC; HTML, CSS, JavaScript, Canvas, Node built-in test runner.

## Global Constraints

- Approved spec: `docs/superpowers/specs/2026-10-15-usb-telemetry-dashboard-design.md`.
- Default 50 Hz, 2048-byte row/header buffers, 1-second repeated header.
- 3000-sample / 60-second browser window; 20 Hz display; stale after 500 ms.
- Local-only tool. No hosting registration, cloud data service, or motor control commands.
- Preserve existing dirty files and stage only this task's hunks.
- USB telemetry and USB vision are exclusive; compile and verify both modes.
- Existing Vx/Vy values are control units, and motor current values are native DJI units.

## Shared CSV v1 contract

Exact order (72 columns):

```csv
version,seq,tick,tx_drop_count,chassis_valid,chassis_tick_ms,chassis_mode,vx_cmd_milli,vy_cmd_milli,wz_cmd_milli,rc_online,rc_age_ms,rc_lx,rc_ly,rc_rx,rc_ry,rc_dial,rc_sw_left,rc_sw_right,mouse_x,mouse_y,mouse_left,mouse_right,keys,model_tick_ms,motor_online_mask,motor_enabled_mask,rpm_lf,rpm_rf,rpm_lb,rpm_rb,target_lf_rpm,target_rf_rpm,target_lb_rpm,target_rb_rpm,i_lf_cmd,i_rf_cmd,i_lb_cmd,i_rb_cmd,i_lf_fb,i_rf_fb,i_lb_fb,i_rb_fb,power_valid,p_unlimited_cw,p_limited_cw,effective_limit_cw,p_meter_cw,meter_online,meter_age_ms,nominal_budget_cw,target_power_cw,power_source,referee_online,referee_limit_cw,buffer_j,buffer_age_ms,buffer_control_enabled,buffer_estimate_valid,buffer_lower_cj,buffer_predicted_cj,buffer_recovering,cap_online,cap_output_enabled,cap_error,cap_energy_pm,cap_chassis_cw,cap_limit_cw,cap_age_ms,cap_input_online,cap_input_cw,cap_input_age_ms
```

`version=1`. Metadata/ticks/ages, booleans, masks, sequence and counters are unsigned decimal integers. Signed control values, rpm, currents and power can be negative. Power uses 0.01 W, buffer estimates use 0.01 J, `cap_energy_pm` uses 0.001 ratio, and command fields ending `_milli` use 0.001 original control unit. `buffer_j` is integer J. Unknown ages use UINT32_MAX. Chassis mode: 0 zero-force, 1 spin, 2 no-follow, 3 follow-gimbal. Switches: 1 up, 3 middle, 2 down. Wheel order is LF, RF, LB, RB. Source mode: 0 direct, 1 supercapacitor.

A C test emits a hand-set row as a fixture. Browser integration must parse that actual output and validate unit conversions and source validity; a duplicate independent hand-maintained browser fixture alone is insufficient.

### Task 1: Firmware protocol, USB transport and data collection

**Files:**
- Create: `modules/telemetry/usb_telemetry_config.h`, `usb_telemetry_format.h/c`, `usb_telemetry.h/c`.
- Modify: `Src/usbd_cdc_if.c`, `Inc/usbd_cdc_if.h`, `bsp/usb/bsp_usb.h/c`, `modules/master_machine/master_process.c`.
- Modify: `application/chassis/chassis.h/c`, `modules/remote/remote_control.h/c`, `application/robot_task.h`, `Makefile`, `CMakeLists.txt`.
- Create: `tests/usb_telemetry` with host tests, hardware stubs only at HAL/USB boundaries.

**Interfaces:** Export a pure bounded formatter, source snapshot methods, and `USBTelemetryInit()` / `USBTelemetryTask()`. Use the exact CSV contract above. Export a status-returning USB transmission entry that does not lose BUSY/FAIL. Provide the exact formatter signatures and fixture command to the integration owner.

- [x] Write and run failing tests for signed values, finite/saturated conversion, full schema and truncation.
- [x] Implement the formatter and emit a sample CSV from the host test for browser consumption.
- [x] Reproduce disconnected CDC / BUSY / in-flight buffer lifetime failures with boundary stubs.
- [x] Implement null/configuration checks, line coding, DTR/open tracking and immutable in-flight buffers.
- [x] Add bounded coherent application / remote snapshots; reuse existing motor/referee/meter/cap interfaces.
- [x] Create a low-priority USB task with a static formatting buffer; drop new samples while busy.
- [x] Add USB mode option, prevent vision registration/transmission in telemetry mode, preserve vision mode.
- [x] Run host tests and build telemetry and vision configurations in separate build directories.

Tests should prove observable wire behavior, e.g.:

```c
char small[8] = "before";
assert(USBTelemetryFormatRow(&sample, small, sizeof small) == 0);
assert(small[0] == '\0');
/* Send row A, try row B before completion: bytes of A must remain unchanged. */
```

Build using `make -j8 BUILD_DIR=build_usb_telemetry UART1_USE=0` and a separate directory with the final USB option disabled. Do not flash the board as part of software verification.

### Task 2: Streaming protocol and local dashboard

**Files:**
- Create: `tools/usb-telemetry/index.html`, `styles.css`, `protocol.js`, `serial.js`, `app.js`, `charts.js`.
- Create: `tools/usb-telemetry/tests/*.test.cjs`.
- Keep the existing offline waveform viewer functional.

**Interfaces:** Browser scripts expose testable protocol/state logic via CommonJS as well as browser globals. Contract uses the exact 72 fields above; rows can arrive in arbitrary Uint8Array chunks. Serial lifecycle is testable with an injected serial provider at the hardware boundary.

- [x] Write failing stream tests for half-lines, CRLF split, multiple rows, repeated headers, oversized rows, malformed values and unsupported versions.
- [x] Implement a bounded parser and sample validity/unit conversion helpers.
- [x] Write failing lifecycle tests for cancelled selection, read loop termination, reader release, disconnect and reconnect.
- [x] Implement Web Serial access from the Connect click; request port then open with 115200 and set DTR. The firmware's USB CDC ignores baud rate as a hardware timing constraint.
- [x] Implement connection toolbar, numeric cards, separate W/J/rpm plots, current table and controller visualization.
- [x] Separate receive state from display state; cap redraw at 20 Hz and preserve reception during pause.
- [x] Retain at most 3000 samples and at most 60 seconds; segment on reboot, drop/gap and offline data.
- [x] Export the bounded raw CSV with header; add explicit simulation mode and stop simulation before real connection.
- [x] Run `node --test tools/usb-telemetry/tests/*.test.cjs` and review keyboard/narrow-screen behavior.

Behavior examples:

```js
parser.push("version,seq,tick");
parser.push("\r");
parser.push("\n1,1,20\r\n");
// A header split across reads must be treated as one complete record.
```

### Task 3: End-to-end integration, local launch and review

**Files:** Create `tools/usb-telemetry/serve.cjs`, `start.cmd`, `README.md`, protocol fixture integration and browser QA scripts; update configuration guide and approved spec status.

- [x] Provide a localhost-only static server with no packages and a launcher; no arbitrary filesystem exposure.
- [x] Exercise the C fixture through the actual browser parser and verify 1234 centiwatts becomes 12.34 W.
- [x] Run the affected existing C host tests and existing waveform core tests.
- [x] Use the installed Playwright CLI for desktop/narrow screenshots and actual app flows with a stream-backed fake port.
- [x] Read real USB behavior and code for resource lifetime / stale-state / source validity issues.
- [x] Request a read-only reviewer subagent as required by the code-review skill; fix material findings.
- [x] Compare against the initial dirty-file patch, stage only task-specific hunks, run diff checks, and commit locally.
- [x] Report the local launch command, firmware modes, test evidence, hardware verification limit and commit hash.

## Validation record — 2026-10-15

- `make -C tests/usb_telemetry test`: formatter, real CDC/BSP transport lifecycle, and real remote decoder/snapshot executables pass.
- `node --test tools/usb-telemetry/tests/*.test.cjs`: 17 tests pass, including actual C-output-to-JavaScript parsing.
- Browser QA: 15 interaction/display cases pass in Edge, including 375 px layout, USB stream errors/reconnect, CSV download, keyboard bit mapping and motor feedback when the power model is invalid.
- Existing RTT telemetry, DJI power RTT integration, referee power/integration, supercap, power meter, chassis budget and kinematics host tests pass; the existing waveform viewer's 22 tests pass.
- Telemetry ARM build: RAM 89,184 bytes, flash 122,068 bytes. Vision ARM build: RAM 84,856 bytes, flash 114,176 bytes. Linker emits the existing newlib nosys warnings; both builds complete.
- Independent review identified A/D key label order and coupling motor health to power-model validity; both have regression coverage and are corrected.
- No board flashing or physical USB enumeration/throughput test was performed. Use the documented build and existing flash workflow for board acceptance.
