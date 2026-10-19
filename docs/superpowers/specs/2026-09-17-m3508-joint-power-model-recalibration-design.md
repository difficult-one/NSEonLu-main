# M3508 Joint Power Model Recalibration Design

## Goal

Replace the six M3508 power-model coefficients with a joint fit of the original
high-speed data and the new loaded data. Keep current conversion, power
budgeting, allocation, and every safety or limiting parameter unchanged.

## Source Data

The fit combines two captures:

| Capture | Raw samples | Selected samples | Maximum current command | Maximum speed |
| --- | ---: | ---: | ---: | ---: |
| `C:\Users\20802\Desktop\3.csv` | 4,182 | 3,826 | 5,081 | 5,755 rpm |
| `C:\Users\20802\Desktop\工作簿1.xlsx` | 9,022 | 7,020 | 10,227 | 3,731 rpm |

The workbook stores one 19-field, comma-separated RTT record in each cell of
column A. All 9,022 rows contain the expected number of fields. Its normal
sampling interval is 10 ms, with one 3,571 ms interruption in the capture.

For both captures:

```text
I_i = abs(current_command_i) / 1000
n_i = abs(actual_speed_rpm_i)
P_measured = p_cap_cw / 100
```

## Sample Selection

The existing model represents positive consumption only because it uses
absolute current and speed. Apply the same motoring-domain filter to both data
sets:

```text
P_measured >= 0
sum(current_command_i * actual_speed_rpm_i) >= 0
```

This leaves 10,846 samples in total. Idle samples remain in the fit so the
constant term includes the baseline power measured by the capacitor module.

## Regression Model

All four motors continue to share one coefficient set. Each selected row is
fitted against the aggregate equation:

```text
P_measured = 4*k0
           + k1*sum(I_i)
           + k2*sum(n_i)
           + k3*sum(I_i*n_i)
           + k4*sum(I_i^2)
           + k5*sum(n_i^2)
```

Use unweighted ordinary least squares across all 10,846 selected samples. This
lets every captured operating point contribute equally and gives the loaded
capture proportionally more influence because it contains more observations.
The resulting coefficients are:

```text
k0 = 1.09451151
k1 = 0.0177460735
k2 = 0.000300027877
k3 = 0.00216024892
k4 = 0.158999868
k5 = 1.25787511e-7
```

All fitted coefficients are positive, so predicted power remains monotonic in
absolute current and absolute speed over the model domain.

## Validation Result

The comparison uses the currently installed first-capture calibration as the
baseline:

| Capture | Model | MAE | RMSE | Bias (prediction - measurement) | R-squared |
| --- | --- | ---: | ---: | ---: | ---: |
| Loaded workbook | Current | 3.38 W | 4.94 W | -3.20 W | 0.930 |
| Loaded workbook | Joint fit | 0.82 W | 1.32 W | -0.08 W | 0.995 |
| Original CSV | Current | 1.26 W | 1.88 W | approximately 0 W | 0.951 |
| Original CSV | Joint fit | 1.40 W | 2.17 W | +0.14 W | 0.935 |

The joint fit substantially improves the loaded capture while accepting a small
loss on the original capture. This tradeoff is preferred because the loaded
capture extends current-command coverage and the combined result behaves more
physically than the current negative linear-current coefficient.

## Code Change

Only `m3508_power_model.k0` through `.k5` in
`application/chassis/chassis.c` change, together with the host-test coefficient
fixture and its known prediction. The following remain unchanged:

- `current_conversion = 1000.0f`;
- `safety_factor = 0.95f`;
- small-error and reserved-power thresholds;
- per-motor reserved power;
- maximum current command;
- power-allocation and current-limiting algorithms.

## Risk Boundary

The combined captures cover current commands only through 10,227, while the
firmware permits 15,000. The maximum current and maximum speed also do not
necessarily occur simultaneously. Therefore the new coefficients are validated
only inside the measured envelope; operation above 10,227 current command or at
previously unseen high-current/high-speed combinations still requires a
controlled bench retest before competition use.

## Verification

1. Change the host-side known-prediction expectation first and confirm that it
   fails against the old coefficients.
2. Update the six production constants and matching host-test fixture.
3. Run the complete power-model host test suite.
4. Run the chassis power budget and feedback regression suites.
5. Build the complete ARM firmware.
6. Inspect the diff to confirm that only the six constants and their directly
   dependent test fixture changed.

## Completion Criteria

- The six firmware coefficients exactly match the approved joint-fit values
  within `float` precision.
- No power-control policy parameter changes.
- Related host tests pass.
- The ARM firmware links successfully without new warnings or errors.
- Existing unrelated working-tree changes remain untouched.
