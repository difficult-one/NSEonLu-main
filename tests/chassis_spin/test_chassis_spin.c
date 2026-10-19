#include "chassis_spin.h"
#include "chassis_spin_config.h"

#include <math.h>
#include <stdio.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); ++failures; } } while (0)
#define NEAR(expected, actual) do { const float actual_ = (actual); \
    CHECK(isfinite(actual_) && fabsf((expected) - actual_) < 0.01f); } while (0)

static const ChassisSpinConfig_s config = {
    .fixed_wz = 4000, .min_wz = 1000, .max_wz = 6000,
    .gain_wz_per_w_s = 40, .rise_wz_per_s = 400, .fall_wz_per_s = 2000,
    .power_lpf_tau_s = .1f, .power_deadband_w = 1,
    .settle_time_s = .2f, .budget_drop_reset_w = 1,
};

static ChassisSpinInput_s ReadyInput(void)
{
    return (ChassisSpinInput_s){
        .spin_enabled = true, .control_enabled = true,
        .feedback_valid = true, .increase_allowed = true,
        .target_power_w = 38, .measured_power_w = 20,
    };
}

static float Update(ChassisSpinState_s *state, ChassisSpinInput_s input, unsigned cycles)
{
    float target = 0;
    for (unsigned i = 0; i < cycles; ++i)
        CHECK(ChassisSpinUpdate(&config, &input, .005f, state, &target));
    return target;
}

#if CHASSIS_SPIN_MODE == CHASSIS_SPIN_MODE_ADAPTIVE
/* A fixed legacy target must fail these tests: spare power now increases speed. */
static void TestSparePowerWaitsForTrackingAndRisesWithinRate(void)
{
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = ReadyInput();
    NEAR(4000, Update(&state, input, 20));
    const float target = Update(&state, input, 40);
    CHECK(target > 4000 && target <= 4082);
    const float next = Update(&state, input, 1);
    NEAR(2, next - target);
}

static void TestContinuousDeadbandDoesNotHunt(void)
{
    const float powers[] = {37, 37.5f, 38, 38.5f, 39};
    for (unsigned i = 0; i < sizeof(powers) / sizeof(powers[0]); ++i)
    {
        ChassisSpinState_s state = {0};
        ChassisSpinInput_s input = ReadyInput();
        input.measured_power_w = powers[i];
        NEAR(4000, Update(&state, input, 100));
    }
    ChassisSpinConfig_s fast = config;
    fast.settle_time_s = 0;
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = ReadyInput();
    input.measured_power_w = 36.5f;
    float target;
    CHECK(ChassisSpinUpdate(&fast, &input, .005f, &state, &target));
    NEAR(4000.1f, target); /* (38 - 36.5 - 1) * 40 * .005 */
}

static void TestExcessPowerLowersTargetWithoutGrowthPermission(void)
{
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = ReadyInput();
    input.measured_power_w = 100;
    input.increase_allowed = false;
    NEAR(3990, Update(&state, input, 1));
}

static void TestLimitingOrStartupCannotAccumulateTarget(void)
{
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = ReadyInput();
    input.increase_allowed = false;
    NEAR(4000, Update(&state, input, 500));
    input.increase_allowed = true;
    CHECK(Update(&state, input, 60) > 4000);
    const float held = state.target_wz;
    input.increase_allowed = false;
    NEAR(held, Update(&state, input, 1));
    input.increase_allowed = true;
    NEAR(held, Update(&state, input, 20));
}

static void TestBothSpeedBounds(void)
{
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = ReadyInput();
    NEAR(6000, Update(&state, input, 2000));
    input.measured_power_w = 1000;
    NEAR(1000, Update(&state, input, 2000));
    input.measured_power_w = 20;
    const float recovered = Update(&state, input, 400);
    CHECK(recovered > 1000 && recovered < 1800);
}

