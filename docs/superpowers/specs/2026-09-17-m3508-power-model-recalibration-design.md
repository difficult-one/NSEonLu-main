# M3508 Power Model Recalibration Design

## Goal

Replace the six M3508 power-model coefficients with values fitted from
`C:\Users\20802\Desktop\3.csv`, while leaving current conversion, power budgeting,
allocation, and safety parameters unchanged.

## Source Data

The CSV contains 4,182 samples at an exact 10 ms interval, covering 41.81 seconds.
Each row provides four limited current commands, four measured motor speeds, four
target speeds, the existing model prediction, and the super-capacitor module's
measured chassis power.

The fit uses:

```text
I_i = abs(current_command_i) / 1000
n_i = abs(actual_speed_rpm_i)
P_measured = p_cap_cw / 100
```

## Sample Selection

The current model represents positive consumption only because it uses absolute
current and speed. Regenerative braking samples cannot be represented by this
model and are excluded when either condition is true:

```text
P_measured < 0
sum(current_command_i * actual_speed_rpm_i) < 0
```

This leaves 3,826 motoring and idle samples. Idle samples remain in the fit so
the constant term accounts for the measured chassis baseline seen by the
super-capacitor power measurement.

## Regression Model

All four motors continue to share one coefficient set. Each CSV row is fitted
against the aggregate equation:

```text
P_measured = 4*k0
           + k1*sum(I_i)
           + k2*sum(n_i)
           + k3*sum(I_i*n_i)
           + k4*sum(I_i^2)
           + k5*sum(n_i^2)
```

Ordinary least squares on the selected samples produces:

```text
k0 = 1.10671243
k1 = -0.275458501
k2 = 0.000465227105
k3 = 0.00166421676
k4 = 0.228857816
k5 = 1.11735923e-7
```

The firmware constants use enough significant digits to preserve the fitted
prediction within normal `float` precision.

## Validation Result

On the selected 3,826 samples:

| Metric | Existing model | Recalibrated model |
| --- | ---: | ---: |
| MAE | 2.02 W | 1.26 W |
| RMSE | 2.57 W | 1.88 W |
| R-squared | 0.909 | 0.951 |

The recalibrated per-motor prediction remains positive for every observed CSV
sample. Regenerative samples remain outside the model's intended domain and are
not acceptance data.

## Code Change

Only `m3508_power_model.k0` through `.k5` in
`application/chassis/chassis.c` change. The following remain unchanged:

- `current_conversion = 1000.0f`;
- `safety_factor = 0.95f`;
- small-error and reserved-power thresholds;
- per-motor reserved power;
- maximum current command;
- power-allocation and current-limiting algorithms.

## Risk Boundary

The captured current commands range only to approximately 5,081, while firmware
allows commands up to 15,000. The new coefficients are validated only over the
captured operating envelope. They must first be used for a controlled bench
retest with the wheels safely raised or on a controlled load. Competition use at
higher simultaneous current and speed requires additional measured data.

## Verification

After changing the constants:

1. Update the host-side known-prediction fixture to the new literal result and
   confirm it fails before the production constants change.
2. Run the complete power-model host test suite.
3. Run the chassis power budget and feedback regression suites.
4. Build the complete ARM firmware.
5. Inspect the diff to confirm that only the six constants and their directly
   dependent test fixture changed.

## Completion Criteria

- The six firmware coefficients exactly match the approved fitted values within
  `float` precision.
- No power-control policy parameter changes.
- Related host tests pass.
- The ARM firmware links successfully without new warnings or errors.
- Existing unrelated working-tree changes remain untouched.
