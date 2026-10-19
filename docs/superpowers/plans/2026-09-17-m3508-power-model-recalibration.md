# M3508 Power Model Recalibration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the six chassis M3508 power-model coefficients with the approved motoring-only fit from `3.csv` and verify the resulting firmware.

**Architecture:** Keep the existing polynomial, data flow, and control policy unchanged. Update the application configuration and its host-test calibration fixture together, using one hand-calculated prediction point to prove that the test changes from the old calibration to the new calibration.

**Tech Stack:** C11, MinGW host tests, STM32F407 GNU Arm Embedded build.

## Global Constraints

- Change only `k0` through `k5` for the M3508 model.
- Keep `current_conversion = 1000.0f` unchanged.
- Keep `safety_factor = 0.95f` and all allocation/current-limit settings unchanged.
- Do not change the polynomial or regenerative-power behavior.
- Treat the new calibration as validated only within the captured range of approximately 5,081 current-command units.
- Preserve unrelated working-tree changes in `application/cmd/robot_cmd.c` and `application/robot_def.h`.

---

### Task 1: Apply and verify the recalibrated coefficients

**Files:**
- Modify: `tests/power_model/test_power_model.c:59-67,124-133`
- Modify: `application/chassis/chassis.c:72-80`

**Interfaces:**
- Consumes: the existing `MotorPowerModelConfig_s` and `PowerModelPredict()` polynomial.
- Produces: the approved application configuration and a regression fixture for `current_command = 5000`, `speed_rpm = 3000`.

- [ ] **Step 1: Write the failing calibration expectation**

Rename `TestPredictionMatchesPolynomial()` to
`TestPredictionMatchesRecalibratedM3508Sample()` and replace the expected value
with the independently calculated literal for the approved coefficients:

```c
static void TestPredictionMatchesRecalibratedM3508Sample(void)
{
    const float actual = PowerModelPredict(&m3508_model, 5000.0f, 3000.0f);
    ASSERT_NEAR(32.81542f, actual, 1.0e-4f);
}
```

Update the call in `main()` to use the new test name. Do not change the test
fixture coefficients yet.

- [ ] **Step 2: Run the power-model test and verify RED**

Run:

```powershell
mingw32-make -C tests/power_model clean test
```

Expected: the renamed calibration test fails because the old model predicts
approximately `42.40873 W`, not `32.81542 W`.

- [ ] **Step 3: Update the application and test fixture coefficients**

Set both `m3508_power_model` in `application/chassis/chassis.c` and
`m3508_model` in `tests/power_model/test_power_model.c` to:

```c
.k0 = 1.10671243f,
.k1 = -0.275458501f,
.k2 = 0.000465227105f,
.k3 = 0.00166421676f,
.k4 = 0.228857816f,
.k5 = 1.11735923e-7f,
.current_conversion = 1000.0f,
```

Do not modify any `ChassisPowerAlgorithmConfig_s` field.

- [ ] **Step 4: Run the focused test and verify GREEN**

Run:

```powershell
mingw32-make -C tests/power_model clean test
```

Expected: `PASS: power model tests` without compiler warnings.

- [ ] **Step 5: Run related regression suites**

Run:

```powershell
mingw32-make -C tests/chassis_power_budget clean test
mingw32-make -C tests/chassis_power_feedback clean test
```

Expected: both suites print `PASS`.

- [ ] **Step 6: Build the complete firmware**

Run:

```powershell
mingw32-make -j4
```

Expected: the firmware compiles and links. Existing newlib syscall warnings may
remain; no new source warning or error is acceptable.

- [ ] **Step 7: Inspect the exact diff**

Run:

```powershell
git diff --check
git diff -- application/chassis/chassis.c tests/power_model/test_power_model.c
git status --short
```

Confirm that:

- only the six application coefficients changed;
- the test fixture has the same six values;
- `current_conversion`, `safety_factor`, thresholds, and limits did not change;
- existing changes in `application/cmd/robot_cmd.c` and `application/robot_def.h`
  remain untouched.

- [ ] **Step 8: Commit the recalibration**

```powershell
git add application/chassis/chassis.c tests/power_model/test_power_model.c
git commit -m "tune: recalibrate M3508 chassis power model"
```

---

### Task 2: Bench validation

**Files:**
- No source changes expected.

**Interfaces:**
- Consumes: the rebuilt firmware and the 19-column RTT CSV stream.
- Produces: a second dataset proving measured-versus-predicted accuracy on hardware.

- [ ] **Step 1: Flash and collect a controlled dataset**

Run with wheels safely raised or on a controlled load. Preserve
`safety_factor = 0.95f`; do not raise the current limit for this validation.

- [ ] **Step 2: Confirm the logged prediction improvement**

Exclude rows with `p_cap_cw < 0` or
`sum(current_command_i * actual_speed_rpm_i) < 0`, then compare
`p_total_cw / 100` with `p_cap_cw / 100`. The acceptance target is no regression
from the calibration dataset's approximate `1.26 W` MAE and `1.88 W` RMSE within
the same operating envelope.

- [ ] **Step 3: Keep the high-current boundary explicit**

Do not treat this calibration as validated above approximately 5,081 command
units until a controlled loaded test supplies measurements in that range.
