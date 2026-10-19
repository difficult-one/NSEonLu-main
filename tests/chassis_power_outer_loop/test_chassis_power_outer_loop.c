#include "chassis_power_outer_loop.h"

#include <math.h>
#include <stdio.h>

static int failures;

#define ASSERT_TRUE(condition)                                                            \
    do                                                                                    \
    {                                                                                     \
        if (!(condition))                                                                 \
        {                                                                                 \
            printf("FAIL %s:%d: expected true: %s\n", __FILE__, __LINE__, #condition);   \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_EQ_INT(expected, actual)                                                    \
    do                                                                                    \
    {                                                                                     \
        const int expected_ = (expected);                                                  \
        const int actual_ = (actual);                                                      \
        if (expected_ != actual_)                                                          \
        {                                                                                 \
            printf("FAIL %s:%d: expected %d, got %d\n",                                  \
                   __FILE__, __LINE__, expected_, actual_);                                \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

#define ASSERT_NEAR(expected, actual, tolerance)                                           \
    do                                                                                    \
    {                                                                                     \
        const float expected_ = (expected);                                                \
        const float actual_ = (actual);                                                    \
        if (!isfinite(expected_) || !isfinite(actual_) ||                                  \
            fabsf(expected_ - actual_) > (tolerance))                                      \
        {                                                                                 \
            printf("FAIL %s:%d: expected %.6f, got %.6f\n",                              \
                   __FILE__, __LINE__, (double)expected_, (double)actual_);                \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

static const ChassisPowerOuterLoopConfig_s valid_config = {
    .kp = 0.5f,
    .ki = 1.0f,
    .measurement_lpf_tau_s = 0.030f,
    .activation_margin_w = 5.0f,
    .error_deadband_w = 1.0f,
    .max_positive_correction_w = 8.0f,
    .positive_slew_rate_w_per_s = 20.0f,
    .budget_drop_reset_w = 1.0f,
};

static ChassisPowerOuterLoopInput_s BaseInput(void)
{
    const ChassisPowerOuterLoopInput_s input = {
        .nominal_budget_w = 60.0f,
        .target_power_w = 57.0f,
        .measured_power_w = 20.0f,
        .predicted_unlimited_power_w = 20.0f,
        .meter_online = true,
        .allow_positive_correction = true,
        .control_enabled = true,
    };
    return input;
}

static void TestSpinGovernorHoldsPositiveLearningWithoutDisablingOverpower(void)
{
    for (unsigned isolated = 0; isolated < 2; ++isolated)
    {
        ChassisPowerOuterLoopState_s state = {
            .integral_w = 1, .correction_w = 2, .applied_correction_w = 2,
            .filtered_power_w = 20, .measurement_initialized = true,
        };
        ChassisPowerOuterLoopInput_s input = BaseInput();
        input.predicted_unlimited_power_w = 55;
        input.learning_allowed = true;
        input.hold_positive_learning = true;
        ChassisPowerOuterLoopOutput_s output;
        for (unsigned i = 0; i < 50; ++i)
        {
            ASSERT_TRUE(isolated ? ChassisPowerOuterLoopUpdateIsolated(&valid_config, &input, .005f, &state, &output) :
                                  ChassisPowerOuterLoopUpdate(&valid_config, &input, .005f, &state, &output));
        }
        ASSERT_NEAR(1, state.integral_w, 1e-5f);
        ASSERT_NEAR(2, output.correction_w, 1e-5f);
        ASSERT_EQ_INT(CHASSIS_POWER_OUTER_STANDBY, output.mode);
        input.hold_positive_learning = false;
        input.base_limited = true;
        ASSERT_TRUE(isolated ? ChassisPowerOuterLoopUpdateIsolated(&valid_config, &input, .005f, &state, &output) :
                              ChassisPowerOuterLoopUpdate(&valid_config, &input, .005f, &state, &output));
        ASSERT_TRUE(output.correction_w > 2);
        input.hold_positive_learning = true;
        input.measured_power_w = 100;
        ASSERT_TRUE(isolated ? ChassisPowerOuterLoopUpdateIsolated(&valid_config, &input, .005f, &state, &output) :
                              ChassisPowerOuterLoopUpdate(&valid_config, &input, .005f, &state, &output));
        ASSERT_TRUE(output.correction_w < 0);
    }
    /* Stage 0 calls the unwrapped loop: braking must revoke retained positive output. */
    ChassisPowerOuterLoopState_s braking_state = {
        .integral_w = 1, .correction_w = 2,
        .filtered_power_w = 20, .measurement_initialized = true,
    };
    ChassisPowerOuterLoopInput_s braking_input = BaseInput();
    braking_input.nominal_budget_w = 40;
    braking_input.target_power_w = 38;
    braking_input.predicted_unlimited_power_w = 35;
    braking_input.hold_positive_learning = true;
    braking_input.allow_positive_correction = false;
    ChassisPowerOuterLoopOutput_s braking_output;
    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &braking_input, .005f,
                                          &braking_state, &braking_output));
    ASSERT_NEAR(0, braking_output.correction_w, 1e-5f);
    ASSERT_NEAR(40, braking_output.corrected_budget_w, 1e-5f);
}

static void AssertStateIsReset(const ChassisPowerOuterLoopState_s *state)
{
    ASSERT_NEAR(0.0f, state->filtered_power_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, state->integral_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, state->correction_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, state->previous_nominal_budget_w, 1.0e-5f);
    ASSERT_FALSE(state->measurement_initialized);
}

static void AssertOutputFailsClosed(const ChassisPowerOuterLoopOutput_s *output)
{
    ASSERT_NEAR(0.0f, output->corrected_budget_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, output->correction_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, output->filtered_power_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, output->error_w, 1.0e-5f);
    ASSERT_EQ_INT(CHASSIS_POWER_OUTER_OFFLINE, output->mode);
}

static void TestResetClearsEveryStateField(void)
{
    ChassisPowerOuterLoopState_s state = {
        .filtered_power_w = 40.0f,
        .integral_w = 3.0f,
        .correction_w = -4.0f,
        .previous_nominal_budget_w = 60.0f,
        .measurement_initialized = true,
    };

    ChassisPowerOuterLoopReset(&state);

    AssertStateIsReset(&state);
    ChassisPowerOuterLoopReset(NULL);
}

static void TestOfflineMeterPassesNominalBudgetFromReset(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);
    input.meter_online = false;

    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_EQ_INT(CHASSIS_POWER_OUTER_OFFLINE, output.mode);
    ASSERT_NEAR(60.0f, output.corrected_budget_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, output.correction_w, 1.0e-5f);
    ASSERT_FALSE(state.measurement_initialized);
    ASSERT_NEAR(60.0f, state.previous_nominal_budget_w, 1.0e-5f);
}

static void TestFirstOnlineSampleSeedsFilterWithoutZeroRamp(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);

    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_EQ_INT(CHASSIS_POWER_OUTER_STANDBY, output.mode);
    ASSERT_NEAR(20.0f, output.filtered_power_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, output.correction_w, 1.0e-5f);
    ASSERT_TRUE(state.measurement_initialized);
}

static void TestSubsequentOnlineSampleUsesLowPassFilter(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);
    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.010f,
                                            &state, &output));

    input.measured_power_w = 40.0f;
    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.010f,
                                            &state, &output));

    ASSERT_EQ_INT(CHASSIS_POWER_OUTER_STANDBY, output.mode);
    ASSERT_NEAR(25.0f, output.filtered_power_w, 1.0e-5f);
}

