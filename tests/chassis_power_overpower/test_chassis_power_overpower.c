#include "chassis_power_outer_loop.h"
#include "chassis_power_transient.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static ChassisPowerOuterLoopConfig_s slow_config;
static ChassisPowerOuterLoopState_s slow_state;
static ChassisPowerOuterLoopOutput_s slow_output;
static ChassisPowerTransientConfig_s fast_config;
static ChassisPowerTransientState_s fast_state;
static ChassisPowerTransientInput_s fast_input;
static ChassisPowerTransientOutput_s fast_output;

static void Prepare(void)
{
    slow_config = (ChassisPowerOuterLoopConfig_s){
        .kp = 1, .ki = 1.5f, .measurement_lpf_tau_s = .030f,
        .activation_margin_w = 5, .error_deadband_w = .3f,
        .max_positive_correction_w = 8, .positive_slew_rate_w_per_s = 60,
        .budget_drop_reset_w = 1, .antiwindup_gain_per_s = 10,
        .low_demand_delay_s = .5f, .low_demand_decay_w_per_s = 1,
    };
    fast_config = ChassisPowerTransientDefaultConfig();
    fast_config.eta = .98f;
    ChassisPowerOuterLoopReset(&slow_state);
    ChassisPowerTransientReset(&fast_state);
    slow_output = (ChassisPowerOuterLoopOutput_s){0};
    fast_output = (ChassisPowerTransientOutput_s){0};
    fast_input = (ChassisPowerTransientInput_s){
        .budget_w = 50, .target_w = 49, .meter_w = 55,
        .power_deadband_w = slow_config.error_deadband_w,
        .meter_enabled = true, .meter_online = true, .control_enabled = true,
        .online_mask = 15, .enabled_mask = 15, .startup_enabled = true,
    };
    for (unsigned w = 0; w < 4; ++w) {
        fast_input.raw_current[w] = 1500;
        fast_input.actual_rpm[w] = 2000;
        fast_input.target_rpm[w] = 2500;
    }
}

static void FastTick(uint32_t tick, bool new_meter, bool new_feedback, float dt)
{
    fast_input.now_ms = tick;
    fast_input.slow_w = slow_output.correction_w;
    if (new_meter) fast_input.meter_tick_ms = tick;
    if (new_feedback) {
        for (unsigned w = 0; w < 4; ++w) {
            fast_input.feedback_tick_ms[w] = tick;
            ++fast_input.feedback_sequence[w];
        }
    }
    assert(ChassisPowerTransientUpdate(&fast_config, &fast_input, dt,
                                     &fast_state, &fast_output));
}

static void SlowTick(bool base_limited, bool demand_sufficient)
{
    const ChassisPowerOuterLoopInput_s input = {
        .nominal_budget_w = fast_input.budget_w, .target_power_w = fast_input.target_w,
        .measured_power_w = fast_input.meter_w,
        .meter_online = fast_output.meter_fresh,
        .control_enabled = fast_input.control_enabled,
        .complete_slow_control = true,
        .allow_positive_correction = fast_output.positive_allowed &&
            fast_input.meter_w <= fast_input.budget_w,
        .learning_allowed = fast_output.learning_allowed &&
            fast_input.meter_w <= fast_input.budget_w,
        .negative_learning_allowed = fast_output.negative_learning_allowed &&
            fast_output.negative_learning_threshold_w >= fast_input.target_w + slow_config.error_deadband_w,
        .base_limited = base_limited, .demand_sufficient = demand_sufficient,
        .braking = fast_output.brake_mask != 0,
        .meter_tick_ms = fast_input.meter_tick_ms,
    };
    assert(ChassisPowerOuterLoopUpdateIsolated(&slow_config, &input, .005f,
                                            &slow_state, &slow_output));
}

static void TestPersistentOverpowerConverges(void)
{
    Prepare();
    float measured = 57;
    for (uint32_t t = 0; t <= 5000; ++t) {
        if (t % 5 == 0) fast_input.meter_w = measured;
        FastTick(t, t % 5 == 0, true, .001f);
        if (t % 5 == 0) SlowTick(true, true);
        /* A controllable response with 8 W model bias, not an actual motor model. */
        const float power = .98f * fast_output.model_budget_w + 8;
        measured += .001f / .030f * (power - measured);
    }
    printf("Persistent bias: measured=%.3f W, slow=%.3f W, fast=%.3f W\n",
           measured, slow_output.correction_w, fast_output.fast_w);
    assert(fabsf(measured - 49) <= .5f);
    assert(slow_output.correction_w < -1);
}

static void TestMeasuredOverpowerBypassesModelDemandGates(void)
{
    Prepare();
    for (uint32_t t = 0; t <= 2000; ++t) {
        FastTick(t, true, true, .001f);
        if (t % 5 == 0) SlowTick(false, false);
    }
    assert(slow_output.correction_w < -10);
    assert(slow_state.integral_w < -5);
    assert(slow_state.low_demand_s == 0);
    assert(slow_output.mode == CHASSIS_POWER_OUTER_ACTIVE);

    /* The target margin also needs correction before the hard budget is crossed. */
    Prepare(); fast_input.meter_w = 49.8f;
    for (uint32_t t = 0; t <= 1000; ++t) {
        FastTick(t, true, true, .001f);
        if (t % 5 == 0) SlowTick(false, false);
    }
    assert(slow_output.correction_w < -.5f);
    assert(fast_output.fast_w == 0);
}

