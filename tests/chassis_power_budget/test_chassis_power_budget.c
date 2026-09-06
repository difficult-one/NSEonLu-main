#include "chassis_power_budget.h"

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
        if (fabsf(expected_ - actual_) > (tolerance))                                      \
        {                                                                                 \
            printf("FAIL %s:%d: expected %.6f, got %.6f\n",                              \
                   __FILE__, __LINE__, (double)expected_, (double)actual_);                \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

static const ChassisPowerBudgetConfig_s valid_config = {
    .empty_energy_ratio = 0.10f,
    .full_energy_ratio = 0.30f,
    .max_boost_w = 100.0f,
    .rise_rate_w_per_s = 200.0f,
};

static ChassisPowerBudgetInput_s ReadyInput(float energy_ratio)
{
    const ChassisPowerBudgetInput_s input = {
        .referee_limit_w = 100.0f,
        .cap_online = true,
        .cap_output_enabled = true,
        .cap_error_code = 0,
        .cap_energy_ratio = energy_ratio,
        .cap_reported_limit_w = 180.0f,
    };
    return input;
}

static ChassisPowerBudgetOutput_s UpdateFrom(float initial_budget_w,
                                              ChassisPowerBudgetInput_s input,
                                              float dt_s)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetOutput_s output = {0};
    ChassisPowerBudgetReset(&state, initial_budget_w);
    ASSERT_TRUE(ChassisPowerBudgetUpdate(&valid_config, &input, dt_s, &state, &output));
    return output;
}

static void TestEnergyBreakpoints(void)
{
    ChassisPowerBudgetOutput_s output = UpdateFrom(100.0f, ReadyInput(0.05f), 1.0f);
    ASSERT_EQ_INT(CHASSIS_POWER_BUDGET_DEGRADED, output.mode);
    ASSERT_NEAR(0.0f, output.energy_factor, 0.0001f);
    ASSERT_NEAR(100.0f, output.target_budget_w, 0.0001f);
    ASSERT_NEAR(100.0f, output.applied_budget_w, 0.0001f);

    output = UpdateFrom(100.0f, ReadyInput(0.20f), 1.0f);
    ASSERT_EQ_INT(CHASSIS_POWER_BUDGET_READY, output.mode);
    ASSERT_NEAR(0.5f, output.energy_factor, 0.0001f);
    ASSERT_NEAR(140.0f, output.target_budget_w, 0.0001f);
    ASSERT_NEAR(140.0f, output.applied_budget_w, 0.0001f);

    output = UpdateFrom(100.0f, ReadyInput(0.50f), 1.0f);
    ASSERT_EQ_INT(CHASSIS_POWER_BUDGET_READY, output.mode);
    ASSERT_NEAR(1.0f, output.energy_factor, 0.0001f);
    ASSERT_NEAR(180.0f, output.target_budget_w, 0.0001f);
    ASSERT_NEAR(180.0f, output.applied_budget_w, 0.0001f);
}

static void TestBoostCeiling(void)
{
    ChassisPowerBudgetInput_s input = ReadyInput(1.0f);
    input.cap_reported_limit_w = 400.0f;
    const ChassisPowerBudgetOutput_s output = UpdateFrom(100.0f, input, 1.0f);
    ASSERT_NEAR(200.0f, output.target_budget_w, 0.0001f);
    ASSERT_NEAR(200.0f, output.applied_budget_w, 0.0001f);
}

static void TestRiseIsSlewLimited(void)
{
    const ChassisPowerBudgetOutput_s output =
        UpdateFrom(100.0f, ReadyInput(1.0f), 0.005f);
    ASSERT_NEAR(101.0f, output.applied_budget_w, 0.0001f);
}

static void TestFallbackIsImmediate(void)
{
    ChassisPowerBudgetInput_s input = ReadyInput(1.0f);
    input.cap_online = false;
    ChassisPowerBudgetOutput_s output = UpdateFrom(180.0f, input, 0.005f);
    ASSERT_EQ_INT(CHASSIS_POWER_BUDGET_OFFLINE, output.mode);
    ASSERT_NEAR(100.0f, output.applied_budget_w, 0.0001f);

    input = ReadyInput(1.0f);
    input.cap_output_enabled = false;
    output = UpdateFrom(180.0f, input, 0.005f);
    ASSERT_EQ_INT(CHASSIS_POWER_BUDGET_DEGRADED, output.mode);
    ASSERT_NEAR(100.0f, output.applied_budget_w, 0.0001f);
}

