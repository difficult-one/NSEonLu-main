#include "chassis_power_link.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
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

#define ASSERT_EQ_U32(expected, actual)                                                    \
    do                                                                                    \
    {                                                                                     \
        const uint32_t expected_ = (uint32_t)(expected);                                  \
        const uint32_t actual_ = (uint32_t)(actual);                                      \
        if (expected_ != actual_)                                                         \
        {                                                                                 \
            printf("FAIL %s:%d: expected %lu, got %lu\n",                               \
                   __FILE__, __LINE__,                                                     \
                   (unsigned long)expected_, (unsigned long)actual_);                      \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

#define ASSERT_NEAR(expected, actual)                                                      \
    do                                                                                    \
    {                                                                                     \
        const float expected_ = (expected);                                                \
        const float actual_ = (actual);                                                    \
        if (!isfinite(actual_) || fabsf(expected_ - actual_) > 0.001f)                     \
        {                                                                                 \
            printf("FAIL %s:%d: expected %.6f, got %.6f\n",                             \
                   __FILE__, __LINE__, (double)expected_, (double)actual_);                \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

static void TestValidRefereeSnapshotDrivesCommandAndBudget(void)
{
    const ChassisPowerLinkInput_s input = {
        .referee = {
            .valid = true,
            .chassis_output_enabled = true,
            .chassis_power_limit_w = 120U,
            .buffer_energy_j = 55U,
        },
        .bench_enabled = false,
        .bench_limit_w = 0.0f,
        .chassis_zero_force = false,
    };
    ChassisPowerLinkOutput_s output;

    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_TRUE(output.command.enable_dcdc);
    ASSERT_EQ_U32(120U, output.command.referee_power_limit_w);
    ASSERT_EQ_U32(55U, output.command.referee_buffer_energy_j);
    ASSERT_NEAR(120.0f, output.base_power_limit_w);
}

static void TestInvalidSnapshotFailsSafeInProduction(void)
{
    const ChassisPowerLinkInput_s input = {
        .bench_enabled = false,
        .bench_limit_w = 60.0f,
    };
    ChassisPowerLinkOutput_s output;

    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_FALSE(output.command.enable_dcdc);
    ASSERT_EQ_U32(0U, output.command.referee_power_limit_w);
    ASSERT_EQ_U32(0U, output.command.referee_buffer_energy_j);
    ASSERT_NEAR(0.0f, output.base_power_limit_w);
}

static void TestRefereeOutputPermissionAndZeroForceDisableDcdc(void)
{
    ChassisPowerLinkInput_s input = {
        .referee = {
            .valid = true,
            .chassis_output_enabled = false,
            .chassis_power_limit_w = 100U,
            .buffer_energy_j = 40U,
        },
    };
    ChassisPowerLinkOutput_s output;

    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_FALSE(output.command.enable_dcdc);
    ASSERT_EQ_U32(100U, output.command.referee_power_limit_w);
    ASSERT_EQ_U32(40U, output.command.referee_buffer_energy_j);
    ASSERT_NEAR(100.0f, output.base_power_limit_w);

    input.referee.chassis_output_enabled = true;
    input.chassis_zero_force = true;
    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_FALSE(output.command.enable_dcdc);
    ASSERT_EQ_U32(100U, output.command.referee_power_limit_w);
    ASSERT_EQ_U32(40U, output.command.referee_buffer_energy_j);
    ASSERT_NEAR(100.0f, output.base_power_limit_w);
}

static void TestZeroRefereeLimitDisablesDcdcButPreservesSnapshot(void)
{
    const ChassisPowerLinkInput_s input = {
        .referee = {
            .valid = true,
            .chassis_output_enabled = true,
            .chassis_power_limit_w = 0U,
            .buffer_energy_j = 35U,
        },
        .bench_enabled = true,
        .bench_limit_w = 60.0f,
    };
    ChassisPowerLinkOutput_s output;

    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_FALSE(output.command.enable_dcdc);
    ASSERT_EQ_U32(0U, output.command.referee_power_limit_w);
    ASSERT_EQ_U32(35U, output.command.referee_buffer_energy_j);
    ASSERT_NEAR(0.0f, output.base_power_limit_w);
}

static void TestExplicitBenchModeUsesLimitButZeroBuffer(void)
{
    const ChassisPowerLinkInput_s input = {
        .bench_enabled = true,
        .bench_limit_w = 60.0f,
        .chassis_zero_force = false,
    };
    ChassisPowerLinkOutput_s output;

    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_TRUE(output.command.enable_dcdc);
    ASSERT_EQ_U32(60U, output.command.referee_power_limit_w);
    ASSERT_EQ_U32(0U, output.command.referee_buffer_energy_j);
    ASSERT_NEAR(60.0f, output.base_power_limit_w);
}

static void TestValidRefereeSnapshotTakesPriorityOverBenchLimit(void)
{
    const ChassisPowerLinkInput_s input = {
        .referee = {
            .valid = true,
            .chassis_output_enabled = true,
            .chassis_power_limit_w = 90U,
            .buffer_energy_j = 25U,
        },
        .bench_enabled = true,
        .bench_limit_w = 200.0f,
    };
    ChassisPowerLinkOutput_s output;

    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_TRUE(output.command.enable_dcdc);
    ASSERT_EQ_U32(90U, output.command.referee_power_limit_w);
    ASSERT_EQ_U32(25U, output.command.referee_buffer_energy_j);
    ASSERT_NEAR(90.0f, output.base_power_limit_w);
}

static void TestBenchLimitRoundsAndSaturatesForCommand(void)
{
    ChassisPowerLinkInput_s input = {
        .bench_enabled = true,
        .bench_limit_w = 60.5f,
    };
    ChassisPowerLinkOutput_s output;

    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_EQ_U32(61U, output.command.referee_power_limit_w);
    ASSERT_NEAR(60.5f, output.base_power_limit_w);

    input.bench_limit_w = 70000.0f;
    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_EQ_U32(UINT16_MAX, output.command.referee_power_limit_w);
    ASSERT_NEAR(70000.0f, output.base_power_limit_w);
}

static void TestInvalidBenchLimitsDoNotEnableFallback(void)
{
    const float invalid_limits[] = {0.0f, -1.0f, NAN, INFINITY};

    for (size_t i = 0U; i < sizeof(invalid_limits) / sizeof(invalid_limits[0]); ++i)
    {
        const ChassisPowerLinkInput_s input = {
            .bench_enabled = true,
            .bench_limit_w = invalid_limits[i],
        };
        ChassisPowerLinkOutput_s output;

        ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
        ASSERT_FALSE(output.command.enable_dcdc);
        ASSERT_EQ_U32(0U, output.command.referee_power_limit_w);
        ASSERT_EQ_U32(0U, output.command.referee_buffer_energy_j);
        ASSERT_NEAR(0.0f, output.base_power_limit_w);
    }
}

static void TestZeroForceDisablesBenchDcdc(void)
{
    const ChassisPowerLinkInput_s input = {
        .bench_enabled = true,
        .bench_limit_w = 60.0f,
        .chassis_zero_force = true,
    };
    ChassisPowerLinkOutput_s output;

    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_FALSE(output.command.enable_dcdc);
    ASSERT_EQ_U32(60U, output.command.referee_power_limit_w);
    ASSERT_NEAR(60.0f, output.base_power_limit_w);
}

static void TestValidSnapshotEncodesExistingCanLayout(void)
{
    const ChassisPowerLinkInput_s input = {
        .referee = {
            .valid = true,
            .chassis_output_enabled = true,
            .chassis_power_limit_w = 100U,
            .buffer_energy_j = 50U,
        },
    };
    const uint8_t expected[8] = {
        0x01U, 0x64U, 0x00U, 0x32U, 0x00U, 0x00U, 0x00U, 0x00U,
    };
    uint8_t actual[8] = {0};
    ChassisPowerLinkOutput_s output;

    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_TRUE(SuperCapProtocolEncodeCommand(&output.command, actual));
    for (size_t i = 0U; i < sizeof(expected); ++i)
        ASSERT_EQ_U32(expected[i], actual[i]);
}

static void TestRejectsNullPointers(void)
{
    const ChassisPowerLinkInput_s input = {0};
    ChassisPowerLinkOutput_s output;

    ASSERT_FALSE(ChassisPowerLinkBuild(NULL, &output));
    ASSERT_FALSE(ChassisPowerLinkBuild(&input, NULL));
}

int main(void)
{
    const ChassisPowerLinkInput_s disabled_partial = {
        .bench_enabled = true, .bench_limit_w = 50,
        .referee = {.valid = false, .chassis_output_valid = true,
                    .chassis_output_enabled = false},
    };
    ChassisPowerLinkOutput_s disabled_output;
    ASSERT_TRUE(ChassisPowerLinkBuild(&disabled_partial, &disabled_output));
    ASSERT_FALSE(disabled_output.command.enable_dcdc);
    TestValidRefereeSnapshotDrivesCommandAndBudget();
    TestInvalidSnapshotFailsSafeInProduction();
    TestRefereeOutputPermissionAndZeroForceDisableDcdc();
    TestZeroRefereeLimitDisablesDcdcButPreservesSnapshot();
    TestExplicitBenchModeUsesLimitButZeroBuffer();
    TestValidRefereeSnapshotTakesPriorityOverBenchLimit();
    TestBenchLimitRoundsAndSaturatesForCommand();
    TestInvalidBenchLimitsDoNotEnableFallback();
    TestZeroForceDisablesBenchDcdc();
    TestValidSnapshotEncodesExistingCanLayout();
    TestRejectsNullPointers();

    if (failures != 0)
        return 1;

    puts("PASS: chassis power link tests");
    return 0;
}
