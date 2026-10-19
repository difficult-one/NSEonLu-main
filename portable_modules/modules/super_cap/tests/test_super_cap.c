#include "super_cap.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;
static uint32_t fake_now_ms;
static bool fake_send_result;
static uint32_t sent_id;
static uint8_t sent_data[SUPER_CAP_FRAME_DLC];
static uint8_t sent_dlc;
static unsigned int send_calls;
static unsigned int enter_calls;
static unsigned int exit_calls;
static unsigned int critical_depth;
static bool send_called_in_critical;
static int callback_context;
static void *send_context;
static void *clock_context;

#define ASSERT_TRUE(condition)                                                          \
    do                                                                                  \
    {                                                                                   \
        if (!(condition))                                                               \
        {                                                                               \
            printf("FAIL %s:%d: expected true: %s\n", __FILE__, __LINE__, #condition); \
            ++failures;                                                                 \
        }                                                                               \
    } while (0)

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_EQ_U32(expected, actual)                                                  \
    do                                                                                  \
    {                                                                                   \
        const uint32_t expected_ = (uint32_t)(expected);                                 \
        const uint32_t actual_ = (uint32_t)(actual);                                     \
        if (expected_ != actual_)                                                        \
        {                                                                               \
            printf("FAIL %s:%d: expected %lu, got %lu\n", __FILE__, __LINE__,          \
                   (unsigned long)expected_, (unsigned long)actual_);                    \
            ++failures;                                                                 \
        }                                                                               \
    } while (0)

#define ASSERT_NEAR(expected, actual, tolerance)                                         \
    do                                                                                  \
    {                                                                                   \
        const float expected_ = (expected);                                              \
        const float actual_ = (actual);                                                  \
        if (!isfinite(expected_) || !isfinite(actual_) ||                                \
            !isfinite(fabsf(expected_ - actual_)) ||                                     \
            fabsf(expected_ - actual_) > (tolerance))                                    \
        {                                                                               \
            printf("FAIL %s:%d: expected %.6f, got %.6f\n", __FILE__, __LINE__,        \
                   (double)expected_, (double)actual_);                                  \
            ++failures;                                                                 \
        }                                                                               \
    } while (0)

static void AssertBytes(const uint8_t *expected, const uint8_t *actual, size_t length)
{
    size_t index;

    for (index = 0U; index < length; ++index)
        ASSERT_EQ_U32(expected[index], actual[index]);
}

static uint32_t FakeNow(void *context)
{
    clock_context = context;
    return fake_now_ms;
}

static bool FakeSend(void *context,
                     uint32_t standard_id,
                     const uint8_t *data,
                     uint8_t dlc)
{
    send_context = context;
    sent_id = standard_id;
    sent_dlc = dlc;
    ++send_calls;
    if (critical_depth != 0U)
        send_called_in_critical = true;
    if (data != NULL && dlc <= SUPER_CAP_FRAME_DLC)
        memcpy(sent_data, data, dlc);
    return fake_send_result;
}

static void EnterCritical(void *context)
{
    ASSERT_TRUE(context == &callback_context);
    ++enter_calls;
    ++critical_depth;
}

static void ExitCritical(void *context)
{
    ASSERT_TRUE(context == &callback_context);
    ASSERT_TRUE(critical_depth > 0U);
    ++exit_calls;
    --critical_depth;
}

static void ResetFakes(void)
{
    fake_now_ms = 0U;
    fake_send_result = true;
    sent_id = 0U;
    memset(sent_data, 0xA5, sizeof(sent_data));
    sent_dlc = 0U;
    send_calls = 0U;
    enter_calls = 0U;
    exit_calls = 0U;
    critical_depth = 0U;
    send_called_in_critical = false;
    send_context = NULL;
    clock_context = NULL;
}

static SuperCapConfig DefaultConfig(void)
{
    const SuperCapConfig config = {
        .can_send = FakeSend,
        .now_ms = FakeNow,
        .enter_critical = NULL,
        .exit_critical = NULL,
        .context = &callback_context,
        .command_id = 0U,
        .status_id = 0U,
        .offline_timeout_ms = 0U,
    };
    return config;
}

static void TestInitValidatesCallbacksCriticalPairAndStandardIds(void)
{
    SuperCap instance;
    SuperCapConfig config;

    ResetFakes();
    config = DefaultConfig();
    ASSERT_FALSE(SuperCapInit(NULL, &config));
    ASSERT_FALSE(SuperCapInit(&instance, NULL));

    config = DefaultConfig();
    config.can_send = NULL;
    ASSERT_FALSE(SuperCapInit(&instance, &config));

    config = DefaultConfig();
    config.now_ms = NULL;
    ASSERT_FALSE(SuperCapInit(&instance, &config));

    config = DefaultConfig();
    config.enter_critical = EnterCritical;
    ASSERT_FALSE(SuperCapInit(&instance, &config));

    config = DefaultConfig();
    config.exit_critical = ExitCritical;
    ASSERT_FALSE(SuperCapInit(&instance, &config));

    config = DefaultConfig();
    config.command_id = 0x800U;
    ASSERT_FALSE(SuperCapInit(&instance, &config));

    config = DefaultConfig();
    config.status_id = 0x800U;
    ASSERT_FALSE(SuperCapInit(&instance, &config));

    config = DefaultConfig();
    ASSERT_TRUE(SuperCapInit(&instance, &config));
}

static void TestDefaultIdsAndExactCommandBytes(void)
{
    static const uint8_t expected_command[SUPER_CAP_FRAME_DLC] = {
        0x01U, 0x64U, 0x00U, 0x32U, 0x00U, 0x00U, 0x00U, 0x00U
    };
    const SuperCapCommand command = {
        .enable_dcdc = true,
        .referee_power_limit_w = 100U,
        .referee_buffer_energy_j = 50U,
    };
    SuperCap instance;
    SuperCapConfig config;

    ResetFakes();
    config = DefaultConfig();
    ASSERT_TRUE(SuperCapInit(&instance, &config));
    ASSERT_TRUE(SuperCapSendCommand(&instance, &command));
    ASSERT_EQ_U32(1U, send_calls);
    ASSERT_EQ_U32(SUPER_CAP_DEFAULT_COMMAND_ID, sent_id);
    ASSERT_EQ_U32(SUPER_CAP_FRAME_DLC, sent_dlc);
    AssertBytes(expected_command, sent_data, sizeof(expected_command));
    ASSERT_TRUE(send_context == &callback_context);
    ASSERT_FALSE(send_called_in_critical);
}

static void TestCustomIdsAreUsedForTransmitAndReceive(void)
{
    static const uint8_t status_frame[SUPER_CAP_FRAME_DLC] = {
        0x00U, 0x00U, 0x00U, 0xF1U, 0x42U, 0xB4U, 0x00U, 0x80U
    };
    const SuperCapCommand command = {0};
    SuperCap instance;
    SuperCapConfig config;
    SuperCapStatus status;

    ResetFakes();
    config = DefaultConfig();
    config.command_id = 0x321U;
    config.status_id = 0x456U;
    ASSERT_TRUE(SuperCapInit(&instance, &config));
    ASSERT_TRUE(SuperCapSendCommand(&instance, &command));
    ASSERT_EQ_U32(0x321U, sent_id);
    ASSERT_FALSE(SuperCapReceive(&instance, SUPER_CAP_DEFAULT_STATUS_ID,
                                 status_frame, SUPER_CAP_FRAME_DLC));
    ASSERT_TRUE(SuperCapReceive(&instance, 0x456U,
                                status_frame, SUPER_CAP_FRAME_DLC));
    ASSERT_TRUE(SuperCapGetStatus(&instance, &status));
    ASSERT_TRUE(status.online);
}

static void TestValidStatusDecodesExactFieldsAndDiagnostics(void)
{
    static const uint8_t status_frame[SUPER_CAP_FRAME_DLC] = {
        0x00U, 0x00U, 0x00U, 0xF1U, 0x42U, 0xB4U, 0x00U, 0x80U
    };
    SuperCap instance;
    SuperCapConfig config;
    SuperCapStatus status;
    SuperCapDiagnostics diagnostics;

    ResetFakes();
    fake_now_ms = 1234U;
    config = DefaultConfig();
    ASSERT_TRUE(SuperCapInit(&instance, &config));
    ASSERT_TRUE(SuperCapReceive(&instance, SUPER_CAP_DEFAULT_STATUS_ID,
                                status_frame, SUPER_CAP_FRAME_DLC));
    ASSERT_TRUE(SuperCapGetStatus(&instance, &status));
    ASSERT_TRUE(status.online);
    ASSERT_TRUE(status.output_enabled);
    ASSERT_EQ_U32(0U, status.error_code);
    ASSERT_NEAR(120.5f, status.chassis_power_w, 0.001f);
    ASSERT_EQ_U32(180U, status.available_power_limit_w);
    ASSERT_NEAR(128.0f / 255.0f, status.energy_ratio, 0.0001f);
    ASSERT_TRUE(SuperCapGetDiagnostics(&instance, &diagnostics));
    ASSERT_EQ_U32(1U, diagnostics.received_frame_count);
    ASSERT_EQ_U32(0U, diagnostics.rejected_frame_count);
    ASSERT_EQ_U32(0U, diagnostics.transmit_failure_count);
    ASSERT_EQ_U32(1234U, diagnostics.last_receive_tick_ms);
    ASSERT_TRUE(clock_context == &callback_context);
}

static void TestStatusFlagsAndNegativeFinitePowerDecode(void)
{
    static const uint8_t status_frame[SUPER_CAP_FRAME_DLC] = {
        0xC5U, 0x00U, 0x00U, 0x80U, 0xBFU, 0x34U, 0x12U, 0xFFU
    };
    SuperCap instance;
    SuperCapConfig config;
    SuperCapStatus status;

    ResetFakes();
    config = DefaultConfig();
    ASSERT_TRUE(SuperCapInit(&instance, &config));
    ASSERT_TRUE(SuperCapReceive(&instance, SUPER_CAP_DEFAULT_STATUS_ID,
                                status_frame, SUPER_CAP_FRAME_DLC));
    ASSERT_TRUE(SuperCapGetStatus(&instance, &status));
    ASSERT_FALSE(status.output_enabled);
    ASSERT_EQ_U32(0x45U, status.error_code);
    ASSERT_NEAR(-1.0f, status.chassis_power_w, 0.0001f);
    ASSERT_EQ_U32(0x1234U, status.available_power_limit_w);
    ASSERT_NEAR(1.0f, status.energy_ratio, 0.0001f);
}

static void TestInvalidFramesAreRejectedWithoutOverwritingStatus(void)
{
    static const uint8_t valid_frame[SUPER_CAP_FRAME_DLC] = {
        0x00U, 0x00U, 0x00U, 0xF1U, 0x42U, 0xB4U, 0x00U, 0x80U
    };
    static const uint8_t nan_frame[SUPER_CAP_FRAME_DLC] = {
        0x00U, 0x00U, 0x00U, 0xC0U, 0x7FU, 0x00U, 0x00U, 0x00U
    };
    static const uint8_t infinity_frame[SUPER_CAP_FRAME_DLC] = {
        0x00U, 0x00U, 0x00U, 0x80U, 0x7FU, 0x00U, 0x00U, 0x00U
    };
    SuperCap instance;
    SuperCapConfig config;
    SuperCapStatus before;
    SuperCapStatus after;
    SuperCapDiagnostics diagnostics;

    ResetFakes();
    fake_now_ms = 10U;
    config = DefaultConfig();
    ASSERT_TRUE(SuperCapInit(&instance, &config));
    ASSERT_TRUE(SuperCapReceive(&instance, SUPER_CAP_DEFAULT_STATUS_ID,
                                valid_frame, SUPER_CAP_FRAME_DLC));
    ASSERT_TRUE(SuperCapGetStatus(&instance, &before));

    fake_now_ms = 20U;
    ASSERT_FALSE(SuperCapReceive(&instance, 0x052U,
                                 valid_frame, SUPER_CAP_FRAME_DLC));
    ASSERT_FALSE(SuperCapReceive(&instance, SUPER_CAP_DEFAULT_STATUS_ID,
                                 valid_frame, 7U));
    ASSERT_FALSE(SuperCapReceive(&instance, SUPER_CAP_DEFAULT_STATUS_ID,
                                 NULL, SUPER_CAP_FRAME_DLC));
    ASSERT_FALSE(SuperCapReceive(&instance, SUPER_CAP_DEFAULT_STATUS_ID,
                                 nan_frame, SUPER_CAP_FRAME_DLC));
    ASSERT_FALSE(SuperCapReceive(&instance, SUPER_CAP_DEFAULT_STATUS_ID,
                                 infinity_frame, SUPER_CAP_FRAME_DLC));

    ASSERT_TRUE(SuperCapGetStatus(&instance, &after));
    ASSERT_EQ_U32(before.output_enabled, after.output_enabled);
    ASSERT_EQ_U32(before.error_code, after.error_code);
    ASSERT_NEAR(before.chassis_power_w, after.chassis_power_w, 0.0001f);
    ASSERT_EQ_U32(before.available_power_limit_w,
                  after.available_power_limit_w);
    ASSERT_NEAR(before.energy_ratio, after.energy_ratio, 0.0001f);
    ASSERT_TRUE(SuperCapGetDiagnostics(&instance, &diagnostics));
    ASSERT_EQ_U32(1U, diagnostics.received_frame_count);
    ASSERT_EQ_U32(5U, diagnostics.rejected_frame_count);
    ASSERT_EQ_U32(10U, diagnostics.last_receive_tick_ms);
}

static void TestOfflineTimeoutBoundaryAndZeroDefault(void)
{
    static const uint8_t status_frame[SUPER_CAP_FRAME_DLC] = {0};
    SuperCap instance;
    SuperCapConfig config;
    SuperCapStatus status;

    ResetFakes();
    fake_now_ms = 100U;
    config = DefaultConfig();
    ASSERT_TRUE(SuperCapInit(&instance, &config));
    ASSERT_TRUE(SuperCapReceive(&instance, SUPER_CAP_DEFAULT_STATUS_ID,
                                status_frame, SUPER_CAP_FRAME_DLC));

    fake_now_ms = 300U;
    ASSERT_TRUE(SuperCapGetStatus(&instance, &status));
    ASSERT_TRUE(status.online);
    fake_now_ms = 301U;
    ASSERT_TRUE(SuperCapGetStatus(&instance, &status));
    ASSERT_FALSE(status.online);
}

static void TestOnlineTimeoutUsesUnsignedTickWraparound(void)
{
    static const uint8_t status_frame[SUPER_CAP_FRAME_DLC] = {0};
    SuperCap instance;
    SuperCapConfig config;
    SuperCapStatus status;

    ResetFakes();
    fake_now_ms = UINT32_MAX - 10U;
    config = DefaultConfig();
    config.offline_timeout_ms = 20U;
    ASSERT_TRUE(SuperCapInit(&instance, &config));
    ASSERT_TRUE(SuperCapReceive(&instance, SUPER_CAP_DEFAULT_STATUS_ID,
                                status_frame, SUPER_CAP_FRAME_DLC));

    fake_now_ms = 9U;
    ASSERT_TRUE(SuperCapGetStatus(&instance, &status));
    ASSERT_TRUE(status.online);
    fake_now_ms = 10U;
    ASSERT_TRUE(SuperCapGetStatus(&instance, &status));
    ASSERT_FALSE(status.online);
}

static void TestFailedSendIsCountedAndNeverOccursInsideCriticalSection(void)
{
    const SuperCapCommand command = {0};
    SuperCap instance;
    SuperCapConfig config;
    SuperCapDiagnostics diagnostics;

    ResetFakes();
    config = DefaultConfig();
    config.enter_critical = EnterCritical;
    config.exit_critical = ExitCritical;
    ASSERT_TRUE(SuperCapInit(&instance, &config));
    fake_send_result = false;
    ASSERT_FALSE(SuperCapSendCommand(&instance, &command));
    ASSERT_EQ_U32(1U, send_calls);
    ASSERT_FALSE(send_called_in_critical);
    ASSERT_EQ_U32(1U, enter_calls);
    ASSERT_EQ_U32(1U, exit_calls);
    ASSERT_EQ_U32(0U, critical_depth);
    ASSERT_TRUE(SuperCapGetDiagnostics(&instance, &diagnostics));
    ASSERT_EQ_U32(1U, diagnostics.transmit_failure_count);
}

static void TestQueriesAndReceiveUseMatchingCriticalCallbacks(void)
{
    static const uint8_t status_frame[SUPER_CAP_FRAME_DLC] = {0};
    SuperCap instance;
    SuperCapConfig config;
    SuperCapStatus status;
    SuperCapDiagnostics diagnostics;

    ResetFakes();
    config = DefaultConfig();
    config.enter_critical = EnterCritical;
    config.exit_critical = ExitCritical;
    ASSERT_TRUE(SuperCapInit(&instance, &config));
    ASSERT_TRUE(SuperCapReceive(&instance, SUPER_CAP_DEFAULT_STATUS_ID,
                                status_frame, SUPER_CAP_FRAME_DLC));
    ASSERT_TRUE(SuperCapGetStatus(&instance, &status));
    ASSERT_TRUE(SuperCapGetDiagnostics(&instance, &diagnostics));
    ASSERT_EQ_U32(3U, enter_calls);
    ASSERT_EQ_U32(3U, exit_calls);
    ASSERT_EQ_U32(0U, critical_depth);
}

static void TestNullArgumentsAreRejectedWithoutCallbacks(void)
{
    static const uint8_t status_frame[SUPER_CAP_FRAME_DLC] = {0};
    const SuperCapCommand command = {0};
    SuperCap instance;
    SuperCapConfig config;
    SuperCapStatus status;
    SuperCapDiagnostics diagnostics;

    ResetFakes();
    config = DefaultConfig();
    ASSERT_TRUE(SuperCapInit(&instance, &config));
    ASSERT_FALSE(SuperCapReceive(NULL, SUPER_CAP_DEFAULT_STATUS_ID,
                                 status_frame, SUPER_CAP_FRAME_DLC));
    ASSERT_FALSE(SuperCapSendCommand(NULL, &command));
    ASSERT_FALSE(SuperCapSendCommand(&instance, NULL));
    ASSERT_FALSE(SuperCapGetStatus(NULL, &status));
    ASSERT_FALSE(SuperCapGetStatus(&instance, NULL));
    ASSERT_FALSE(SuperCapGetDiagnostics(NULL, &diagnostics));
    ASSERT_FALSE(SuperCapGetDiagnostics(&instance, NULL));
    ASSERT_EQ_U32(0U, send_calls);
}

int main(void)
{
    TestInitValidatesCallbacksCriticalPairAndStandardIds();
    TestDefaultIdsAndExactCommandBytes();
    TestCustomIdsAreUsedForTransmitAndReceive();
    TestValidStatusDecodesExactFieldsAndDiagnostics();
    TestStatusFlagsAndNegativeFinitePowerDecode();
    TestInvalidFramesAreRejectedWithoutOverwritingStatus();
    TestOfflineTimeoutBoundaryAndZeroDefault();
    TestOnlineTimeoutUsesUnsignedTickWraparound();
    TestFailedSendIsCountedAndNeverOccursInsideCriticalSection();
    TestQueriesAndReceiveUseMatchingCriticalCallbacks();
    TestNullArgumentsAreRejectedWithoutCallbacks();

    if (failures != 0)
        return 1;

    puts("PASS: portable supercapacitor tests");
    return 0;
}
