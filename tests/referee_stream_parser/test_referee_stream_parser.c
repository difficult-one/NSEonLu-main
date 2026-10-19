#include "referee_stream_parser.h"
#include "crc_ref.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    uint16_t command_id;
    uint16_t payload_length;
    uint8_t sequence;
    uint8_t payload[247];
} CapturedFrame_s;

static CapturedFrame_s captured[8];
static size_t captured_count;
static int failures;

#define ASSERT_TRUE(value) do { if (!(value)) { ++failures; } } while (0)
#define ASSERT_EQ_U32(expected, actual) \
    do { if ((uint32_t)(expected) != (uint32_t)(actual)) { ++failures; } } while (0)

static void CaptureFrame(void *context,
                         uint16_t command_id,
                         const uint8_t *payload,
                         uint16_t payload_length,
                         uint8_t sequence)
{
    (void)context;
    ASSERT_TRUE(captured_count < 8U);
    if (captured_count >= 8U)
        return;
    CapturedFrame_s *frame = &captured[captured_count++];
    frame->command_id = command_id;
    frame->payload_length = payload_length;
    frame->sequence = sequence;
    ASSERT_TRUE(payload_length <= sizeof(frame->payload));
    if (payload_length <= sizeof(frame->payload))
        memcpy(frame->payload, payload, payload_length);
}

static size_t BuildFrame(uint16_t command_id,
                         uint8_t sequence,
                         const uint8_t *payload,
                         uint16_t payload_length,
                         uint8_t *frame)
{
    const size_t frame_length = (size_t)payload_length + 9U;
    frame[0] = 0xA5U;
    frame[1] = (uint8_t)payload_length;
    frame[2] = (uint8_t)(payload_length >> 8);
    frame[3] = sequence;
    Append_CRC8_Check_Sum(frame, 5U);
    frame[5] = (uint8_t)command_id;
    frame[6] = (uint8_t)(command_id >> 8);
    memcpy(&frame[7], payload, payload_length);
    Append_CRC16_Check_Sum(frame, (uint32_t)frame_length);
    return frame_length;
}

static void TestParsesTwoFramesFromOneChunk(void)
{
    RefereeStreamParser_s parser;
    uint8_t bytes[64];
    const uint8_t first_payload[] = {0x11U, 0x22U};
    const uint8_t second_payload[] = {0x33U};
    size_t used = BuildFrame(0x0201U, 7U, first_payload, 2U, bytes);
    used += BuildFrame(0x0202U, 8U, second_payload, 1U, &bytes[used]);

    captured_count = 0U;
    ASSERT_TRUE(RefereeStreamParserInit(&parser, CaptureFrame, NULL));
    RefereeStreamParserFeed(&parser, bytes, used);

    ASSERT_EQ_U32(2U, captured_count);
    ASSERT_EQ_U32(0x0201U, captured[0].command_id);
    ASSERT_EQ_U32(7U, captured[0].sequence);
    ASSERT_EQ_U32(0x22U, captured[0].payload[1]);
    ASSERT_EQ_U32(0x0202U, captured[1].command_id);
}

static void TestPreservesFrameSplitAcrossChunks(void)
{
    RefereeStreamParser_s parser;
    uint8_t bytes[32];
    const uint8_t payload[] = {1U, 2U, 3U};
    const size_t length = BuildFrame(0x0202U, 9U, payload, 3U, bytes);

    captured_count = 0U;
    ASSERT_TRUE(RefereeStreamParserInit(&parser, CaptureFrame, NULL));
    RefereeStreamParserFeed(&parser, bytes, 4U);
    ASSERT_EQ_U32(0U, captured_count);
    RefereeStreamParserFeed(&parser, &bytes[4], length - 4U);
    ASSERT_EQ_U32(1U, captured_count);
    ASSERT_EQ_U32(3U, captured[0].payload_length);
}

