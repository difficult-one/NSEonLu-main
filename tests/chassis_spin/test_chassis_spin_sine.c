#include "chassis_spin.h"

#include <math.h>
#include <stdio.h>

static int failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)
#define NEAR(want, got) do { const float actual_ = (got); \
    CHECK(isfinite(actual_) && fabsf((want) - actual_) < 0.5f); } while (0)

static const ChassisSpinConfig_s config = {
    .fixed_wz = 4000, .min_wz = 1000, .max_wz = 6000,
    .gain_wz_per_w_s = 40, .rise_wz_per_s = 400, .fall_wz_per_s = 2000,
    .power_lpf_tau_s = .1f, .power_deadband_w = 1,
    .settle_time_s = .2f, .budget_drop_reset_w = 1,
    .sine_amplitude_wz = 1000, .sine_period_s = 4, .sine_ramp_s = 1,
    .sine_rise_wz_per_s = 2000, .sine_fall_wz_per_s = 2000,
    .sine_direction = 1,
};

static ChassisSpinInput_s Ready(void)
{
    return (ChassisSpinInput_s){.spin_enabled = true, .control_enabled = true,
        .feedback_valid = true, .increase_allowed = true,
        .target_power_w = 80, .measured_power_w = 20};
}

static float Run(const ChassisSpinConfig_s *c, ChassisSpinState_s *s,
                 ChassisSpinInput_s input, unsigned cycles)
{
    float target = 0;
    for (unsigned i = 0; i < cycles; ++i)
        CHECK(ChassisSpinUpdate(c, &input, .005f, s, &target));
    return target;
}

static void FullWaveKeepsDirectionAndHasTheRequestedPeriod(void)
{
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = Ready();
    NEAR(4000, Run(&config, &state, input, 1600)); /* 8 s, fully settled. */
    NEAR(5000, Run(&config, &state, input, 200));
    NEAR(4000, Run(&config, &state, input, 200));
    NEAR(3000, Run(&config, &state, input, 200));
    NEAR(4000, Run(&config, &state, input, 200));
    float minimum = 6000, maximum = 0;
    for (unsigned i = 0; i < 800; ++i)
    {
        const float target = Run(&config, &state, input, 1);
        CHECK(target > 0);
        minimum = fminf(minimum, target);
        maximum = fmaxf(maximum, target);
    }
    NEAR(3000, minimum);
    NEAR(5000, maximum);
}

static void StartupWaitsForGuardsAndNormalStepsAreBounded(void)
{
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = Ready();
    input.increase_allowed = false;
    NEAR(1000, Run(&config, &state, input, 400));
    input.increase_allowed = true;
    NEAR(1000, Run(&config, &state, input, 20));
    float previous = 1000;
    for (unsigned i = 0; i < 2000; ++i)
    {
        const float target = Run(&config, &state, input, 1);
        CHECK(target >= 1000 && target <= 5000.1f);
        CHECK(fabsf(target - previous) <= 10.01f);
        previous = target;
    }
}

static void NegativeDirectionIsLockedUntilModeExit(void)
{
    ChassisSpinConfig_s negative = config;
    negative.sine_direction = -1;
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = Ready();
    CHECK(Run(&negative, &state, input, 1800) < -4900);
    negative.sine_direction = 1; /* A live config change cannot reverse a running spin. */
    CHECK(Run(&negative, &state, input, 800) < 0);
    input.spin_enabled = false;
    NEAR(0, Run(&negative, &state, input, 1));
    input.spin_enabled = true;
    NEAR(1000, Run(&negative, &state, input, 1));
}

