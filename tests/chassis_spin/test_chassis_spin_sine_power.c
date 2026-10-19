#include "chassis_spin.h"
#include "chassis_spin_config.h"
#include "chassis_power_budget.h"
#include "power_model.h"

#include <math.h>
#include <stdio.h>

static int failures;
#define CHECK(x) do { if (!(x)) { if (failures < 15) printf("FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)

static float Clamp(float x, float lo, float hi) { return fminf(hi, fmaxf(lo, x)); }

/* Same selected parameters as the chassis application, including user overrides. */
static const ChassisSpinConfig_s spin_config = {
    .fixed_wz = CHASSIS_SPIN_SINE_CENTER_WZ,
    .min_wz = CHASSIS_SPIN_SINE_MIN_WZ,
    .max_wz = CHASSIS_SPIN_MAX_WZ,
    .gain_wz_per_w_s = CHASSIS_SPIN_POWER_GAIN,
    .rise_wz_per_s = CHASSIS_SPIN_RISE_RATE,
    .fall_wz_per_s = CHASSIS_SPIN_FALL_RATE,
    .power_lpf_tau_s = CHASSIS_SPIN_POWER_LPF_TAU_S,
    .power_deadband_w = CHASSIS_SPIN_POWER_DEADBAND_W,
    .settle_time_s = CHASSIS_SPIN_SETTLE_TIME_S,
    .budget_drop_reset_w = CHASSIS_SPIN_BUDGET_DROP_RESET_W,
    .sine_amplitude_wz = CHASSIS_SPIN_SINE_AMPLITUDE_WZ,
    .sine_period_s = CHASSIS_SPIN_SINE_PERIOD_S,
    .sine_ramp_s = CHASSIS_SPIN_SINE_RAMP_S,
    .sine_rise_wz_per_s = CHASSIS_SPIN_SINE_RISE_RATE,
    .sine_fall_wz_per_s = CHASSIS_SPIN_SINE_FALL_RATE,
    .sine_direction = CHASSIS_SPIN_SINE_DIRECTION,
};

