#include "chassis_power_budget.h"
#include <math.h>
#include <stdio.h>

static int failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)
#define NEAR(want, got) CHECK(isfinite(got) && fabsf((want) - (got)) < 0.01f)

static const ChassisPowerBudgetConfig_s config = {
    .empty_energy_ratio = 0.1f, .full_energy_ratio = 0.3f,
    .max_boost_w = 100, .rise_rate_w_per_s = 200, .demand_margin_w = 5,
    .buffer = {.max_energy_j = 60, .recovery_enter_j = 15,
        .recovery_exit_j = 30, .hard_floor_j = 5,
        .prediction_window_s = 0.5f, .guard_window_s = 0.3f,
        .settlement_delay_s = 0.1f, .energy_margin_j = 3,
        .power_margin_w = 5, .max_boost_w = 60,
        .recovery_power_w = 20, .recovery_target_time_s = 1,
        .observer_max_step_s = 0.020f, .recovery_hold_s = 0.1f,
        .correction_rise_j_per_s = 20,
        .rise_rate_w_per_s = 600, .safety_factor = 0.98f},
};

static ChassisPowerBudgetInput_s Input(float energy)
{
    const ChassisPowerBudgetInput_s input = {
        .referee_limit_w = 80, .buffer_control_enabled = true,
        .buffer_data_valid = true, .buffer_energy_j = energy,
        .input_power_valid = true, .measured_input_power_w = 80,
        .demand_valid = true, .requested_load_power_w = 180,
        .confirmed_input_target_w = 137.2f,
    };
    return input;
}

static ChassisPowerBudgetOutput_s Update(ChassisPowerBudgetInput_s input)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s output = {0};
    ChassisPowerBudgetReset(&state, 80);
    for (unsigned k = 0; k < 1000; ++k)
        CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &output));
    return output;
}

static void HighBufferWorksWithoutCap(void)
{
    ChassisPowerBudgetOutput_s out = Update(Input(60));
    NEAR(140, out.target_budget_w);
    NEAR(137.2f, out.input_target_w);
    CHECK(!out.recovering);
}

static void LowBufferActuallyReducesBudget(void)
{
    ChassisPowerBudgetOutput_s out = Update(Input(20));
    NEAR(60.5f, out.applied_budget_w);
    NEAR(59.29f, out.input_target_w);
    CHECK(out.recovering);
}

static void MissingFeedbackRemovesBoost(void)
{
    ChassisPowerBudgetInput_s input = Input(60);
    input.input_power_valid = false;
    NEAR(80, Update(input).applied_budget_w);
    input.input_power_valid = true;
    input.buffer_data_valid = false;
    NEAR(80, Update(input).applied_budget_w);
}

static void CapBudgetCountsBoardTotalOnlyOnce(void)
{
    ChassisPowerBudgetInput_s input = Input(60);
    input.use_supercap = true;
    input.cap_online = input.cap_output_enabled = true;
    input.cap_input_control_ready = true;
    input.load_power_valid = true;
    input.cap_energy_ratio = 0.5f;
    input.cap_reported_limit_w = 180;
    ChassisPowerBudgetOutput_s out = Update(input);
    CHECK(out.applied_budget_w * config.buffer.safety_factor <= 180.01f);
    CHECK(out.applied_budget_w > 140);
    NEAR(137.2f, out.input_target_w);
    input.cap_input_control_ready = false;
    NEAR(80, Update(input).applied_budget_w);
}

static void PredictionInterruptsHighConsumption(void)
{
    ChassisPowerBudgetInput_s input = Input(30);
    input.measured_input_power_w = 140;
    ChassisPowerBudgetOutput_s out = Update(input);
    CHECK(out.recovering);
    CHECK(out.applied_budget_w < 80);
}

static void RecoveryHasHysteresisAndImmediateDownwardLimit(void)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out = {0};
    ChassisPowerBudgetInput_s input = Input(60);
    ChassisPowerBudgetReset(&state, 80);
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(83, out.applied_budget_w);
    input = Input(20);
    input.buffer_tick_ms = 100;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(60.5f, out.applied_budget_w);
    /* Recover at 60 W under an 80 W limit, with 10 Hz integer energy frames. */
    input.measured_input_power_w = 60;
    for (unsigned k = 1; k <= 200; ++k)
    {
        input.buffer_energy_j = 20 + k / 20 * 2;
        input.buffer_tick_ms = 100 + k / 20 * 100;
        input.buffer_age_s = (k % 20) * 0.005f;
        CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
        if (k <= 100) CHECK(out.recovering);
    }
    CHECK(!out.recovering);
    CHECK(out.applied_budget_w > 80 && out.applied_budget_w < 140);
}

