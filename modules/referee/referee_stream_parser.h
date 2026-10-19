#ifndef REFEREE_STREAM_PARSER_H
#define REFEREE_STREAM_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define REFEREE_STREAM_BUFFER_CAPACITY 256U

/*
 * The handler runs synchronously from RefereeStreamParserFeed(). The payload
 * pointer is valid only for the duration of that callback. The handler and
 * context must remain valid for the parser lifetime, and the handler must not
 * call RefereeStreamParserFeed() reentrantly on the same parser.
 */
typedef void (*RefereeFrameHandler)(void *context,
                                    uint16_t command_id,
                                    const uint8_t *payload,
                                    uint16_t payload_length,
                                    uint8_t sequence);

typedef struct
{
    uint32_t valid_frame_count;
    uint32_t crc8_error_count;
    uint32_t crc16_error_count;
    uint32_t oversize_frame_count;
    uint32_t discarded_byte_count;
} RefereeStreamParserStats_s;

typedef struct
{
    uint8_t buffer[REFEREE_STREAM_BUFFER_CAPACITY];
    size_t used;
    RefereeFrameHandler handler;
    void *context;
    RefereeStreamParserStats_s stats;
} RefereeStreamParser_s;

bool RefereeStreamParserInit(RefereeStreamParser_s *parser,
                             RefereeFrameHandler handler,
                             void *context);
void RefereeStreamParserReset(RefereeStreamParser_s *parser);
void RefereeStreamParserFeed(RefereeStreamParser_s *parser,
                             const uint8_t *data,
                             size_t length);
const RefereeStreamParserStats_s *RefereeStreamParserGetStats(
    const RefereeStreamParser_s *parser);

#endif
