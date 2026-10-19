#include "referee.h"
#include "referee_crc.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#define FAIL(message)                                                        \
    do                                                                       \
    {                                                                        \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, (message)); \
        exit(EXIT_FAILURE);                                                  \
    } while (0)

#define ASSERT_TRUE(condition)                                               \
    do                                                                       \
    {                                                                        \
        if (!(condition))                                                    \
            FAIL(#condition);                                                \
    } while (0)

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_EQ_U8(expected, actual)                                       \
    do                                                                       \
    {                                                                        \
        if ((uint8_t)(expected) != (uint8_t)(actual))                        \
            FAIL(#actual " did not equal " #expected);                     \
    } while (0)

#define ASSERT_EQ_U16(expected, actual)                                      \
    do                                                                       \
    {                                                                        \
        if ((uint16_t)(expected) != (uint16_t)(actual))                      \
            FAIL(#actual " did not equal " #expected);                     \
    } while (0)

#define ASSERT_EQ_U32(expected, actual)                                      \
    do                                                                       \
    {                                                                        \
        if ((uint32_t)(expected) != (uint32_t)(actual))                      \
            FAIL(#actual " did not equal " #expected);                     \
    } while (0)

#define ASSERT_EQ_U64(expected, actual)                                      \
    do                                                                       \
    {                                                                        \
        if ((uint64_t)(expected) != (uint64_t)(actual))                      \
            FAIL(#actual " did not equal " #expected);                     \
    } while (0)

#define ASSERT_EQ_SIZE(expected, actual)                                     \
    do                                                                       \
    {                                                                        \
        if ((size_t)(expected) != (size_t)(actual))                          \
            FAIL(#actual " did not equal " #expected);                     \
    } while (0)

#define ASSERT_FLOAT_NEAR(expected, actual, tolerance)                       \
    do                                                                       \
    {                                                                        \
        const float actual_ = (float)(actual);                               \
        const float expected_ = (float)(expected);                           \
        if (!isfinite(actual_) || !isfinite(expected_) ||                    \
            fabsf(expected_ - actual_) > (float)(tolerance))                 \
            FAIL(#actual " was outside tolerance of " #expected);          \
    } while (0)

#define ASSERT_BYTES(expected, actual, length)                               \
    do                                                                       \
    {                                                                        \
        if (memcmp((expected), (actual), (length)) != 0)                     \
            FAIL(#actual " did not match " #expected);                     \
    } while (0)

typedef struct
{
    uint32_t now_ms;
    unsigned critical_depth;
    unsigned enter_count;
    unsigned exit_count;
    unsigned now_in_critical_count;
    bool send_result;
    uint8_t sent_frame[REFEREE_MAX_FRAME_SIZE];
    size_t sent_length;
} TestContext;

static uint32_t FakeNow(void *context)
{
    TestContext *test = context;
    if (test->critical_depth > 0U)
        ++test->now_in_critical_count;
    return test->now_ms;
}

static bool FakeSend(void *context, const uint8_t *data, size_t length)
{
    TestContext *test = context;
    if (data == NULL || length > sizeof(test->sent_frame) ||
        test->critical_depth != 0U)
    {
        return false;
    }

    memcpy(test->sent_frame, data, length);
    test->sent_length = length;
    return test->send_result;
}

static void FakeEnterCritical(void *context)
{
    TestContext *test = context;
    ++test->enter_count;
    ++test->critical_depth;
}

static void FakeExitCritical(void *context)
{
    TestContext *test = context;
    ASSERT_EQ_U32(1U, test->critical_depth);
    --test->critical_depth;
    ++test->exit_count;
}

static RefereeConfig MakeConfig(TestContext *context)
{
    RefereeConfig config = {
        .uart_send = FakeSend,
        .now_ms = FakeNow,
        .enter_critical = NULL,
        .exit_critical = NULL,
        .context = context,
        .offline_timeout_ms = 0U,
    };
    return config;
}

static void ResetContext(TestContext *context)
{
    memset(context, 0, sizeof(*context));
    context->send_result = true;
}

static size_t BuildFrame(uint8_t *frame,
                         uint16_t command_id,
                         const uint8_t *payload,
                         size_t payload_length,
                         uint8_t sequence)
{
    const size_t frame_length = payload_length + REFEREE_FRAME_OVERHEAD;
    ASSERT_TRUE(frame != NULL);
    ASSERT_TRUE(payload != NULL || payload_length == 0U);
    ASSERT_TRUE(frame_length <= REFEREE_MAX_FRAME_SIZE);

    frame[0] = REFEREE_SOF;
    frame[1] = (uint8_t)(payload_length & 0xFFU);
    frame[2] = (uint8_t)((payload_length >> 8U) & 0xFFU);
    frame[3] = sequence;
    RefereeCrc8Append(frame, REFEREE_HEADER_SIZE);
    frame[5] = (uint8_t)(command_id & 0xFFU);
    frame[6] = (uint8_t)(command_id >> 8U);
    if (payload_length > 0U)
        memcpy(&frame[7], payload, payload_length);
    RefereeCrc16Append(frame, frame_length);
    return frame_length;
}

static void FeedFrame(Referee *referee,
                      uint16_t command_id,
                      const uint8_t *payload,
                      size_t payload_length,
                      uint8_t sequence)
{
    uint8_t frame[REFEREE_MAX_FRAME_SIZE];
    const size_t length =
        BuildFrame(frame, command_id, payload, payload_length, sequence);
    ASSERT_TRUE(RefereeFeed(referee, frame, length));
}

static void AssertCriticalBalanced(const TestContext *context,
                                   unsigned expected_calls)
{
    ASSERT_EQ_U32(expected_calls, context->enter_count);
    ASSERT_EQ_U32(expected_calls, context->exit_count);
    ASSERT_EQ_U32(0U, context->critical_depth);
}

static void TestCrcKnownVectorsAndNullHandling(void)
{
    static const uint8_t text[] = "123456789";
    uint8_t header[REFEREE_HEADER_SIZE] = {REFEREE_SOF, 1U, 0U, 7U, 0U};
    uint8_t frame[10] = {REFEREE_SOF, 1U, 0U, 7U, 0U,
                         2U, 0U, 9U, 0U, 0U};

    ASSERT_EQ_U8(0x0BU,
                 RefereeCrc8(text, sizeof(text) - 1U, 0xFFU));
    ASSERT_EQ_U16(0x6F91U,
                  RefereeCrc16(text, sizeof(text) - 1U, 0xFFFFU));
    ASSERT_EQ_U8(0x5AU, RefereeCrc8(NULL, 4U, 0x5AU));
    ASSERT_EQ_U16(0xA55AU, RefereeCrc16(NULL, 4U, 0xA55AU));
    ASSERT_FALSE(RefereeCrc8Verify(NULL, REFEREE_HEADER_SIZE));
    ASSERT_FALSE(RefereeCrc16Verify(NULL, sizeof(frame)));

    RefereeCrc8Append(header, sizeof(header));
    ASSERT_TRUE(RefereeCrc8Verify(header, sizeof(header)));
    RefereeCrc8Append(frame, REFEREE_HEADER_SIZE);
    RefereeCrc16Append(frame, sizeof(frame));
    ASSERT_TRUE(RefereeCrc8Verify(frame, REFEREE_HEADER_SIZE));
    ASSERT_TRUE(RefereeCrc16Verify(frame, sizeof(frame)));
    frame[7] ^= 1U;
    ASSERT_FALSE(RefereeCrc16Verify(frame, sizeof(frame)));
}

static void TestInitRejectsMissingCallbacksAndMismatchedCriticalPair(void)
{
    Referee referee;
    TestContext context;
    RefereeConfig config;

    ResetContext(&context);
    config = MakeConfig(&context);
    ASSERT_FALSE(RefereeInit(NULL, &config));
    ASSERT_FALSE(RefereeInit(&referee, NULL));

    config.uart_send = NULL;
    ASSERT_FALSE(RefereeInit(&referee, &config));
    config = MakeConfig(&context);
    config.now_ms = NULL;
    ASSERT_FALSE(RefereeInit(&referee, &config));
    config = MakeConfig(&context);
    config.enter_critical = FakeEnterCritical;
    ASSERT_FALSE(RefereeInit(&referee, &config));
    config = MakeConfig(&context);
    config.exit_critical = FakeExitCritical;
    ASSERT_FALSE(RefereeInit(&referee, &config));

    config = MakeConfig(&context);
    ASSERT_TRUE(RefereeInit(&referee, &config));
    ASSERT_EQ_U32(REFEREE_DEFAULT_OFFLINE_TIMEOUT_MS,
                  referee.config.offline_timeout_ms);
    config.offline_timeout_ms = 77U;
    ASSERT_TRUE(RefereeInit(&referee, &config));
    ASSERT_EQ_U32(77U, referee.config.offline_timeout_ms);

    referee.config = config;
    ASSERT_TRUE(RefereeInit(&referee, &referee.config));
    ASSERT_TRUE(referee.config.uart_send == FakeSend);
    ASSERT_TRUE(referee.config.now_ms == FakeNow);
    ASSERT_EQ_U32(77U, referee.config.offline_timeout_ms);
}

static void TestSplitAndCoalescedFramesDecodeWithoutLosingBytes(void)
{
    Referee referee;
    RefereeData data;
    RefereeDiagnostics diagnostics;
    TestContext context;
    RefereeConfig config;
    uint8_t first[REFEREE_MAX_FRAME_SIZE];
    uint8_t second[REFEREE_MAX_FRAME_SIZE];
    uint8_t joined[REFEREE_MAX_FRAME_SIZE];
    uint8_t stale_header[REFEREE_HEADER_SIZE] = {
        REFEREE_SOF, 100U, 0U, 0x22U, 0U};
    const uint8_t winner[] = {3U};
    const uint8_t event[] = {0x78U, 0x56U, 0x34U, 0x12U};
    size_t first_length;
    size_t second_length;

    ResetContext(&context);
    config = MakeConfig(&context);
    ASSERT_TRUE(RefereeInit(&referee, &config));
    first_length = BuildFrame(first, REFEREE_CMD_GAME_RESULT,
                              winner, sizeof(winner), 4U);
    second_length = BuildFrame(second, REFEREE_CMD_EVENT_DATA,
                               event, sizeof(event), 5U);

    ASSERT_TRUE(RefereeFeed(&referee, first, 3U));
    ASSERT_TRUE(RefereeGetData(&referee, &data));
    ASSERT_FALSE(data.has_game_result);

    memcpy(joined, &first[3], first_length - 3U);
    memcpy(&joined[first_length - 3U], second, second_length);
    ASSERT_TRUE(RefereeFeed(&referee, joined,
                            first_length - 3U + second_length));
    ASSERT_TRUE(RefereeGetData(&referee, &data));
    ASSERT_TRUE(data.has_game_result);
    ASSERT_EQ_U8(3U, data.game_result.winner);
    ASSERT_TRUE(data.has_event_data);
    ASSERT_EQ_U32(0x12345678U, data.event_data.event_type);
    ASSERT_EQ_U16(REFEREE_CMD_EVENT_DATA, data.last_command_id);
    ASSERT_EQ_U8(5U, data.last_sequence);

    RefereeCrc8Append(stale_header, sizeof(stale_header));
    memcpy(joined, stale_header, sizeof(stale_header));
    memcpy(&joined[sizeof(stale_header)], first, first_length);
    ASSERT_TRUE(RefereeFeed(&referee, joined,
                            sizeof(stale_header) + first_length));
    ASSERT_TRUE(RefereeGetData(&referee, &data));
    ASSERT_EQ_U16(REFEREE_CMD_GAME_RESULT, data.last_command_id);
    ASSERT_EQ_U8(4U, data.last_sequence);

    ASSERT_TRUE(RefereeGetDiagnostics(&referee, &diagnostics));
    ASSERT_EQ_U32(3U, diagnostics.valid_frame_count);
    ASSERT_TRUE(diagnostics.discarded_byte_count >=
                sizeof(stale_header));
}

static void TestNoiseBadCrcAndOversizeFramesUpdateDiagnostics(void)
{
    Referee referee;
    RefereeDiagnostics diagnostics;
    TestContext context;
    RefereeConfig config;
    uint8_t frame[REFEREE_MAX_FRAME_SIZE];
    const uint8_t noise[] = {0x11U, 0x22U, 0x33U};
    const uint8_t winner[] = {1U};
    size_t length;

    ResetContext(&context);
    config = MakeConfig(&context);
    ASSERT_TRUE(RefereeInit(&referee, &config));
    ASSERT_TRUE(RefereeFeed(&referee, noise, sizeof(noise)));

    length = BuildFrame(frame, REFEREE_CMD_GAME_RESULT,
                        winner, sizeof(winner), 1U);
    frame[4] ^= 0x01U;
    ASSERT_TRUE(RefereeFeed(&referee, frame, length));

    frame[0] = REFEREE_SOF;
    frame[1] = 248U;
    frame[2] = 0U;
    frame[3] = 2U;
    RefereeCrc8Append(frame, REFEREE_HEADER_SIZE);
    ASSERT_TRUE(RefereeFeed(&referee, frame, REFEREE_HEADER_SIZE));

    length = BuildFrame(frame, REFEREE_CMD_GAME_RESULT,
                        winner, sizeof(winner), 3U);
    frame[length - 1U] ^= 0x01U;
    ASSERT_TRUE(RefereeFeed(&referee, frame, length));

    FeedFrame(&referee, REFEREE_CMD_GAME_RESULT,
              winner, sizeof(winner), 4U);
    ASSERT_TRUE(RefereeGetDiagnostics(&referee, &diagnostics));
    ASSERT_EQ_U32(1U, diagnostics.valid_frame_count);
    ASSERT_EQ_U32(1U, diagnostics.crc8_error_count);
    ASSERT_EQ_U32(1U, diagnostics.crc16_error_count);
    ASSERT_EQ_U32(1U, diagnostics.oversize_frame_count);
    ASSERT_TRUE(diagnostics.discarded_byte_count >= sizeof(noise) + 3U);
}

static void TestEverySupportedCommandDecodesExactLittleEndianFields(void)
{
    Referee referee;
    RefereeData data;
    RefereeDiagnostics diagnostics;
    TestContext context;
    RefereeConfig config;
    uint8_t hp[32];
    size_t i;
    static const uint8_t game_state[] = {
        0xA3U, 0x34U, 0x12U, 0x08U, 0x07U, 0x06U,
        0x05U, 0x04U, 0x03U, 0x02U, 0x01U};
    static const uint8_t game_result[] = {0x02U};
    static const uint8_t event_data[] = {0x78U, 0x56U, 0x34U, 0x12U};
    static const uint8_t supply_action[] = {1U, 103U, 2U, 50U};
    static const uint8_t robot_state[] = {
        3U, 2U, 0x34U, 0x12U, 0x78U, 0x56U, 0x11U,
        0x22U, 0x33U, 0x44U, 0xCDU, 0xABU, 0x05U};
    static const uint8_t power_heat[] = {
        0x11U, 0x22U, 0x33U, 0x44U, 0x00U, 0x00U, 0x48U,
        0x41U, 0x66U, 0x55U, 0x88U, 0x77U, 0xAAU, 0x99U};
    static const uint8_t position[] = {
        0x00U, 0x00U, 0xC0U, 0x3FU, 0x00U, 0x00U, 0x10U, 0xC0U,
        0x00U, 0x00U, 0x50U, 0x40U, 0x00U, 0x00U, 0x90U, 0xC0U};
    static const uint8_t buff[] = {1U, 2U, 3U, 4U, 0x34U, 0x12U};
    static const uint8_t aerial[] = {2U, 99U};
    static const uint8_t hurt[] = {0xB6U};
    static const uint8_t shoot[] = {
        1U, 2U, 9U, 0x00U, 0x00U, 0xE2U, 0x41U};
    static const uint8_t interaction[] = {
        0x34U, 0x12U, 0x03U, 0x00U, 0x67U, 0x00U,
        0xDEU, 0xADU, 0xBEU};

    ResetContext(&context);
    config = MakeConfig(&context);
    ASSERT_TRUE(RefereeInit(&referee, &config));
    for (i = 0U; i < ARRAY_COUNT(hp) / 2U; ++i)
    {
        const uint16_t value = (uint16_t)(0x1000U + i);
        hp[2U * i] = (uint8_t)value;
        hp[2U * i + 1U] = (uint8_t)(value >> 8U);
    }

    FeedFrame(&referee, REFEREE_CMD_GAME_STATE,
              game_state, sizeof(game_state), 1U);
    FeedFrame(&referee, REFEREE_CMD_GAME_RESULT,
              game_result, sizeof(game_result), 2U);
    FeedFrame(&referee, REFEREE_CMD_GAME_ROBOT_HP,
              hp, sizeof(hp), 3U);
    FeedFrame(&referee, REFEREE_CMD_EVENT_DATA,
              event_data, sizeof(event_data), 4U);
    FeedFrame(&referee, REFEREE_CMD_SUPPLY_ACTION,
              supply_action, sizeof(supply_action), 5U);
    FeedFrame(&referee, REFEREE_CMD_ROBOT_STATE,
              robot_state, sizeof(robot_state), 6U);
    FeedFrame(&referee, REFEREE_CMD_POWER_HEAT,
              power_heat, sizeof(power_heat), 7U);
    FeedFrame(&referee, REFEREE_CMD_ROBOT_POSITION,
              position, sizeof(position), 8U);
    FeedFrame(&referee, REFEREE_CMD_BUFF,
              buff, sizeof(buff), 9U);
    FeedFrame(&referee, REFEREE_CMD_AERIAL_ENERGY,
              aerial, sizeof(aerial), 10U);
    FeedFrame(&referee, REFEREE_CMD_ROBOT_HURT,
              hurt, sizeof(hurt), 11U);
    FeedFrame(&referee, REFEREE_CMD_SHOOT_DATA,
              shoot, sizeof(shoot), 12U);
    FeedFrame(&referee, REFEREE_CMD_STUDENT_INTERACTION,
              interaction, sizeof(interaction), 13U);

    ASSERT_TRUE(RefereeGetData(&referee, &data));
    ASSERT_TRUE(data.has_game_state);
    ASSERT_EQ_U8(3U, data.game_state.game_type);
    ASSERT_EQ_U8(10U, data.game_state.game_progress);
    ASSERT_EQ_U16(0x1234U, data.game_state.stage_remain_time);
    ASSERT_EQ_U64(UINT64_C(0x0102030405060708),
                  data.game_state.sync_timestamp);
    ASSERT_EQ_U8(2U, data.game_result.winner);
    ASSERT_EQ_U16(0x1000U, data.game_robot_hp.red_1_robot_hp);
    ASSERT_EQ_U16(0x1007U, data.game_robot_hp.red_base_hp);
    ASSERT_EQ_U16(0x100FU, data.game_robot_hp.blue_base_hp);
    ASSERT_EQ_U32(0x12345678U, data.event_data.event_type);
    ASSERT_EQ_U8(103U, data.supply_action.supply_robot_id);
    ASSERT_EQ_U8(50U, data.supply_action.supply_projectile_num);
    ASSERT_EQ_U16(0x1234U, data.robot_state.current_hp);
    ASSERT_EQ_U16(0xABCDU, data.robot_state.chassis_power_limit_w);
    ASSERT_TRUE(data.robot_state.gimbal_output_enabled);
    ASSERT_FALSE(data.robot_state.chassis_output_enabled);
    ASSERT_TRUE(data.robot_state.shooter_output_enabled);
    ASSERT_EQ_U16(0x2211U, data.power_heat.reserved_voltage);
    ASSERT_FLOAT_NEAR(12.5F, data.power_heat.reserved_power, 0.0001F);
    ASSERT_EQ_U16(0x5566U, data.power_heat.buffer_energy_j);
    ASSERT_FLOAT_NEAR(1.5F, data.robot_position.x, 0.0001F);
    ASSERT_FLOAT_NEAR(-2.25F, data.robot_position.y, 0.0001F);
    ASSERT_FLOAT_NEAR(3.25F, data.robot_position.z, 0.0001F);
    ASSERT_FLOAT_NEAR(-4.5F, data.robot_position.yaw, 0.0001F);
    ASSERT_EQ_U16(0x1234U, data.buff.attack_buff);
    ASSERT_EQ_U8(99U, data.aerial_energy.time_remaining_s);
    ASSERT_EQ_U8(6U, data.robot_hurt.armor_id);
    ASSERT_EQ_U8(11U, data.robot_hurt.hurt_type);
    ASSERT_EQ_U8(9U, data.shoot_data.bullet_frequency_hz);
    ASSERT_FLOAT_NEAR(28.25F, data.shoot_data.bullet_speed_mps, 0.0001F);
    ASSERT_EQ_U16(0x1234U, data.interaction.data_command_id);
    ASSERT_EQ_U16(3U, data.interaction.sender_id);
    ASSERT_EQ_U16(103U, data.interaction.receiver_id);
    ASSERT_EQ_SIZE(3U, data.interaction.data_length);
    ASSERT_BYTES(&interaction[6], data.interaction.data, 3U);
    ASSERT_EQ_U16(REFEREE_CMD_STUDENT_INTERACTION,
                  data.last_command_id);
    ASSERT_EQ_U8(13U, data.last_sequence);

    ASSERT_TRUE(RefereeGetDiagnostics(&referee, &diagnostics));
    ASSERT_EQ_U32(13U, diagnostics.valid_frame_count);
    ASSERT_EQ_U32(0U, diagnostics.payload_length_error_count);
    ASSERT_EQ_U32(0U, diagnostics.unsupported_command_count);
}

static void TestKnownCommandWithWrongLengthKeepsPreviousValue(void)
{
    Referee referee;
    RefereeData before;
    RefereeData after;
    RefereeDiagnostics diagnostics;
    TestContext context;
    RefereeConfig config;
    static const uint8_t valid[] = {4U};
    static const uint8_t wrong[] = {9U, 8U};
    static const uint8_t unknown[] = {1U, 2U, 3U};

    ResetContext(&context);
    config = MakeConfig(&context);
    ASSERT_TRUE(RefereeInit(&referee, &config));
    FeedFrame(&referee, REFEREE_CMD_GAME_RESULT,
              valid, sizeof(valid), 1U);
    ASSERT_TRUE(RefereeGetData(&referee, &before));
    FeedFrame(&referee, REFEREE_CMD_GAME_RESULT,
              wrong, sizeof(wrong), 2U);
    FeedFrame(&referee, 0x7777U, unknown, sizeof(unknown), 3U);
    ASSERT_TRUE(RefereeGetData(&referee, &after));

    ASSERT_EQ_U8(4U, before.game_result.winner);
    ASSERT_EQ_U8(4U, after.game_result.winner);
    ASSERT_TRUE(after.has_game_result);
    ASSERT_EQ_U16(0x7777U, after.last_command_id);
    ASSERT_EQ_U8(3U, after.last_sequence);
    ASSERT_TRUE(RefereeGetDiagnostics(&referee, &diagnostics));
    ASSERT_EQ_U32(3U, diagnostics.valid_frame_count);
    ASSERT_EQ_U32(1U, diagnostics.payload_length_error_count);
    ASSERT_EQ_U32(1U, diagnostics.unsupported_command_count);
}

static void TestInteractionAcceptsZeroThrough113DataBytes(void)
{
    Referee referee;
    RefereeData data;
    RefereeDiagnostics diagnostics;
    TestContext context;
    RefereeConfig config;
    uint8_t payload[6U + REFEREE_MAX_INTERACTION_DATA + 1U];
    size_t i;

    ResetContext(&context);
    config = MakeConfig(&context);
    ASSERT_TRUE(RefereeInit(&referee, &config));
    payload[0] = 0x01U;
    payload[1] = 0x02U;
    payload[2] = 3U;
    payload[3] = 0U;
    payload[4] = 103U;
    payload[5] = 0U;
    for (i = 0U; i < REFEREE_MAX_INTERACTION_DATA + 1U; ++i)
        payload[6U + i] = (uint8_t)i;

    FeedFrame(&referee, REFEREE_CMD_STUDENT_INTERACTION,
              payload, 6U, 1U);
    ASSERT_TRUE(RefereeGetData(&referee, &data));
    ASSERT_TRUE(data.has_interaction);
    ASSERT_EQ_SIZE(0U, data.interaction.data_length);

    FeedFrame(&referee, REFEREE_CMD_STUDENT_INTERACTION,
              payload, 6U + REFEREE_MAX_INTERACTION_DATA, 2U);
    ASSERT_TRUE(RefereeGetData(&referee, &data));
    ASSERT_EQ_SIZE(REFEREE_MAX_INTERACTION_DATA,
                   data.interaction.data_length);
    ASSERT_EQ_U8(112U,
                 data.interaction.data[REFEREE_MAX_INTERACTION_DATA - 1U]);

    FeedFrame(&referee, REFEREE_CMD_STUDENT_INTERACTION,
              payload, 5U, 3U);
    FeedFrame(&referee, REFEREE_CMD_STUDENT_INTERACTION,
              payload, sizeof(payload), 4U);
    ASSERT_TRUE(RefereeGetData(&referee, &data));
    ASSERT_EQ_SIZE(REFEREE_MAX_INTERACTION_DATA,
                   data.interaction.data_length);
    ASSERT_EQ_U8(112U,
                 data.interaction.data[REFEREE_MAX_INTERACTION_DATA - 1U]);
    ASSERT_TRUE(RefereeGetDiagnostics(&referee, &diagnostics));
    ASSERT_EQ_U32(2U, diagnostics.payload_length_error_count);
}

static void TestPowerRequiresFreshRobotStateAndPowerHeatFrames(void)
{
    Referee referee;
    RefereePower power;
    TestContext context;
    RefereeConfig config;
    static const uint8_t robot_state[13] = {
        0x03U, 0x02U, 0x34U, 0x12U, 0x78U, 0x56U, 0x10U,
        0x00U, 0x20U, 0x00U, 0x78U, 0x00U, 0x02U};
    static const uint8_t power_heat[14] = {
        0U, 0U, 0U, 0U, 0U, 0U, 0U,
        0U, 0x37U, 0x00U, 0U, 0U, 0U, 0U};

    ResetContext(&context);
    config = MakeConfig(&context);
    ASSERT_TRUE(RefereeInit(&referee, &config));
    ASSERT_TRUE(RefereeGetPower(&referee, &power));
    ASSERT_FALSE(power.valid);

    context.now_ms = 100U;
    FeedFrame(&referee, REFEREE_CMD_ROBOT_STATE,
              robot_state, sizeof(robot_state), 1U);
    ASSERT_TRUE(RefereeGetPower(&referee, &power));
    ASSERT_FALSE(power.valid);

    context.now_ms = 150U;
    FeedFrame(&referee, REFEREE_CMD_POWER_HEAT,
              power_heat, sizeof(power_heat), 2U);
    ASSERT_TRUE(RefereeGetPower(&referee, &power));
    ASSERT_TRUE(power.valid);
    ASSERT_TRUE(power.chassis_output_enabled);
    ASSERT_EQ_U16(120U, power.chassis_power_limit_w);
    ASSERT_EQ_U16(55U, power.buffer_energy_j);

    context.now_ms = 400U;
    ASSERT_TRUE(RefereeGetPower(&referee, &power));
    ASSERT_TRUE(power.valid);
    context.now_ms = 401U;
    ASSERT_TRUE(RefereeGetPower(&referee, &power));
    ASSERT_FALSE(power.valid);

    context.now_ms = 451U;
    FeedFrame(&referee, REFEREE_CMD_ROBOT_STATE,
              robot_state, sizeof(robot_state), 3U);
    ASSERT_TRUE(RefereeGetPower(&referee, &power));
    ASSERT_FALSE(power.valid);
    FeedFrame(&referee, REFEREE_CMD_POWER_HEAT,
              power_heat, sizeof(power_heat), 4U);
    ASSERT_TRUE(RefereeGetPower(&referee, &power));
    ASSERT_TRUE(power.valid);
}

static void TestOnlineTimeoutUsesUnsignedWraparoundArithmetic(void)
{
    Referee referee;
    RefereeDiagnostics diagnostics;
    TestContext context;
    RefereeConfig config;
    static const uint8_t winner[] = {1U};

    ResetContext(&context);
    config = MakeConfig(&context);
    config.offline_timeout_ms = 50U;
    ASSERT_TRUE(RefereeInit(&referee, &config));
    ASSERT_FALSE(RefereeIsOnline(&referee));

    context.now_ms = UINT32_MAX - 15U;
    FeedFrame(&referee, REFEREE_CMD_GAME_RESULT,
              winner, sizeof(winner), 1U);
    context.now_ms = 34U;
    ASSERT_TRUE(RefereeIsOnline(&referee));
    context.now_ms = 35U;
    ASSERT_FALSE(RefereeIsOnline(&referee));

    ASSERT_TRUE(RefereeGetDiagnostics(&referee, &diagnostics));
    ASSERT_TRUE(diagnostics.has_valid_frame);
    ASSERT_EQ_U32(UINT32_MAX - 15U, diagnostics.last_valid_frame_ms);
}

static void TestQueriesUseMatchingCriticalCallbacks(void)
{
    Referee referee;
    RefereeData data;
    RefereePower power;
    RefereeDiagnostics diagnostics;
    TestContext context;
    RefereeConfig config;
    static const uint8_t winner[] = {1U};

    ResetContext(&context);
    config = MakeConfig(&context);
    config.enter_critical = FakeEnterCritical;
    config.exit_critical = FakeExitCritical;
    ASSERT_TRUE(RefereeInit(&referee, &config));
    context.now_ms = 10U;
    FeedFrame(&referee, REFEREE_CMD_GAME_RESULT,
              winner, sizeof(winner), 1U);

    ASSERT_TRUE(RefereeGetData(&referee, &data));
    AssertCriticalBalanced(&context, 1U);
    ASSERT_TRUE(RefereeGetPower(&referee, &power));
    AssertCriticalBalanced(&context, 2U);
    ASSERT_TRUE(RefereeGetDiagnostics(&referee, &diagnostics));
    AssertCriticalBalanced(&context, 3U);
    ASSERT_TRUE(RefereeIsOnline(&referee));
    AssertCriticalBalanced(&context, 4U);
    ASSERT_EQ_U32(2U, context.now_in_critical_count);
}

static void TestNullInputsAreRejectedWithoutChangingState(void)
{
    Referee referee;
    Referee uninitialized = {0};
    RefereeData before;
    RefereeData after;
    RefereeDiagnostics diagnostics;
    RefereePower power;
    TestContext context;
    RefereeConfig config;
    static const uint8_t winner[] = {7U};

    ASSERT_FALSE(RefereeFeed(&uninitialized, winner, sizeof(winner)));
    ResetContext(&context);
    config = MakeConfig(&context);
    ASSERT_TRUE(RefereeInit(&referee, &config));
    FeedFrame(&referee, REFEREE_CMD_GAME_RESULT,
              winner, sizeof(winner), 1U);
    ASSERT_TRUE(RefereeGetData(&referee, &before));

    ASSERT_FALSE(RefereeFeed(NULL, winner, sizeof(winner)));
    ASSERT_FALSE(RefereeFeed(&referee, NULL, sizeof(winner)));
    ASSERT_FALSE(RefereeFeed(&referee, winner, 0U));
    ASSERT_FALSE(RefereeGetData(NULL, &after));
    ASSERT_FALSE(RefereeGetData(&referee, NULL));
    ASSERT_FALSE(RefereeGetPower(NULL, &power));
    ASSERT_FALSE(RefereeGetPower(&referee, NULL));
    ASSERT_FALSE(RefereeGetDiagnostics(NULL, &diagnostics));
    ASSERT_FALSE(RefereeGetDiagnostics(&referee, NULL));
    ASSERT_FALSE(RefereeIsOnline(NULL));
    ASSERT_TRUE(RefereeGetData(&referee, &after));
    ASSERT_EQ_U8(before.game_result.winner, after.game_result.winner);
    ASSERT_EQ_U16(0x0103U, RefereeClientId(3U));
    ASSERT_EQ_U16(0x0167U, RefereeClientId(103U));
}

int main(void)
{
    TestCrcKnownVectorsAndNullHandling();
    TestInitRejectsMissingCallbacksAndMismatchedCriticalPair();
    TestSplitAndCoalescedFramesDecodeWithoutLosingBytes();
    TestNoiseBadCrcAndOversizeFramesUpdateDiagnostics();
    TestEverySupportedCommandDecodesExactLittleEndianFields();
    TestKnownCommandWithWrongLengthKeepsPreviousValue();
    TestInteractionAcceptsZeroThrough113DataBytes();
    TestPowerRequiresFreshRobotStateAndPowerHeatFrames();
    TestOnlineTimeoutUsesUnsignedWraparoundArithmetic();
    TestQueriesUseMatchingCriticalCallbacks();
    TestNullInputsAreRejectedWithoutChangingState();

    puts("PASS: portable referee receive tests");
    return EXIT_SUCCESS;
}
