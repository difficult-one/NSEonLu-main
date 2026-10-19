# Super-Capacitor Power RTT Telemetry Design

## Goal

Append the chassis power measured by the super-capacitor module to the existing
100 Hz chassis-power RTT CSV stream so offline model fitting can compare measured
and predicted total power in the same log row.

## Scope

The existing telemetry fields, control behavior, power budget, motor output, and
RTT rate remain unchanged. The CSV gains one final column:

```text
p_cap_cw
```

`p_cap_cw` is the latest `SuperCapStatus_s.chassis_power_w` value converted to
centiwatts and rounded with the same saturating conversion used by the predicted
power columns.

No validity column or special offline representation is added. Before the first
valid super-capacitor status frame the value is zero. If the super-capacitor later
goes offline, its module retains the last decoded power and the RTT stream keeps
printing that retained value.

## Data Flow

1. `UpdateChassisPowerBudget()` obtains the current `SuperCapStatus_s` snapshot.
2. The chassis application passes `chassis_power_w` to the DJI chassis-power
   telemetry layer through a dedicated setter.
3. The motor-control task keeps building the existing coherent four-motor
   telemetry snapshot at 1 kHz.
4. Every tenth motor-control invocation, the RTT formatter appends the latest
   super-capacitor measured power to the existing CSV row.

The measured value is observational only. It must not affect power allocation,
current limiting, the applied budget, or motor commands.

## CSV Format

The current columns stay in their existing order. The header becomes:

```text
tick,i_lf,i_rf,i_lb,i_rb,rpm_lf,rpm_rf,rpm_lb,rpm_rb,target_lf,target_rf,target_lb,target_rb,p_lf_cw,p_rf_cw,p_lb_cw,p_rb_cw,p_total_cw,p_cap_cw
```

All power columns use centiwatts because the reduced RTT formatting path does not
print floating-point values.

## Interfaces

The DJI chassis-power layer exposes one setter for the latest observed chassis
power. The setter accepts watts and normalizes non-finite input to zero before it
is stored. The RTT formatter accepts or reads this stored value and emits it as
the final CSV field.

No super-capacitor type is introduced into the algorithm module, preserving the
existing separation between the pure power-model code and hardware modules.

## Testing

Host-side power-model tests verify that:

- a finite measured value is appended in centiwatts;
- negative measured power preserves its sign;
- a value too large for the integer CSV representation saturates safely;
- the existing fields and their order remain unchanged;
- a too-small output buffer still rejects the complete row.

After host tests pass, build the complete firmware to verify the application,
DJI motor, super-capacitor, and RTT integration.

## Completion Criteria

- Each RTT data row contains exactly 19 integer fields.
- `p_cap_cw` is the last field and corresponds to the latest retained
  `SuperCapStatus_s.chassis_power_w` value.
- Existing power-control behavior is unchanged.
- Power-model host tests and the ARM firmware build pass without new warnings.