static void HealthyCapCanAssistWhileBufferRecovers(void)
{
    ChassisPowerBudgetInput_s input = Input(20);
    input.use_supercap = true;
    input.cap_online = input.cap_output_enabled = true;
    input.cap_input_control_ready = true;
    input.load_power_valid = true;
    input.cap_energy_ratio = 0.5f;
    input.cap_reported_limit_w = 180;
    ChassisPowerBudgetOutput_s out = Update(input);
    CHECK(out.recovering);
    NEAR(59.29f, out.input_target_w);
    NEAR(160.5f, out.applied_budget_w);
    input.cap_energy_ratio = 0.05f;
    NEAR(60.5f, Update(input).applied_budget_w);
    input.cap_error_code = 1;
    NEAR(60.5f, Update(input).applied_budget_w);
}

static void OldFramesAndInvalidNumbersCannotProvideFullBoost(void)
{
    ChassisPowerBudgetInput_s input = Input(60);
    input.buffer_age_s = 0.2f;
    NEAR(125, Update(input).target_budget_w);
    input.buffer_energy_j = NAN;
    NEAR(80, Update(input).applied_budget_w);
    input = Input(60);
    input.measured_input_power_w = INFINITY;
    NEAR(80, Update(input).applied_budget_w);
}

static void BufferSpendingAndRecoveryWithDelayedPowerResponse(void)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out;
    ChassisPowerBudgetInput_s input = Input(60);
    ChassisPowerBudgetReset(&state, 80);
    float energy = 60, actual_power = 80, minimum = 60, peak = 0;
    float sample_energy = 60;
    for (int tick = 0; tick < 1200; ++tick)
    {
        if (tick % 20 == 0)
            sample_energy = floorf(energy);
        input.buffer_energy_j = sample_energy;
        input.buffer_tick_ms = (uint32_t)(tick / 20) * 100U;
        input.buffer_age_s = (float)(tick % 20) * 0.005f;
        input.measured_input_power_w = actual_power;
        const float demand = tick < 400 ? 180 : 30;
        input.requested_load_power_w = demand;
        CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
        CHECK(out.buffer_lower_j <= energy + 0.01f);
        const float requested = fminf(demand, out.applied_budget_w * 0.98f);
        /* Independent plant: 20 ms power response, 10 Hz integer energy frames. */
        actual_power += 0.25f * (requested - actual_power);
        energy = fminf(60, energy + (80 - actual_power) * 0.005f);
        minimum = fminf(minimum, energy);
        peak = fmaxf(peak, actual_power);
    }
    CHECK(peak > 120);
    CHECK(minimum > 5 && minimum < 45);
    CHECK(energy > 59);
}

static void CapRequiresLoadFeedbackAndAlwaysHonorsBoardCapacity(void)
{
    ChassisPowerBudgetInput_s input = Input(60);
    input.use_supercap = true;
    input.cap_online = input.cap_output_enabled = true;
    input.cap_input_control_ready = true;
    input.cap_energy_ratio = 0.5f;
    input.cap_reported_limit_w = 180;
    input.load_power_valid = false;
    NEAR(80, Update(input).applied_budget_w);
    input.cap_reported_limit_w = 40;
    CHECK(Update(input).applied_budget_w * config.buffer.safety_factor <= 40.01f);
}

static void ContinuousEstimateUsesConservativeMeasuredIntervals(void)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out;
    ChassisPowerBudgetInput_s input = Input(60);
    input.measured_input_power_w = 40;
    ChassisPowerBudgetReset(&state, 80);
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(50.5f, out.buffer_lower_j);
    input.buffer_age_s = 0.005f;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(50.675f, out.buffer_lower_j);
    input.measured_input_power_w = 140;
    input.buffer_age_s = 0.010f;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(50.35f, out.buffer_lower_j);
    input.measured_input_power_w = 40;
    input.buffer_age_s = 0.015f;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(50.025f, out.buffer_lower_j);
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(50.025f, out.buffer_lower_j); /* Same observation is not counted twice. */
    input.referee_limit_w = 100;
    input.buffer_age_s = 0.020f;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(50.2f, out.buffer_lower_j); /* New higher limit does not rewrite history. */
    input.referee_limit_w = 60;
    input.buffer_age_s = 0.025f;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(50.275f, out.buffer_lower_j);
}

