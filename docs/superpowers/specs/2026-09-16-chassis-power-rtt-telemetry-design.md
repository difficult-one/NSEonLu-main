# Chassis Power RTT Telemetry Design

## Goal

Add one coherent telemetry structure for observing the four chassis motors while validating the existing power model. Update the structure from the 1 kHz DJI motor-control path and print a decimated CSV row through J-Link RTT at 100 Hz.

## Scope

The telemetry structure contains only:

- four limited current commands actually sent to the chassis motors;
- four measured motor speeds in rpm;
- four target motor speeds in rpm;
- four power-model predictions corresponding to the limited current commands;
- the sum of the four predicted powers.

Super-capacitor status and measured power-meter power are explicitly excluded. The existing power-control behavior, budget calculation, and motor output are unchanged.

## Data Structure

Define `ChassisPowerTelemetry_s` next to the power-model input and output types so the algorithm and DJI motor layer can share it without introducing an application-layer dependency:

```c
typedef struct
{
    float current_command[POWER_MODEL_MOTOR_COUNT];
    float actual_speed_rpm[POWER_MODEL_MOTOR_COUNT];
    float target_speed_rpm[POWER_MODEL_MOTOR_COUNT];
    float predicted_power_w[POWER_MODEL_MOTOR_COUNT];
    float predicted_total_power_w;
} ChassisPowerTelemetry_s;
```

`current_command` and `predicted_power_w` use the limited values because these are the values corresponding to the commands actually transmitted to the motors. The target speed is reconstructed as measured speed plus speed error from the same model input snapshot.

## Data Flow

After `PowerModelApply()` succeeds in `ApplyChassisPowerGroup()`:

1. Build the telemetry structure from the same `ChassisPowerInput_s` and `ChassisPowerOutput_s` used by that model invocation.
2. Store it in the existing read-only chassis power state.
3. Increment a private divider counter.
4. Every tenth motor-control invocation, emit one RTT CSV row.

The motor-control task is nominally 1 kHz, so the telemetry snapshot is refreshed at 1 kHz and the RTT stream is nominally 100 Hz.

If the model invocation fails, clear the telemetry structure so stale valid-looking values are not printed.

## RTT Format

Print one header at registration and then one data row per output interval:

```text
tick,i_lf,i_rf,i_lb,i_rb,rpm_lf,rpm_rf,rpm_lb,rpm_rb,target_lf,target_rf,target_lb,target_rb,p_lf_cw,p_rf_cw,p_lb_cw,p_rb_cw,p_total_cw
```

RTT's reduced `printf` implementation does not support `%f`, so values are printed as integers:

- current commands: native command units;
- speeds: rounded rpm;
- powers: centiwatts (`W * 100`);
- tick: `HAL_GetTick()` milliseconds.

The output uses RTT channel 0 and the existing non-blocking skip mode. A full RTT buffer may drop a row rather than block the motor-control task.

## Testing

Add host-side tests for the pure telemetry-building operation:

- copies limited current, measured speed, and predicted limited power for all four motors;
- reconstructs target speed from measured speed plus error;
- sums the four predicted powers;
- rejects null inputs and clears telemetry for invalid model output.

Run the existing power-model and chassis-power-feedback test suites, then build the firmware to catch embedded integration and formatting errors.
