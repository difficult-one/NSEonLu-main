#include "power_model.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define ASSERT_NEAR(expected, actual, tolerance)                                          \
    do                                                                                    \
    {                                                                                     \
        const float expected_ = (expected);                                               \
        const float actual_ = (actual);                                                   \
        if (!isfinite(actual_) || fabsf(expected_ - actual_) > (tolerance))                \
        {                                                                                 \
            printf("FAIL %s:%d: expected %.6f, got %.6f\n",                              \
                   __FILE__, __LINE__, (double)expected_, (double)actual_);                \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

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
        if (expected_ != actual_)                                                         \
        {                                                                                 \
            printf("FAIL %s:%d: expected %d, got %d\n",                                  \
                   __FILE__, __LINE__, expected_, actual_);                               \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

_Static_assert(sizeof(Chassis_Power_Control_Data_s) == 3U * sizeof(float),
               "power feedback must contain exactly three floats");
_Static_assert(offsetof(Chassis_Power_Control_Data_s, predicted_unlimited_power_w) == 0U,
               "unexpected unlimited prediction offset");
_Static_assert(offsetof(Chassis_Power_Control_Data_s, predicted_limited_power_w) == sizeof(float),
               "unexpected limited prediction offset");
_Static_assert(offsetof(Chassis_Power_Control_Data_s, effective_limit_w) == 2U * sizeof(float),
               "unexpected effective limit offset");

static const MotorPowerModelConfig_s m3508_model = {
    .k0 = 1.10671243f,
    .k1 = -0.275458501f,
    .k2 = 0.000465227105f,
    .k3 = 0.00166421676f,
    .k4 = 0.228857816f,
    .k5 = 1.11735923e-7f,
    .current_conversion = 1000.0f,
};

static const ChassisPowerAlgorithmConfig_s algorithm_config = {
    .safety_factor = 0.98f,
    .small_error_threshold_rpm = 500.0f,
    .reserved_power_threshold_w = 54.0f,
    .per_motor_reserved_power_w = 8.0f,
    .max_current_command = 15000.0f,
};

static void FillModels(MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT],
                       MotorPowerModelConfig_s model)
{
    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
        models[i] = model;
}

static ChassisPowerInput_s ValidInput(void)
{
    ChassisPowerInput_s input = {
        .desired_current = {1000.0f, 1000.0f, 1000.0f, 1000.0f},
        .speed_rpm = {1000.0f, 1000.0f, 1000.0f, 1000.0f},
        .speed_error_rpm = {100.0f, 100.0f, 100.0f, 100.0f},
        .referee_power_limit_w = 100.0f,
        .attenuation = 1.0f,
    };
    return input;
}

static float Sum4(const float values[POWER_MODEL_MOTOR_COUNT])
{
    float sum = 0.0f;
    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
        sum += values[i];
    return sum;
}

static void AssertFeedbackIsZero(const Chassis_Power_Control_Data_s *feedback)
{
    ASSERT_NEAR(0.0f, feedback->predicted_unlimited_power_w, 1.0e-6f);
    ASSERT_NEAR(0.0f, feedback->predicted_limited_power_w, 1.0e-6f);
    ASSERT_NEAR(0.0f, feedback->effective_limit_w, 1.0e-6f);
}

static void TestFeedbackBeforeInitFailsSafe(void)
{
    Chassis_Power_Control_Data_s feedback = {1.0f, 2.0f, 3.0f};
    const ChassisPowerOutput_s power = {.status = POWER_MODEL_STATUS_OK};

    ChassisPowerFeedbackUpdate(true, &power);

    ASSERT_FALSE(ChassisPowerFeedbackGet(&feedback));
    AssertFeedbackIsZero(&feedback);
}

static void TestFeedbackInitRejectsDuplicate(void)
{
    ASSERT_TRUE(ChassisPowerFeedbackInit());
    ASSERT_FALSE(ChassisPowerFeedbackInit());
}

static void TestFeedbackSumsValidPowerState(void)
{
    const ChassisPowerOutput_s power = {
        .effective_limit_w = 60.0f,
        .predicted_unlimited_power_w = {10.0f, 20.0f, 30.0f, 40.0f},
        .predicted_limited_power_w = {8.0f, 12.0f, 16.0f, 20.0f},
        .status = POWER_MODEL_STATUS_OK,
    };
    Chassis_Power_Control_Data_s feedback = {0};

    ChassisPowerFeedbackUpdate(true, &power);

    ASSERT_TRUE(ChassisPowerFeedbackGet(&feedback));
    ASSERT_NEAR(100.0f, feedback.predicted_unlimited_power_w, 1.0e-6f);
    ASSERT_NEAR(56.0f, feedback.predicted_limited_power_w, 1.0e-6f);
    ASSERT_NEAR(60.0f, feedback.effective_limit_w, 1.0e-6f);
}

static void AssertFeedbackClearedAfterUpdate(bool registered,
                                             const ChassisPowerOutput_s *power)
{
    Chassis_Power_Control_Data_s feedback = {1.0f, 2.0f, 3.0f};

    ChassisPowerFeedbackUpdate(registered, power);

    ASSERT_TRUE(ChassisPowerFeedbackGet(&feedback));
    AssertFeedbackIsZero(&feedback);
}

static void TestFeedbackClearsInvalidPowerStates(void)
{
    const ChassisPowerOutput_s valid_power = {
        .effective_limit_w = 60.0f,
        .predicted_unlimited_power_w = {10.0f, 20.0f, 30.0f, 40.0f},
        .predicted_limited_power_w = {8.0f, 12.0f, 16.0f, 20.0f},
        .status = POWER_MODEL_STATUS_OK,
    };
    ChassisPowerOutput_s invalid_power = valid_power;
    ChassisPowerOutput_s nonfinite_power = valid_power;
    invalid_power.status = POWER_MODEL_STATUS_INVALID_BUDGET;
    nonfinite_power.predicted_limited_power_w[2] = NAN;

    AssertFeedbackClearedAfterUpdate(false, &valid_power);
    AssertFeedbackClearedAfterUpdate(true, NULL);
    AssertFeedbackClearedAfterUpdate(true, &invalid_power);
    AssertFeedbackClearedAfterUpdate(true, &nonfinite_power);
}

static void TestFeedbackGetRejectsNullOutput(void)
{
    ASSERT_FALSE(ChassisPowerFeedbackGet(NULL));
}

static void TestDisabledBenchFallbackPreservesInvalidLimit(void)
{
    ASSERT_NEAR(0.0f, PowerModelSelectLimit(0.0f, 0.0f), 1.0e-6f);
}

static void TestBenchFallbackReplacesInvalidRefereeLimit(void)
{
    ASSERT_NEAR(40.0f, PowerModelSelectLimit(NAN, 40.0f), 1.0e-6f);
}

static void TestRefereeLimitHasPriorityOverBenchFallback(void)
{
    ASSERT_NEAR(80.0f, PowerModelSelectLimit(80.0f, 40.0f), 1.0e-6f);
}

static void TestInvalidBenchFallbackIsIgnored(void)
{
    ASSERT_NEAR(0.0f, PowerModelSelectLimit(0.0f, -40.0f), 1.0e-6f);
}

static void TestPredictionMatchesRecalibratedM3508Sample(void)
{
    const float actual = PowerModelPredict(&m3508_model, 5000.0f, 3000.0f);
    ASSERT_NEAR(32.81542f, actual, 1.0e-4f);
}

static void TestSmallErrorEvenlySplitsBudget(void)
{
    MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT];
    ChassisPowerInput_s input = ValidInput();
    ChassisPowerOutput_s output;
    FillModels(models, m3508_model);

    ASSERT_TRUE(PowerModelApply(models, &algorithm_config, &input, &output));
    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
        ASSERT_NEAR(24.5f, output.allocated_power_w[i], 1.0e-4f);
    ASSERT_NEAR(98.0f, Sum4(output.allocated_power_w), 1.0e-4f);
}

static void TestLargeErrorUsesReservedPower(void)
{
    MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT];
    ChassisPowerInput_s input = ValidInput();
    ChassisPowerOutput_s output;
    FillModels(models, m3508_model);
    input.speed_error_rpm[0] = 700.0f;

    ASSERT_TRUE(PowerModelApply(models, &algorithm_config, &input, &output));
    ASSERT_NEAR(54.2f, output.allocated_power_w[0], 1.0e-3f);
    ASSERT_NEAR(14.6f, output.allocated_power_w[1], 1.0e-3f);
    ASSERT_NEAR(14.6f, output.allocated_power_w[2], 1.0e-3f);
    ASSERT_NEAR(14.6f, output.allocated_power_w[3], 1.0e-3f);
    ASSERT_NEAR(98.0f, Sum4(output.allocated_power_w), 1.0e-4f);
}