static void TestResynchronizesAfterNoiseAndBadCrc(void)
{
    RefereeStreamParser_s parser;
    uint8_t bytes[80] = {0x10U, 0x20U, 0x30U};
    const uint8_t payload[] = {0x5AU};
    size_t used = 3U;
    const size_t bad_offset = used;
    used += BuildFrame(0x0201U, 1U, payload, 1U, &bytes[used]);
    bytes[bad_offset + 7U] ^= 0x01U;
    used += BuildFrame(0x0202U, 2U, payload, 1U, &bytes[used]);

    captured_count = 0U;
    ASSERT_TRUE(RefereeStreamParserInit(&parser, CaptureFrame, NULL));
    RefereeStreamParserFeed(&parser, bytes, used);
    const RefereeStreamParserStats_s *stats = RefereeStreamParserGetStats(&parser);

    ASSERT_EQ_U32(1U, captured_count);
    ASSERT_EQ_U32(0x0202U, captured[0].command_id);
    ASSERT_EQ_U32(1U, stats->crc16_error_count);
    ASSERT_TRUE(stats->discarded_byte_count >= 3U);
}

static void TestRejectsBadHeaderCrcThenFindsGoodFrame(void)
{
    RefereeStreamParser_s parser;
    uint8_t bytes[48];
    const uint8_t payload[] = {0x66U};
    size_t used = BuildFrame(0x0201U, 1U, payload, 1U, bytes);
    bytes[4] ^= 0x01U;
    used += BuildFrame(0x0202U, 2U, payload, 1U, &bytes[used]);

    captured_count = 0U;
    ASSERT_TRUE(RefereeStreamParserInit(&parser, CaptureFrame, NULL));
    RefereeStreamParserFeed(&parser, bytes, used);
    const RefereeStreamParserStats_s *stats = RefereeStreamParserGetStats(&parser);

    ASSERT_EQ_U32(1U, captured_count);
    ASSERT_EQ_U32(0x0202U, captured[0].command_id);
    ASSERT_EQ_U32(1U, stats->crc8_error_count);
}

static void TestRejectsOversize16BitLengthThenFindsGoodFrame(void)
{
    RefereeStreamParser_s parser;
    uint8_t bytes[32] = {0xA5U, 0x00U, 0x01U, 0x03U, 0U};
    const uint8_t payload[] = {0x44U};
    Append_CRC8_Check_Sum(bytes, 5U);
    size_t used = 5U;
    used += BuildFrame(0x0201U, 4U, payload, 1U, &bytes[used]);

    captured_count = 0U;
    ASSERT_TRUE(RefereeStreamParserInit(&parser, CaptureFrame, NULL));
    RefereeStreamParserFeed(&parser, bytes, used);
    const RefereeStreamParserStats_s *stats = RefereeStreamParserGetStats(&parser);

    ASSERT_EQ_U32(1U, captured_count);
    ASSERT_EQ_U32(1U, stats->valid_frame_count);
    ASSERT_EQ_U32(1U, stats->oversize_frame_count);
}

static void TestParsesMaximumCapacityFrame(void)
{
    RefereeStreamParser_s parser;
    uint8_t payload[247];
    uint8_t bytes[256];
    memset(payload, 0x5AU, sizeof(payload));
    payload[246] = 0xA6U;
    const size_t length = BuildFrame(0x0201U, 5U, payload, 247U, bytes);

    captured_count = 0U;
    ASSERT_EQ_U32(256U, length);
    ASSERT_TRUE(RefereeStreamParserInit(&parser, CaptureFrame, NULL));
    RefereeStreamParserFeed(&parser, bytes, length);
    const RefereeStreamParserStats_s *stats = RefereeStreamParserGetStats(&parser);

    ASSERT_EQ_U32(1U, captured_count);
    ASSERT_EQ_U32(247U, captured[0].payload_length);
    ASSERT_EQ_U32(0xA6U, captured[0].payload[246]);
    ASSERT_EQ_U32(1U, stats->valid_frame_count);
    ASSERT_EQ_U32(0U, stats->oversize_frame_count);
}