static void RecoveryAndLimitingRemoveTheHighSpeedWave(void)
{
    const bool recovery_flags[] = {false, true};
    for (unsigned k = 0; k < 2; ++k)
    {
        ChassisSpinState_s state = {0};
        ChassisSpinInput_s input = Ready();
        const float peak = Run(&config, &state, input, 1800);
        CHECK(peak > 4900);
        input.energy_recovering = recovery_flags[k];
        input.power_limited = !recovery_flags[k];
        input.increase_allowed = false;
        input.target_power_w = 40; /* The budget may also fall during recovery. */
        const float initial = Run(&config, &state, input, 1);
        CHECK(initial >= peak - 10.01f); /* Protection cannot bypass target slew. */
        float target = initial;
        for (unsigned i = 0; i < 800; ++i)
        {
            const float next = Run(&config, &state, input, 1);
            CHECK(next <= target + .01f);
            CHECK(target - next <= 10.01f);
            CHECK(next > 0);
            target = next;
        }
        CHECK(target <= 4000 && target < initial);
        input.energy_recovering = input.power_limited = false;
        input.increase_allowed = true;
        const float first = Run(&config, &state, input, 1);
        CHECK(first <= target + 10.01f);
    }
}

static void OverpowerReducesTheTargetWithoutBypassingSlew(void)
{
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = Ready();
    const float peak = Run(&config, &state, input, 1800);
    input.target_power_w = 40;
    input.measured_power_w = 100;
    input.increase_allowed = false;
    const float dropped = Run(&config, &state, input, 1);
    CHECK(dropped <= peak && dropped >= peak - 10.01f);
    CHECK(Run(&config, &state, input, 400) < dropped);
}

static void ABlockedWaveDoesNotAccumulateADeferredPeak(void)
{
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = Ready();
    Run(&config, &state, input, 1600);
    input.increase_allowed = false;
    const float held = Run(&config, &state, input, 200);
    CHECK(held <= 4000.5f); /* At 9 s the requested peak was not accumulated. */
    input.increase_allowed = true;
    const float next = Run(&config, &state, input, 1);
    CHECK(next <= held + 10.01f);
}

static void LossAndDisableResetWaveWithoutChangingItsDirection(void)
{
    ChassisSpinConfig_s negative = config;
    negative.sine_direction = -1;
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = Ready();
    CHECK(Run(&negative, &state, input, 1800) < -4900);
    input.feedback_valid = false;
    negative.sine_direction = 1;
    NEAR(-1000, Run(&negative, &state, input, 1));
    input.feedback_valid = true;
    input.increase_allowed = false;
    NEAR(-1000, Run(&negative, &state, input, 100));
    input.control_enabled = false;
    NEAR(0, Run(&negative, &state, input, 1));
    input.control_enabled = true;
    NEAR(1000, Run(&negative, &state, input, 1));
}

static void InvalidInputsCannotLeaveAnOldWaveRunning(void)
{
    const float bad_dt[] = {0, -.005f, .021f, NAN, INFINITY};
    for (unsigned i = 0; i < sizeof(bad_dt) / sizeof(bad_dt[0]); ++i)
    {
        ChassisSpinState_s state = {0};
        ChassisSpinInput_s input = Ready();
        Run(&config, &state, input, 1800);
        float target = 9999;
        CHECK(!ChassisSpinUpdate(&config, &input, bad_dt[i], &state, &target));
        NEAR(0, target);
        NEAR(1000, Run(&config, &state, input, 1));
    }
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = Ready();
    float target;
    input.measured_power_w = NAN;
    CHECK(!ChassisSpinUpdate(&config, &input, .005f, &state, &target));
    NEAR(0, target);
    input = Ready();
    Run(&config, &state, input, 100);
    state.sine_phase_s = NAN;
    CHECK(!ChassisSpinUpdate(&config, &input, .005f, &state, &target));
    NEAR(0, target);
}

static void InvalidSineConfigurationsAreRejected(void)
{
    for (unsigned i = 0; i < 5; ++i)
    {
        ChassisSpinConfig_s invalid = config;
        if (i == 0) invalid.sine_period_s = 0;
        if (i == 1) invalid.sine_amplitude_wz = 4000;
        if (i == 2) invalid.sine_ramp_s = NAN;
        if (i == 3) invalid.sine_direction = 0;
        if (i == 4) invalid.sine_fall_wz_per_s = -1;
        ChassisSpinState_s state = {0};
        ChassisSpinInput_s input = Ready();
        float target = 9999;
        CHECK(!ChassisSpinUpdate(&invalid, &input, .005f, &state, &target));
        NEAR(0, target);
    }
}

