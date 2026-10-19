#include "super_cap_protocol.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static uint16_t ReadU16LE(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static void WriteU16LE(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static float ReadF32LE(const uint8_t *bytes)
{
    const uint32_t bits = (uint32_t)bytes[0] |
                          ((uint32_t)bytes[1] << 8) |
                          ((uint32_t)bytes[2] << 16) |
                          ((uint32_t)bytes[3] << 24);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

bool SuperCapProtocolEncodeCommand(const SuperCapCommand_s *command,
                                   uint8_t frame[SUPERCAP_CAN_DLC])
{
    if (command == NULL || frame == NULL)
        return false;

    memset(frame, 0, SUPERCAP_CAN_DLC);
    if (command->enable_dcdc)
        frame[0] = 0x01U;
    WriteU16LE(&frame[1], command->referee_power_limit_w);
    WriteU16LE(&frame[3], command->referee_buffer_energy_j);
    if (command->enable_dcdc && command->input_control_enabled)
    {
        WriteU16LE(&frame[5], command->input_power_target_w);
        frame[7] = SUPERCAP_INPUT_COMMAND_TAG;
    }
    return true;
}

bool SuperCapProtocolDecodeStatus(const uint8_t frame[SUPERCAP_CAN_DLC],
                                  uint8_t dlc,
                                  SuperCapStatus_s *status)
{
    if (frame == NULL || status == NULL || dlc != SUPERCAP_CAN_DLC)
        return false;

    const float chassis_power_w = ReadF32LE(&frame[1]);
    if (!isfinite(chassis_power_w))
        return false;

    SuperCapStatus_s decoded = {
        .online = true,
        .output_enabled = (frame[0] & SUPERCAP_OUTPUT_DISABLED_MASK) == 0U,
        .error_code = frame[0] & SUPERCAP_ERROR_MASK,
        .chassis_power_w = chassis_power_w,
        .available_power_limit_w = ReadU16LE(&frame[5]),
        .energy_ratio = (float)frame[7] / 255.0f,
    };
    *status = decoded;
    return true;
}

bool SuperCapProtocolDecodeInputPower(const uint8_t frame[SUPERCAP_CAN_DLC],
                                      uint8_t dlc, SuperCapInputPower_s *power)
{
    if (frame == NULL || power == NULL || dlc != SUPERCAP_CAN_DLC ||
        frame[6] != SUPERCAP_INPUT_FEEDBACK_VERSION || (frame[7] & ~1U) != 0)
        return false;
    const float measured = ReadF32LE(frame);
    if (!isfinite(measured))
        return false;
    *power = (SuperCapInputPower_s){
        .input_power_w = measured,
        .input_power_target_w = ReadU16LE(&frame[4]),
        .input_control_active = (frame[7] & 1U) != 0,
        .online = true,
    };
    return true;
}