static void TestBudgetDropRemovesPositiveSpeedCorrection(void)
{
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = ReadyInput();
    CHECK(Update(&state, input, 1000) > 5500);
    input.target_power_w = 19;
    NEAR(4000, Update(&state, input, 1));
    input.measured_power_w = 5;
    NEAR(4000, Update(&state, input, 20));
}

static void TestFallbackAndModeReentryDoNotKeepHighTarget(void)
{
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = ReadyInput();
    CHECK(Update(&state, input, 100) > 4000);
    input.feedback_valid = false;
    NEAR(4000, Update(&state, input, 1));
    input.feedback_valid = true;
    NEAR(4000, Update(&state, input, 20));
    CHECK(Update(&state, input, 100) > 4000);
    input.spin_enabled = false;
    NEAR(4000, Update(&state, input, 1));
    input.spin_enabled = true;
    NEAR(4000, Update(&state, input, 20));
    CHECK(Update(&state, input, 100) > 4000);
    input.control_enabled = false;
    NEAR(4000, Update(&state, input, 1));
}

static void TestInvalidMeasurementAndTimingCannotIncreaseTarget(void)
{
    const float bad_values[] = {NAN, INFINITY, -INFINITY};
    for (unsigned i = 0; i < sizeof(bad_values) / sizeof(bad_values[0]); ++i)
    {
        ChassisSpinState_s state = {0};
        ChassisSpinInput_s input = ReadyInput();
        CHECK(Update(&state, input, 100) > 4000);
        input.measured_power_w = bad_values[i];
        float target;
        CHECK(!ChassisSpinUpdate(&config, &input, .005f, &state, &target));
        NEAR(4000, target);
    }
    const float bad_dt[] = {0, -.005f, .021f, NAN, INFINITY};
    for (unsigned i = 0; i < sizeof(bad_dt) / sizeof(bad_dt[0]); ++i)
    {
        ChassisSpinState_s state = {0};
        ChassisSpinInput_s input = ReadyInput();
        CHECK(Update(&state, input, 100) > 4000);
        float target;
        CHECK(!ChassisSpinUpdate(&config, &input, bad_dt[i], &state, &target));
        NEAR(4000, target);
    }
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = ReadyInput();
    input.target_power_w = 0;
    float target;
    CHECK(!ChassisSpinUpdate(&config, &input, .005f, &state, &target));
    NEAR(4000, target);
    input = ReadyInput();
    state.target_wz = NAN;
    state.initialized = true;
    CHECK(!ChassisSpinUpdate(&config, &input, .005f, &state, &target));
    NEAR(4000, target);
}

static void TestClosedLoopUsesSparePowerAndRespondsToTranslationLoad(void)
{
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = ReadyInput();
    float target = 4000;
    for (unsigned i = 0; i < 2500; ++i)
    {
        /* Independent monotonic load, with ideal wheel tracking. */
        input.measured_power_w = 20 + .04f * (target - 4000);
        CHECK(ChassisSpinUpdate(&config, &input, .005f, &state, &target));
    }
    CHECK(target > 4400 && target < 4500);
    CHECK(input.measured_power_w >= 36.8f && input.measured_power_w <= 39.2f);
    const float no_translation = target;
    for (unsigned i = 0; i < 2500; ++i)
    {
        input.measured_power_w = 30 + .04f * (target - 4000);
        CHECK(ChassisSpinUpdate(&config, &input, .005f, &state, &target));
    }
    CHECK(target < no_translation - 150);
    CHECK(input.measured_power_w >= 36.8f && input.measured_power_w <= 39.2f);
}
#else
static void TestDisabledMacroKeepsLegacyFixedTarget(void)
{
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = ReadyInput();
    NEAR(4000, Update(&state, input, 2000));
    input.measured_power_w = 100;
    NEAR(4000, Update(&state, input, 2000));
    input.feedback_valid = false;
    input.spin_enabled = false;
    NEAR(4000, Update(&state, input, 1));
}
#endif