static void TestSpikeKeepsSlowHistoryFrozen(void)
{
    Prepare();
    slow_state.integral_w = slow_state.correction_w = slow_state.applied_correction_w = -2;
    slow_output.correction_w = -2;
    for (uint32_t t = 0; t <= 300; ++t) {
        fast_input.meter_w = t >= 20 && t < 40 ? 70 : 49;
        FastTick(t, true, true, .001f);
        if (t % 5 == 0) SlowTick(false, false);
        assert(slow_state.integral_w == -2);
        assert(slow_state.correction_w == -2);
    }
}

static void TestPositiveHistoryCannotIncreaseOverpowerBudget(void)
{
    Prepare();
    slow_state.integral_w = slow_state.correction_w = slow_state.applied_correction_w = 8;
    slow_output.correction_w = 8;
    for (uint32_t t = 0; t <= 200; ++t) {
        FastTick(t, true, true, .001f);
        if (t % 5 == 0) SlowTick(false, false);
    }
    assert(slow_output.correction_w < 0);
    assert(slow_state.integral_w < 0);
}

static void TestDeadbandDoesNotPrearmSpikeLearning(void)
{
    Prepare();
    for (uint32_t t = 0; t <= 300; ++t) {
        fast_input.meter_w = t >= 200 && t < 220 ? 70 : 49.2f;
        FastTick(t, true, true, .001f);
        if (t % 5 == 0) SlowTick(false, false);
    }
    assert(slow_state.integral_w == 0);
    assert(slow_state.correction_w == 0);
}

static void TestRecoveredRawPowerDoesNotIntegrateFilterTail(void)
{
    Prepare();
    for (uint32_t t = 0; t <= 200; ++t) {
        FastTick(t, true, true, .001f);
        if (t % 5 == 0) SlowTick(false, false);
    }
    assert(slow_state.integral_w < 0);
    const float integral = slow_state.integral_w;
    const float correction = slow_state.correction_w;
    fast_input.meter_w = 48;
    FastTick(201, true, true, .001f);
    SlowTick(false, false);
    assert(slow_state.filtered_power_w > 49.3f);
    assert(slow_state.integral_w == integral);
    assert(slow_state.correction_w == correction);
}

static void TestUnsafeFeedbackKeepsNegativeLearningFrozen(void)
{
    for (unsigned scenario = 0; scenario < 6; ++scenario) {
        Prepare();
        if (scenario == 0) fast_input.raw_current[0] = -1500; /* braking */
        if (scenario == 1)
            for (unsigned w = 0; w < 4; ++w) fast_input.raw_current[w] = 0; /* coasting */
        if (scenario == 5) fast_input.control_enabled = false;
        for (uint32_t t = 0; t <= 300; ++t) {
            FastTick(t, scenario != 2 || t == 0, scenario != 3 || t == 0,
                     scenario == 4 ? .006f : .001f);
            if (t % 5 == 0) SlowTick(false, false);
            assert(slow_state.integral_w == 0);
            assert(slow_state.correction_w == 0);
        }
    }
}

static void TestStartupToleranceKeepsSlowLoopFrozen(void)
{
    Prepare(); fast_input.meter_w = 10;
    for (unsigned w = 0; w < 4; ++w)
        fast_input.raw_current[w] = fast_input.actual_rpm[w] = fast_input.target_rpm[w] = 0;
    for (uint32_t t = 0; t <= 100; ++t) {
        FastTick(t, true, true, .001f);
        if (t % 5 == 0) SlowTick(false, false);
    }
    for (unsigned w = 0; w < 4; ++w) {
        fast_input.raw_current[w] = 5000;
        fast_input.target_rpm[w] = 1000;
    }
    FastTick(101, true, true, .001f);
    assert(fast_output.startup_triggered);
    fast_input.meter_w = 60;
    for (uint32_t t = 102; t < 131; ++t) {
        FastTick(t, true, true, .001f);
        if (t % 5 == 0) SlowTick(false, false);
        assert(fast_output.startup_active);
        assert(slow_state.integral_w == 0);
    }
}

static void TestInterruptedConfirmationRestartsDelay(void)
{
    Prepare();
    for (uint32_t t = 0; t <= 95; ++t) FastTick(t, true, true, .001f);
    fast_input.meter_w = 48;
    FastTick(96, true, true, .001f);
    fast_input.meter_w = 55;
    for (uint32_t t = 97; t <= 196; ++t) {
        FastTick(t, true, true, .001f);
        if (t % 5 == 0) SlowTick(false, false);
        assert(slow_state.integral_w == 0);
    }
    FastTick(197, true, true, .001f);
    SlowTick(false, false);
    assert(slow_state.integral_w < 0);
}

