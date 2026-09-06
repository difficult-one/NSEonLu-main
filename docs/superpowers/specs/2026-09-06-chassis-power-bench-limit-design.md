# Chassis Power Bench Limit Design

## Goal

Allow deliberate chassis bench testing without referee data while keeping the production firmware fail-safe by default.

## Design

- Add `CHASSIS_POWER_BENCH_TEST` to `application/robot_def.h`; its checked-in value is `0`.
- Add `CHASSIS_POWER_BENCH_LIMIT_W` beside it; use a conservative `40.0f` default when bench mode is explicitly enabled.
- Resolve the limit at the chassis boundary before calling `DJIChassisPowerSetLimit()`.
- A positive referee limit always wins. The bench limit is used only when bench mode is enabled and the referee limit is non-positive or non-finite.
- With bench mode disabled, preserve the current fail-safe behavior exactly: an invalid referee limit reaches the power model and produces zero motor current.

## Verification

- Host tests cover disabled fallback, enabled fallback, referee priority, and invalid bench configuration.
- The existing power-model test suite remains green.
- A clean ARM firmware build succeeds with the checked-in default (`CHASSIS_POWER_BENCH_TEST == 0`).

## Safety

Bench mode is compile-time only and must not be enabled in committed production configuration. Wheels must be lifted before using it.