static void TestOnlineSampleAfterOfflineReseedsFilter(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);
    input.measured_power_w = 40.0f;
    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));

    input.meter_online = false;
    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_FALSE(state.measurement_initialized);

    input.meter_online = true;
    input.measured_power_w = 62.0f;
    input.predicted_unlimited_power_w = 70.0f;
    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));

    ASSERT_NEAR(62.0f, output.filtered_power_w, 1.0e-5f);
    ASSERT_TRUE(output.correction_w < 0.0f);
}

static void TestLowDemandStaysStandbyAndDoesNotAccumulateCorrection(void)
{
    ChassisPowerOuterLoopState_s state = {
        .filtered_power_w = 20.0f,
        .integral_w = 3.0f,
        .correction_w = -1.0f,
        .previous_nominal_budget_w = 60.0f,
        .measurement_initialized = true,
    };
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();

    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_EQ_INT(CHASSIS_POWER_OUTER_STANDBY, output.mode);
    ASSERT_NEAR(0.0f, state.integral_w, 1.0e-5f);
    ASSERT_NEAR(-0.9f, output.correction_w, 1.0e-5f);
    ASSERT_NEAR(59.1f, output.corrected_budget_w, 1.0e-5f);

    for (int i = 0; i < 10; ++i)
    {
        ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                                &state, &output));
    }
    ASSERT_NEAR(0.0f, output.correction_w, 1.0e-5f);
    ASSERT_NEAR(60.0f, output.corrected_budget_w, 1.0e-5f);
}

