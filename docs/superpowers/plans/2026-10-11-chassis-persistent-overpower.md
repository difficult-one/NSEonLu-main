# Chassis Persistent Overpower Implementation Plan

> **For agentic workers:** Execute inline with test-driven-development and requesting-code-review. Steps use checkbox syntax for tracking.

**Goal:** Allow stage 3 to remove persistent measured overpower without learning short transient spikes.

**Architecture:** The motor-cycle transient controller confirms sustained fresh driving overpower and publishes a negative-learning flag. The chassis task passes it to stage 3, where a negative-only PI path bypasses model-demand gates while retaining recovery, timing, braking and measurement checks.

**Tech Stack:** C11 host regression tests, GNU Make, STM32F407 ARM GNU firmware build.

## Global Constraints

- Confirmation uses the existing `stable_run_ms` (default 100 ms).
- Stages 0/1/2 keep their existing slow-loop behavior.
- Preserve all six pre-existing modified files' unrelated edits; stage only this task's chassis and DJI driver hunks.
- No flashing or hardware performance claims; create a local Git commit after validation.

## Task 1: Persistent negative feedback

**Files:** `modules/algorithm/chassis_power_transient.{c,h}`, `modules/algorithm/chassis_power_outer_loop.{c,h}`, `modules/motor/DJImotor/dji_motor.{c,h}`, `application/chassis/chassis.c`, `tests/chassis_power_overpower/{Makefile,test_chassis_power_overpower.c}`, `docs/chassis-power-startup.md`.

**Interfaces:** Add `negative_learning_allowed` to transient output and outer-loop input, a confirmed threshold to transient output, and shared deadband to the atomic control snapshot. Add the continuous-overpower timer and previous fresh power to transient state; all existing initializers remain valid through zero initialization.

- [x] Add regression tests using actual transient/outer functions. A 50 W budget, 49 W target and controllable 8 W model bias must settle within 0.5 W after 5 s; 20 ms spikes must leave slow state frozen.
- [x] Run `make -C tests/chassis_power_overpower CC=gcc test` and observe the existing controller fail sustained-convergence assertions before changing production code.
- [x] Confirm sustained fresh non-braking drive above target plus deadband in the transient controller, expose the flag and threshold, and pass them through the chassis snapshot freshness/threshold gate.
- [x] Add a stage-3 negative-only learning branch: raw and filtered error beyond deadband, no model-demand gate, upper saturation 0 W, no low-demand decay while actively correcting.
- [x] Run the new suite plus outer-loop, transient, power-model, budget and DJI stage/meter combinations. Build fresh ARM firmware in `build/persistent-overpower-stage3`.
- [x] Document behavior and checks, request read-only review, resolve actionable findings, stage only task changes and commit `fix: correct persistent chassis overpower in stage 3`.
