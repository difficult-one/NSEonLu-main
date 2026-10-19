# Online Tuning Implementation Plan

**Goal:** RAM-only live PID, power and spin tuning, plus synchronization of confirmed values into project build defaults.
**Architecture:** A versioned group mailbox connects the CDC parser to owning control tasks. Separate browser protocol and form controllers isolate tuning from telemetry charts.
**Tech Stack:** STM32 C / FreeRTOS, HTML/CSS/JS, GCC host tests, Node tests, Playwright CLI.

## Constraints

- Preserve existing user edits and commit only task changes.
- No Flash storage; defaults come from initialized control objects.
- Whole-group validation, next-cycle application, ACK after application.
- No writes in demo mode; unchanged CSV v1 field order.
- Separate responsive panels sharing key telemetry.

## Task 1: Protocol and mailbox

- [x] Add failing C tests in tests/usb_telemetry/test_tuning.c for GET, delayed SET acknowledgement, invalid/count/version rejection, callback rejection and cancellation.
- [x] Add modules/telemetry/runtime_tuning.h/.c with bounded parsing, registry, mailbox, consumer callbacks and integer-only response formatting. Group sizes: PID 24, power 19, spin 16.
- [x] Connect CDC receive buffering/reply scheduling in usb_telemetry_stream.c; retain immutable transmit buffers and reset receive state on session changes.
- [x] Run make -C tests/usb_telemetry test.

## Task 2: Control integration

- [x] Register actual PID defaults and apply all four motors before DJIMotorControl calculations.
- [x] Register chassis power/spin configs; validate candidate copies and apply before ChassisTask calculations; reset relevant integrators.
- [x] Expose spin validation, retain fixed mode, add Makefile source and motor integration coverage.
- [x] Build telemetry and vision firmware.

## Task 3: Browser transport and forms

- [x] Add failing Node tests for encoding, reply matching, timeout, disconnect and serial writes.
- [x] Add tuning.js schemas/client and tuning-ui.js draft, read, apply and error handling.
- [x] Add serialized writer lifecycle and tuning/CSV demultiplexing.
- [x] Add spacious tabs/group forms, acknowledged values and shared live readouts.
- [x] Add same-origin POST /api/tuning, atomic source header writes, startup restoration and retry; compile the actual saved header in tests.
- [x] Run Node suites and desktop/mobile browser checks.

## Task 4: Review and delivery

- [x] Update README with supported fields, operation, protocol and RAM lifetime.
- [x] Request independent code review and fix substantive findings.
- [x] Review staged diff against original worktree and create local Git commit.

## Validation evidence

45 Node tests passed, including source-header persistence and compiled startup restoration. USB transport/mailbox, DJI motor integration, six self-spin configurations and dynamic-buffer tests passed. Telemetry and vision builds passed. Browser checks passed for tuning, monitor, dialogs, zoom, trace selection and history; tuning layout checked at 1440/1000/390/320 px. One zoom assertion failed in a shared browser session; rerunning zoom, selection and history in a clean session passed. Independent review findings were fixed and re-reviewed. No physical board was flashed or tested.
