#ifndef SUPER_CAP_PROTOCOL_H
#define SUPER_CAP_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#define SUPERCAP_COMMAND_CAN_ID 0x061U
#define SUPERCAP_STATUS_CAN_ID 0x051U
#define SUPERCAP_CAN_DLC 8U

#define SUPERCAP_ERROR_UNDER_VOLTAGE 0x01U
#define SUPERCAP_ERROR_OVER_VOLTAGE 0x02U
#define SUPERCAP_ERROR_BUCK_BOOST 0x04U
#define SUPERCAP_ERROR_SHORT_CIRCUIT 0x08U
#define SUPERCAP_ERROR_HIGH_TEMPERATURE 0x10U
#define SUPERCAP_ERROR_NO_POWER_INPUT 0x20U
#define SUPERCAP_ERROR_CAPACITOR 0x40U
#define SUPERCAP_OUTPUT_DISABLED_MASK 0x80U
#define SUPERCAP_ERROR_MASK 0x7FU

typedef struct
{
    bool enable_dcdc;
    uint16_t referee_power_limit_w;
    uint16_t referee_buffer_energy_j;
} SuperCapCommand_s;

typedef struct
{
    bool online;
    bool output_enabled;
    uint8_t error_code;
    float chassis_power_w;
    uint16_t available_power_limit_w;
    float energy_ratio;
} SuperCapStatus_s;

bool SuperCapProtocolEncodeCommand(const SuperCapCommand_s *command,
                                   uint8_t frame[SUPERCAP_CAN_DLC]);
bool SuperCapProtocolDecodeStatus(const uint8_t frame[SUPERCAP_CAN_DLC],
                                  uint8_t dlc,
                                  SuperCapStatus_s *status);

#endif