static void TestLowBudgetUsesPureErrorRatio(void)
{
    MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT];
    ChassisPowerInput_s input = ValidInput();
    ChassisPowerOutput_s output;
    FillModels(models, m3508_model);
    input.referee_power_limit_w = 50.0f;
    input.speed_error_rpm[0] = 700.0f;

    ASSERT_TRUE(PowerModelApply(models, &algorithm_config, &input, &output));
    ASSERT_NEAR(34.3f, output.allocated_power_w[0], 1.0e-3f);
    ASSERT_NEAR(4.9f, output.allocated_power_w[1], 1.0e-3f);
    ASSERT_NEAR(49.0f, Sum4(output.allocated_power_w), 1.0e-4f);
}

static void TestNoLimitKeepsCurrent(void)
{
    MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT];
    ChassisPowerInput_s input = ValidInput();
    ChassisPowerOutput_s output;
    const MotorPowerModelConfig_s zero_power_model = {.current_conversion = 1.0f};
    FillModels(models, zero_power_model);

    ASSERT_TRUE(PowerModelApply(models, &algorithm_config, &input, &output));
    ASSERT_NEAR(1000.0f, output.limited_current[0], 1.0e-4f);
    ASSERT_NEAR(1.0f, output.current_scale[0], 1.0e-6f);
}