static void NewSamplesReanchorEvenWhenTheirEnergyIsUnchanged(void)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out;
    ChassisPowerBudgetInput_s input = Input(60);
    input.measured_input_power_w = 140;
    ChassisPowerBudgetReset(&state, 80);
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    input.buffer_age_s = 0.020f;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.020f, &state, &out));
    NEAR(49.2f, out.buffer_lower_j);
    input.buffer_tick_ms = 100;
    input.buffer_age_s = 0;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(46, out.buffer_lower_j); /* 80 ms gap loss, then bounded new-frame correction. */
    input.buffer_energy_j = 20;
    input.buffer_tick_ms = 200;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(10.5f, out.buffer_lower_j);
    CHECK(out.recovering);
}

static void FeedbackGapsSpendWorstCaseAndRequireContinuousRecovery(void)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out;
    ChassisPowerBudgetInput_s input = Input(60);
    input.measured_input_power_w = 40;
    ChassisPowerBudgetReset(&state, 80);
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    input.input_power_valid = false;
    input.buffer_age_s = 0.005f;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(50.175f, out.buffer_lower_j);
    NEAR(80, out.applied_budget_w);
    input.input_power_valid = true;
    input.buffer_age_s = 0.010f;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(49.85f, out.buffer_lower_j);
    NEAR(80, out.applied_budget_w);
    input.buffer_age_s = 0.015f;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(50.025f, out.buffer_lower_j);
    CHECK(out.applied_budget_w > 80);
    input.buffer_age_s = 0.065f;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.050f, &state, &out));
    NEAR(46.775f, out.buffer_lower_j);
    NEAR(80, out.applied_budget_w);
}

static void TimestampWrapWorksAndOlderFramesCannotRestoreBoost(void)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out;
    ChassisPowerBudgetInput_s input = Input(60);
    input.buffer_tick_ms = UINT32_MAX - 9U;
    ChassisPowerBudgetReset(&state, 80);
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    input.buffer_energy_j = 20;
    input.buffer_tick_ms = 5;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(11.4f, out.buffer_lower_j); /* 15 ms known + 85 ms unknown settlement history. */
    input.buffer_energy_j = 60;
    input.buffer_tick_ms = UINT32_MAX - 9U;
    for (unsigned i = 0; i < 2; ++i)
    {
        CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
        NEAR(80, out.target_budget_w);
        CHECK(out.applied_budget_w <= 80);
        CHECK(out.mode == CHASSIS_POWER_BUDGET_DEGRADED);
    }
    input.buffer_tick_ms = 10;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(11.175f, out.buffer_lower_j); /* A new frame cannot restore discarded energy at once. */
}

static void NewFramesCannotHideFeedbackOrDispatchGaps(void)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out;
    ChassisPowerBudgetInput_s input = Input(60);
    input.measured_input_power_w = 40;
    ChassisPowerBudgetReset(&state, 80);
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    input.input_power_valid = false;
    input.buffer_age_s = 0.005f;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    input.input_power_valid = true;
    input.buffer_tick_ms = 10;
    input.buffer_age_s = 0;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(80, out.target_budget_w);
    CHECK(out.mode == CHASSIS_POWER_BUDGET_DEGRADED);
    input.buffer_age_s = 0.005f;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    CHECK(out.applied_budget_w > 80);
    input.buffer_tick_ms = 1000;
    input.buffer_age_s = 0;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(80, out.target_budget_w);
    CHECK(out.mode == CHASSIS_POWER_BUDGET_DEGRADED);
}

static void RepeatedObservationsDoNotAdvanceRecoveryConfirmation(void)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out;
    ChassisPowerBudgetInput_s input = Input(50);
    input.measured_input_power_w = 60;
    ChassisPowerBudgetReset(&state, 80);
    state.recovering = true;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    input = Input(50);
    input.measured_input_power_w = 60;
    input.buffer_tick_ms = 5;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    for (unsigned i = 0; i < 30; ++i)
        CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    CHECK(out.recovering);
    NEAR(80, out.target_budget_w);
    for (unsigned i = 1; i <= 21; ++i)
    {
        input.buffer_age_s = (float)i * 0.005f;
        CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    }
    CHECK(!out.recovering);
}

