# Chassis Power Bench Limit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a compile-time-gated chassis bench power fallback that is disabled in the checked-in production build.

**Architecture:** Keep the power algorithm unchanged. Add a pure limit-selection function beside it, configure the compile-time switch in `robot_def.h`, and let `chassis.c` pass either zero (production) or the configured fallback into that function.

**Tech Stack:** C11, GNU Arm Embedded Toolchain, MinGW/GCC host tests, Make.

## Global Constraints

- `CHASSIS_POWER_BENCH_TEST` is checked in as `0`.
- `CHASSIS_POWER_BENCH_LIMIT_W` is `40.0f`.
- A valid positive referee limit always has priority.
- Disabled or invalid fallback preserves zero-current fail-safe behavior.

---

### Task 1: Test and implement power-limit selection

**Files:**
- Modify: `tests/power_model/test_power_model.c`
- Modify: `modules/algorithm/power_model.h`
- Modify: `modules/algorithm/power_model.c`

**Interfaces:**
- Produces: `float PowerModelSelectLimit(float referee_limit_w, float fallback_limit_w)`

- [x] Add tests asserting `0` fallback remains invalid, `40.0f` is selected for an invalid referee limit, and a positive referee limit overrides `40.0f`.
- [x] Run `make -C tests/power_model clean test` and verify compilation fails because `PowerModelSelectLimit` is missing.
- [x] Implement the minimal finite-positive selection rule.
- [x] Run `make -C tests/power_model clean test` and verify all tests pass.

### Task 2: Add the compile-time gate and integrate it

**Files:**
- Modify: `application/robot_def.h`
- Modify: `application/chassis/chassis.c`
- Modify: `modules/motor/power_control.md`

**Interfaces:**
- Consumes: `PowerModelSelectLimit(float referee_limit_w, float fallback_limit_w)`

- [x] Define `CHASSIS_POWER_BENCH_TEST 0` and `CHASSIS_POWER_BENCH_LIMIT_W 40.0f` with safety comments.
- [x] In `ChassisTask`, pass `0.0f` as fallback when disabled and the configured limit when enabled.
- [x] Document how to enable bench mode, rebuild, test with lifted wheels, and disable it again.
- [x] Run the host tests plus clean full firmware builds with bench mode enabled and disabled.
