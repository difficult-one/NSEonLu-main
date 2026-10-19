#ifndef PORTABLE_REFEREE_PROTOCOL_H
#define PORTABLE_REFEREE_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define REFEREE_SOF 0xA5U
#define REFEREE_HEADER_SIZE 5U
#define REFEREE_FRAME_OVERHEAD 9U
#define REFEREE_STREAM_CAPACITY 256U
#define REFEREE_MAX_FRAME_SIZE 256U
#define REFEREE_MAX_INTERACTION_DATA 113U

#define REFEREE_CMD_GAME_STATE 0x0001U
#define REFEREE_CMD_GAME_RESULT 0x0002U
#define REFEREE_CMD_GAME_ROBOT_HP 0x0003U
#define REFEREE_CMD_EVENT_DATA 0x0101U
#define REFEREE_CMD_SUPPLY_ACTION 0x0102U
#define REFEREE_CMD_ROBOT_STATE 0x0201U
#define REFEREE_CMD_POWER_HEAT 0x0202U
#define REFEREE_CMD_ROBOT_POSITION 0x0203U
#define REFEREE_CMD_BUFF 0x0204U
#define REFEREE_CMD_AERIAL_ENERGY 0x0205U
#define REFEREE_CMD_ROBOT_HURT 0x0206U
#define REFEREE_CMD_SHOOT_DATA 0x0207U
#define REFEREE_CMD_STUDENT_INTERACTION 0x0301U

#define REFEREE_GAME_STATE_LENGTH 11U
#define REFEREE_GAME_RESULT_LENGTH 1U
#define REFEREE_GAME_ROBOT_HP_LENGTH 32U
#define REFEREE_EVENT_DATA_LENGTH 4U
#define REFEREE_SUPPLY_ACTION_LENGTH 4U
#define REFEREE_ROBOT_STATE_LENGTH 13U
#define REFEREE_POWER_HEAT_LENGTH 14U
#define REFEREE_ROBOT_POSITION_LENGTH 16U
#define REFEREE_BUFF_LENGTH 6U
#define REFEREE_AERIAL_ENERGY_LENGTH 2U
#define REFEREE_ROBOT_HURT_LENGTH 1U
#define REFEREE_SHOOT_DATA_LENGTH 7U
#define REFEREE_INTERACTION_HEADER_LENGTH 6U

typedef struct
{
    uint8_t game_type;
    uint8_t game_progress;
    uint16_t stage_remain_time;
    uint64_t sync_timestamp;
} RefereeGameState;

typedef struct
{
    uint8_t winner;
} RefereeGameResult;

typedef struct
{
    uint16_t red_1_robot_hp;
    uint16_t red_2_robot_hp;
    uint16_t red_3_robot_hp;
    uint16_t red_4_robot_hp;
    uint16_t red_5_robot_hp;
    uint16_t red_7_robot_hp;
    uint16_t red_outpost_hp;
    uint16_t red_base_hp;
    uint16_t blue_1_robot_hp;
    uint16_t blue_2_robot_hp;
    uint16_t blue_3_robot_hp;
    uint16_t blue_4_robot_hp;
    uint16_t blue_5_robot_hp;
    uint16_t blue_7_robot_hp;
    uint16_t blue_outpost_hp;
    uint16_t blue_base_hp;
} RefereeGameRobotHp;

typedef struct
{
    uint32_t event_type;
} RefereeEventData;

typedef struct
{
    uint8_t supply_projectile_id;
    uint8_t supply_robot_id;
    uint8_t supply_projectile_step;
    uint8_t supply_projectile_num;
} RefereeSupplyAction;

typedef struct
{
    uint8_t robot_id;
    uint8_t robot_level;
    uint16_t current_hp;
    uint16_t maximum_hp;
    uint16_t shooter_barrel_cooling_value;
    uint16_t shooter_barrel_heat_limit;
    uint16_t chassis_power_limit_w;
    bool gimbal_output_enabled;
    bool chassis_output_enabled;
    bool shooter_output_enabled;
} RefereeRobotState;

typedef struct
{
    uint16_t reserved_voltage;
    uint16_t reserved_current;
    float reserved_power;
    uint16_t buffer_energy_j;
    uint16_t shooter_17mm_barrel_heat;
    uint16_t shooter_42mm_barrel_heat;
} RefereePowerHeat;

typedef struct
{
    float x;
    float y;
    float z;
    float yaw;
} RefereeRobotPosition;

typedef struct
{
    uint8_t recovery_buff;
    uint8_t cooling_buff;
    uint8_t defence_buff;
    uint8_t vulnerability_buff;
    uint16_t attack_buff;
} RefereeBuff;

typedef struct
{
    uint8_t airforce_status;
    uint8_t time_remaining_s;
} RefereeAerialEnergy;

typedef struct
{
    uint8_t armor_id;
    uint8_t hurt_type;
} RefereeRobotHurt;

typedef struct
{
    uint8_t bullet_type;
    uint8_t shooter_id;
    uint8_t bullet_frequency_hz;
    float bullet_speed_mps;
} RefereeShootData;

typedef struct
{
    uint16_t data_command_id;
    uint16_t sender_id;
    uint16_t receiver_id;
    uint8_t data[REFEREE_MAX_INTERACTION_DATA];
    size_t data_length;
} RefereeInteraction;

typedef struct
{
    RefereeGameState game_state;
    RefereeGameResult game_result;
    RefereeGameRobotHp game_robot_hp;
    RefereeEventData event_data;
    RefereeSupplyAction supply_action;
    RefereeRobotState robot_state;
    RefereePowerHeat power_heat;
    RefereeRobotPosition robot_position;
    RefereeBuff buff;
    RefereeAerialEnergy aerial_energy;
    RefereeRobotHurt robot_hurt;
    RefereeShootData shoot_data;
    RefereeInteraction interaction;
    uint16_t last_command_id;
    uint8_t last_sequence;
    bool has_game_state;
    bool has_game_result;
    bool has_game_robot_hp;
    bool has_event_data;
    bool has_supply_action;
    bool has_robot_state;
    bool has_power_heat;
    bool has_robot_position;
    bool has_buff;
    bool has_aerial_energy;
    bool has_robot_hurt;
    bool has_shoot_data;
    bool has_interaction;
} RefereeData;

#endif
