# PID Autotune Implementation Plan

> Execute inline in this session. The user has approved the design and requested implementation. Request a separate code reviewer after integration.

**Goal:** Add a functioning single-wheel/four-wheel PID autotuning workspace to the existing USB dashboard.

**Architecture:** A pure C trial engine owns timing and metrics. A bounded @A1 mailbox connects it to the motor task; a temporary PID overlay never changes durable groups. The browser runs bounded candidate search and verification, then uses the existing confirmed SET/file/Flash flow for adoption.

**Tech Stack:** STM32F407 C11/FreeRTOS, native browser JS/CSS, Node HTTP service, GCC and Node test runners.

## Global Constraints

- Preserve all pre-existing working-tree parameter changes; commit only task changes.
- Keep CSV v1 and existing @T1 behavior compatible. All experimental motion is controlled by the motor task and expires on missing heartbeat.
- Trial PID values never enter Flash or source configuration; formal adoption is explicit.
- Require fresh remote and motor/control feedback, centered controls, no-follow mode, single-wheel output, and zero-force rearm after exit.

### Task 1: Trial engine and protocol

Files: create `modules/telemetry/pid_autotune.h`, `.c`, `autotune_protocol.h`, `.c`, `tests/pid_autotune/Makefile`, `test_autotune.c`; modify runtime tuning and USB stream.

- [x] Write C assertions for begin/start, 300 ms pre/post zero phases, hand-calculated constant-error RMSE, timeout, wraparound, stale feedback, limiting and stop. Run `make -C tests/pid_autotune test` and observe missing-feature failure.
- [x] Implement `PIDAutotuneBegin`, `PIDAutotuneStart`, `PIDAutotuneTick`, `PIDAutotuneObserve`, `PIDAutotuneStop`; use unsigned elapsed times and bounded metrics.
- [x] Add runtime trial lock and @A1 parser/mailbox; validate request field counts/ranges and session/revision before execution. Test malformed/obsolete requests and lock persistence isolation.

### Task 2: Motor integration

Files: modify DJI motor C/header, chassis C, Makefile and dependent test Makefiles.

- [x] Add regression assertions that only the selected wheel receives a target and exit restores original PID, with a zero-force latch preventing automatic resumption.
- [x] Publish fresh remote bench permission in the control snapshot. Run the protocol service before PID calculations; apply/revert the saved PID overlay and observe actual final power-limited output after protection.
- [x] Recheck stop/control freshness before CAN output, account CAN errors, add sources to builds, run motor and USB/Flash regressions.

### Task 3: Search and integrated workspace

Files: create `tools/usb-telemetry/autotune.js`, `autotune-ui.js`, `autotune.css`, `tests/autotune.test.cjs`; modify index/app/protocol/tuning UI.

- [x] Write Node tests for candidate bounds/deduplication, scoring, independent verification, cancellation and stale revision adoption. Run `node --test tools/usb-telemetry/tests/autotune.test.cjs` before implementation.
- [x] Implement @A1 client, candidate runner and synthetic demo; no hardware sends in demo. Poll every 200 ms; stop and END in finally, preserving unconfirmed outcomes.
- [x] Add accessible third tab, controls, progress, comparison table and trial chart. Lock manual controls while the autotuner owns the connection. Use confirmed SET results for file sync, retry sync independently and show pending/saved/error by GET.

### Task 4: Verification, review and delivery

- [x] Run all Node dashboard tests, engine and DJI/USB/Flash host suites; build isolated USB and vision firmware.
- [x] Exercise real browser demo, keyboard tab navigation, mobile width, persistence and cancellation; capture a screenshot under `output/playwright`.
- [x] Request an independent reviewer with the approved spec, exact task diff and test evidence; fix actionable issues and rerun affected checks.
- [x] Update README with operation and firmware requirements, stage only task changes, create a local Git commit and report its hash with hardware-test limitation.


## Validation results

- 2026-10-16: 30 dashboard Node tests and 22 offline waveform tests passed.
- Forced rebuild passed for trial engine, protocol, DJI integration, USB format/transport/remote/tuning/storage, and Flash adapter.
- Browser passed complete four-wheel demo, cancellation, 390px layout, fragmented serial search/verification, stale baseline rejection, one SET adoption, independent file retry, Flash status and disconnect/reconnect during a pending sync. All seven existing browser suites also passed in isolated contexts.
- USB and vision firmware builds passed. USB image uses 93,760 B RAM and 136,428 B application Flash; the existing newlib syscall linker warnings remain.
- Independent reviewers verified fixes for PID timing restoration, effective-current saturation, explicit zero-force latch release, retained abort status, same-cycle session restart and stale async UI updates.
- No physical board was moved or flashed; hardware timing, bench response and loaded validation remain for the user.