static void TestDisabledControlResetsStateAndOutputsZeroBudget(void)
{
    ChassisPowerOuterLoopState_s state = {
        .filtered_power_w = 40.0f,
        .integral_w = 3.0f,
        .correction_w = 2.0f,
        .previous_nominal_budget_w = 60.0f,
        .measurement_initialized = true,
    };
    ChassisPowerOuterLoopOutput_s output = {
        .corrected_budget_w = 60.0f,
        .correction_w = 2.0f,
        .filtered_power_w = 40.0f,
        .error_w = 17.0f,
        .mode = CHASSIS_POWER_OUTER_ACTIVE,
    };
    ChassisPowerOuterLoopInput_s input = BaseInput();
    input.control_enabled = false;

    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_EQ_INT(CHASSIS_POWER_OUTER_STANDBY, output.mode);
    ASSERT_NEAR(0.0f, output.corrected_budget_w, 1.0e-5f);
    ASSERT_FALSE(state.measurement_initialized);
    AssertStateIsReset(&state);
}

static void AssertInvalidInputFailsClosed(ChassisPowerOuterLoopInput_s input)
{
    ChassisPowerOuterLoopState_s state = {
        .filtered_power_w = 40.0f,
        .integral_w = 3.0f,
        .correction_w = 2.0f,
        .previous_nominal_budget_w = 60.0f,
        .measurement_initialized = true,
    };
    ChassisPowerOuterLoopOutput_s output = {
        .corrected_budget_w = 60.0f,
        .correction_w = 2.0f,
        .filtered_power_w = 40.0f,
        .error_w = 17.0f,
        .mode = CHASSIS_POWER_OUTER_ACTIVE,
    };

    ASSERT_FALSE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                             &state, &output));
    AssertStateIsReset(&state);
    AssertOutputFailsClosed(&output);
}

static void TestInvalidBudgetFailsClosed(void)
{
    ChassisPowerOuterLoopInput_s input = BaseInput();
    input.nominal_budget_w = 0.0f;
    AssertInvalidInputFailsClosed(input);
    input.nominal_budget_w = -1.0f;
    AssertInvalidInputFailsClosed(input);
    input.nominal_budget_w = NAN;
    AssertInvalidInputFailsClosed(input);

    input = BaseInput();
    input.target_power_w = 0.0f;
    AssertInvalidInputFailsClosed(input);
    input.target_power_w = -1.0f;
    AssertInvalidInputFailsClosed(input);
    input.target_power_w = INFINITY;
    AssertInvalidInputFailsClosed(input);

    input = BaseInput();
    input.predicted_unlimited_power_w = -1.0f;
    AssertInvalidInputFailsClosed(input);
    input.predicted_unlimited_power_w = NAN;
    AssertInvalidInputFailsClosed(input);
}

static void AssertInvalidConfigFailsClosed(ChassisPowerOuterLoopConfig_s config)
{
    ChassisPowerOuterLoopState_s state = {
        .filtered_power_w = 40.0f,
        .integral_w = 3.0f,
        .correction_w = 2.0f,
        .previous_nominal_budget_w = 60.0f,
        .measurement_initialized = true,
    };
    ChassisPowerOuterLoopOutput_s output = {
        .corrected_budget_w = 60.0f,
        .correction_w = 2.0f,
        .filtered_power_w = 40.0f,
        .error_w = 17.0f,
        .mode = CHASSIS_POWER_OUTER_ACTIVE,
    };
    ChassisPowerOuterLoopInput_s input = BaseInput();

    ASSERT_FALSE(ChassisPowerOuterLoopConfigIsValid(&config));
    ASSERT_FALSE(ChassisPowerOuterLoopUpdate(&config, &input, 0.005f,
                                             &state, &output));
    AssertStateIsReset(&state);
    AssertOutputFailsClosed(&output);
}