static void EstimateIsBoundedAndDoesNotInventRegenerativeEnergy(void)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out;
    ChassisPowerBudgetInput_s input = Input(60);
    input.measured_input_power_w = -1000;
    ChassisPowerBudgetReset(&state, 80);
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    input.buffer_age_s = 0.005f;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(50.875f, out.buffer_lower_j); /* Input is clamped to zero, not -1000 W. */
    for (unsigned i = 2; i <= 25; ++i)
    {
        input.buffer_age_s = (float)i * 0.005f;
        CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    }
    NEAR(57, out.buffer_lower_j);
    input = Input(5);
    input.buffer_tick_ms = 200;
    input.measured_input_power_w = 140;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(0, out.buffer_lower_j);
}

static void RecoveryReductionFollowsDeficitAndHonorsTheHardFloor(void)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out;
    ChassisPowerBudgetInput_s input = Input(24.5f); /* Lower bound = 15 J. */
    input.measured_input_power_w = 60;
    ChassisPowerBudgetReset(&state, 80);
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    CHECK(out.recovering);
    NEAR(65, out.target_budget_w);
    for (unsigned k = 1; k < 20; ++k)
    {
        input.buffer_age_s = k * 0.005f;
        CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    }
    input.buffer_energy_j = 26.5f; /* Actual energy gained 2 J at 60 W for 100 ms. */
    input.buffer_tick_ms = 100;
    input.buffer_age_s = 0;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(68.5f, out.target_budget_w);
    CHECK(out.applied_budget_w <= 68.51f);
    input = Input(14.5f); /* Hard floor at 5 J forces the full reduction. */
    input.measured_input_power_w = 60;
    input.buffer_tick_ms = 200;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(60, out.applied_budget_w);
    input = Input(30);
    input.measured_input_power_w = 140;
    input.buffer_tick_ms = 300;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(60, out.target_budget_w); /* Predicted floor also forces full reduction. */
    input.referee_limit_w = 10;
    input.buffer_energy_j = 0;
    input.buffer_tick_ms = 400;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(0, out.applied_budget_w);
}

static void InvalidObserverAndRecoveryConfigFailsSafe(void)
{
    ChassisPowerBudgetConfig_s invalid = config;
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out;
    ChassisPowerBudgetInput_s input = Input(60);
    invalid.buffer.recovery_target_time_s = 0;
    ChassisPowerBudgetReset(&state, 80);
    CHECK(!ChassisPowerBudgetUpdate(&invalid, &input, 0.005f, &state, &out));
    NEAR(0, out.applied_budget_w);
    invalid = config;
    invalid.buffer.observer_max_step_s = NAN;
    CHECK(!ChassisPowerBudgetUpdate(&invalid, &input, 0.005f, &state, &out));
    NEAR(0, out.applied_budget_w);
}

static void InvalidSamplesAndModeChangesReinitializeConservatively(void)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out;
    ChassisPowerBudgetInput_s input = Input(60);
    input.measured_input_power_w = 40;
    ChassisPowerBudgetReset(&state, 80);
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    input.buffer_age_s = 0.010f;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.010f, &state, &out));
    NEAR(50.85f, out.buffer_lower_j);
    input.buffer_age_s = 0.005f; /* Same frame cannot move backwards in time. */
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    CHECK(out.mode == CHASSIS_POWER_BUDGET_DEGRADED);
    input.buffer_age_s = 0.015f;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(49.525f, out.buffer_lower_j);
    input.buffer_data_valid = false;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(0, out.buffer_lower_j);
    input.buffer_data_valid = true;
    input.buffer_tick_ms = 100;
    input.buffer_age_s = 0;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(50.5f, out.buffer_lower_j);
    input.buffer_control_enabled = false;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    input.buffer_control_enabled = true;
    input.buffer_age_s = 0.020f;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(49.2f, out.buffer_lower_j);
}

static void ConfiguredRecoveryTimeChangesThePhysicalTarget(void)
{
    ChassisPowerBudgetConfig_s slower = config;
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out;
    ChassisPowerBudgetInput_s input = Input(24.5f);
    input.measured_input_power_w = 60;
    slower.buffer.recovery_target_time_s = 2;
    ChassisPowerBudgetReset(&state, 80);
    CHECK(ChassisPowerBudgetUpdate(&slower, &input, 0.005f, &state, &out));
    NEAR(72.5f, out.target_budget_w);
    NEAR(7.5f, out.recovery_reduction_w);
}