static void TestQuadraticLimitScalesCurrent(void)
{
    MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT];
    ChassisPowerInput_s input = ValidInput();
    ChassisPowerOutput_s output;
    const MotorPowerModelConfig_s square_model = {
        .k4 = 1.0f,
        .current_conversion = 1.0f,
    };
    const ChassisPowerAlgorithmConfig_s config = {
        .safety_factor = 1.0f,
        .small_error_threshold_rpm = 500.0f,
        .reserved_power_threshold_w = 1000.0f,
        .per_motor_reserved_power_w = 0.0f,
        .max_current_command = 100.0f,
    };
    FillModels(models, square_model);
    input.desired_current[0] = 10.0f;
    input.desired_current[1] = 0.0f;
    input.desired_current[2] = 0.0f;
    input.desired_current[3] = 0.0f;
    input.speed_rpm[0] = 0.0f;
    input.referee_power_limit_w = 100.0f;

    ASSERT_TRUE(PowerModelApply(models, &config, &input, &output));
    ASSERT_NEAR(5.0f, output.limited_current[0], 1.0e-4f);
    ASSERT_NEAR(0.5f, output.current_scale[0], 1.0e-5f);
    ASSERT_NEAR(25.0f, output.predicted_limited_power_w[0], 1.0e-3f);
}

static void TestLinearBranchActuallyScalesCurrent(void)
{
    MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT];
    ChassisPowerInput_s input = ValidInput();
    ChassisPowerOutput_s output;
    const MotorPowerModelConfig_s linear_model = {
        .k1 = 2.0f,
        .current_conversion = 1.0f,
    };
    const ChassisPowerAlgorithmConfig_s config = {
        .safety_factor = 1.0f,
        .small_error_threshold_rpm = 500.0f,
        .reserved_power_threshold_w = 1000.0f,
        .per_motor_reserved_power_w = 0.0f,
        .max_current_command = 100.0f,
    };
    FillModels(models, linear_model);
    input.desired_current[0] = 10.0f;
    input.desired_current[1] = 0.0f;
    input.desired_current[2] = 0.0f;
    input.desired_current[3] = 0.0f;
    input.speed_rpm[0] = 0.0f;
    input.referee_power_limit_w = 40.0f;

    ASSERT_TRUE(PowerModelApply(models, &config, &input, &output));
    ASSERT_NEAR(5.0f, output.limited_current[0], 1.0e-4f);
    ASSERT_NEAR(0.5f, output.current_scale[0], 1.0e-5f);
}