static void DefaultSineRecoversItsActualWaveAfterPowerLimiting(bool dynamic_buffer)
{
    const float dt = .005f, lever = 5.67232007f;
    const ChassisPowerAlgorithmConfig_s algorithm = {
        .safety_factor = .98f, .small_error_threshold_rpm = 500,
        .reserved_power_threshold_w = 54, .per_motor_reserved_power_w = 8,
        .max_current_command = 15000,
    };
    MotorPowerModelConfig_s models[4];
    for (unsigned w = 0; w < 4; ++w)
        models[w] = (MotorPowerModelConfig_s){
            .k0 = 1.3123816f, .k1 = -.25836727f, .k2 = .0007424524f,
            .k3 = .0024271158f, .k4 = .081198552f, .k5 = 3.890596e-8f,
            .current_conversion = 1000,
        };
    ChassisPowerTransientConfig_s guard_config = ChassisPowerTransientDefaultConfig();
    guard_config.eta = algorithm.safety_factor;
    ChassisPowerTransientState_s guard_state = {0};
    ChassisSpinState_s spin = {0};
    const ChassisPowerBudgetConfig_s budget_config = {
        .empty_energy_ratio = .1f, .full_energy_ratio = .3f,
        .max_boost_w = 100, .rise_rate_w_per_s = 200, .demand_margin_w = 5,
        .buffer = {.max_energy_j = 60, .recovery_enter_j = 15,
            .recovery_exit_j = 30, .hard_floor_j = 5,
            .prediction_window_s = .5f, .guard_window_s = .3f,
            .settlement_delay_s = .1f, .energy_margin_j = 3, .power_margin_w = 5,
            .max_boost_w = 60, .recovery_power_w = 20, .recovery_target_time_s = 1,
            .observer_max_step_s = .020f, .recovery_hold_s = .1f,
            .rise_rate_w_per_s = 600, .safety_factor = .98f,
            .correction_rise_j_per_s = 20},
    };
    ChassisPowerBudgetState_s budget_state = {0};
    ChassisPowerBudgetInput_s budget_input = {.buffer_control_enabled = true,
        .buffer_data_valid = true, .input_power_valid = true};
    ChassisSpinInput_s input = {.spin_enabled = true, .control_enabled = true,
        .target_power_w = 49};
    ChassisPowerTransientInput_s motor = {.budget_w = 50, .target_w = 49,
        .power_ceiling_w = 50, .power_deadband_w = .3f,
        .meter_enabled = true, .meter_online = true, .control_enabled = true,
        .online_mask = 15, .enabled_mask = 15};
    float target = 0, actual_rpm = 0, sent[4] = {0};
    float cycle_min[3] = {30000, 30000, 30000}, cycle_max[3] = {0};
    float phase_speed[800] = {0}, repeat_error = 0, max_power = 0;
    unsigned compared = 0;
    bool saw_limiting = false, saw_braking = false, saw_boost = false, saw_withdrawal = false;
    float buffer_energy_j = 60, previous_budget = 0, max_target_step = 0;
    for (unsigned n = 0; n < 4800; ++n)
    {
        motor.now_ms = motor.meter_tick_ms = (n + 1) * 5;
        motor.meter_w = 4 * PowerModelPredict(&models[0], sent[0], actual_rpm);
        /* Include a real source reduction, then restore enough average power.
         * The dynamic case uses previous motor demand, like the application. */
        const float base_w = n >= 800 && n < 1200 ? 25 : 40;
        if (dynamic_buffer)
        {
            buffer_energy_j = Clamp(buffer_energy_j + (base_w - motor.meter_w) * dt, 0, 60);
            budget_input.referee_limit_w = base_w;
            budget_input.measured_input_power_w = motor.meter_w;
            if (n % 20 == 0)
            {
                budget_input.buffer_tick_ms = motor.now_ms;
                budget_input.buffer_energy_j = floorf(buffer_energy_j);
            }
            budget_input.buffer_age_s = (motor.now_ms - budget_input.buffer_tick_ms) * .001f;
            ChassisPowerBudgetOutput_s budget;
            CHECK(ChassisPowerBudgetUpdate(&budget_config, &budget_input, dt, &budget_state, &budget));
            motor.budget_w = budget.applied_budget_w;
            input.energy_recovering = budget.recovering;
            saw_boost |= motor.budget_w > base_w + 1;
            saw_withdrawal |= previous_budget > motor.budget_w + 1 && motor.budget_w >= base_w;
            previous_budget = motor.budget_w;
        }
        else motor.budget_w = n >= 800 && n < 1200 ? 35 : 50;
        motor.power_ceiling_w = motor.budget_w;
        motor.target_w = input.target_power_w = .98f * motor.budget_w;
        ChassisPowerInput_s power_input = {.referee_power_limit_w = 50, .attenuation = 1};
        bool tracking = true, saturated = false;
        for (unsigned w = 0; w < 4; ++w)
        {
            motor.actual_rpm[w] = actual_rpm;
            motor.target_rpm[w] = target * lever;
            const float error = motor.target_rpm[w] - actual_rpm;
            motor.raw_current[w] = Clamp(4.5f * error, -15000, 15000);
            motor.feedback_tick_ms[w] = motor.now_ms;
            motor.feedback_sequence[w] = n + 1;
            power_input.desired_current[w] = motor.raw_current[w];
            power_input.speed_rpm[w] = actual_rpm;
            power_input.speed_error_rpm[w] = error;
            tracking &= fabsf(error) <= 500;
            saturated |= fabsf(motor.raw_current[w]) >= 15000;
        }
        ChassisPowerTransientOutput_s guard;
        CHECK(ChassisPowerTransientUpdate(&guard_config, &motor, dt, &guard_state, &guard));
        power_input.referee_power_limit_w = guard.model_budget_w;
        ChassisPowerOutput_s power;
        CHECK(PowerModelApply(models, &algorithm, &power_input, &power));
        budget_input.requested_load_power_w = fmaxf(motor.meter_w,
            power.predicted_unlimited_power_w[0] + power.predicted_unlimited_power_w[1] +
            power.predicted_unlimited_power_w[2] + power.predicted_unlimited_power_w[3]);
        budget_input.demand_valid = guard.drive_mask != 0 && guard.brake_mask == 0;
        (void)ChassisPowerTransientGuard(&guard_config, models, &motor,
            power.limited_current, &guard_state, &guard);
        bool limited = saturated || !ChassisSpinGuardAllowsIncrease(&guard);
        for (unsigned w = 0; w < 4; ++w)
        {
            limited |= (guard.drive_mask & (1U << w)) &&
                fabsf(power.limited_current[w]) < fabsf(motor.raw_current[w]);
            sent[w] = (float)(int16_t)guard.guarded_current[w];
            CHECK(fabsf(sent[w]) <= 15000);
        }
        ChassisPowerTransientRecordSent(&guard_state, sent);
        input.feedback_valid = !guard.fault && guard.feedback_fresh && guard.meter_fresh;
        input.measured_power_w = motor.meter_w;
        input.power_limited = limited;
        input.increase_allowed = tracking && !limited && ChassisSpinTransientAllowsIncrease(&guard);
        const float previous = target;
        CHECK(ChassisSpinUpdate(&spin_config, &input, dt, &spin, &target));
        CHECK(target >= 0 && !guard.fault);
        if (previous > 0)
        {
            CHECK(target - previous <= spin_config.sine_rise_wz_per_s * dt + .01f);
            CHECK(previous - target <= spin_config.sine_fall_wz_per_s * dt + .01f);
            max_target_step = fmaxf(max_target_step, fabsf(target - previous));
        }
        if (previous > 0 && !input.increase_allowed) CHECK(target <= previous + .01f);
        saw_limiting |= limited;
        saw_braking |= guard.brake_mask != 0;
        max_power = fmaxf(max_power, guard.guard_power_w);
        if (!guard.guard_infeasible && !guard.guard_search_exhausted)
        {
            CHECK(guard.guard_power_w <= guard.allowed_guard_w + .001f);
            CHECK(guard.guard_power_w <= motor.power_ceiling_w + .001f);
        }
        else
            for (unsigned w = 0; w < 4; ++w)
                if (guard.drive_mask & (1U << w)) CHECK(sent[w] == 0);

        /* Independent 50 ms speed response with 100 command units of drag and
         * a 9000 rpm voltage ceiling. Only the final guarded current drives it. */
        const float drag = 100 * Clamp(actual_rpm / 200, -1, 1);
        actual_rpm = Clamp(actual_rpm + dt * (sent[0] - drag) / (4.5f * .05f), -9000, 9000);
        if (n >= 2400)
        {
            const unsigned cycle = (n - 2400) / 800, phase = n % 800;
            cycle_min[cycle] = fminf(cycle_min[cycle], actual_rpm);
            cycle_max[cycle] = fmaxf(cycle_max[cycle], actual_rpm);
            if (cycle != 0) { repeat_error += fabsf(actual_rpm - phase_speed[phase]); ++compared; }
            phase_speed[phase] = actual_rpm;
        }
    }
    printf("%s sine: rpm %.0f..%.0f / %.0f..%.0f / %.0f..%.0f, repeat error %.1f rpm, guard peak %.1f W, max target step %.2f\n",
        dynamic_buffer ? "dynamic buffer" : "50 W",
        cycle_min[0], cycle_max[0], cycle_min[1], cycle_max[1], cycle_min[2], cycle_max[2],
        repeat_error / compared, max_power, max_target_step);
    CHECK(saw_limiting && saw_braking);
    if (dynamic_buffer) CHECK(saw_boost && saw_withdrawal);
    for (unsigned c = 0; c < 3; ++c)
    {
        CHECK(cycle_min[c] > 3000 && cycle_min[c] < 3800);
        /* At 40 W the genuine protection gates may reduce the peak. */
        CHECK(cycle_max[c] > (dynamic_buffer ? 4800 : 5300) && cycle_max[c] < 6000);
        CHECK(cycle_max[c] - cycle_min[c] > 1500);
    }
    CHECK(repeat_error / compared < 150);
}

int main(void)
{
    DefaultSineRecoversItsActualWaveAfterPowerLimiting(false);
    DefaultSineRecoversItsActualWaveAfterPowerLimiting(true);
    if (failures) return 1;
    puts("PASS: configured sine spin through final power guards");
    return 0;
}
