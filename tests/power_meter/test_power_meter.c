#include "power_meter.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;
static CAN_Init_Config_s captured_can_config;
static Daemon_Init_Config_s captured_daemon_config;
static CANInstance fake_can_instance;
static DaemonInstance fake_daemon_instance;
static uint8_t captured_dlc;
static uint32_t fake_tick_ms;
static uint32_t daemon_reload_calls;
static uint32_t can_register_calls;
static uint32_t daemon_register_calls;
static bool can_registration_fails;
static PowerMeterInstance *meter;

#define ASSERT_TRUE(value)                                                                         \
    do                                                                                             \
    {                                                                                              \
        if (!(value))                                                                              \
        {                                                                                          \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #value);                              \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)
#define ASSERT_FALSE(value) ASSERT_TRUE(!(value))
#define ASSERT_EQ_U32(expected, actual)                                                             \
    do                                                                                             \
    {                                                                                              \
        if ((uint32_t)(expected) != (uint32_t)(actual))                                            \
        {                                                                                          \
            printf("FAIL %s:%d: expected %lu, got %lu\n", __FILE__, __LINE__,                    \
                   (unsigned long)(uint32_t)(expected), (unsigned long)(uint32_t)(actual));         \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)
#define ASSERT_NEAR(expected, actual, tolerance)                                                    \
    do                                                                                             \
    {                                                                                              \
        if (fabsf((expected) - (actual)) > (tolerance))                                            \
        {                                                                                          \
            printf("FAIL %s:%d: expected %.6f, got %.6f\n", __FILE__, __LINE__,                  \
                   (double)(expected), (double)(actual));                                          \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

CANInstance *CANRegister(CAN_Init_Config_s *config)
{
    ++can_register_calls;
    captured_can_config = *config;
    if (can_registration_fails)
        return NULL;

    memset(&fake_can_instance, 0, sizeof(fake_can_instance));
    fake_can_instance.can_handle = config->can_handle;
    fake_can_instance.tx_id = config->tx_id;
    fake_can_instance.rx_id = config->rx_id;
    fake_can_instance.can_module_callback = config->can_module_callback;
    fake_can_instance.id = config->id;
    return &fake_can_instance;
}

void CANSetDLC(CANInstance *instance, uint8_t length)
{
    (void)instance;
    captured_dlc = length;
}

DaemonInstance *DaemonRegister(Daemon_Init_Config_s *config)
{
    ++daemon_register_calls;
    captured_daemon_config = *config;
    fake_daemon_instance.reload_count = config->reload_count;
    fake_daemon_instance.temp_count = config->init_count;
    fake_daemon_instance.callback = config->callback;
    fake_daemon_instance.owner_id = config->owner_id;
    return &fake_daemon_instance;
}

void DaemonReload(DaemonInstance *instance)
{
    instance->temp_count = instance->reload_count;
    ++daemon_reload_calls;
}

uint8_t DaemonIsOnline(DaemonInstance *instance)
{
    return instance->temp_count > 0U;
}

uint32_t HAL_GetTick(void)
{
    return fake_tick_ms;
}

static void ResetFakes(void)
{
    memset(&captured_can_config, 0, sizeof(captured_can_config));
    memset(&captured_daemon_config, 0, sizeof(captured_daemon_config));
    memset(&fake_can_instance, 0, sizeof(fake_can_instance));
    memset(&fake_daemon_instance, 0, sizeof(fake_daemon_instance));
    captured_dlc = 0U;
    fake_tick_ms = 0U;
    daemon_reload_calls = 0U;
    can_register_calls = 0U;
    daemon_register_calls = 0U;
    can_registration_fails = false;
}

static void ResetMeterState(void)
{
    memset(&meter->status, 0, sizeof(meter->status));
    meter->received_frame_count = 0U;
    meter->rejected_frame_count = 0U;
    meter->last_rx_tick_ms = 0U;
    fake_daemon_instance.temp_count = fake_daemon_instance.reload_count;
    fake_tick_ms = 0U;
    daemon_reload_calls = 0U;
}

static void DeliverFrame(const uint8_t frame[8], uint8_t dlc)
{
    memcpy(fake_can_instance.rx_buff, frame, sizeof(fake_can_instance.rx_buff));
    fake_can_instance.rx_len = dlc;
    fake_can_instance.can_module_callback(&fake_can_instance);
}

static void TestInitRegistersFrameworkInstances(void)
{
    PowerMeterStatus_s status = {0};
    ResetFakes();
    CAN_HandleTypeDef can1 = {0};
    const PowerMeter_Init_Config_s config = {
        .can_config = {
            .can_handle = &can1,
            .tx_id = POWER_METER_STATUS_CAN_ID,
            .rx_id = POWER_METER_STATUS_CAN_ID,
        },
        .offline_reload_count = 0U,
    };

    ASSERT_FALSE(PowerMeterGetStatus(&status));
    ASSERT_FALSE(PowerMeterInit(NULL));
    ASSERT_TRUE(PowerMeterInit(&config));
    meter = (PowerMeterInstance *)captured_can_config.id;
    ASSERT_TRUE(meter != NULL);
    ASSERT_FALSE(PowerMeterInit(&config));
    ASSERT_TRUE(captured_can_config.can_handle == &can1);
    ASSERT_EQ_U32(POWER_METER_STATUS_CAN_ID, captured_can_config.rx_id);
    ASSERT_TRUE(captured_can_config.can_module_callback != NULL);
    ASSERT_TRUE(captured_can_config.id == meter);
    ASSERT_EQ_U32(10U, captured_daemon_config.reload_count);
    ASSERT_TRUE(captured_daemon_config.owner_id == meter);
    ASSERT_EQ_U32(POWER_METER_CAN_DLC, captured_dlc);
}

static void TestCanRegistrationFailureIsLatched(void)
{
    static CAN_HandleTypeDef can1;
    PowerMeterStatus_s status = {0};
    const PowerMeter_Init_Config_s config = {
        .can_config = {
            .can_handle = &can1,
            .tx_id = POWER_METER_STATUS_CAN_ID,
            .rx_id = POWER_METER_STATUS_CAN_ID,
        },
        .offline_reload_count = 10U,
    };

    ResetFakes();
    can_registration_fails = true;
    ASSERT_FALSE(PowerMeterInit(&config));
    ASSERT_EQ_U32(1U, daemon_register_calls);
    ASSERT_EQ_U32(1U, can_register_calls);
    ASSERT_FALSE(PowerMeterGetStatus(&status));

    ASSERT_FALSE(PowerMeterInit(&config));
    ASSERT_EQ_U32(1U, daemon_register_calls);
    ASSERT_EQ_U32(1U, can_register_calls);
    captured_daemon_config.callback(captured_daemon_config.owner_id);
}

static void TestConfiguredFreshnessWindow(void)
{
    static CAN_HandleTypeDef can1;
    static const uint8_t frame[8] = {0, 1, 0, 2, 0, 3, 0, 4};
    PowerMeterStatus_s status = {0};
    const PowerMeter_Init_Config_s config = {
        .can_config = {
            .can_handle = &can1,
            .tx_id = POWER_METER_STATUS_CAN_ID,
            .rx_id = POWER_METER_STATUS_CAN_ID,
        },
        .offline_reload_count = 2U,
    };

    ResetFakes();
    ASSERT_TRUE(PowerMeterInit(&config));
    meter = (PowerMeterInstance *)captured_can_config.id;
    ASSERT_EQ_U32(2U, captured_daemon_config.reload_count);

    fake_tick_ms = 100U;
    DeliverFrame(frame, 8U);
    fake_tick_ms = 119U;
    ASSERT_TRUE(PowerMeterGetStatus(&status));
    ASSERT_TRUE(status.online);
    fake_tick_ms = 120U;
    ASSERT_TRUE(PowerMeterGetStatus(&status));
    ASSERT_FALSE(status.online);
}

static void TestStartsOfflineAndRejectsNullArguments(void)
{
    PowerMeterStatus_s status = {0};
    ResetMeterState();

    ASSERT_TRUE(PowerMeterGetStatus(&status));
    ASSERT_FALSE(status.online);
    ASSERT_EQ_U32(0U, status.last_rx_tick_ms);
    ASSERT_FALSE(PowerMeterGetStatus(NULL));
}

static void TestValidFrameUpdatesSnapshotAndDiagnostics(void)
{
    static const uint8_t frame[8] = {0x00U, 0x0CU, 0x5FU, 0xB4U,
                                     0x0CU, 0xB2U, 0x1FU, 0x1BU};
    PowerMeterStatus_s status = {0};
    ResetMeterState();
    fake_tick_ms = 1234U;

    DeliverFrame(frame, 8U);

    ASSERT_TRUE(PowerMeterGetStatus(&status));
    ASSERT_TRUE(status.online);
    ASSERT_NEAR(0.012f, status.shunt_voltage_v, 0.00001f);
    ASSERT_NEAR(24.5f, status.bus_voltage_v, 0.00001f);
    ASSERT_NEAR(3.25f, status.current_a, 0.00001f);
    ASSERT_NEAR(79.63f, status.power_w, 0.00001f);
    ASSERT_EQ_U32(1234U, status.last_rx_tick_ms);
    ASSERT_EQ_U32(1U, meter->received_frame_count);
    ASSERT_EQ_U32(0U, meter->rejected_frame_count);
    ASSERT_EQ_U32(1234U, meter->last_rx_tick_ms);
    ASSERT_EQ_U32(1U, daemon_reload_calls);
}

static void TestBadLengthDoesNotOverwriteOrFeedDaemon(void)
{
    static const uint8_t valid[8] = {0, 1, 0, 2, 0, 3, 0, 4};
    static const uint8_t invalid[8] = {0, 9, 0, 9, 0, 9, 0, 9};
    PowerMeterStatus_s status_before = {0};
    PowerMeterStatus_s status_after = {0};
    ResetMeterState();
    fake_tick_ms = 4321U;
    DeliverFrame(valid, 8U);

    ASSERT_TRUE(PowerMeterGetStatus(&status_before));
    const uint32_t received_before = meter->received_frame_count;
    const uint32_t rejected_before = meter->rejected_frame_count;
    const uint32_t last_rx_before = meter->last_rx_tick_ms;
    const uint32_t reloads_before = daemon_reload_calls;

    fake_tick_ms = 4370U;
    DeliverFrame(invalid, 7U);

    ASSERT_TRUE(PowerMeterGetStatus(&status_after));
    ASSERT_EQ_U32(status_before.online, status_after.online);
    ASSERT_NEAR(status_before.shunt_voltage_v, status_after.shunt_voltage_v, 0.00001f);
    ASSERT_NEAR(status_before.bus_voltage_v, status_after.bus_voltage_v, 0.00001f);
    ASSERT_NEAR(status_before.current_a, status_after.current_a, 0.00001f);
    ASSERT_NEAR(status_before.power_w, status_after.power_w, 0.00001f);
    ASSERT_EQ_U32(status_before.last_rx_tick_ms, status_after.last_rx_tick_ms);
    ASSERT_EQ_U32(received_before, meter->received_frame_count);
    ASSERT_EQ_U32(rejected_before + 1U, meter->rejected_frame_count);
    ASSERT_EQ_U32(last_rx_before, meter->last_rx_tick_ms);
    ASSERT_EQ_U32(reloads_before, daemon_reload_calls);
}

static void TestOfflineCallbackClearsOnlyOnlineState(void)
{
    static const uint8_t frame[8] = {0, 1, 0, 2, 0, 3, 0, 4};
    PowerMeterStatus_s status = {0};
    ResetMeterState();
    fake_tick_ms = 42U;
    DeliverFrame(frame, 8U);

    fake_tick_ms = 142U;
    fake_daemon_instance.temp_count = 0U;
    captured_daemon_config.callback(captured_daemon_config.owner_id);

    ASSERT_TRUE(PowerMeterGetStatus(&status));
    ASSERT_FALSE(status.online);
    ASSERT_NEAR(0.04f, status.power_w, 0.00001f);
    ASSERT_EQ_U32(42U, status.last_rx_tick_ms);
}

static void TestStaleOfflineCallbackKeepsReloadedMeterOnline(void)
{
    static const uint8_t frame[8] = {0, 1, 0, 2, 0, 3, 0, 4};
    PowerMeterStatus_s status = {0};
    ResetMeterState();

    fake_daemon_instance.temp_count = 0U;
    DeliverFrame(frame, 8U);
    captured_daemon_config.callback(captured_daemon_config.owner_id);

    ASSERT_TRUE(PowerMeterGetStatus(&status));
    ASSERT_TRUE(status.online);
}

static void TestLostDaemonReloadDoesNotMarkFreshFrameOffline(void)
{
    static const uint8_t frame[8] = {0, 1, 0, 2, 0, 3, 0, 4};
    PowerMeterStatus_s status = {0};
    ResetMeterState();

    fake_tick_ms = 100U;
    DeliverFrame(frame, 8U);
    fake_daemon_instance.temp_count = 0U;
    fake_tick_ms = 101U;
    captured_daemon_config.callback(captured_daemon_config.owner_id);

    ASSERT_TRUE(PowerMeterGetStatus(&status));
    ASSERT_TRUE(status.online);
}

static void TestGetterUsesHundredMillisecondFreshnessBoundary(void)
{
    static const uint8_t frame[8] = {0, 1, 0, 2, 0, 3, 0, 4};
    PowerMeterStatus_s status = {0};
    ResetMeterState();

    fake_tick_ms = 100U;
    DeliverFrame(frame, 8U);

    fake_tick_ms = 199U;
    ASSERT_TRUE(PowerMeterGetStatus(&status));
    ASSERT_TRUE(status.online);

    fake_tick_ms = 200U;
    ASSERT_TRUE(PowerMeterGetStatus(&status));
    ASSERT_FALSE(status.online);
}

static void TestFrameCounterWrapKeepsFreshMeterOnline(void)
{
    static const uint8_t frame[8] = {0, 1, 0, 2, 0, 3, 0, 4};
    PowerMeterStatus_s status = {0};
    ResetMeterState();

    meter->received_frame_count = UINT32_MAX;
    fake_tick_ms = 100U;
    DeliverFrame(frame, 8U);

    ASSERT_EQ_U32(0U, meter->received_frame_count);
    ASSERT_TRUE(PowerMeterGetStatus(&status));
    ASSERT_TRUE(status.online);
}

static void TestOfflineStateStaysLatchedAcrossFullTickCycle(void)
{
    static const uint8_t frame[8] = {0, 1, 0, 2, 0, 3, 0, 4};
    PowerMeterStatus_s status = {0};
    ResetMeterState();

    fake_tick_ms = 100U;
    DeliverFrame(frame, 8U);
    fake_tick_ms = 200U;
    fake_daemon_instance.temp_count = 0U;
    captured_daemon_config.callback(captured_daemon_config.owner_id);

    ASSERT_TRUE(PowerMeterGetStatus(&status));
    ASSERT_FALSE(status.online);

    fake_tick_ms = 100U;
    ASSERT_TRUE(PowerMeterGetStatus(&status));
    ASSERT_FALSE(status.online);
}

static void TestGetterFreshnessHandlesTickWrap(void)
{
    static const uint8_t frame[8] = {0, 1, 0, 2, 0, 3, 0, 4};
    PowerMeterStatus_s status = {0};
    ResetMeterState();

    fake_tick_ms = UINT32_MAX - 9U;
    DeliverFrame(frame, 8U);

    fake_tick_ms = 89U;
    ASSERT_TRUE(PowerMeterGetStatus(&status));
    ASSERT_TRUE(status.online);

    fake_tick_ms = 90U;
    ASSERT_TRUE(PowerMeterGetStatus(&status));
    ASSERT_FALSE(status.online);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "can_failure") == 0)
    {
        TestCanRegistrationFailureIsLatched();
        return failures == 0 ? 0 : 1;
    }
    if (argc > 1 && strcmp(argv[1], "custom_timeout") == 0)
    {
        TestConfiguredFreshnessWindow();
        return failures == 0 ? 0 : 1;
    }

    TestInitRegistersFrameworkInstances();
    TestStartsOfflineAndRejectsNullArguments();
    TestValidFrameUpdatesSnapshotAndDiagnostics();
    TestBadLengthDoesNotOverwriteOrFeedDaemon();
    TestOfflineCallbackClearsOnlyOnlineState();
    TestStaleOfflineCallbackKeepsReloadedMeterOnline();
    TestLostDaemonReloadDoesNotMarkFreshFrameOffline();
    TestGetterUsesHundredMillisecondFreshnessBoundary();
    TestFrameCounterWrapKeepsFreshMeterOnline();
    TestOfflineStateStaysLatchedAcrossFullTickCycle();
    TestGetterFreshnessHandlesTickWrap();

    if (failures != 0)
        return 1;
    puts("PASS: power meter module tests");
    return 0;
}