static void TestNegativeDiscriminantZerosCurrent(void)
{
    MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT];
    ChassisPowerInput_s input = ValidInput();
    ChassisPowerOutput_s output;
    const MotorPowerModelConfig_s impossible_model = {
        .k0 = 10.0f,
        .k4 = 1.0f,
        .current_conversion = 1.0f,
    };
    const ChassisPowerAlgorithmConfig_s config = {
        .safety_factor = 1.0f,
        .small_error_threshold_rpm = 500.0f,
        .reserved_power_threshold_w = 1000.0f,
        .per_motor_reserved_power_w = 0.0f,
        .max_current_command = 100.0f,
    };
    FillModels(models, impossible_model);
    input.desired_current[0] = 10.0f;
    input.referee_power_limit_w = 20.0f;

    ASSERT_TRUE(PowerModelApply(models, &config, &input, &output));
    ASSERT_NEAR(0.0f, output.limited_current[0], 1.0e-6f);
    ASSERT_NEAR(0.0f, output.current_scale[0], 1.0e-6f);
}

static void TestCurrentCommandIsHardClamped(void)
{
    MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT];
    ChassisPowerInput_s input = ValidInput();
    ChassisPowerOutput_s output;
    const MotorPowerModelConfig_s zero_power_model = {.current_conversion = 1.0f};
    FillModels(models, zero_power_model);
    input.desired_current[0] = 20000.0f;

    ASSERT_TRUE(PowerModelApply(models, &algorithm_config, &input, &output));
    ASSERT_NEAR(15000.0f, output.limited_current[0], 1.0e-4f);
}

static void TestAttenuationAboveOneIsClamped(void)
{
    MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT];
    ChassisPowerInput_s input = ValidInput();
    ChassisPowerOutput_s output;
    FillModels(models, m3508_model);
    input.attenuation = 2.0f;

    ASSERT_TRUE(PowerModelApply(models, &algorithm_config, &input, &output));
    ASSERT_NEAR(98.0f, output.effective_limit_w, 1.0e-4f);
}

static void TestInvalidBudgetFailsSafe(void)
{
    MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT];
    ChassisPowerInput_s input = ValidInput();
    ChassisPowerOutput_s output;
    FillModels(models, m3508_model);
    input.referee_power_limit_w = 0.0f;

    ASSERT_FALSE(PowerModelApply(models, &algorithm_config, &input, &output));
    ASSERT_EQ_INT(POWER_MODEL_STATUS_INVALID_BUDGET, output.status);
    ASSERT_NEAR(0.0f, Sum4(output.limited_current), 1.0e-6f);
}

static void TestNanInputFailsSafe(void)
{
    MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT];
    ChassisPowerInput_s input = ValidInput();
    ChassisPowerOutput_s output;
    FillModels(models, m3508_model);
    input.desired_current[2] = NAN;

    ASSERT_FALSE(PowerModelApply(models, &algorithm_config, &input, &output));
    ASSERT_EQ_INT(POWER_MODEL_STATUS_NUMERIC_ERROR, output.status);
    ASSERT_NEAR(0.0f, Sum4(output.limited_current), 1.0e-6f);
}

static void TestInvalidConfigurationIsRejected(void)
{
    MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT];
    ChassisPowerInput_s input = ValidInput();
    ChassisPowerOutput_s output;
    FillModels(models, m3508_model);
    models[1].current_conversion = 0.0f;

    ASSERT_FALSE(PowerModelApply(models, &algorithm_config, &input, &output));
    ASSERT_EQ_INT(POWER_MODEL_STATUS_INVALID_CONFIG, output.status);
}