static void TestConfirmationSurvivesTickWrap(void)
{
    Prepare();
    for (uint32_t t = 0; t <= 200; ++t) {
        FastTick(UINT32_MAX - 50 + t, true, true, .001f);
        if (t % 5 == 0) SlowTick(false, false);
    }
    assert(slow_output.correction_w < 0);
}

static void TestRaisedThresholdRestartsConfirmation(void)
{
    Prepare();
    for (uint32_t t = 0; t <= 95; ++t) FastTick(t, true, true, .001f);
    fast_input.budget_w = 55; fast_input.target_w = 53.9f;
    for (uint32_t t = 96; t < 196; ++t) {
        FastTick(t, true, true, .001f);
        if (t % 5 == 0) SlowTick(false, false);
        assert(slow_state.integral_w == 0);
    }
    FastTick(196, true, true, .001f);
    SlowTick(false, false);
    assert(slow_state.integral_w < 0);
}

static void TestFallingBudgetRetainsOverpowerConfirmation(void)
{
    Prepare();
    for (uint32_t t = 0; t <= 2000; ++t) {
        fast_input.budget_w = 50 - .005f * (float)(t / 10);
        fast_input.target_w = .98f * fast_input.budget_w;
        FastTick(t, true, true, .001f);
        if (t % 5 == 0) SlowTick(false, false);
    }
    assert(slow_state.integral_w < -5);
    assert(slow_output.correction_w < -10);
}

static void TestSpikeAfterConfirmationDoesNotEnterSlowHistory(void)
{
    float baseline_integral = 0;
    for (unsigned spike = 0; spike < 2; ++spike) {
        Prepare();
        for (uint32_t t = 0; t <= 500; ++t) {
            fast_input.meter_w = spike && t >= 200 && t < 220 ? 70 : 49.4f;
            FastTick(t, true, true, .001f);
            if (t % 5 == 0) SlowTick(false, false);
        }
        if (!spike) baseline_integral = slow_state.integral_w;
        else assert(fabsf(slow_state.integral_w - baseline_integral) < .1f);
    }
}

static void TestRaisedChassisTargetRejectsOldMotorConfirmation(void)
{
    Prepare();
    FastTick(0, true, true, .001f); /* Establish an earlier fast clamp. */
    fast_input.meter_w = 49.4f;
    for (uint32_t t = 1; t <= 195; ++t) {
        FastTick(t, true, true, .001f);
        if (t % 5 == 0) SlowTick(false, false);
    }
    fast_input.meter_w = 51; /* Fresh old-target motor snapshot. */
    FastTick(196, true, true, .001f);
    const float integral = slow_state.integral_w;
    fast_input.budget_w = 50.5f; fast_input.target_w = 49.49f;
    /* The chassis updates its target before another motor cycle runs. */
    SlowTick(false, false);
    assert(slow_state.integral_w == integral);
}

static void TestNewSmallClampFreezesConfirmedSlowLoop(void)
{
    Prepare(); fast_input.meter_w = 49.4f;
    for (uint32_t t = 0; t <= 195; ++t) {
        FastTick(t, true, true, .001f);
        if (t % 5 == 0) SlowTick(false, false);
    }
    const float integral = slow_state.integral_w;
    fast_input.meter_w = 51;
    for (uint32_t t = 196; t < 216; ++t) {
        FastTick(t, true, true, .001f);
        if (t % 5 == 0) SlowTick(false, false);
        assert(slow_state.integral_w == integral);
    }
}

static void TestRampSpikeOnExistingClampFreezesSlowLoop(void)
{
    Prepare();
    float integral = 0;
    for (uint32_t t = 0; t <= 500; ++t) {
        if (t == 200) integral = slow_state.integral_w;
        fast_input.meter_w = t >= 200 && t < 220 ? 55 + 4 * (float)(1 + (t - 200) / 5) : 55;
        FastTick(t, true, true, .001f);
        if (t % 5 == 0) SlowTick(false, false);
        if (t >= 200 && t < 220) assert(slow_state.integral_w == integral);
    }
    assert(slow_state.integral_w < integral); /* Sustained error resumes learning. */
}

int main(void)
{
    TestPersistentOverpowerConverges();
    TestMeasuredOverpowerBypassesModelDemandGates();
    TestSpikeKeepsSlowHistoryFrozen();
    TestPositiveHistoryCannotIncreaseOverpowerBudget();
    TestDeadbandDoesNotPrearmSpikeLearning();
    TestRecoveredRawPowerDoesNotIntegrateFilterTail();
    TestUnsafeFeedbackKeepsNegativeLearningFrozen();
    TestStartupToleranceKeepsSlowLoopFrozen();
    TestInterruptedConfirmationRestartsDelay();
    TestConfirmationSurvivesTickWrap();
    TestRaisedThresholdRestartsConfirmation();
    TestFallingBudgetRetainsOverpowerConfirmation();
    TestSpikeAfterConfirmationDoesNotEnterSlowHistory();
    TestRaisedChassisTargetRejectsOldMotorConfirmation();
    TestNewSmallClampFreezesConfirmedSlowLoop();
    TestRampSpikeOnExistingClampFreezesSlowLoop();
    puts("PASS: persistent chassis overpower tests");
    return 0;
}