static void TestRejectsFrameLargerThanCapacityThenFindsGoodFrame(void)
{
    RefereeStreamParser_s parser;
    uint8_t bytes[32] = {0xA5U, 0xF8U, 0x00U, 6U, 0U};
    const uint8_t payload[] = {0x77U};
    Append_CRC8_Check_Sum(bytes, 5U);
    size_t used = 5U;
    used += BuildFrame(0x0202U, 7U, payload, 1U, &bytes[used]);

    captured_count = 0U;
    ASSERT_TRUE(RefereeStreamParserInit(&parser, CaptureFrame, NULL));
    RefereeStreamParserFeed(&parser, bytes, used);
    const RefereeStreamParserStats_s *stats = RefereeStreamParserGetStats(&parser);

    ASSERT_EQ_U32(1U, captured_count);
    ASSERT_EQ_U32(0x0202U, captured[0].command_id);
    ASSERT_EQ_U32(1U, stats->valid_frame_count);
    ASSERT_EQ_U32(1U, stats->oversize_frame_count);
}

static void TestEscapesIncompleteFalseFrameForCompleteValidFrame(void)
{
    RefereeStreamParser_s parser;
    uint8_t bytes[32] = {0xA5U, 200U, 0U, 6U, 0U};
    const uint8_t payload[] = {0x78U};
    Append_CRC8_Check_Sum(bytes, 5U);
    size_t used = 5U;
    used += BuildFrame(0x0202U, 7U, payload, 1U, &bytes[used]);

    captured_count = 0U;
    ASSERT_TRUE(RefereeStreamParserInit(&parser, CaptureFrame, NULL));
    RefereeStreamParserFeed(&parser, bytes, used);
    const RefereeStreamParserStats_s *stats = RefereeStreamParserGetStats(&parser);

    if (captured_count != 1U)
        printf("FAIL incomplete false frame: expected 1 frame, got %zu\n",
               captured_count);
    ASSERT_EQ_U32(1U, captured_count);
    ASSERT_EQ_U32(0x0202U, captured[0].command_id);
    ASSERT_EQ_U32(1U, stats->valid_frame_count);
    ASSERT_EQ_U32(5U, stats->discarded_byte_count);
}

static void TestIncompleteOuterFrameKeepsBadCrc16CandidateInPayload(void)
{
    RefereeStreamParser_s parser;
    uint8_t outer_payload[20] = {0};
    uint8_t outer_frame[29];
    const uint8_t inner_payload[] = {0x55U};
    const size_t inner_length =
        BuildFrame(0x0102U, 0x11U, inner_payload, 1U, outer_payload);
    outer_payload[inner_length - 1U] ^= 0x01U;
    outer_payload[19] = 0xCCU;
    const size_t outer_length =
        BuildFrame(0x0301U, 0x22U, outer_payload, 20U, outer_frame);
    const size_t first_chunk_length = 7U + inner_length;

    ASSERT_TRUE(Verify_CRC8_Check_Sum(outer_payload, 5U));
    ASSERT_TRUE(!Verify_CRC16_Check_Sum(outer_payload,
                                        (uint32_t)inner_length));
    captured_count = 0U;
    ASSERT_TRUE(RefereeStreamParserInit(&parser, CaptureFrame, NULL));
    RefereeStreamParserFeed(&parser, outer_frame, first_chunk_length);
    const RefereeStreamParserStats_s *stats = RefereeStreamParserGetStats(&parser);

    ASSERT_EQ_U32(0U, captured_count);
    ASSERT_EQ_U32(0U, stats->discarded_byte_count);

    RefereeStreamParserFeed(&parser,
                            &outer_frame[first_chunk_length],
                            outer_length - first_chunk_length);

    ASSERT_EQ_U32(1U, captured_count);
    ASSERT_EQ_U32(0x0301U, captured[0].command_id);
    ASSERT_EQ_U32(20U, captured[0].payload_length);
    ASSERT_EQ_U32(0x22U, captured[0].sequence);
    ASSERT_TRUE(memcmp(captured[0].payload,
                       outer_payload,
                       sizeof(outer_payload)) == 0);
    ASSERT_EQ_U32(1U, stats->valid_frame_count);
    ASSERT_EQ_U32(0U, stats->discarded_byte_count);
}