static void TestInvalidConfigurationAndPointersFailSafely(void)
{
    ChassisSpinConfig_s bad = config;
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = ReadyInput();
    float target = 9999;
    bad.max_wz = 3000;
    CHECK(!ChassisSpinUpdate(&bad, &input, .005f, &state, &target));
    NEAR(0, target);
    bad = config;
    bad.min_wz = 0;
    CHECK(!ChassisSpinUpdate(&bad, &input, .005f, &state, &target));
    NEAR(0, target);
    bad = config;
    bad.gain_wz_per_w_s = NAN;
    CHECK(!ChassisSpinUpdate(&bad, &input, .005f, &state, &target));
    NEAR(0, target);
    CHECK(!ChassisSpinUpdate(NULL, &input, .005f, &state, &target));
    CHECK(!ChassisSpinUpdate(&config, NULL, .005f, &state, &target));
    CHECK(!ChassisSpinUpdate(&config, &input, .005f, NULL, &target));
    NEAR(0, target);
    CHECK(!ChassisSpinUpdate(&config, &input, .005f, &state, NULL));
}

static void TestDefaultMinimumKeepsARecoveryDemand(void)
{
    ChassisSpinConfig_s configured = config;
    configured.min_wz = CHASSIS_SPIN_MIN_WZ;
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = ReadyInput();
    input.measured_power_w = 1000;
    float target = 0;
    for (unsigned i = 0; i < 2000; ++i)
        CHECK(ChassisSpinUpdate(&configured, &input, .005f, &state, &target));
    CHECK(target > 0); /* A zero target would remove the transient drive demand. */
}

static void TestAppliedPredictiveGuardBlocksFurtherGrowth(void)
{
    ChassisPowerTransientOutput_s guard = {.guard_scale = 1, .positive_allowed = true};
    CHECK(ChassisSpinGuardAllowsIncrease(&guard));
    guard.guard_scale = .5f;
    CHECK(!ChassisSpinGuardAllowsIncrease(&guard));
    ChassisSpinState_s state = {0};
    ChassisSpinInput_s input = ReadyInput();
    input.increase_allowed = ChassisSpinGuardAllowsIncrease(&guard);
    NEAR(4000, Update(&state, input, 500));
    guard.guard_scale = 1;
    guard.growth_fallback = true;
    CHECK(!ChassisSpinGuardAllowsIncrease(&guard));
    guard.growth_fallback = false;
    guard.guard_infeasible = true;
    CHECK(!ChassisSpinGuardAllowsIncrease(&guard));
    guard.guard_infeasible = false;
    guard.guard_search_exhausted = true;
    CHECK(!ChassisSpinGuardAllowsIncrease(&guard));
    guard.guard_search_exhausted = false;
    guard.guard_scale = NAN;
    CHECK(!ChassisSpinGuardAllowsIncrease(&guard));
    CHECK(!ChassisSpinGuardAllowsIncrease(NULL));
}

int main(void)
{
#if CHASSIS_SPIN_MODE == CHASSIS_SPIN_MODE_ADAPTIVE
    TestSparePowerWaitsForTrackingAndRisesWithinRate();
    TestContinuousDeadbandDoesNotHunt();
    TestExcessPowerLowersTargetWithoutGrowthPermission();
    TestLimitingOrStartupCannotAccumulateTarget();
    TestBothSpeedBounds();
    TestBudgetDropRemovesPositiveSpeedCorrection();
    TestFallbackAndModeReentryDoNotKeepHighTarget();
    TestInvalidMeasurementAndTimingCannotIncreaseTarget();
    TestClosedLoopUsesSparePowerAndRespondsToTranslationLoad();
#else
    TestDisabledMacroKeepsLegacyFixedTarget();
#endif
    TestInvalidConfigurationAndPointersFailSafely();
    TestDefaultMinimumKeepsARecoveryDemand();
    TestAppliedPredictiveGuardBlocksFurtherGrowth();
    if (failures) return 1;
    printf("PASS: chassis spin (mode=%d, legacy=%d)\n", CHASSIS_SPIN_MODE, CHASSIS_SPIN_ADAPTIVE_ENABLE);
    return 0;
}
