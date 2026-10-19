# Power Loop Deadband and Calibration Telemetry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Tighten the power-loop deadband to 0.3 W and provide opt-in calibration data without changing the default 17-column CSV.

**Architecture:** Keep the existing control formatter and snapshots. A compile-time flag selects an extended writer/header that appends signed limited currents, signed actual rpm, and the meter receive tick to the same row.

**Tech Stack:** C11, STM32F407, GCC host tests, ARM GNU Toolchain, Make/CMake.

## Global Constraints

- User approved this scope; execute inline without repeating design approval.
- Preserve model coefficients, PI gains, filtering, protection, scheduling and allocation.
- Default CSV has 17 columns; opt-in calibration CSV has the same prefix plus 9 fields.
- `CHASSIS_POWER_CALIBRATION_ENABLE` defaults to 0 and accepts only 0/1.
- Do not flash firmware as part of this task. The user subsequently authorized automatic local Git commits after completed and verified work, including these changes.

### Task 1: Calibration output

**Files:** `modules/telemetry/rtt_telemetry.h`, `modules/telemetry/rtt_telemetry.c`, `tests/rtt_telemetry/test_rtt_telemetry.c`, `tests/dji_chassis_power_rtt/test_dji_chassis_power_rtt.c`, their Makefiles, root `Makefile`, `CMakeLists.txt`.

**Interfaces:** Existing public signatures are unchanged. `RTTTelemetryFormatChassisPowerCsv()` remains 17-column; `RTTTelemetryWriteChassisPowerHeader/Row()` select the mode. The new internal formatter consumes the existing motor snapshot, control snapshot and meter tuple.

- [x] Add header/row expectations for flag 1; verify signed currents, rpm, disabled outputs and timestamp with literal expected CSV rows.
- [x] Observe missing-mode failures with:

```powershell
make -B -C tests/rtt_telemetry CC=gcc CFLAGS="-std=c11 -Wall -Wextra -Werror -pedantic -O0 -g -DCHASSIS_POWER_CALIBRATION_ENABLE=1" test
```

- [x] Define the flag and compile-time validation in the telemetry header:

```c
#ifndef CHASSIS_POWER_CALIBRATION_ENABLE
#define CHASSIS_POWER_CALIBRATION_ENABLE 0
#endif
#if CHASSIS_POWER_CALIBRATION_ENABLE != 0 && CHASSIS_POWER_CALIBRATION_ENABLE != 1
#error CHASSIS_POWER_CALIBRATION_ENABLE must be 0 or 1
#endif
```

- [x] Add the extended header and internal formatter. Format the existing 17 columns, replace its CRLF with a comma and 9 additional integer fields, and terminate with CRLF. Reject insufficient buffers by clearing the row. Use the existing saturating rounding function.
- [x] Forward the flag from Make/CMake and the two host-test Makefiles; run both test modes with `-B` to prevent stale objects.

### Task 2: Deadband and verification

**Files:** `application/chassis/chassis.c`, `docs/chassis-power-calibration.md`, this plan.

- [x] Apply only the approved configuration change:

```c
.error_deadband_w = 0.3f,
```

- [x] Run outer-loop, model, budget, meter and telemetry regressions. Confirm the model coefficient diff is empty.
- [x] Fully build default, calibration and meter-disabled firmware in separate directories. Example:

```powershell
New-Item -ItemType Directory -Force build/power_calibration | Out-Null
make -j8 BUILD_DIR=build/power_calibration CHASSIS_POWER_CALIBRATION_ENABLE=1
```

- [x] Document field units, opt-in commands, default-mode recovery, timestamp limitations and real-car collection steps.
- [x] Request an independent read-only review and address actionable findings. Finish with verified test/build results and hardware-validation limits.

## Verification record

- Telemetry and DJI power integration suites first failed on missing calibration output, then passed in both macro modes after implementation.
- Additional telemetry cases cover signed rounding, nonfinite values, integer saturation, timestamp wraparound and null input. The integration suite verifies disabled/offline outputs and uses a nonzero power model to distinguish limited currents from PID demand.
- Eleven relevant host suites passed: `rtt_telemetry`, `dji_chassis_power_rtt`, `chassis_power_outer_loop`, `power_model`, `chassis_power_budget`, `chassis_power_link`, `power_meter`, `power_meter_protocol`, `super_cap`, `super_cap_protocol`, and `referee_power`. The two telemetry-related suites passed with both macro values. Invalid value 2 was rejected at compile time.
- A probe using the actual chassis configuration and the supplied final-row state produced error 0.96 W, correction 1.16 W and integral 1.0648 W. The original 1 W deadband would have suppressed that error. This checks software behavior, not vehicle convergence.
- Complete ARM builds passed for `build/power_control` (flag 0), `build/power_calibration` (flag 1), and `build/power_no_meter` (flag 0, meter disabled). Flash usage was 88472, 88788 and 85960 bytes respectively. Existing newlib syscall-stub linker warnings remain. CMake option forwarding was inspected; CMake builds were not run.
- Independent review found no runtime correctness issue. Its mixed driving/braking sample concern was addressed with per-wheel sign filtering in the collection guide; the suggested limited-current integration coverage was added and passed in both modes.
- `git diff --check` passed. The chassis configuration diff changes only the deadband; all six model coefficients remain unchanged.
- Firmware has not been flashed. Real-car target tracking, noise and model fit require fresh calibration logs across multiple operating conditions.