static void CapPlantCanRecoverBufferWhileSupplyingHigherLoad(void)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out;
    ChassisPowerBudgetInput_s input = Input(20);
    input.use_supercap = true;
    input.cap_online = input.cap_output_enabled = true;
    input.cap_input_control_ready = input.load_power_valid = true;
    ChassisPowerBudgetReset(&state, 80);
    float buffer_energy = 20, cap_energy = 120;
    float input_power = 60, load_power = 60, input_target = 60;
    float sampled_buffer = 20, minimum_buffer = 20, peak_load = 0;
    bool assisted_recovery = false;
    for (int tick = 0; tick < 1200; ++tick)
    {
        if (tick % 20 == 0) sampled_buffer = floorf(buffer_energy);
        input.buffer_energy_j = sampled_buffer;
        input.buffer_tick_ms = (uint32_t)(tick / 20) * 100U;
        input.buffer_age_s = (float)(tick % 20) * 0.005f;
        input.measured_input_power_w = input_power;
        input.cap_energy_ratio = cap_energy / 200;
        input.cap_reported_limit_w = input_target + 80;
        input.confirmed_input_target_w = floorf(input_target);
        const float demand = tick < 200 ? 140 : 30;
        input.requested_load_power_w = demand;
        CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
        CHECK(out.buffer_lower_j <= buffer_energy + 0.01f);
        CHECK(out.applied_budget_w * 0.98f <= input.cap_reported_limit_w + 0.01f);
        input_target = out.input_target_w;
        input_power += 0.25f * (input_target - input_power);
        const float available = input_power + fminf(80, cap_energy / 0.005f);
        const float load_target = fminf(available, fminf(demand, out.applied_budget_w * 0.98f));
        load_power += 0.25f * (load_target - load_power);
        /* Independent lossless plant: separate input, load and two energy stores. */
        const float charged_input = fminf(input_power, load_power + (200 - cap_energy) / 0.005f);
        cap_energy += (charged_input - load_power) * 0.005f;
        buffer_energy = fminf(60, buffer_energy + (80 - charged_input) * 0.005f);
        minimum_buffer = fminf(minimum_buffer, buffer_energy);
        peak_load = fmaxf(peak_load, load_power);
        assisted_recovery |= out.recovering && charged_input < 80 && load_power > 90;
        CHECK(cap_energy >= 0 && cap_energy <= 200.01f);
    }
    CHECK(assisted_recovery && peak_load > 100);
    CHECK(minimum_buffer > 5 && buffer_energy > 59);
}

static void LowDemandPreservesBufferAndStaleDemandRemovesBoost(void)
{
    ChassisPowerBudgetInput_s input = Input(60);
    input.requested_load_power_w = 30;
    NEAR(78.4f, Update(input).input_target_w);
    input.requested_load_power_w = 90;
    NEAR(95, Update(input).input_target_w); /* Physical demand plus 5 W. */
    input.requested_load_power_w = 180;
    CHECK(Update(input).input_target_w > 130);
    input.demand_valid = false;
    NEAR(78.4f, Update(input).input_target_w);
    input.demand_valid = true;
    input.requested_load_power_w = NAN;
    NEAR(78.4f, Update(input).input_target_w);
}

static void InputAndCapLoadHaveIndependentRiseRates(void)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out;
    ChassisPowerBudgetInput_s input = Input(60);
    input.use_supercap = true;
    input.cap_online = input.cap_output_enabled = true;
    input.cap_input_control_ready = input.load_power_valid = true;
    input.cap_energy_ratio = 0.5f;
    input.cap_reported_limit_w = 180;
    input.confirmed_input_target_w = 78.4f;
    ChassisPowerBudgetReset(&state, 80);
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(81.34f, out.input_target_w);
    NEAR(81, out.applied_budget_w);
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(84.28f, out.input_target_w);
    NEAR(82, out.applied_budget_w);
    input.cap_error_code = 1;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    CHECK(out.applied_budget_w <= 80);
}

