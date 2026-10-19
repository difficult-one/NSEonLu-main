#include "chassis_spin.h"
#include "chassis_power_budget.h"

#include <math.h>
#include <stdio.h>

static int failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)

static const ChassisSpinConfig_s spin_config = {
    .fixed_wz = 800, .min_wz = 200, .max_wz = 6000,
    .gain_wz_per_w_s = 40, .rise_wz_per_s = 400, .fall_wz_per_s = 2000,
    .power_lpf_tau_s = .1f, .power_deadband_w = 1,
    .settle_time_s = .2f, .budget_drop_reset_w = 1,
    .sine_amplitude_wz = 200, .sine_period_s = 4, .sine_ramp_s = 1,
    .sine_rise_wz_per_s = 400, .sine_fall_wz_per_s = 2000,
    .sine_direction = 1,
};

static const ChassisPowerBudgetConfig_s budget_config = {
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

static void DemandWithdrawalKeepsTheHealthyWave(int direction, bool withdraw_demand)
{
    ChassisSpinConfig_s config = spin_config;
    config.sine_direction = (int8_t)direction;
    ChassisSpinState_s spin = {0}, reference = {0};
    ChassisSpinInput_s input = {.spin_enabled = true, .control_enabled = true,
        .feedback_valid = true, .increase_allowed = true, .measured_power_w = 20};
    ChassisPowerBudgetInput_s budget_input = {.referee_limit_w = 80,
        .buffer_control_enabled = true, .buffer_data_valid = true,
        .buffer_energy_j = 60, .input_power_valid = true, .measured_input_power_w = 20};
    ChassisPowerBudgetState_s budget_state = {0};
    ChassisPowerBudgetOutput_s budget = {0};
    float target = 0, reference_target = 0, max_error = 0, max_step = 0;
    float min_budget = 200, max_budget = 0;
    const float quadrants[] = {800, 1000, 800, 600, 800};
    for (unsigned n = 0; n < 2400; ++n)
    {
        const uint32_t now_ms = (n + 1) * 5;
        if (n % 20 == 0) budget_input.buffer_tick_ms = now_ms;
        budget_input.buffer_age_s = (now_ms - budget_input.buffer_tick_ms) * .001f;
        /* Alternate real drive demand and a low-demand/no-drive interval while
         * the input meter and referee buffer remain healthy. */
        const bool high_demand = n < 1600 || n % 80 < 40;
        budget_input.demand_valid = high_demand || !withdraw_demand;
        budget_input.requested_load_power_w = high_demand || withdraw_demand ? 135 : 60;
        CHECK(ChassisPowerBudgetUpdate(&budget_config, &budget_input, .005f, &budget_state, &budget));
        CHECK(!budget.recovering && budget.buffer_lower_j > 30);
        input.target_power_w = budget.applied_budget_w * .98f;
        input.energy_recovering = budget.recovering;
        const float previous = target;
        CHECK(ChassisSpinUpdate(&config, &input, .005f, &spin, &target));
        ChassisSpinInput_s reference_input = input;
        reference_input.target_power_w = 137.2f;
        CHECK(ChassisSpinUpdate(&config, &reference_input, .005f, &reference, &reference_target));
        max_error = fmaxf(max_error, fabsf(target - reference_target));
        if (n >= 1600)
        {
            max_step = fmaxf(max_step, fabsf(target - previous));
            min_budget = fminf(min_budget, input.target_power_w);
            max_budget = fmaxf(max_budget, input.target_power_w);
        }
        if (n >= 1599 && (n - 1599) % 200 == 0)
            CHECK(fabsf(target - direction * quadrants[(n - 1599) / 200]) < .5f);
    }
    printf("buffer sine direction %d, withdraw %d: budget %.1f..%.1f W, max error %.2f, max 5 ms step %.2f\n",
        direction, withdraw_demand, min_budget, max_budget, max_error, max_step);
    CHECK(min_budget < 79 && max_budget > 137);
    CHECK(max_error < .01f);
    CHECK(max_step <= 2.01f); /* Full 4 s / amplitude 200 sine needs <= 1.58. */
}

int main(void)
{
    DemandWithdrawalKeepsTheHealthyWave(1, false);
    DemandWithdrawalKeepsTheHealthyWave(-1, false);
    DemandWithdrawalKeepsTheHealthyWave(1, true);
    DemandWithdrawalKeepsTheHealthyWave(-1, true);
    if (failures) return 1;
    puts("PASS: demand-driven buffer budgets preserve healthy sine targets");
    return 0;
}
