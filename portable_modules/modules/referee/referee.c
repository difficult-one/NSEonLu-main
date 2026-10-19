#include "referee.h"

#include "referee_crc.h"

#include <float.h>
#include <string.h>

_Static_assert(sizeof(float) == sizeof(uint32_t),
               "referee protocol requires 32-bit float");
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
                   FLT_MIN_EXP == -125 && FLT_MAX_EXP == 128,
               "referee protocol requires IEEE-754 binary32 float");

static uint16_t ReadU16LE(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
                      ((uint16_t)bytes[1] << 8U));
}

static void WriteU16LE(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
}

static uint32_t ReadU32LE(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static uint64_t ReadU64LE(const uint8_t *bytes)
{
    return (uint64_t)bytes[0] |
           ((uint64_t)bytes[1] << 8U) |
           ((uint64_t)bytes[2] << 16U) |
           ((uint64_t)bytes[3] << 24U) |
           ((uint64_t)bytes[4] << 32U) |
           ((uint64_t)bytes[5] << 40U) |
           ((uint64_t)bytes[6] << 48U) |
           ((uint64_t)bytes[7] << 56U);
}

static float ReadF32LE(const uint8_t *bytes)
{
    const uint32_t bits = ReadU32LE(bytes);
    float value;
    memcpy(&value, &bits, sizeof(bits));
    return value;
}

static void EnterCritical(Referee *instance)
{
    if (instance->config.enter_critical != NULL)
        instance->config.enter_critical(instance->config.context);
}

static void ExitCritical(Referee *instance)
{
    if (instance->config.exit_critical != NULL)
        instance->config.exit_critical(instance->config.context);
}

static void RemovePrefix(Referee *instance, size_t count)
{
    if (count >= instance->stream_used)
    {
        instance->stream_used = 0U;
        return;
    }

    memmove(instance->stream_buffer,
            &instance->stream_buffer[count],
            instance->stream_used - count);
    instance->stream_used -= count;
}

static void DiscardPrefix(Referee *instance, size_t count)
{
    const size_t discarded =
        count < instance->stream_used ? count : instance->stream_used;
    RemovePrefix(instance, count);
    instance->diagnostics.discarded_byte_count += (uint32_t)discarded;
}

static size_t FindCompleteValidFrameOffset(const Referee *instance)
{
    size_t offset;

    for (offset = 1U;
         offset + REFEREE_HEADER_SIZE <= instance->stream_used;
         ++offset)
    {
        const uint8_t *candidate = &instance->stream_buffer[offset];
        uint16_t payload_length;
        size_t frame_length;

        if (candidate[0] != REFEREE_SOF ||
            !RefereeCrc8Verify(candidate, REFEREE_HEADER_SIZE))
        {
            continue;
        }
        payload_length = ReadU16LE(&candidate[1]);
        if (payload_length >
            REFEREE_MAX_FRAME_SIZE - REFEREE_FRAME_OVERHEAD)
        {
            continue;
        }
        frame_length = (size_t)payload_length + REFEREE_FRAME_OVERHEAD;
        if (frame_length > instance->stream_used - offset)
        {
            continue;
        }
        if (RefereeCrc16Verify(candidate, frame_length))
            return offset;
    }

    return instance->stream_used;
}

static bool ExpectedPayloadLength(uint16_t command_id, size_t *length)
{
    switch (command_id)
    {
    case REFEREE_CMD_GAME_STATE:
        *length = REFEREE_GAME_STATE_LENGTH;
        return true;
    case REFEREE_CMD_GAME_RESULT:
        *length = REFEREE_GAME_RESULT_LENGTH;
        return true;
    case REFEREE_CMD_GAME_ROBOT_HP:
        *length = REFEREE_GAME_ROBOT_HP_LENGTH;
        return true;
    case REFEREE_CMD_EVENT_DATA:
        *length = REFEREE_EVENT_DATA_LENGTH;
        return true;
    case REFEREE_CMD_SUPPLY_ACTION:
        *length = REFEREE_SUPPLY_ACTION_LENGTH;
        return true;
    case REFEREE_CMD_ROBOT_STATE:
        *length = REFEREE_ROBOT_STATE_LENGTH;
        return true;
    case REFEREE_CMD_POWER_HEAT:
        *length = REFEREE_POWER_HEAT_LENGTH;
        return true;
    case REFEREE_CMD_ROBOT_POSITION:
        *length = REFEREE_ROBOT_POSITION_LENGTH;
        return true;
    case REFEREE_CMD_BUFF:
        *length = REFEREE_BUFF_LENGTH;
        return true;
    case REFEREE_CMD_AERIAL_ENERGY:
        *length = REFEREE_AERIAL_ENERGY_LENGTH;
        return true;
    case REFEREE_CMD_ROBOT_HURT:
        *length = REFEREE_ROBOT_HURT_LENGTH;
        return true;
    case REFEREE_CMD_SHOOT_DATA:
        *length = REFEREE_SHOOT_DATA_LENGTH;
        return true;
    default:
        return false;
    }
}

static void DecodeRobotHp(RefereeGameRobotHp *hp, const uint8_t *payload)
{
    hp->red_1_robot_hp = ReadU16LE(&payload[0]);
    hp->red_2_robot_hp = ReadU16LE(&payload[2]);
    hp->red_3_robot_hp = ReadU16LE(&payload[4]);
    hp->red_4_robot_hp = ReadU16LE(&payload[6]);
    hp->red_5_robot_hp = ReadU16LE(&payload[8]);
    hp->red_7_robot_hp = ReadU16LE(&payload[10]);
    hp->red_outpost_hp = ReadU16LE(&payload[12]);
    hp->red_base_hp = ReadU16LE(&payload[14]);
    hp->blue_1_robot_hp = ReadU16LE(&payload[16]);
    hp->blue_2_robot_hp = ReadU16LE(&payload[18]);
    hp->blue_3_robot_hp = ReadU16LE(&payload[20]);
    hp->blue_4_robot_hp = ReadU16LE(&payload[22]);
    hp->blue_5_robot_hp = ReadU16LE(&payload[24]);
    hp->blue_7_robot_hp = ReadU16LE(&payload[26]);
    hp->blue_outpost_hp = ReadU16LE(&payload[28]);
    hp->blue_base_hp = ReadU16LE(&payload[30]);
}

static void DecodeFixedFrame(Referee *instance,
                             uint16_t command_id,
                             const uint8_t *payload,
                             uint32_t now_ms)
{
    RefereeData *data = &instance->data;

    switch (command_id)
    {
    case REFEREE_CMD_GAME_STATE:
        data->game_state.game_type = payload[0] & 0x0FU;
        data->game_state.game_progress = (payload[0] >> 4U) & 0x0FU;
        data->game_state.stage_remain_time = ReadU16LE(&payload[1]);
        data->game_state.sync_timestamp = ReadU64LE(&payload[3]);
        data->has_game_state = true;
        break;
    case REFEREE_CMD_GAME_RESULT:
        data->game_result.winner = payload[0];
        data->has_game_result = true;
        break;
    case REFEREE_CMD_GAME_ROBOT_HP:
        DecodeRobotHp(&data->game_robot_hp, payload);
        data->has_game_robot_hp = true;
        break;
    case REFEREE_CMD_EVENT_DATA:
        data->event_data.event_type = ReadU32LE(payload);
        data->has_event_data = true;
        break;
    case REFEREE_CMD_SUPPLY_ACTION:
        data->supply_action.supply_projectile_id = payload[0];
        data->supply_action.supply_robot_id = payload[1];
        data->supply_action.supply_projectile_step = payload[2];
        data->supply_action.supply_projectile_num = payload[3];
        data->has_supply_action = true;
        break;
    case REFEREE_CMD_ROBOT_STATE:
        data->robot_state.robot_id = payload[0];
        data->robot_state.robot_level = payload[1];
        data->robot_state.current_hp = ReadU16LE(&payload[2]);
        data->robot_state.maximum_hp = ReadU16LE(&payload[4]);
        data->robot_state.shooter_barrel_cooling_value =
            ReadU16LE(&payload[6]);
        data->robot_state.shooter_barrel_heat_limit =
            ReadU16LE(&payload[8]);
        data->robot_state.chassis_power_limit_w = ReadU16LE(&payload[10]);
        data->robot_state.gimbal_output_enabled =
            (payload[12] & 0x01U) != 0U;
        data->robot_state.chassis_output_enabled =
            (payload[12] & 0x02U) != 0U;
        data->robot_state.shooter_output_enabled =
            (payload[12] & 0x04U) != 0U;
        data->has_robot_state = true;
        instance->robot_state_tick_ms = now_ms;
        break;
    case REFEREE_CMD_POWER_HEAT:
        data->power_heat.reserved_voltage = ReadU16LE(&payload[0]);
        data->power_heat.reserved_current = ReadU16LE(&payload[2]);
        data->power_heat.reserved_power = ReadF32LE(&payload[4]);
        data->power_heat.buffer_energy_j = ReadU16LE(&payload[8]);
        data->power_heat.shooter_17mm_barrel_heat =
            ReadU16LE(&payload[10]);
        data->power_heat.shooter_42mm_barrel_heat =
            ReadU16LE(&payload[12]);
        data->has_power_heat = true;
        instance->power_heat_tick_ms = now_ms;
        break;
    case REFEREE_CMD_ROBOT_POSITION:
        data->robot_position.x = ReadF32LE(&payload[0]);
        data->robot_position.y = ReadF32LE(&payload[4]);
        data->robot_position.z = ReadF32LE(&payload[8]);
        data->robot_position.yaw = ReadF32LE(&payload[12]);
        data->has_robot_position = true;
        break;
    case REFEREE_CMD_BUFF:
        data->buff.recovery_buff = payload[0];
        data->buff.cooling_buff = payload[1];
        data->buff.defence_buff = payload[2];
        data->buff.vulnerability_buff = payload[3];
        data->buff.attack_buff = ReadU16LE(&payload[4]);
        data->has_buff = true;
        break;
    case REFEREE_CMD_AERIAL_ENERGY:
        data->aerial_energy.airforce_status = payload[0];
        data->aerial_energy.time_remaining_s = payload[1];
        data->has_aerial_energy = true;
        break;
    case REFEREE_CMD_ROBOT_HURT:
        data->robot_hurt.armor_id = payload[0] & 0x0FU;
        data->robot_hurt.hurt_type = (payload[0] >> 4U) & 0x0FU;
        data->has_robot_hurt = true;
        break;
    case REFEREE_CMD_SHOOT_DATA:
        data->shoot_data.bullet_type = payload[0];
        data->shoot_data.shooter_id = payload[1];
        data->shoot_data.bullet_frequency_hz = payload[2];
        data->shoot_data.bullet_speed_mps = ReadF32LE(&payload[3]);
        data->has_shoot_data = true;
        break;
    default:
        break;
    }
}

static void HandleValidFrame(Referee *instance,
                             uint16_t command_id,
                             const uint8_t *payload,
                             size_t payload_length,
                             uint8_t sequence)
{
    size_t expected_length;
    const uint32_t now_ms = instance->config.now_ms(instance->config.context);

    ++instance->diagnostics.valid_frame_count;
    instance->diagnostics.has_valid_frame = true;
    instance->diagnostics.last_valid_frame_ms = now_ms;
    instance->data.last_command_id = command_id;
    instance->data.last_sequence = sequence;

    if (command_id == REFEREE_CMD_STUDENT_INTERACTION)
    {
        RefereeInteraction *interaction = &instance->data.interaction;
        const size_t maximum_length =
            REFEREE_INTERACTION_HEADER_LENGTH + REFEREE_MAX_INTERACTION_DATA;

        if (payload_length < REFEREE_INTERACTION_HEADER_LENGTH ||
            payload_length > maximum_length)
        {
            ++instance->diagnostics.payload_length_error_count;
            return;
        }

        interaction->data_command_id = ReadU16LE(&payload[0]);
        interaction->sender_id = ReadU16LE(&payload[2]);
        interaction->receiver_id = ReadU16LE(&payload[4]);
        interaction->data_length =
            payload_length - REFEREE_INTERACTION_HEADER_LENGTH;
        memset(interaction->data, 0, sizeof(interaction->data));
        if (interaction->data_length > 0U)
        {
            memcpy(interaction->data, &payload[6], interaction->data_length);
        }
        instance->data.has_interaction = true;
        return;
    }

    if (!ExpectedPayloadLength(command_id, &expected_length))
    {
        ++instance->diagnostics.unsupported_command_count;
        return;
    }
    if (payload_length != expected_length)
    {
        ++instance->diagnostics.payload_length_error_count;
        return;
    }

    DecodeFixedFrame(instance, command_id, payload, now_ms);
}

static void ProcessBufferedData(Referee *instance)
{
    while (instance->stream_used > 0U)
    {
        size_t sof_offset = 0U;
        uint16_t payload_length;
        size_t frame_length;

        while (sof_offset < instance->stream_used &&
               instance->stream_buffer[sof_offset] != REFEREE_SOF)
        {
            ++sof_offset;
        }
        if (sof_offset > 0U)
            DiscardPrefix(instance, sof_offset);
        if (instance->stream_used < REFEREE_HEADER_SIZE)
            return;

        if (!RefereeCrc8Verify(instance->stream_buffer,
                               REFEREE_HEADER_SIZE))
        {
            ++instance->diagnostics.crc8_error_count;
            DiscardPrefix(instance, 1U);
            continue;
        }

        payload_length = ReadU16LE(&instance->stream_buffer[1]);
        if (payload_length >
            REFEREE_MAX_FRAME_SIZE - REFEREE_FRAME_OVERHEAD)
        {
            ++instance->diagnostics.oversize_frame_count;
            DiscardPrefix(instance, 1U);
            continue;
        }
        frame_length = (size_t)payload_length + REFEREE_FRAME_OVERHEAD;

        if (instance->stream_used < frame_length)
        {
            const size_t valid_frame_offset =
                FindCompleteValidFrameOffset(instance);
            if (valid_frame_offset < instance->stream_used)
            {
                DiscardPrefix(instance, valid_frame_offset);
                continue;
            }
            return;
        }

        if (!RefereeCrc16Verify(instance->stream_buffer, frame_length))
        {
            ++instance->diagnostics.crc16_error_count;
            DiscardPrefix(instance, 1U);
            continue;
        }

        HandleValidFrame(instance,
                         ReadU16LE(&instance->stream_buffer[5]),
                         &instance->stream_buffer[7],
                         payload_length,
                         instance->stream_buffer[3]);
        RemovePrefix(instance, frame_length);
    }
}

bool RefereeInit(Referee *instance, const RefereeConfig *config)
{
    RefereeConfig saved_config;
    const bool critical_pair_mismatched =
        config != NULL &&
        ((config->enter_critical == NULL) !=
         (config->exit_critical == NULL));

    if (instance == NULL || config == NULL ||
        config->uart_send == NULL || config->now_ms == NULL ||
        critical_pair_mismatched)
    {
        return false;
    }

    saved_config = *config;
    *instance = (Referee){0};
    instance->config = saved_config;
    if (instance->config.offline_timeout_ms == 0U)
    {
        instance->config.offline_timeout_ms =
            REFEREE_DEFAULT_OFFLINE_TIMEOUT_MS;
    }
    instance->initialized = true;
    return true;
}

bool RefereeFeed(Referee *instance, const uint8_t *data, size_t length)
{
    if (instance == NULL || !instance->initialized ||
        data == NULL || length == 0U)
        return false;

    while (length > 0U)
    {
        size_t available;
        size_t copy_length;

        ProcessBufferedData(instance);
        if (instance->stream_used == REFEREE_STREAM_CAPACITY)
            DiscardPrefix(instance, 1U);

        available = REFEREE_STREAM_CAPACITY - instance->stream_used;
        copy_length = length < available ? length : available;
        memcpy(&instance->stream_buffer[instance->stream_used],
               data, copy_length);
        instance->stream_used += copy_length;
        data += copy_length;
        length -= copy_length;
    }

    ProcessBufferedData(instance);
    return true;
}

bool RefereeGetData(Referee *instance, RefereeData *data)
{
    RefereeData snapshot;

    if (instance == NULL || !instance->initialized || data == NULL)
        return false;
    EnterCritical(instance);
    snapshot = instance->data;
    ExitCritical(instance);
    *data = snapshot;
    return true;
}

bool RefereeGetPower(Referee *instance, RefereePower *power)
{
    RefereePower snapshot = {0};
    uint32_t now_ms;
    uint32_t robot_state_tick_ms;
    uint32_t power_heat_tick_ms;
    uint32_t timeout_ms;
    bool has_robot_state;
    bool has_power_heat;

    if (instance == NULL || !instance->initialized || power == NULL)
        return false;

    EnterCritical(instance);
    snapshot.chassis_output_enabled =
        instance->data.robot_state.chassis_output_enabled;
    snapshot.chassis_power_limit_w =
        instance->data.robot_state.chassis_power_limit_w;
    snapshot.buffer_energy_j = instance->data.power_heat.buffer_energy_j;
    has_robot_state = instance->data.has_robot_state;
    has_power_heat = instance->data.has_power_heat;
    robot_state_tick_ms = instance->robot_state_tick_ms;
    power_heat_tick_ms = instance->power_heat_tick_ms;
    timeout_ms = instance->config.offline_timeout_ms;
    now_ms = instance->config.now_ms(instance->config.context);
    ExitCritical(instance);

    snapshot.valid =
        has_robot_state && has_power_heat &&
        (uint32_t)(now_ms - robot_state_tick_ms) <= timeout_ms &&
        (uint32_t)(now_ms - power_heat_tick_ms) <= timeout_ms;
    if (!snapshot.valid)
    {
        snapshot.chassis_output_enabled = false;
        snapshot.chassis_power_limit_w = 0U;
        snapshot.buffer_energy_j = 0U;
    }
    *power = snapshot;
    return true;
}

bool RefereeGetDiagnostics(Referee *instance,
                           RefereeDiagnostics *diagnostics)
{
    RefereeDiagnostics snapshot;

    if (instance == NULL || !instance->initialized || diagnostics == NULL)
        return false;
    EnterCritical(instance);
    snapshot = instance->diagnostics;
    ExitCritical(instance);
    *diagnostics = snapshot;
    return true;
}

bool RefereeIsOnline(Referee *instance)
{
    uint32_t now_ms;
    uint32_t last_valid_frame_ms;
    uint32_t timeout_ms;
    bool has_valid_frame;

    if (instance == NULL || !instance->initialized)
        return false;
    EnterCritical(instance);
    has_valid_frame = instance->diagnostics.has_valid_frame;
    last_valid_frame_ms = instance->diagnostics.last_valid_frame_ms;
    timeout_ms = instance->config.offline_timeout_ms;
    now_ms = instance->config.now_ms(instance->config.context);
    ExitCritical(instance);
    return has_valid_frame &&
           (uint32_t)(now_ms - last_valid_frame_ms) <= timeout_ms;
}

uint16_t RefereeClientId(uint16_t robot_id)
{
    return (uint16_t)(0x0100U + robot_id);
}

bool RefereeSendInteraction(Referee *instance, uint16_t sender_id,
                            uint16_t receiver_id, uint16_t data_command_id,
                            const uint8_t *data, size_t data_length)
{
    const size_t payload_length =
        REFEREE_INTERACTION_HEADER_LENGTH + data_length;
    const size_t frame_length = payload_length + REFEREE_FRAME_OVERHEAD;
    uint8_t *frame;
    uint8_t sequence;
    bool sent;

    if (instance == NULL || !instance->initialized ||
        data_length > REFEREE_MAX_INTERACTION_DATA ||
        (data == NULL && data_length != 0U))
    {
        return false;
    }

    EnterCritical(instance);
    sequence = instance->tx_sequence;
    instance->tx_sequence = (uint8_t)(instance->tx_sequence + 1U);
    ExitCritical(instance);

    frame = instance->tx_buffer;
    memset(frame, 0, frame_length);
    frame[0] = REFEREE_SOF;
    WriteU16LE(&frame[1], (uint16_t)payload_length);
    frame[3] = sequence;
    RefereeCrc8Append(frame, REFEREE_HEADER_SIZE);
    WriteU16LE(&frame[5], REFEREE_CMD_STUDENT_INTERACTION);
    WriteU16LE(&frame[7], data_command_id);
    WriteU16LE(&frame[9], sender_id);
    WriteU16LE(&frame[11], receiver_id);
    if (data_length != 0U)
        memcpy(&frame[13], data, data_length);
    RefereeCrc16Append(frame, frame_length);
    instance->tx_length = frame_length;

    sent = instance->config.uart_send(instance->config.context,
                                      frame, frame_length);

    if (!sent)
    {
        EnterCritical(instance);
        ++instance->diagnostics.transmit_failure_count;
        ExitCritical(instance);
    }
    return sent;
}