static void LoadUsesConfirmedInputAndLongDispatchCannotRaiseBudget(void)
{
    ChassisPowerBudgetInput_s input = Input(60);
    input.use_supercap = true;
    input.cap_online = input.cap_output_enabled = true;
    input.cap_input_control_ready = input.load_power_valid = true;
    input.cap_energy_ratio = 0.1f; /* No capacitor assist. */
    input.cap_reported_limit_w = 180;
    input.confirmed_input_target_w = 78.4f;
    ChassisPowerBudgetOutput_s out = Update(input);
    CHECK(out.input_target_w > 130);
    NEAR(80, out.applied_budget_w);
    input.confirmed_input_target_w = 137.2f;
    NEAR(140, Update(input).applied_budget_w);
    input.confirmed_input_target_w = NAN;
    NEAR(80, Update(input).applied_budget_w);
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetReset(&state, 80);
    input.use_supercap = false;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.2f, &state, &out));
    NEAR(80, out.applied_budget_w);
    NEAR(78.4f, out.input_target_w);
}

static void ContinuousHistoryShrinksSettlementUncertainty(void)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out;
    ChassisPowerBudgetInput_s input = Input(60);
    ChassisPowerBudgetReset(&state, 80);
    for (unsigned k = 0; k <= 80; ++k)
    {
        input.buffer_tick_ms = k / 20 * 100;
        input.buffer_age_s = (k % 20) * 0.005f;
        CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
        CHECK(out.buffer_lower_j <= 60);
        if (k == 0) NEAR(50.5f, out.buffer_lower_j);
    }
    NEAR(56.5f, out.buffer_lower_j); /* 3 J fixed + 5 W * 0.1 s. */
    const float previous = out.buffer_lower_j;
    for (unsigned k = 0; k < 30; ++k)
        CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(previous, out.buffer_lower_j);
}

static void UpwardCorrectionIsBoundedAndDownwardCorrectionIsImmediate(void)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out;
    ChassisPowerBudgetInput_s input = Input(45);
    ChassisPowerBudgetReset(&state, 80);
    for (unsigned k = 0; k < 20; ++k)
    {
        input.buffer_age_s = k * 0.005f;
        CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    }
    input.buffer_tick_ms = 100;
    input.buffer_age_s = 0;
    input.buffer_energy_j = 60;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(37, out.buffer_lower_j); /* 35.5 - 0.5 + 20 J/s * 0.1 s. */
    input.buffer_tick_ms = 105;
    input.buffer_energy_j = 10;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    CHECK(out.buffer_lower_j < 8 && out.recovering);
}

static void RecentPowerSpikeIsChargedEvenAfterPowerFalls(void)
{
    ChassisPowerBudgetConfig_s fast_correction = config;
    fast_correction.buffer.correction_rise_j_per_s = 1000;
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out;
    ChassisPowerBudgetInput_s input = Input(60);
    ChassisPowerBudgetReset(&state, 80);
    for (unsigned k = 0; k <= 40; ++k)
    {
        input.buffer_tick_ms = k / 20 * 100;
        input.buffer_age_s = (k % 20) * 0.005f;
        input.measured_input_power_w = k == 35 || k == 36 ? 200 : 80;
        CHECK(ChassisPowerBudgetUpdate(&fast_correction, &input, 0.005f, &state, &out));
    }
    NEAR(54.7f, out.buffer_lower_j); /* Three conservative 5 ms spike intervals. */
    input.input_power_valid = false;
    input.buffer_age_s = 0.005f;
    CHECK(ChassisPowerBudgetUpdate(&fast_correction, &input, 0.005f, &state, &out));
    CHECK(out.mode == CHASSIS_POWER_BUDGET_DEGRADED);
    NEAR(54.375f, out.buffer_lower_j);
}

static void MeterGapCannotBypassUpwardCorrectionLimit(void)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out;
    ChassisPowerBudgetInput_s input = Input(45);
    ChassisPowerBudgetReset(&state, 80);
    for (unsigned k = 0; k < 20; ++k)
    {
        input.buffer_age_s = k * 0.005f;
        CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    }
    input.buffer_tick_ms = 100;
    input.buffer_age_s = 0;
    input.buffer_energy_j = 60;
    input.input_power_valid = false;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    NEAR(36.7f, out.buffer_lower_j);
    NEAR(80, out.applied_budget_w);
    input.input_power_valid = true;
    input.buffer_age_s = 0.005f;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    input.buffer_age_s = 0.010f;
    CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
    CHECK(out.buffer_lower_j < 37);
}