static void TestEveryFaultRemovesBoost(void)
{
    const uint8_t faults[] = {0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x20U, 0x40U};
    for (size_t i = 0; i < sizeof(faults) / sizeof(faults[0]); ++i)
    {
        ChassisPowerBudgetInput_s input = ReadyInput(1.0f);
        input.cap_error_code = faults[i];
        const ChassisPowerBudgetOutput_s output = UpdateFrom(180.0f, input, 0.005f);
        ASSERT_EQ_INT(CHASSIS_POWER_BUDGET_DEGRADED, output.mode);
        ASSERT_NEAR(100.0f, output.applied_budget_w, 0.0001f);
    }
}

static void TestInvalidCapFieldsDegradeSafely(void)
{
    ChassisPowerBudgetInput_s input = ReadyInput(NAN);
    ChassisPowerBudgetOutput_s output = UpdateFrom(180.0f, input, 0.005f);
    ASSERT_EQ_INT(CHASSIS_POWER_BUDGET_DEGRADED, output.mode);
    ASSERT_NEAR(100.0f, output.applied_budget_w, 0.0001f);

    input = ReadyInput(1.1f);
    output = UpdateFrom(180.0f, input, 0.005f);
    ASSERT_EQ_INT(CHASSIS_POWER_BUDGET_DEGRADED, output.mode);

    input = ReadyInput(1.0f);
    input.cap_reported_limit_w = -1.0f;
    output = UpdateFrom(180.0f, input, 0.005f);
    ASSERT_EQ_INT(CHASSIS_POWER_BUDGET_DEGRADED, output.mode);
}

static void AssertInvalidBaseInputFails(ChassisPowerBudgetInput_s input, float dt_s)
{
    ChassisPowerBudgetState_s state = {
        .applied_budget_w = 180.0f,
        .mode = CHASSIS_POWER_BUDGET_READY,
    };
    ChassisPowerBudgetOutput_s output = {
        .energy_factor = 1.0f,
        .target_budget_w = 180.0f,
        .applied_budget_w = 180.0f,
        .mode = CHASSIS_POWER_BUDGET_READY,
    };

    ASSERT_FALSE(ChassisPowerBudgetUpdate(&valid_config, &input, dt_s, &state, &output));
    ASSERT_NEAR(0.0f, state.applied_budget_w, 0.0001f);
    ASSERT_EQ_INT(CHASSIS_POWER_BUDGET_OFFLINE, state.mode);
    ASSERT_NEAR(0.0f, output.applied_budget_w, 0.0001f);
    ASSERT_EQ_INT(CHASSIS_POWER_BUDGET_OFFLINE, output.mode);
}

static void TestInvalidBaseInputsFailSafe(void)
{
    ChassisPowerBudgetInput_s input = ReadyInput(1.0f);
    input.referee_limit_w = 0.0f;
    AssertInvalidBaseInputFails(input, 0.005f);
    input.referee_limit_w = -1.0f;
    AssertInvalidBaseInputFails(input, 0.005f);
    input.referee_limit_w = NAN;
    AssertInvalidBaseInputFails(input, 0.005f);

    input = ReadyInput(1.0f);
    AssertInvalidBaseInputFails(input, -1.0f);
    AssertInvalidBaseInputFails(input, NAN);
}

static void TestResetNormalizesInvalidValue(void)
{
    ChassisPowerBudgetState_s state;
    ChassisPowerBudgetReset(&state, NAN);
    ASSERT_NEAR(0.0f, state.applied_budget_w, 0.0001f);
    ASSERT_EQ_INT(CHASSIS_POWER_BUDGET_OFFLINE, state.mode);
}

int main(void)
{
    TestEnergyBreakpoints();
    TestBoostCeiling();
    TestRiseIsSlewLimited();
    TestFallbackIsImmediate();
    TestEveryFaultRemovesBoost();
    TestInvalidCapFieldsDegradeSafely();
    TestInvalidBaseInputsFailSafe();
    TestResetNormalizesInvalidValue();

    if (failures != 0)
        return 1;

    puts("PASS: chassis power budget tests");
    return 0;
}
