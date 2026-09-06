#include "power_model.h"

#include <math.h>
#include <stdio.h>

static int failures;

#define ASSERT_NEAR(expected, actual, tolerance)                                          \
    do                                                                                    \
    {                                                                                     \
        const float expected_ = (expected);                                               \
        const float actual_ = (actual);                                                   \
        if (fabsf(expected_ - actual_) > (tolerance))                                     \
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

static const MotorPowerModelConfig_s m3508_model = {
    .k0 = 0.65213f,
    .k1 = -0.15659f,
    .k2 = 0.00041660f,
    .k3 = 0.00235415f,
    .k4 = 0.20022f,
    .k5 = 1.08e-7f,
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

static void TestPredictionMatchesPolynomial(void)
{
    const float actual = PowerModelPredict(&m3508_model, 5000.0f, 3000.0f);
    const float expected = 0.65213f - 0.15659f * 5.0f
                         + 0.00041660f * 3000.0f
                         + 0.00235415f * 5.0f * 3000.0f
                         + 0.20022f * 25.0f
                         + 1.08e-7f * 9000000.0f;
    ASSERT_NEAR(expected, actual, 1.0e-4f);
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

int main(void)
{
    TestDisabledBenchFallbackPreservesInvalidLimit();
    TestBenchFallbackReplacesInvalidRefereeLimit();
    TestRefereeLimitHasPriorityOverBenchFallback();
    TestInvalidBenchFallbackIsIgnored();
    TestPredictionMatchesPolynomial();
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
    if (failures != 0)
        return 1;

    puts("PASS: power model tests");
    return 0;
}