static void TestInvalidConfigFailsClosed(void)
{
    ChassisPowerOuterLoopConfig_s config = valid_config;
    ASSERT_TRUE(ChassisPowerOuterLoopConfigIsValid(&valid_config));
    ASSERT_FALSE(ChassisPowerOuterLoopConfigIsValid(NULL));

    config.kp = -1.0f;
    AssertInvalidConfigFailsClosed(config);
    config = valid_config;
    config.ki = -1.0f;
    AssertInvalidConfigFailsClosed(config);
    config = valid_config;
    config.measurement_lpf_tau_s = 0.0f;
    AssertInvalidConfigFailsClosed(config);
    config = valid_config;
    config.activation_margin_w = -1.0f;
    AssertInvalidConfigFailsClosed(config);
    config = valid_config;
    config.error_deadband_w = -1.0f;
    AssertInvalidConfigFailsClosed(config);
    config = valid_config;
    config.max_positive_correction_w = 0.0f;
    AssertInvalidConfigFailsClosed(config);
    config = valid_config;
    config.positive_slew_rate_w_per_s = 0.0f;
    AssertInvalidConfigFailsClosed(config);
    config = valid_config;
    config.budget_drop_reset_w = -1.0f;
    AssertInvalidConfigFailsClosed(config);

    config = valid_config;
    config.kp = NAN;
    AssertInvalidConfigFailsClosed(config);
    config = valid_config;
    config.ki = INFINITY;
    AssertInvalidConfigFailsClosed(config);
    config = valid_config;
    config.measurement_lpf_tau_s = NAN;
    AssertInvalidConfigFailsClosed(config);
    config = valid_config;
    config.activation_margin_w = NAN;
    AssertInvalidConfigFailsClosed(config);
    config = valid_config;
    config.error_deadband_w = NAN;
    AssertInvalidConfigFailsClosed(config);
    config = valid_config;
    config.max_positive_correction_w = NAN;
    AssertInvalidConfigFailsClosed(config);
    config = valid_config;
    config.positive_slew_rate_w_per_s = NAN;
    AssertInvalidConfigFailsClosed(config);
    config = valid_config;
    config.budget_drop_reset_w = NAN;
    AssertInvalidConfigFailsClosed(config);
}

static void TestInvalidPointersFailSafely(void)
{
    ChassisPowerOuterLoopState_s state = {
        .filtered_power_w = 40.0f,
        .integral_w = 3.0f,
        .correction_w = 2.0f,
        .previous_nominal_budget_w = 60.0f,
        .measurement_initialized = true,
    };
    ChassisPowerOuterLoopOutput_s output = {
        .corrected_budget_w = 60.0f,
        .correction_w = 2.0f,
        .filtered_power_w = 40.0f,
        .error_w = 17.0f,
        .mode = CHASSIS_POWER_OUTER_ACTIVE,
    };
    ChassisPowerOuterLoopInput_s input = BaseInput();

    ASSERT_FALSE(ChassisPowerOuterLoopUpdate(NULL, &input, 0.005f, &state, &output));
    AssertStateIsReset(&state);
    AssertOutputFailsClosed(&output);

    state.measurement_initialized = true;
    output.corrected_budget_w = 60.0f;
    output.mode = CHASSIS_POWER_OUTER_ACTIVE;
    ASSERT_FALSE(ChassisPowerOuterLoopUpdate(&valid_config, NULL, 0.005f,
                                             &state, &output));
    AssertStateIsReset(&state);
    AssertOutputFailsClosed(&output);

    ASSERT_FALSE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                             NULL, &output));
    AssertOutputFailsClosed(&output);
    ASSERT_FALSE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                             &state, NULL));
}

static void TestOverPowerImmediatelyReducesBudget(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);
    input.measured_power_w = 62.0f;
    input.predicted_unlimited_power_w = 70.0f;

    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_EQ_INT(CHASSIS_POWER_OUTER_ACTIVE, output.mode);
    ASSERT_NEAR(-2.525f, output.correction_w, 1.0e-4f);
    ASSERT_NEAR(57.475f, output.corrected_budget_w, 1.0e-4f);
}

static void TestPositiveCorrectionIsSlewLimited(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);
    input.measured_power_w = 52.0f;
    input.predicted_unlimited_power_w = 70.0f;

    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_NEAR(0.1f, output.correction_w, 1.0e-5f);
    ASSERT_NEAR(60.1f, output.corrected_budget_w, 1.0e-5f);
}

static void TestDeadbandProducesNoCorrection(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);
    input.measured_power_w = 57.5f;
    input.predicted_unlimited_power_w = 70.0f;

    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_EQ_INT(CHASSIS_POWER_OUTER_ACTIVE, output.mode);
    ASSERT_NEAR(0.0f, output.error_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, output.correction_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, state.integral_w, 1.0e-5f);
}

static void TestPositiveCorrectionStopsAtEightWatts(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);
    input.measured_power_w = 52.0f;
    input.predicted_unlimited_power_w = 70.0f;

    for (int i = 0; i < 400; ++i)
    {
        ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                                &state, &output));
    }

    ASSERT_NEAR(8.0f, output.correction_w, 1.0e-5f);
    ASSERT_NEAR(68.0f, output.corrected_budget_w, 1.0e-5f);
}

static void TestLargeOverPowerCanReduceBudgetToZero(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);
    input.measured_power_w = 200.0f;
    input.predicted_unlimited_power_w = 200.0f;

    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_NEAR(-60.0f, output.correction_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, output.corrected_budget_w, 1.0e-5f);
}