static void TestParsesLargeFeedAcrossInternalBufferBoundaries(void)
{
    RefereeStreamParser_s parser;
    uint8_t bytes[510];
    const uint8_t first_payload[] = {0x12U};
    const uint8_t second_payload[] = {0x34U};
    memset(bytes, 0x10, sizeof(bytes));
    size_t used = 250U;
    used += BuildFrame(0x0201U, 8U, first_payload, 1U, &bytes[used]);
    used += 240U;
    used += BuildFrame(0x0202U, 9U, second_payload, 1U, &bytes[used]);

    captured_count = 0U;
    ASSERT_EQ_U32(510U, used);
    ASSERT_TRUE(RefereeStreamParserInit(&parser, CaptureFrame, NULL));
    RefereeStreamParserFeed(&parser, bytes, used);
    const RefereeStreamParserStats_s *stats = RefereeStreamParserGetStats(&parser);

    ASSERT_EQ_U32(2U, captured_count);
    ASSERT_EQ_U32(0x0201U, captured[0].command_id);
    ASSERT_EQ_U32(0x0202U, captured[1].command_id);
    ASSERT_EQ_U32(2U, stats->valid_frame_count);
    ASSERT_EQ_U32(490U, stats->discarded_byte_count);
}

static void TestResetClearsPendingBytesAndPreservesConfigurationAndStats(void)
{
    RefereeStreamParser_s parser;
    int context;
    uint8_t bad_header[5] = {0xA5U, 0U, 0U, 10U, 0U};
    Append_CRC8_Check_Sum(bad_header, 5U);
    bad_header[4] ^= 0x01U;

    captured_count = 0U;
    ASSERT_TRUE(RefereeStreamParserInit(&parser, CaptureFrame, &context));
    RefereeStreamParserFeed(&parser, bad_header, sizeof(bad_header));
    RefereeStreamParserFeed(&parser, bad_header, 4U);
    ASSERT_EQ_U32(4U, parser.used);

    RefereeStreamParserReset(&parser);
    const RefereeStreamParserStats_s *stats = RefereeStreamParserGetStats(&parser);

    ASSERT_EQ_U32(0U, parser.used);
    ASSERT_EQ_U32(1U, stats->crc8_error_count);
    ASSERT_TRUE(parser.handler == CaptureFrame);
    ASSERT_TRUE(parser.context == &context);
    ASSERT_EQ_U32(0U, captured_count);
}

static void TestNullAndZeroLengthInputsAreSafe(void)
{
    RefereeStreamParser_s parser;
    const uint8_t byte = 0xA5U;

    captured_count = 0U;
    ASSERT_TRUE(!RefereeStreamParserInit(NULL, CaptureFrame, NULL));
    ASSERT_TRUE(!RefereeStreamParserInit(&parser, NULL, NULL));
    RefereeStreamParserReset(NULL);
    ASSERT_TRUE(RefereeStreamParserInit(&parser, CaptureFrame, NULL));
    RefereeStreamParserFeed(NULL, &byte, 1U);
    RefereeStreamParserFeed(&parser, NULL, 1U);
    RefereeStreamParserFeed(&parser, &byte, 0U);

    ASSERT_EQ_U32(0U, parser.used);
    ASSERT_EQ_U32(0U, captured_count);
    ASSERT_TRUE(RefereeStreamParserGetStats(NULL) == NULL);
}

int main(void)
{
    TestParsesTwoFramesFromOneChunk();
    TestPreservesFrameSplitAcrossChunks();
    TestResynchronizesAfterNoiseAndBadCrc();
    TestRejectsBadHeaderCrcThenFindsGoodFrame();
    TestRejectsOversize16BitLengthThenFindsGoodFrame();
    TestParsesMaximumCapacityFrame();
    TestRejectsFrameLargerThanCapacityThenFindsGoodFrame();
    TestEscapesIncompleteFalseFrameForCompleteValidFrame();
    TestIncompleteOuterFrameKeepsBadCrc16CandidateInPayload();
    TestParsesLargeFeedAcrossInternalBufferBoundaries();
    TestResetClearsPendingBytesAndPreservesConfigurationAndStats();
    TestNullAndZeroLengthInputsAreSafe();

    if (failures != 0)
        return 1;

    puts("PASS: referee stream parser tests");
    return 0;
}
