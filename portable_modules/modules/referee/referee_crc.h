#ifndef PORTABLE_REFEREE_CRC_H
#define PORTABLE_REFEREE_CRC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint8_t RefereeCrc8(const uint8_t *data, size_t length, uint8_t initial);
uint16_t RefereeCrc16(const uint8_t *data, size_t length, uint16_t initial);
bool RefereeCrc8Verify(const uint8_t *frame, size_t length);
bool RefereeCrc16Verify(const uint8_t *frame, size_t length);
void RefereeCrc8Append(uint8_t *frame, size_t length);
void RefereeCrc16Append(uint8_t *frame, size_t length);

#endif
