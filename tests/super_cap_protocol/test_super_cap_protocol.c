#include "super_cap_protocol.h"

#include <math.h>
#include <stddef.h>
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

static void AssertBytes(const uint8_t *expected, const uint8_t *actual, size_t length)
{
    for (size_t i = 0; i < length; ++i)
        ASSERT_EQ_INT(expected[i], actual[i]);
}

static void TestEncodeKnownCommand(void)
{
    const SuperCapCommand_s command = {
        .enable_dcdc = true,
        .referee_power_limit_w = 100U,
        .referee_buffer_energy_j = 50U,
    };
    const uint8_t expected[8] = {0x01, 0x64, 0x00, 0x32, 0x00, 0, 0, 0};
    uint8_t actual[8] = {0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5};

    ASSERT_TRUE(SuperCapProtocolEncodeCommand(&command, actual));
    AssertBytes(expected, actual, 8U);
}

static void TestEncodeDisabledCommandClearsFlagsAndReservedBytes(void)
{
    const SuperCapCommand_s command = {
        .enable_dcdc = false,
        .referee_power_limit_w = 0x1234U,
        .referee_buffer_energy_j = 0x5678U,
    };
    const uint8_t expected[8] = {0x00, 0x34, 0x12, 0x78, 0x56, 0, 0, 0};
    uint8_t actual[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    ASSERT_TRUE(SuperCapProtocolEncodeCommand(&command, actual));
    AssertBytes(expected, actual, 8U);
}

static void TestEncodeRejectsNullPointers(void)
{
    const SuperCapCommand_s command = {0};
    uint8_t frame[8] = {0};

    ASSERT_FALSE(SuperCapProtocolEncodeCommand(NULL, frame));
    ASSERT_FALSE(SuperCapProtocolEncodeCommand(&command, NULL));
}

static void TestDecodeKnownStatus(void)
{
    const uint8_t frame[8] = {0x00, 0x00, 0x00, 0xF1, 0x42, 0xB4, 0x00, 0x80};
    SuperCapStatus_s status = {0};

    ASSERT_TRUE(SuperCapProtocolDecodeStatus(frame, 8U, &status));
    ASSERT_TRUE(status.online);
    ASSERT_TRUE(status.output_enabled);
    ASSERT_EQ_INT(0, status.error_code);
    ASSERT_NEAR(120.5f, status.chassis_power_w, 0.001f);
    ASSERT_EQ_INT(180, status.available_power_limit_w);
    ASSERT_NEAR(128.0f / 255.0f, status.energy_ratio, 0.0001f);
}

static void TestDecodeDisabledAndErrors(void)
{
    const uint8_t frame[8] = {0xC5, 0, 0, 0, 0, 0x64, 0, 0};
    SuperCapStatus_s status = {0};

    ASSERT_TRUE(SuperCapProtocolDecodeStatus(frame, 8U, &status));
    ASSERT_FALSE(status.output_enabled);
    ASSERT_EQ_INT(0x45, status.error_code);
}

static void TestDecodeAcceptsNegativeFinitePower(void)
{
    const uint8_t frame[8] = {0x00, 0x00, 0x00, 0x80, 0xBF, 0, 0, 0};
    SuperCapStatus_s status = {0};

    ASSERT_TRUE(SuperCapProtocolDecodeStatus(frame, 8U, &status));
    ASSERT_NEAR(-1.0f, status.chassis_power_w, 0.0001f);
}

static void TestDecodeRejectsBadLengthNanAndNullPointers(void)
{
    const uint8_t finite_frame[8] = {0};
    const uint8_t nan_frame[8] = {0, 0, 0, 0xC0, 0x7F, 0, 0, 0};
    SuperCapStatus_s status = {0};

    ASSERT_FALSE(SuperCapProtocolDecodeStatus(finite_frame, 7U, &status));
    ASSERT_FALSE(SuperCapProtocolDecodeStatus(nan_frame, 8U, &status));
    ASSERT_FALSE(SuperCapProtocolDecodeStatus(NULL, 8U, &status));
    ASSERT_FALSE(SuperCapProtocolDecodeStatus(finite_frame, 8U, NULL));
}

int main(void)
{
    TestEncodeKnownCommand();
    TestEncodeDisabledCommandClearsFlagsAndReservedBytes();
    TestEncodeRejectsNullPointers();
    TestDecodeKnownStatus();
    TestDecodeDisabledAndErrors();
    TestDecodeAcceptsNegativeFinitePower();
    TestDecodeRejectsBadLengthNanAndNullPointers();

    if (failures != 0)
        return 1;

    puts("PASS: super capacitor protocol tests");
    return 0;
}