static void TestTelemetryBuildsCoherentSnapshot(void)
{
    ChassisPowerInput_s input = ValidInput();
    ChassisPowerOutput_s power = {.status = POWER_MODEL_STATUS_OK};
    ChassisPowerTelemetry_s telemetry;
    const bool output_enabled[POWER_MODEL_MOTOR_COUNT] = {true, true, true, true};

    input.speed_rpm[0] = 1000.0f;
    input.speed_rpm[1] = 1100.0f;
    input.speed_rpm[2] = 1200.0f;
    input.speed_rpm[3] = 1300.0f;
    input.speed_error_rpm[0] = 10.0f;
    input.speed_error_rpm[1] = 11.0f;
    input.speed_error_rpm[2] = 12.0f;
    input.speed_error_rpm[3] = 13.0f;
    power.limited_current[0] = 2000.0f;
    power.limited_current[1] = 2100.0f;
    power.limited_current[2] = 2200.0f;
    power.limited_current[3] = 2300.0f;
    power.predicted_limited_power_w[0] = 10.0f;
    power.predicted_limited_power_w[1] = 11.0f;
    power.predicted_limited_power_w[2] = 12.0f;
    power.predicted_limited_power_w[3] = 13.0f;

    ASSERT_TRUE(ChassisPowerTelemetryBuild(&input, &power, output_enabled, &telemetry));
    ASSERT_NEAR(46.0f, telemetry.predicted_total_power_w, 1.0e-6f);
    ASSERT_EQ_INT(0x0F, telemetry.motor_enabled_mask);
    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
    {
        ASSERT_NEAR(power.limited_current[i], telemetry.current_command[i], 1.0e-6f);
        ASSERT_NEAR(input.speed_rpm[i], telemetry.actual_speed_rpm[i], 1.0e-6f);
        ASSERT_NEAR(input.speed_rpm[i] + input.speed_error_rpm[i],
                    telemetry.target_speed_rpm[i],
                    1.0e-6f);
        ASSERT_NEAR(power.predicted_limited_power_w[i],
                    telemetry.predicted_power_w[i],
                    1.0e-6f);
    }
}

static void TestTelemetryRejectsInvalidPowerAndClearsOutput(void)
{
    ChassisPowerInput_s input = ValidInput();
    ChassisPowerOutput_s power = {.status = POWER_MODEL_STATUS_INVALID_BUDGET};
    ChassisPowerTelemetry_s telemetry;
    const bool output_enabled[POWER_MODEL_MOTOR_COUNT] = {true, true, true, true};
    memset(&telemetry, 0xA5, sizeof(telemetry));

    ASSERT_FALSE(ChassisPowerTelemetryBuild(&input, &power, output_enabled, &telemetry));
    ASSERT_EQ_INT(0, telemetry.motor_enabled_mask);
    const unsigned char *bytes = (const unsigned char *)&telemetry;
    for (size_t i = 0; i < sizeof(telemetry); ++i)
        ASSERT_TRUE(bytes[i] == 0U);
}

static void TestTelemetryClearsCommandsDisabledByFinalOutputGate(void)
{
    ChassisPowerInput_s input = ValidInput();
    ChassisPowerOutput_s power = {.status = POWER_MODEL_STATUS_OK};
    ChassisPowerTelemetry_s telemetry;
    const bool output_enabled[POWER_MODEL_MOTOR_COUNT] = {true, false, true, true};

    power.limited_current[0] = 2000.0f;
    power.limited_current[1] = 2100.0f;
    power.limited_current[2] = 2200.0f;
    power.limited_current[3] = 2300.0f;
    power.predicted_limited_power_w[0] = 10.0f;
    power.predicted_limited_power_w[1] = 11.0f;
    power.predicted_limited_power_w[2] = 12.0f;
    power.predicted_limited_power_w[3] = 13.0f;

    ASSERT_TRUE(ChassisPowerTelemetryBuild(&input, &power, output_enabled, &telemetry));
    ASSERT_NEAR(0.0f, telemetry.current_command[1], 1.0e-6f);
    ASSERT_NEAR(0.0f, telemetry.predicted_power_w[1], 1.0e-6f);
    ASSERT_NEAR(35.0f, telemetry.predicted_total_power_w, 1.0e-6f);
    ASSERT_EQ_INT(0x0D, telemetry.motor_enabled_mask);
}