static void DelayedLoadTracksTheWaveAndReducesSpeedWhenBudgetFalls(void)
{
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = Ready();
    float target = 1000, actual = 1000;
    float full_peak = 0, full_trough = 6000, reduced_peak = 0, final_power_sum = 0;
    for (unsigned i = 0; i < 4000; ++i)
    {
        /* Independent aggregate load with 50 ms velocity response; not motor calibration. */
        input.measured_power_w = 15 + .000002f * actual * actual;
        input.target_power_w = i < 2400 ? 100 : 40;
        input.power_limited = input.measured_power_w > input.target_power_w;
        input.increase_allowed = !input.power_limited && fabsf(target - actual) < 500;
        target = Run(&config, &state, input, 1);
        actual += .1f * (target - actual);
        CHECK(actual > 0 && target > 0);
        if (i >= 800 && i < 2400)
        {
            full_peak = fmaxf(full_peak, actual);
            full_trough = fminf(full_trough, actual);
        }
        if (i >= 3600)
        {
            reduced_peak = fmaxf(reduced_peak, actual);
            final_power_sum += 15 + .000002f * actual * actual;
        }
    }
    CHECK(full_peak > 4900 && full_trough < 3100);
    CHECK(reduced_peak < full_peak - 800);
    CHECK(final_power_sum / 400 < 45);
}

static void LowDemandQualificationRetainsTransientProtections(void)
{
    const ChassisPowerTransientOutput_s healthy = {.mode = CHASSIS_POWER_TRANSIENT_RUN,
        .meter_fresh = true, .feedback_fresh = true, .timing_valid = true};
    CHECK(ChassisSpinTransientAllowsIncrease(&healthy));
    ChassisPowerTransientOutput_s transient = healthy;
    transient.mode = CHASSIS_POWER_TRANSIENT_IDLE;
    CHECK(ChassisSpinTransientAllowsIncrease(&transient));
    CHECK(!ChassisSpinTransientAllowsIncrease(NULL));
    for (unsigned i = 0; i < 10; ++i)
    {
        transient = healthy;
        if (i == 0) transient.brake_mask = 1;
        if (i == 1) transient.meter_fresh = false;
        if (i == 2) transient.feedback_fresh = false;
        if (i == 3) transient.timing_valid = false;
        if (i == 4) transient.fault = true;
        if (i == 5) transient.mode = CHASSIS_POWER_TRANSIENT_STARTUP;
        if (i == 6) transient.mode = CHASSIS_POWER_TRANSIENT_CLAMP;
        if (i == 7) transient.mode = CHASSIS_POWER_TRANSIENT_RECOVERY;
        if (i == 8) transient.mode = CHASSIS_POWER_TRANSIENT_OFFLINE;
        if (i == 9) transient.drive_mask = 1; /* Drive permission still required when driving. */
        CHECK(!ChassisSpinTransientAllowsIncrease(&transient));
    }
    transient = healthy;
    transient.drive_mask = 15;
    transient.positive_allowed = true;
    CHECK(ChassisSpinTransientAllowsIncrease(&transient));
}