static void TestRawHardLimitBypassesFilteredMeasurement(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);
    input.measured_power_w = 40.0f;

    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    input.measured_power_w = 65.0f;
    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));

    ASSERT_EQ_INT(CHASSIS_POWER_OUTER_ACTIVE, output.mode);
    ASSERT_NEAR(43.57143f, output.filtered_power_w, 1.0e-4f);
    ASSERT_NEAR(-8.0f, output.error_w, 1.0e-5f);
    ASSERT_NEAR(-4.04f, output.correction_w, 1.0e-4f);
}

static void TestBrakingCannotCreatePositiveCorrection(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);
    input.measured_power_w = 52.0f;
    input.predicted_unlimited_power_w = 70.0f;
    input.allow_positive_correction = false;

    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_EQ_INT(CHASSIS_POWER_OUTER_ACTIVE, output.mode);
    ASSERT_NEAR(0.0f, state.integral_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, output.correction_w, 1.0e-5f);
    ASSERT_NEAR(60.0f, output.corrected_budget_w, 1.0e-5f);
}

static void TestNegativeMeasuredPowerCannotCreatePositiveCorrection(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);
    input.measured_power_w = -25.0f;
    input.predicted_unlimited_power_w = 70.0f;
    input.allow_positive_correction = false;

    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_EQ_INT(CHASSIS_POWER_OUTER_ACTIVE, output.mode);
    ASSERT_NEAR(-25.0f, output.filtered_power_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, state.integral_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, output.correction_w, 1.0e-5f);
    ASSERT_NEAR(60.0f, output.corrected_budget_w, 1.0e-5f);
}

static void TestBrakingSaturationCannotStorePositiveIntegral(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);
    input.measured_power_w = 41.1f;
    input.predicted_unlimited_power_w = 70.0f;
    input.allow_positive_correction = false;

    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_NEAR(0.0f, state.integral_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, output.correction_w, 1.0e-5f);

    input.measured_power_w = 57.0f;
    for (int i = 0; i < 200; ++i)
    {
        ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                                &state, &output));
    }
    input.allow_positive_correction = true;
    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_NEAR(0.0f, state.integral_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, output.correction_w, 1.0e-5f);
}

static void TestOfflineDropsPositiveCorrectionImmediately(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);
    input.measured_power_w = 52.0f;
    input.predicted_unlimited_power_w = 70.0f;
    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_NEAR(0.1f, output.correction_w, 1.0e-5f);

    input.meter_online = false;
    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_EQ_INT(CHASSIS_POWER_OUTER_OFFLINE, output.mode);
    ASSERT_NEAR(0.0f, state.integral_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, output.correction_w, 1.0e-5f);
    ASSERT_NEAR(60.0f, output.corrected_budget_w, 1.0e-5f);
}

static void TestOfflineReleasesNegativeCorrectionAtPointOneWattPerTick(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);
    input.measured_power_w = 62.0f;
    input.predicted_unlimited_power_w = 70.0f;
    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_NEAR(-2.525f, output.correction_w, 1.0e-4f);

    input.meter_online = false;
    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_EQ_INT(CHASSIS_POWER_OUTER_OFFLINE, output.mode);
    ASSERT_NEAR(0.0f, state.integral_w, 1.0e-5f);
    ASSERT_NEAR(-2.425f, output.correction_w, 1.0e-4f);
    ASSERT_NEAR(57.575f, output.corrected_budget_w, 1.0e-4f);
}

static void TestNonfiniteMeasurementUsesOfflinePolicy(void)
{
    ChassisPowerOuterLoopState_s state = {
        .filtered_power_w = 52.0f,
        .integral_w = 1.0f,
        .correction_w = 1.0f,
        .previous_nominal_budget_w = 60.0f,
        .measurement_initialized = true,
    };
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    input.measured_power_w = NAN;

    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_EQ_INT(CHASSIS_POWER_OUTER_OFFLINE, output.mode);
    ASSERT_FALSE(state.measurement_initialized);
    ASSERT_NEAR(0.0f, state.integral_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, output.correction_w, 1.0e-5f);
    ASSERT_NEAR(60.0f, output.corrected_budget_w, 1.0e-5f);
}