static void TestTelemetryBuildsZeroMaskWhenAllOutputsAreDisabled(void)
{
    ChassisPowerInput_s input = ValidInput();
    ChassisPowerOutput_s power = {.status = POWER_MODEL_STATUS_OK};
    ChassisPowerTelemetry_s telemetry;
    const bool output_enabled[POWER_MODEL_MOTOR_COUNT] = {false, false, false, false};

    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
    {
        power.limited_current[i] = 1000.0f + (float)i;
        power.predicted_limited_power_w[i] = 10.0f + (float)i;
    }

    ASSERT_TRUE(ChassisPowerTelemetryBuild(&input, &power, output_enabled, &telemetry));
    ASSERT_EQ_INT(0, telemetry.motor_enabled_mask);
    ASSERT_NEAR(0.0f, telemetry.predicted_total_power_w, 1.0e-6f);
    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
        ASSERT_NEAR(0.0f, telemetry.current_command[i], 1.0e-6f);
}

int main(void)
{
    {
        float scale;
        /* (x-.2)(x-.8)<=0: x=0 is infeasible, largest solution .8. */
        ASSERT_TRUE(PowerModelFindFeasibleScale(1, -1, .16f, 1, &scale));
        ASSERT_NEAR(.8f, scale, 1e-5f);
        ASSERT_TRUE(PowerModelFindFeasibleScale(1, -1, .16f, .5f, &scale));
        ASSERT_NEAR(.5f, scale, 1e-5f);
        ASSERT_FALSE(PowerModelFindFeasibleScale(1, -1, .16f, .1f, &scale));
        ASSERT_TRUE(PowerModelFindFeasibleScale(-1, 1, -.16f, .5f, &scale));
        ASSERT_NEAR(.2f, scale, 1e-5f);
        ASSERT_TRUE(PowerModelFindFeasibleScale(0, -1, .2f, 1, &scale));
        ASSERT_NEAR(1, scale, 1e-6f);
        ASSERT_FALSE(PowerModelFindFeasibleScale(0, 0, 2, 1, &scale));
        ASSERT_FALSE(PowerModelFindFeasibleScale(NAN, 0, 0, 1, &scale));
    }
    TestFeedbackBeforeInitFailsSafe();
    TestFeedbackInitRejectsDuplicate();
    TestFeedbackSumsValidPowerState();
    TestFeedbackClearsInvalidPowerStates();
    TestFeedbackGetRejectsNullOutput();
    TestDisabledBenchFallbackPreservesInvalidLimit();
    TestBenchFallbackReplacesInvalidRefereeLimit();
    TestRefereeLimitHasPriorityOverBenchFallback();
    TestInvalidBenchFallbackIsIgnored();
    TestPredictionMatchesRecalibratedM3508Sample();
    TestSmallErrorEvenlySplitsBudget();
    TestLargeErrorUsesReservedPower();
    TestLowBudgetUsesPureErrorRatio();
    TestNoLimitKeepsCurrent();
    TestQuadraticLimitScalesCurrent();
    TestLinearBranchActuallyScalesCurrent();
    TestNegativeDiscriminantZerosCurrent();
    TestCurrentCommandIsHardClamped();
    TestAttenuationAboveOneIsClamped();
    TestInvalidBudgetFailsSafe();
    TestNanInputFailsSafe();
    TestInvalidConfigurationIsRejected();
    TestTelemetryBuildsCoherentSnapshot();
    TestTelemetryRejectsInvalidPowerAndClearsOutput();
    TestTelemetryClearsCommandsDisabledByFinalOutputGate();
    TestTelemetryBuildsZeroMaskWhenAllOutputsAreDisabled();
    if (failures != 0)
        return 1;

    puts("PASS: power model tests");
    return 0;
}