static void RealTransientGateStartsAndResumesTheLowDragWave(void)
{
    ChassisSpinState_s spin = {0};
    ChassisSpinInput_s input = Ready();
    ChassisPowerTransientConfig_s transient_config = ChassisPowerTransientDefaultConfig();
    ChassisPowerTransientState_s transient_state = {0};
    ChassisPowerTransientOutput_s transient = {0};
    ChassisPowerTransientInput_s motor = {.budget_w = 100, .target_w = 95,
        .meter_enabled = true, .meter_online = true, .control_enabled = true,
        .online_mask = 15, .enabled_mask = 15};
    /* Default mecanum lever and equivalent RPM speed-PID gain. The first-order
     * plant deliberately has no steady drag: demand disappears at a held target. */
    const float lever = 325 * 3.14159265359f / 180;
    float target = 1000, actual = 0, peak = 0, trough = 6000;
    bool saw_low_demand = false, saw_braking = false, resumed_after_braking = false;
    for (unsigned i = 0; i < 4000; ++i)
    {
        motor.now_ms = motor.meter_tick_ms = (i + 1) * 5;
        motor.meter_w = 15 + .000002f * actual * actual;
        bool tracking = true, saturated = false;
        for (unsigned w = 0; w < 4; ++w)
        {
            motor.actual_rpm[w] = -lever * actual;
            motor.target_rpm[w] = -lever * target;
            const float error = motor.target_rpm[w] - motor.actual_rpm[w];
            motor.raw_current[w] = fminf(15000, fmaxf(-15000, 4.5f * error));
            motor.feedback_tick_ms[w] = motor.now_ms;
            motor.feedback_sequence[w] = i + 1;
            tracking &= fabsf(error) <= 500;
            saturated |= fabsf(motor.raw_current[w]) >= 15000;
        }
        CHECK(ChassisPowerTransientUpdate(&transient_config, &motor, .005f,
            &transient_state, &transient));
        ChassisPowerTransientRecordSent(&transient_state, motor.raw_current);
        input.feedback_valid = transient.feedback_fresh && transient.meter_fresh;
        input.measured_power_w = motor.meter_w;
        input.target_power_w = motor.target_w;
        input.power_limited = saturated;
        input.increase_allowed = tracking && !saturated &&
            ChassisSpinTransientAllowsIncrease(&transient);
        saw_low_demand |= transient.drive_mask == 0 && transient.brake_mask == 0 &&
            input.increase_allowed;
        const float previous = target;
        target = Run(&config, &spin, input, 1);
        if (transient.brake_mask != 0)
        {
            saw_braking = true;
            CHECK(target <= previous + .01f);
        }
        if (saw_braking && target > previous + 1) resumed_after_braking = true;
        actual += .1f * (target - actual);
        CHECK(target > 0 && actual > 0);
        CHECK(fabsf(target - previous) <= 10.01f);
        if (i >= 1600)
        {
            peak = fmaxf(peak, actual);
            trough = fminf(trough, actual);
        }
    }
    CHECK(saw_low_demand && saw_braking && resumed_after_braking);
    CHECK(peak > 4900 && trough > 2900 && trough < 3100);
}

int main(void)
{
    ChassisSpinState_s retuned = {0};
    ChassisSpinInput_s retune_input = Ready();
    float previous = Run(&config, &retuned, retune_input, 1800);
    ChassisSpinConfig_s next = config;
    next.sine_period_s = 6;
    CHECK(ChassisSpinRetune(&next, &retuned));
    float after = Run(&next, &retuned, retune_input, 1);
    CHECK(fabsf(after - previous) <= next.sine_fall_wz_per_s * .005f + .01f);
    next.sine_direction = -1;
    CHECK(!ChassisSpinRetune(&next, &retuned));
    next.sine_direction = 1;
    retune_input.control_enabled = false;
    CHECK(ChassisSpinUpdate(&next, &retune_input, .005f, &retuned, &after));
    CHECK(after == 0); /* Retune slew must never delay a stop. */
    FullWaveKeepsDirectionAndHasTheRequestedPeriod();
    StartupWaitsForGuardsAndNormalStepsAreBounded();
    NegativeDirectionIsLockedUntilModeExit();
    RecoveryAndLimitingRemoveTheHighSpeedWave();
    OverpowerReducesTheTargetWithoutBypassingSlew();
    ABlockedWaveDoesNotAccumulateADeferredPeak();
    LossAndDisableResetWaveWithoutChangingItsDirection();
    InvalidInputsCannotLeaveAnOldWaveRunning();
    InvalidSineConfigurationsAreRejected();
    DelayedLoadTracksTheWaveAndReducesSpeedWhenBudgetFalls();
    LowDemandQualificationRetainsTransientProtections();
    RealTransientGateStartsAndResumesTheLowDragWave();
    if (failures) return 1;
    puts("PASS: same-direction sine spin tests");
    return 0;
}