static void TestBudgetDropClearsPositiveState(void)
{
    ChassisPowerOuterLoopState_s state = {
        .filtered_power_w = 47.5f,
        .integral_w = 2.0f,
        .correction_w = 2.0f,
        .previous_nominal_budget_w = 60.0f,
        .measurement_initialized = true,
    };
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    input.nominal_budget_w = 50.0f;
    input.target_power_w = 47.5f;
    input.measured_power_w = 47.5f;
    input.predicted_unlimited_power_w = 70.0f;

    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_EQ_INT(CHASSIS_POWER_OUTER_ACTIVE, output.mode);
    ASSERT_NEAR(0.0f, state.integral_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, output.correction_w, 1.0e-5f);
    ASSERT_NEAR(50.0f, output.corrected_budget_w, 1.0e-5f);
}

static void TestSaturationDoesNotWindIntegralFurther(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);
    input.measured_power_w = 52.0f;
    input.predicted_unlimited_power_w = 70.0f;

    for (int i = 0; i < 400 && state.correction_w < 8.0f; ++i)
    {
        ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                                &state, &output));
    }
    ASSERT_NEAR(8.0f, state.correction_w, 1.0e-5f);
    const float integral_at_saturation_w = state.integral_w;

    for (int i = 0; i < 100; ++i)
    {
        ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                                &state, &output));
    }

    ASSERT_NEAR(integral_at_saturation_w, state.integral_w, 1.0e-5f);
    ASSERT_NEAR(8.0f, output.correction_w, 1.0e-5f);
}

static void TestErrorCanDriveControllerOutOfSaturation(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);
    input.measured_power_w = 52.0f;
    input.predicted_unlimited_power_w = 70.0f;
    for (int i = 0; i < 400 && state.correction_w < 8.0f; ++i)
    {
        ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                                &state, &output));
    }
    const float saturated_integral_w = state.integral_w;

    input.measured_power_w = 65.0f;
    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));

    ASSERT_TRUE(output.correction_w < 8.0f);
    ASSERT_NEAR(saturated_integral_w - 0.04f, state.integral_w, 1.0e-4f);
}

static void TestCandidateSaturationDoesNotWindBeforeSlewCatchesUp(void)
{
    ChassisPowerOuterLoopState_s state;
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    ChassisPowerOuterLoopReset(&state);
    input.measured_power_w = 0.0f;
    input.predicted_unlimited_power_w = 70.0f;

    for (int i = 0; i < 20; ++i)
    {
        ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                                &state, &output));
    }
    ASSERT_NEAR(2.0f, output.correction_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, state.integral_w, 1.0e-5f);

    input.measured_power_w = 65.0f;
    ASSERT_TRUE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.005f,
                                            &state, &output));
    ASSERT_NEAR(-4.04f, output.correction_w, 1.0e-4f);
}

static void TestInvalidDeltaTimeFailsWithoutPositiveOutput(void)
{
    ChassisPowerOuterLoopState_s state = {
        .filtered_power_w = 52.0f,
        .integral_w = 2.0f,
        .correction_w = 2.0f,
        .previous_nominal_budget_w = 60.0f,
        .measurement_initialized = true,
    };
    ChassisPowerOuterLoopOutput_s output;
    ChassisPowerOuterLoopInput_s input = BaseInput();
    input.measured_power_w = 52.0f;
    input.predicted_unlimited_power_w = 70.0f;

    ASSERT_FALSE(ChassisPowerOuterLoopUpdate(&valid_config, &input, 0.0f,
                                             &state, &output));
    ASSERT_EQ_INT(CHASSIS_POWER_OUTER_OFFLINE, output.mode);
    ASSERT_NEAR(0.0f, state.integral_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, output.correction_w, 1.0e-5f);
    ASSERT_NEAR(60.0f, output.corrected_budget_w, 1.0e-5f);
    ASSERT_TRUE(state.measurement_initialized);

    state.integral_w = -1.0f;
    state.correction_w = -2.5f;
    ASSERT_FALSE(ChassisPowerOuterLoopUpdate(&valid_config, &input, NAN,
                                             &state, &output));
    ASSERT_EQ_INT(CHASSIS_POWER_OUTER_OFFLINE, output.mode);
    ASSERT_NEAR(-1.0f, state.integral_w, 1.0e-5f);
    ASSERT_NEAR(-2.5f, output.correction_w, 1.0e-5f);
    ASSERT_NEAR(57.5f, output.corrected_budget_w, 1.0e-5f);

    state = (ChassisPowerOuterLoopState_s){
        .filtered_power_w = 52.0f,
        .integral_w = 2.0f,
        .correction_w = 2.0f,
        .previous_nominal_budget_w = 60.0f,
        .measurement_initialized = true,
    };
    input.control_enabled = false;
    ASSERT_FALSE(ChassisPowerOuterLoopUpdate(&valid_config, &input, -0.005f,
                                             &state, &output));
    ASSERT_EQ_INT(CHASSIS_POWER_OUTER_OFFLINE, output.mode);
    ASSERT_NEAR(0.0f, state.integral_w, 1.0e-5f);
    ASSERT_NEAR(0.0f, output.correction_w, 1.0e-5f);
    ASSERT_NEAR(60.0f, output.corrected_budget_w, 1.0e-5f);
    ASSERT_TRUE(state.measurement_initialized);
}