static void LowLoadChargesCapWithoutSpendingBuffer(void)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s out;
    ChassisPowerBudgetInput_s input = Input(60);
    input.use_supercap = true;
    input.cap_online = input.cap_output_enabled = true;
    input.cap_input_control_ready = input.load_power_valid = true;
    input.requested_load_power_w = 30;
    ChassisPowerBudgetReset(&state, 80);
    float cap_energy = 20, buffer_energy = 60, input_power = 80, input_target = 78;
    for (unsigned k = 0; k < 2000; ++k)
    {
        input.buffer_energy_j = floorf(buffer_energy);
        input.buffer_tick_ms = k / 20 * 100;
        input.buffer_age_s = (k % 20) * 0.005f;
        input.measured_input_power_w = input_power;
        input.cap_energy_ratio = cap_energy / 400;
        input.cap_reported_limit_w = input_target + 80;
        input.confirmed_input_target_w = input_target;
        CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
        CHECK(out.input_target_w <= 78.401f);
        input_target = floorf(out.input_target_w);
        input_power += 0.25f * (input_target - input_power);
        /* Independent plant with 85% charging efficiency and 30 W load. */
        const float charged_input = fminf(input_power,
            30 + (400 - cap_energy) / (0.85f * 0.005f));
        cap_energy += fmaxf(0, charged_input - 30) * 0.85f * 0.005f;
        buffer_energy = fminf(60, buffer_energy + (80 - charged_input) * 0.005f);
        CHECK(buffer_energy >= 59.99f);
        CHECK(out.buffer_lower_j <= buffer_energy + 0.01f);
    }
    CHECK(cap_energy > 399 && cap_energy <= 400.01f);
}

int main(void)
{
    ChassisPowerBudgetConfig_s invalid_tuning = config;
    invalid_tuning.buffer.recovery_power_w = 0;
    CHECK(ChassisPowerBudgetConfigIsValid(&invalid_tuning));
    CHECK(!ChassisBufferBudgetConfigIsValid(&invalid_tuning.buffer));
    invalid_tuning = config;
    invalid_tuning.buffer.recovery_target_time_s = 0;
    CHECK(!ChassisBufferBudgetConfigIsValid(&invalid_tuning.buffer));
    CHECK(ChassisBufferBudgetConfigIsValid(&config.buffer));
    MeterGapCannotBypassUpwardCorrectionLimit();
    LowLoadChargesCapWithoutSpendingBuffer();
    ContinuousHistoryShrinksSettlementUncertainty();
    UpwardCorrectionIsBoundedAndDownwardCorrectionIsImmediate();
    RecentPowerSpikeIsChargedEvenAfterPowerFalls();
    LowDemandPreservesBufferAndStaleDemandRemovesBoost();
    InputAndCapLoadHaveIndependentRiseRates();
    LoadUsesConfirmedInputAndLongDispatchCannotRaiseBudget();
    HighBufferWorksWithoutCap();
    LowBufferActuallyReducesBudget();
    MissingFeedbackRemovesBoost();
    CapBudgetCountsBoardTotalOnlyOnce();
    PredictionInterruptsHighConsumption();
    RecoveryHasHysteresisAndImmediateDownwardLimit();
    HealthyCapCanAssistWhileBufferRecovers();
    OldFramesAndInvalidNumbersCannotProvideFullBoost();
    BufferSpendingAndRecoveryWithDelayedPowerResponse();
    CapRequiresLoadFeedbackAndAlwaysHonorsBoardCapacity();
    ContinuousEstimateUsesConservativeMeasuredIntervals();
    NewSamplesReanchorEvenWhenTheirEnergyIsUnchanged();
    FeedbackGapsSpendWorstCaseAndRequireContinuousRecovery();
    TimestampWrapWorksAndOlderFramesCannotRestoreBoost();
    NewFramesCannotHideFeedbackOrDispatchGaps();
    RepeatedObservationsDoNotAdvanceRecoveryConfirmation();
    EstimateIsBoundedAndDoesNotInventRegenerativeEnergy();
    RecoveryReductionFollowsDeficitAndHonorsTheHardFloor();
    InvalidObserverAndRecoveryConfigFailsSafe();
    InvalidSamplesAndModeChangesReinitializeConservatively();
    ConfiguredRecoveryTimeChangesThePhysicalTarget();
    CapPlantCanRecoverBufferWhileSupplyingHigherLoad();
    if (failures) return 1;
    puts("PASS: dynamic buffer budget tests");
    return 0;
}
