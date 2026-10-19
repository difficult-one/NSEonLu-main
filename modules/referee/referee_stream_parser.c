#include "referee_stream_parser.h"

#include "crc_ref.h"

#include <string.h>

#define REFEREE_SOF 0xA5U
#define REFEREE_HEADER_LENGTH 5U
#define REFEREE_COMMAND_LENGTH 2U
#define REFEREE_TAIL_LENGTH 2U
#define REFEREE_FRAME_OVERHEAD \
    (REFEREE_HEADER_LENGTH + REFEREE_COMMAND_LENGTH + REFEREE_TAIL_LENGTH)

static uint16_t ReadU16LE(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static void RemovePrefix(RefereeStreamParser_s *parser, size_t count)
{
    if (count >= parser->used)
    {
        parser->used = 0U;
        return;
    }

    memmove(parser->buffer, &parser->buffer[count], parser->used - count);
    parser->used -= count;
}

static void DiscardPrefix(RefereeStreamParser_s *parser, size_t count)
{
    const size_t discarded = count < parser->used ? count : parser->used;
    RemovePrefix(parser, count);
    parser->stats.discarded_byte_count += (uint32_t)discarded;
}

static size_t FindCompleteValidFrameOffset(RefereeStreamParser_s *parser)
{
    for (size_t offset = 1U;
         offset + REFEREE_HEADER_LENGTH <= parser->used;
         ++offset)
    {
        uint8_t *candidate = &parser->buffer[offset];
        if (candidate[0] != REFEREE_SOF ||
            !Verify_CRC8_Check_Sum(candidate, REFEREE_HEADER_LENGTH))
        {
            continue;
        }

        const uint16_t payload_length = ReadU16LE(&candidate[1]);
        const size_t frame_length =
            (size_t)payload_length + REFEREE_FRAME_OVERHEAD;
        if (frame_length > REFEREE_STREAM_BUFFER_CAPACITY ||
            frame_length > parser->used - offset)
        {
            continue;
        }

        if (Verify_CRC16_Check_Sum(candidate, (uint32_t)frame_length))
            return offset;
    }

    return parser->used;
}

static void ProcessBufferedData(RefereeStreamParser_s *parser)
{
    while (parser->used > 0U)
    {
        size_t sof_offset = 0U;
        while (sof_offset < parser->used &&
               parser->buffer[sof_offset] != REFEREE_SOF)
        {
            ++sof_offset;
        }
        if (sof_offset > 0U)
            DiscardPrefix(parser, sof_offset);

        if (parser->used < REFEREE_HEADER_LENGTH)
            return;

        if (!Verify_CRC8_Check_Sum(parser->buffer, REFEREE_HEADER_LENGTH))
        {
            ++parser->stats.crc8_error_count;
            DiscardPrefix(parser, 1U);
            continue;
        }

        const uint16_t payload_length = ReadU16LE(&parser->buffer[1]);
        const size_t frame_length = (size_t)payload_length + REFEREE_FRAME_OVERHEAD;
        if (frame_length > REFEREE_STREAM_BUFFER_CAPACITY)
        {
            ++parser->stats.oversize_frame_count;
            DiscardPrefix(parser, 1U);
            continue;
        }

        if (parser->used < frame_length)
        {
            const size_t valid_frame_offset =
                FindCompleteValidFrameOffset(parser);
            if (valid_frame_offset < parser->used)
            {
                DiscardPrefix(parser, valid_frame_offset);
                continue;
            }
            return;
        }

        if (!Verify_CRC16_Check_Sum(parser->buffer, (uint32_t)frame_length))
        {
            ++parser->stats.crc16_error_count;
            DiscardPrefix(parser, 1U);
            continue;
        }

        const uint16_t command_id =
            ReadU16LE(&parser->buffer[REFEREE_HEADER_LENGTH]);
        ++parser->stats.valid_frame_count;
        parser->handler(parser->context,
                        command_id,
                        &parser->buffer[REFEREE_HEADER_LENGTH +
                                        REFEREE_COMMAND_LENGTH],
                        payload_length,
                        parser->buffer[3]);
        RemovePrefix(parser, frame_length);
    }
}

bool RefereeStreamParserInit(RefereeStreamParser_s *parser,
                             RefereeFrameHandler handler,
                             void *context)
{
    if (parser == NULL || handler == NULL)
        return false;

    memset(parser, 0, sizeof(*parser));
    parser->handler = handler;
    parser->context = context;
    return true;
}

void RefereeStreamParserReset(RefereeStreamParser_s *parser)
{
    if (parser == NULL)
        return;

    parser->used = 0U;
}

void RefereeStreamParserFeed(RefereeStreamParser_s *parser,
                             const uint8_t *data,
                             size_t length)
{
    if (parser == NULL || data == NULL || length == 0U ||
        parser->handler == NULL)
    {
        return;
    }

    while (length > 0U)
    {
        ProcessBufferedData(parser);
        if (parser->used == REFEREE_STREAM_BUFFER_CAPACITY)
            DiscardPrefix(parser, 1U);

        const size_t available = REFEREE_STREAM_BUFFER_CAPACITY - parser->used;
        const size_t copy_length = length < available ? length : available;
        memcpy(&parser->buffer[parser->used], data, copy_length);
        parser->used += copy_length;
        data += copy_length;
        length -= copy_length;
    }

    ProcessBufferedData(parser);
}

const RefereeStreamParserStats_s *RefereeStreamParserGetStats(
    const RefereeStreamParser_s *parser)
{
    if (parser == NULL)
        return NULL;

    return &parser->stats;
}