int main(void)
{
    {
        ChassisPowerOuterLoopConfig_s c = valid_config;
        c.error_deadband_w = .3f; c.antiwindup_gain_per_s = 10;
        c.low_demand_delay_s = .5f; c.low_demand_decay_w_per_s = 1;
        ChassisPowerOuterLoopInput_s i = BaseInput();
        i.complete_slow_control = i.learning_allowed = i.base_limited = i.demand_sufficient = true;
        i.measured_power_w = 57.2f; i.meter_tick_ms = 1;
        ChassisPowerOuterLoopState_s s = {0};
        ChassisPowerOuterLoopOutput_s o;
        ASSERT_TRUE(ChassisPowerOuterLoopUpdateIsolated(&c, &i, .005f, &s, &o));
        ASSERT_TRUE(ChassisPowerOuterLoopUpdateIsolated(&c, &i, .005f, &s, &o));
        ASSERT_EQ_INT(1, s.recovery_samples); /* repeats do not establish recovery */
        i.meter_tick_ms = 2;
        ASSERT_TRUE(ChassisPowerOuterLoopUpdateIsolated(&c, &i, .005f, &s, &o));
        ASSERT_NEAR(0, o.error_w, 1e-6f);
        s.filtered_power_w = i.measured_power_w = 57.5f;
        i.meter_tick_ms = 3;
        ASSERT_TRUE(ChassisPowerOuterLoopUpdateIsolated(&c, &i, .005f, &s, &o));
        ASSERT_NEAR(-.2f, o.error_w, 1e-5f);
        for (unsigned gate = 0; gate < 3; ++gate) {
            s.integral_w = 1; s.correction_w = s.applied_correction_w = 1;
            i.base_limited = gate != 0; i.braking = gate == 1; i.demand_sufficient = gate != 2;
            ASSERT_TRUE(ChassisPowerOuterLoopUpdateIsolated(&c, &i, .005f, &s, &o));
            ASSERT_NEAR(1, s.integral_w, 1e-6f);
            ASSERT_NEAR(1, s.correction_w, 1e-6f);
        }
        i.meter_online = false;
        s.integral_w = s.correction_w = s.applied_correction_w = -2;
        ASSERT_TRUE(ChassisPowerOuterLoopUpdateIsolated(&c, &i, .005f, &s, &o));
        ASSERT_NEAR(-2, s.integral_w, 1e-6f);
        i.meter_online = true; i.measured_power_w = 20; i.meter_tick_ms = UINT32_MAX;
        ASSERT_TRUE(ChassisPowerOuterLoopUpdateIsolated(&c, &i, .005f, &s, &o));
        ASSERT_NEAR(20, s.filtered_power_w, 1e-6f);
        ASSERT_EQ_INT(1, s.recovery_samples);
        i.meter_tick_ms = 0;
        ASSERT_TRUE(ChassisPowerOuterLoopUpdateIsolated(&c, &i, .005f, &s, &o));
        ASSERT_EQ_INT(2, s.recovery_samples);
    }
    {
        ChassisPowerOuterLoopConfig_s c = valid_config;
        c.error_deadband_w = .3f; c.antiwindup_gain_per_s = 10;
        c.low_demand_delay_s = .5f; c.low_demand_decay_w_per_s = 1;
        ChassisPowerOuterLoopState_s s = {0};
        ChassisPowerOuterLoopInput_s i = BaseInput();
        i.complete_slow_control = i.learning_allowed = i.base_limited = i.demand_sufficient = true;
        i.measured_power_w = 46.7f; i.meter_tick_ms = 1;
        ChassisPowerOuterLoopOutput_s o;
        ASSERT_TRUE(ChassisPowerOuterLoopUpdateIsolated(&c, &i, .005f, &s, &o));
        ASSERT_NEAR(0, o.correction_w, 1e-6f); /* first fresh sample reinitializes */
        i.meter_tick_ms = 2;
        ASSERT_TRUE(ChassisPowerOuterLoopUpdateIsolated(&c, &i, .005f, &s, &o));
        ASSERT_NEAR(10, o.error_w, 1e-4f);
        ASSERT_NEAR(.1f, o.correction_w, 1e-5f);
        ASSERT_NEAR(-.195f, s.integral_w, 1e-4f);
        i.learning_allowed = i.allow_positive_correction = false; i.measured_power_w = 100;
        ASSERT_TRUE(ChassisPowerOuterLoopUpdateIsolated(&c, &i, .005f, &s, &o));
        ASSERT_NEAR(-.195f, s.integral_w, 1e-4f);
        ASSERT_NEAR(.1f, s.correction_w, 1e-5f);
        ASSERT_NEAR(0, o.correction_w, 1e-6f);
        s.integral_w = s.correction_w = s.applied_correction_w = -2;
        i.demand_sufficient = false;
        for (unsigned t = 0; t < 200; ++t)
            ASSERT_TRUE(ChassisPowerOuterLoopUpdateIsolated(&c, &i, .005f, &s, &o));
        ASSERT_NEAR(-1.5f, s.integral_w, .01f);
        ASSERT_NEAR(-1.5f, o.correction_w, .01f);
        s.integral_w = s.correction_w = s.applied_correction_w = 3;
        i.nominal_budget_w = 58;
        ASSERT_TRUE(ChassisPowerOuterLoopUpdateIsolated(&c, &i, .005f, &s, &o));
        ASSERT_NEAR(0, s.integral_w, 1e-6f);
        i.control_enabled = false;
        ASSERT_TRUE(ChassisPowerOuterLoopUpdateIsolated(&c, &i, .005f, &s, &o));
        ASSERT_NEAR(0, o.corrected_budget_w, 1e-6f);
    }
    /* A spike must freeze both P and I in the separated controller. */
    {
        ChassisPowerOuterLoopState_s s = {.integral_w = 2, .correction_w = 2,
            .applied_correction_w = 2, .measurement_initialized = true, .filtered_power_w = 57};
        ChassisPowerOuterLoopInput_s i = BaseInput();
        i.measured_power_w = 100; i.learning_allowed = false; i.allow_positive_correction = false;
        ChassisPowerOuterLoopOutput_s o;
        ASSERT_TRUE(ChassisPowerOuterLoopUpdateIsolated(&valid_config, &i, .005f, &s, &o));
        ASSERT_NEAR(2, s.integral_w, 1e-6f);
        ASSERT_NEAR(2, s.correction_w, 1e-6f);
        ASSERT_NEAR(0, o.correction_w, 1e-6f);
        i.learning_allowed = true; i.allow_positive_correction = true; i.measured_power_w = 57;
        ASSERT_TRUE(ChassisPowerOuterLoopUpdateIsolated(&valid_config, &i, .005f, &s, &o));
        ASSERT_TRUE(o.correction_w <= .1f);
    }
    TestResetClearsEveryStateField();
    TestSpinGovernorHoldsPositiveLearningWithoutDisablingOverpower();
    TestOfflineMeterPassesNominalBudgetFromReset();
    TestFirstOnlineSampleSeedsFilterWithoutZeroRamp();
    TestSubsequentOnlineSampleUsesLowPassFilter();
    TestOnlineSampleAfterOfflineReseedsFilter();
    TestLowDemandStaysStandbyAndDoesNotAccumulateCorrection();
    TestDisabledControlResetsStateAndOutputsZeroBudget();
    TestInvalidBudgetFailsClosed();
    TestInvalidConfigFailsClosed();
    TestInvalidPointersFailSafely();
    TestOverPowerImmediatelyReducesBudget();
    TestPositiveCorrectionIsSlewLimited();
    TestDeadbandProducesNoCorrection();
    TestPositiveCorrectionStopsAtEightWatts();
    TestLargeOverPowerCanReduceBudgetToZero();
    TestRawHardLimitBypassesFilteredMeasurement();
    TestBrakingCannotCreatePositiveCorrection();
    TestNegativeMeasuredPowerCannotCreatePositiveCorrection();
    TestBrakingSaturationCannotStorePositiveIntegral();
    TestOfflineDropsPositiveCorrectionImmediately();
    TestOfflineReleasesNegativeCorrectionAtPointOneWattPerTick();
    TestNonfiniteMeasurementUsesOfflinePolicy();
    TestBudgetDropClearsPositiveState();
    TestSaturationDoesNotWindIntegralFurther();
    TestErrorCanDriveControllerOutOfSaturation();
    TestCandidateSaturationDoesNotWindBeforeSlewCatchesUp();
    TestInvalidDeltaTimeFailsWithoutPositiveOutput();

    if (failures != 0)
        return 1;

    puts("PASS: chassis power outer-loop tests");
    return 0;
}
