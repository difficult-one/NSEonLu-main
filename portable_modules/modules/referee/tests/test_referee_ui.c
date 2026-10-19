#include "referee.h"
#include "referee_crc.h"
#include "referee_ui.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define ASSERT_EQ_SIZE(expected, actual)                                     \
    do                                                                       \
    {                                                                        \
        if ((size_t)(expected) != (size_t)(actual))                          \
            FAIL(#actual " did not equal " #expected);                     \
    } while (0)

#define ASSERT_BYTES(expected, actual, length)                               \
    do                                                                       \
    {                                                                        \
        if (memcmp((expected), (actual), (length)) != 0)                     \
            FAIL(#actual " did not match " #expected);                     \
    } while (0)

typedef struct
{
    uint8_t frame[REFEREE_MAX_FRAME_SIZE];
    size_t length;
    size_t send_count;
    unsigned critical_depth;
    unsigned enter_count;
    unsigned exit_count;
    unsigned send_in_critical_count;
    bool send_result;
    const uint8_t *last_data_pointer;
} TestContext;

static bool FakeSend(void *context, const uint8_t *data, size_t length)
{
    TestContext *test = context;

    ++test->send_count;
    if (test->critical_depth != 0U)
        ++test->send_in_critical_count;
    if (data == NULL || length > sizeof(test->frame))
        return false;
    test->last_data_pointer = data;
    memcpy(test->frame, data, length);
    test->length = length;
    return test->send_result;
}

static uint32_t FakeNow(void *context)
{
    (void)context;
    return 0U;
}

static void FakeEnterCritical(void *context)
{
    TestContext *test = context;
    ++test->critical_depth;
    ++test->enter_count;
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
    const RefereeConfig config = {
        .uart_send = FakeSend,
        .now_ms = FakeNow,
        .context = context,
    };
    return config;
}

static void ResetContext(TestContext *context)
{
    memset(context, 0, sizeof(*context));
    context->send_result = true;
}

static uint16_t ReadU16LE(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
                      ((uint16_t)bytes[1] << 8U));
}

static void AssertFrameEnvelope(const TestContext *context,
                                size_t frame_length,
                                uint16_t payload_length,
                                uint8_t sequence,
                                uint16_t data_command_id,
                                uint16_t sender_id,
                                uint16_t receiver_id)
{
    ASSERT_EQ_SIZE(frame_length, context->length);
    ASSERT_EQ_U8(REFEREE_SOF, context->frame[0]);
    ASSERT_EQ_U16(payload_length, ReadU16LE(&context->frame[1]));
    ASSERT_EQ_U8(sequence, context->frame[3]);
    ASSERT_TRUE(RefereeCrc8Verify(context->frame, REFEREE_HEADER_SIZE));
    ASSERT_EQ_U16(REFEREE_CMD_STUDENT_INTERACTION,
                  ReadU16LE(&context->frame[5]));
    ASSERT_EQ_U16(data_command_id, ReadU16LE(&context->frame[7]));
    ASSERT_EQ_U16(sender_id, ReadU16LE(&context->frame[9]));
    ASSERT_EQ_U16(receiver_id, ReadU16LE(&context->frame[11]));
    ASSERT_TRUE(RefereeCrc16Verify(context->frame, context->length));
}

static void TestLineUsesVerifiedLittleEndianGraphicLayout(void)
{
    static const uint8_t expected_graphic[15] = {
        0x43U, 0x42U, 0x41U, 0x81U, 0x08U,
        0x00U, 0x00U, 0x05U, 0x90U, 0x01U,
        0x19U, 0x00U, 0xB0U, 0x04U, 0x32U};
    TestContext context;
    Referee referee;
    RefereeGraphic line;
    RefereeConfig config;

    ResetContext(&context);
    config = MakeConfig(&context);
    ASSERT_TRUE(RefereeInit(&referee, &config));
    ASSERT_TRUE(RefereeUiMakeLine(&line, "ABC", REFEREE_UI_ADD, 2U,
                                  REFEREE_UI_GREEN, 5U,
                                  100U, 200U, 300U, 400U));
    ASSERT_TRUE(RefereeUiSendGraphics(&referee, 3U, 0x0103U,
                                      &line, 1U));
    AssertFrameEnvelope(&context, 30U, 21U, 0U, 0x0101U,
                        3U, 0x0103U);
    ASSERT_BYTES(expected_graphic, &context.frame[13],
                 sizeof(expected_graphic));
}

static void TestArcFloatAndIntegerMatchArmGccOracle(void)
{
    static const uint8_t expected_arc[15] = {
        0x43U, 0x52U, 0x41U, 0xE2U, 0x8CU,
        0x07U, 0x3CU, 0x04U, 0xD0U, 0x07U,
        0x4BU, 0x00U, 0x20U, 0x83U, 0x0CU};
    static const uint8_t expected_float[15] = {
        0x54U, 0x4CU, 0x46U, 0x29U, 0x09U,
        0x85U, 0x01U, 0x02U, 0x80U, 0x8CU,
        0x70U, 0x39U, 0x30U, 0x00U, 0x00U};
    static const uint8_t expected_integer[15] = {
        0x54U, 0x4EU, 0x49U, 0x71U, 0x21U,
        0x06U, 0x00U, 0x02U, 0xA0U, 0x8FU,
        0x57U, 0x78U, 0x56U, 0x34U, 0x12U};
    TestContext context;
    Referee referee;
    RefereeGraphic graphic;
    RefereeConfig config;

    ResetContext(&context);
    config = MakeConfig(&context);
    ASSERT_TRUE(RefereeInit(&referee, &config));

    ASSERT_TRUE(RefereeUiMakeArc(&graphic, "ARC", REFEREE_UI_CHANGE,
                                 3U, REFEREE_UI_ORANGE, 30U, 120U,
                                 4U, 500U, 600U, 200U, 100U));
    ASSERT_TRUE(RefereeUiSendGraphics(&referee, 3U, 0x0103U,
                                      &graphic, 1U));
    ASSERT_BYTES(expected_arc, &context.frame[13], sizeof(expected_arc));

    ASSERT_TRUE(RefereeUiMakeFloat(&graphic, "FLT", REFEREE_UI_ADD,
                                   4U, REFEREE_UI_GREEN, 20U, 3U,
                                   2U, 800U, 900U, 12345));
    ASSERT_TRUE(RefereeUiSendGraphics(&referee, 3U, 0x0103U,
                                      &graphic, 1U));
    ASSERT_EQ_U8(1U, context.frame[3]);
    ASSERT_BYTES(expected_float, &context.frame[13],
                 sizeof(expected_float));

    ASSERT_TRUE(RefereeUiMakeInteger(&graphic, "INT", REFEREE_UI_ADD,
                                     5U, REFEREE_UI_WHITE, 24U, 2U,
                                     1000U, 700U, 0x12345678));
    ASSERT_TRUE(RefereeUiSendGraphics(&referee, 3U, 0x0103U,
                                      &graphic, 1U));
    ASSERT_EQ_U8(2U, context.frame[3]);
    ASSERT_BYTES(expected_integer, &context.frame[13],
                 sizeof(expected_integer));
}

static void TestRectangleCircleAndEllipseBuildersSetEveryField(void)
{
    RefereeGraphic graphic;

    memset(&graphic, 0xA5, sizeof(graphic));
    ASSERT_TRUE(RefereeUiMakeRectangle(&graphic, "REC",
                                       REFEREE_UI_CHANGE, 1U,
                                       REFEREE_UI_YELLOW, 7U,
                                       11U, 22U, 33U, 44U));
    ASSERT_BYTES("CER", graphic.name, 3U);
    ASSERT_EQ_U8(REFEREE_UI_RECTANGLE, graphic.type);
    ASSERT_EQ_U16(0U, graphic.start_angle);
    ASSERT_EQ_U16(0U, graphic.end_angle);
    ASSERT_EQ_U16(0U, graphic.radius);
    ASSERT_EQ_U16(33U, graphic.end_x);
    ASSERT_EQ_U16(44U, graphic.end_y);

    memset(&graphic, 0xA5, sizeof(graphic));
    ASSERT_TRUE(RefereeUiMakeCircle(&graphic, "CIR", REFEREE_UI_ADD,
                                    2U, REFEREE_UI_CYAN, 8U,
                                    100U, 200U, 300U));
    ASSERT_BYTES("RIC", graphic.name, 3U);
    ASSERT_EQ_U8(REFEREE_UI_CIRCLE, graphic.type);
    ASSERT_EQ_U16(300U, graphic.radius);
    ASSERT_EQ_U16(0U, graphic.end_x);
    ASSERT_EQ_U16(0U, graphic.end_y);

    memset(&graphic, 0xA5, sizeof(graphic));
    ASSERT_TRUE(RefereeUiMakeEllipse(&graphic, "ELP",
                                     REFEREE_UI_DELETE, 3U,
                                     REFEREE_UI_PINK, 9U,
                                     400U, 500U, 60U, 70U));
    ASSERT_BYTES("PLE", graphic.name, 3U);
    ASSERT_EQ_U8(REFEREE_UI_ELLIPSE, graphic.type);
    ASSERT_EQ_U16(0U, graphic.radius);
    ASSERT_EQ_U16(60U, graphic.end_x);
    ASSERT_EQ_U16(70U, graphic.end_y);
}

static void AssertGraphicUnchanged(const RefereeGraphic *expected,
                                   const RefereeGraphic *actual)
{
    ASSERT_BYTES(expected, actual, sizeof(*expected));
}

static void TestBuildersRejectOutOfRangeWithoutChangingOutput(void)
{
    static const char too_long[] = "1234567890123456789012345678901";
    RefereeGraphic sentinel;
    RefereeGraphic graphic;
    RefereeText text_sentinel;
    RefereeText text;

    memset(&sentinel, 0xA5, sizeof(sentinel));
    graphic = sentinel;
    ASSERT_FALSE(RefereeUiMakeLine(&graphic, "BAD", 8U, 0U, 0U,
                                   1U, 1U, 1U, 1U, 1U));
    AssertGraphicUnchanged(&sentinel, &graphic);
    ASSERT_FALSE(RefereeUiMakeRectangle(&graphic, "BAD", 1U, 16U, 0U,
                                        1U, 1U, 1U, 1U, 1U));
    AssertGraphicUnchanged(&sentinel, &graphic);
    ASSERT_FALSE(RefereeUiMakeCircle(&graphic, "BAD", 1U, 0U, 16U,
                                     1U, 1U, 1U, 1U));
    AssertGraphicUnchanged(&sentinel, &graphic);
    ASSERT_FALSE(RefereeUiMakeEllipse(&graphic, "BAD", 1U, 0U, 0U,
                                      1024U, 1U, 1U, 1U, 1U));
    AssertGraphicUnchanged(&sentinel, &graphic);
    ASSERT_FALSE(RefereeUiMakeLine(&graphic, "BAD", 1U, 0U, 0U,
                                   1U, 2048U, 1U, 1U, 1U));
    AssertGraphicUnchanged(&sentinel, &graphic);
    ASSERT_FALSE(RefereeUiMakeCircle(&graphic, "BAD", 1U, 0U, 0U,
                                     1U, 1U, 1U, 1024U));
    AssertGraphicUnchanged(&sentinel, &graphic);
    ASSERT_FALSE(RefereeUiMakeArc(&graphic, "BAD", 1U, 0U, 0U,
                                  512U, 1U, 1U, 1U, 1U, 1U, 1U));
    AssertGraphicUnchanged(&sentinel, &graphic);
    ASSERT_FALSE(RefereeUiMakeFloat(&graphic, "BAD", 1U, 0U, 0U,
                                    1U, 512U, 1U, 1U, 1U, 0));
    AssertGraphicUnchanged(&sentinel, &graphic);
    ASSERT_FALSE(RefereeUiMakeInteger(&graphic, "BAD", 1U, 0U, 0U,
                                      512U, 1U, 1U, 1U, 0));
    AssertGraphicUnchanged(&sentinel, &graphic);
    ASSERT_FALSE(RefereeUiMakeLine(NULL, "BAD", 1U, 0U, 0U,
                                   1U, 1U, 1U, 1U, 1U));
    ASSERT_FALSE(RefereeUiMakeLine(&graphic, NULL, 1U, 0U, 0U,
                                   1U, 1U, 1U, 1U, 1U));
    AssertGraphicUnchanged(&sentinel, &graphic);

    memset(&text_sentinel, 0x5A, sizeof(text_sentinel));
    text = text_sentinel;
    ASSERT_FALSE(RefereeUiMakeText(&text, "BAD", 1U, 0U, 0U,
                                   1U, 1U, 1U, 1U, too_long));
    ASSERT_BYTES(&text_sentinel, &text, sizeof(text));
    ASSERT_FALSE(RefereeUiMakeText(&text, "BAD", 1U, 0U, 0U,
                                   1U, 1U, 1U, 1U, NULL));
    ASSERT_BYTES(&text_sentinel, &text, sizeof(text));
}

static void TestDeleteFrameMatchesLiteralGolden(void)
{
    static const uint8_t expected_frame[17] = {
        0xA5U, 0x08U, 0x00U, 0x00U, 0xE6U, 0x01U, 0x03U,
        0x00U, 0x01U, 0x03U, 0x00U, 0x03U, 0x01U, 0x02U,
        0x07U, 0x5BU, 0x27U};
    TestContext context;
    Referee referee;
    RefereeConfig config;

    ResetContext(&context);
    config = MakeConfig(&context);
    ASSERT_TRUE(RefereeInit(&referee, &config));
    ASSERT_TRUE(RefereeUiDelete(&referee, 3U, 0x0103U,
                                REFEREE_UI_DELETE_ALL, 7U));
    ASSERT_EQ_SIZE(1U, context.send_count);
    ASSERT_BYTES(expected_frame, context.frame, sizeof(expected_frame));

    ASSERT_FALSE(RefereeUiDelete(&referee, 3U, 0x0103U, 3U, 0U));
    ASSERT_FALSE(RefereeUiDelete(&referee, 3U, 0x0103U,
                                 REFEREE_UI_DELETE_LAYER, 16U));
    ASSERT_EQ_SIZE(1U, context.send_count);
    ASSERT_EQ_U8(1U, referee.tx_sequence);
}

static void TestOnlyOneTwoFiveAndSevenGraphicsAreAccepted(void)
{
    static const size_t counts[] = {1U, 2U, 5U, 7U};
    static const uint16_t commands[] = {
        0x0101U, 0x0102U, 0x0103U, 0x0104U};
    static const size_t invalid_counts[] = {0U, 3U, 4U, 6U, 8U};
    TestContext context;
    Referee referee;
    RefereeGraphic graphics[7];
    RefereeConfig config;
    size_t i;

    ResetContext(&context);
    config = MakeConfig(&context);
    ASSERT_TRUE(RefereeInit(&referee, &config));
    for (i = 0U; i < 7U; ++i)
    {
        ASSERT_TRUE(RefereeUiMakeLine(&graphics[i], "CNT",
                                      REFEREE_UI_ADD, 0U,
                                      REFEREE_UI_MAIN, 1U,
                                      1U, 2U, 3U, 4U));
    }
    for (i = 0U; i < sizeof(counts) / sizeof(counts[0]); ++i)
    {
        ASSERT_TRUE(RefereeUiSendGraphics(&referee, 3U, 0x0103U,
                                          graphics, counts[i]));
        AssertFrameEnvelope(&context, 15U + 15U * counts[i],
                            (uint16_t)(6U + 15U * counts[i]),
                            (uint8_t)i, commands[i], 3U, 0x0103U);
    }
    for (i = 0U;
         i < sizeof(invalid_counts) / sizeof(invalid_counts[0]);
         ++i)
    {
        ASSERT_FALSE(RefereeUiSendGraphics(&referee, 3U, 0x0103U,
                                           graphics, invalid_counts[i]));
    }
    ASSERT_FALSE(RefereeUiSendGraphics(&referee, 3U, 0x0103U,
                                       NULL, 1U));
    ASSERT_EQ_SIZE(4U, context.send_count);
    ASSERT_EQ_U8(4U, referee.tx_sequence);
}

static void TestTextIsBoundedZeroPaddedAndSentAsFortyFiveBytes(void)
{
    static const uint8_t expected_graphic[15] = {
        0x43U, 0x42U, 0x41U, 0xB9U, 0x08U,
        0x05U, 0x01U, 0x03U, 0x90U, 0x01U,
        0x19U, 0x00U, 0x00U, 0x00U, 0x00U};
    static const char thirty[] = "123456789012345678901234567890";
    TestContext context;
    Referee referee;
    RefereeText text;
    RefereeConfig config;
    size_t i;

    ResetContext(&context);
    config = MakeConfig(&context);
    ASSERT_TRUE(RefereeInit(&referee, &config));
    ASSERT_TRUE(RefereeUiMakeText(&text, "ABC", REFEREE_UI_ADD, 2U,
                                  REFEREE_UI_GREEN, 20U, 3U,
                                  100U, 200U, "Hi"));
    ASSERT_EQ_SIZE(2U, text.length);
    ASSERT_EQ_U16(2U, text.graphic.end_angle);
    ASSERT_EQ_U8(REFEREE_UI_TEXT, text.graphic.type);
    ASSERT_EQ_U8((uint8_t)'H', text.text[0]);
    ASSERT_EQ_U8((uint8_t)'i', text.text[1]);
    for (i = 2U; i < sizeof(text.text); ++i)
        ASSERT_EQ_U8(0U, text.text[i]);

    ASSERT_TRUE(RefereeUiSendText(&referee, 3U, 0x0103U, &text));
    AssertFrameEnvelope(&context, 60U, 51U, 0U, 0x0110U,
                        3U, 0x0103U);
    ASSERT_BYTES(expected_graphic, &context.frame[13],
                 sizeof(expected_graphic));
    ASSERT_BYTES("Hi", &context.frame[28], 2U);
    for (i = 30U; i < 58U; ++i)
        ASSERT_EQ_U8(0U, context.frame[i]);

    ASSERT_TRUE(RefereeUiMakeText(&text, "MAX", REFEREE_UI_ADD, 0U,
                                  REFEREE_UI_MAIN, 1U, 1U,
                                  1U, 1U, thirty));
    ASSERT_EQ_SIZE(30U, text.length);
    ASSERT_BYTES(thirty, text.text, 30U);
    ASSERT_FALSE(RefereeUiSendText(NULL, 3U, 0x0103U, &text));
    ASSERT_FALSE(RefereeUiSendText(&referee, 3U, 0x0103U, NULL));
    ASSERT_EQ_SIZE(1U, context.send_count);
}

static void TestInteractionAcceptsZeroAnd113ButRejects114(void)
{
    static const uint8_t expected_empty[15] = {
        0xA5U, 0x06U, 0x00U, 0x00U, 0x12U, 0x01U, 0x03U,
        0x00U, 0x02U, 0x03U, 0x00U, 0x67U, 0x00U, 0x65U, 0x01U};
    TestContext context;
    Referee referee;
    RefereeConfig config;
    uint8_t data[REFEREE_MAX_INTERACTION_DATA + 1U];
    size_t i;

    ResetContext(&context);
    config = MakeConfig(&context);
    ASSERT_TRUE(RefereeInit(&referee, &config));
    for (i = 0U; i < sizeof(data); ++i)
        data[i] = (uint8_t)i;

    ASSERT_TRUE(RefereeSendInteraction(&referee, 3U, 103U, 0x0200U,
                                       NULL, 0U));
    ASSERT_BYTES(expected_empty, context.frame, sizeof(expected_empty));

    ASSERT_TRUE(RefereeSendInteraction(&referee, 3U, 103U, 0x02FFU,
                                       data, REFEREE_MAX_INTERACTION_DATA));
    AssertFrameEnvelope(&context, 128U, 119U, 1U, 0x02FFU, 3U, 103U);
    ASSERT_BYTES(data, &context.frame[13], REFEREE_MAX_INTERACTION_DATA);

    ASSERT_FALSE(RefereeSendInteraction(&referee, 3U, 103U, 0x0200U,
                                        data,
                                        REFEREE_MAX_INTERACTION_DATA + 1U));
    ASSERT_FALSE(RefereeSendInteraction(&referee, 3U, 103U, 0x0200U,
                                        NULL, 1U));
    ASSERT_FALSE(RefereeSendInteraction(NULL, 3U, 103U, 0x0200U,
                                        NULL, 0U));
    ASSERT_EQ_SIZE(2U, context.send_count);
    ASSERT_EQ_U8(2U, referee.tx_sequence);
}

static void TestEverySendAttemptAdvancesSequenceAndCountsFailure(void)
{
    TestContext context;
    Referee referee;
    Referee uninitialized = {0};
    RefereeDiagnostics diagnostics;
    RefereeConfig config;

    ResetContext(&context);
    config = MakeConfig(&context);
    config.enter_critical = FakeEnterCritical;
    config.exit_critical = FakeExitCritical;
    ASSERT_TRUE(RefereeInit(&referee, &config));
    referee.tx_sequence = UINT8_MAX;
    context.send_result = false;

    ASSERT_FALSE(RefereeSendInteraction(&referee, 3U, 103U, 0x0200U,
                                        NULL, 0U));
    ASSERT_EQ_U8(UINT8_MAX, context.frame[3]);
    ASSERT_EQ_U8(0U, referee.tx_sequence);
    ASSERT_TRUE(context.last_data_pointer == referee.tx_buffer);
    ASSERT_EQ_SIZE(context.length, referee.tx_length);
    ASSERT_BYTES(context.frame, referee.tx_buffer, context.length);
    ASSERT_EQ_SIZE(1U, context.send_count);
    ASSERT_EQ_U32(0U, context.send_in_critical_count);
    ASSERT_TRUE(RefereeGetDiagnostics(&referee, &diagnostics));
    ASSERT_EQ_U32(1U, diagnostics.transmit_failure_count);

    context.send_result = true;
    ASSERT_TRUE(RefereeSendInteraction(&referee, 3U, 103U, 0x0200U,
                                       NULL, 0U));
    ASSERT_EQ_U8(0U, context.frame[3]);
    ASSERT_EQ_U8(1U, referee.tx_sequence);
    ASSERT_EQ_U32(0U, context.send_in_critical_count);
    ASSERT_TRUE(RefereeGetDiagnostics(&referee, &diagnostics));
    ASSERT_EQ_U32(1U, diagnostics.transmit_failure_count);
    ASSERT_EQ_U32(context.enter_count, context.exit_count);
    ASSERT_EQ_U32(0U, context.critical_depth);

    ASSERT_FALSE(RefereeSendInteraction(&uninitialized, 3U, 103U,
                                        0x0200U, NULL, 0U));
}

int main(void)
{
    TestLineUsesVerifiedLittleEndianGraphicLayout();
    TestArcFloatAndIntegerMatchArmGccOracle();
    TestRectangleCircleAndEllipseBuildersSetEveryField();
    TestBuildersRejectOutOfRangeWithoutChangingOutput();
    TestDeleteFrameMatchesLiteralGolden();
    TestOnlyOneTwoFiveAndSevenGraphicsAreAccepted();
    TestTextIsBoundedZeroPaddedAndSentAsFortyFiveBytes();
    TestInteractionAcceptsZeroAnd113ButRejects114();
    TestEverySendAttemptAdvancesSequenceAndCountsFailure();

    puts("PASS: portable referee UI tests");
    return EXIT_SUCCESS;
}
