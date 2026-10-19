/**
 * @file rm_referee.C
 * @author kidneygood (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-11-18
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "rm_referee.h"
#include "string.h"
#include "bsp_usart.h"
#include "task.h"
#include "daemon.h"
#include "bsp_log.h"
#include "cmsis_os.h"
#include "referee_stream_parser.h"

#define RE_RX_BUFFER_SIZE 255u // 裁判系统接收缓冲区大小

static USARTInstance *referee_usart_instance; // 裁判系统串口实例
static DaemonInstance *referee_daemon;		  // 裁判系统守护进程
static referee_info_t referee_info;			  // 裁判系统数据
static RefereeStreamParser_s referee_stream_parser;// 裁判系统数据流解析器
static RefereePowerState_s referee_power_state;
static uint32_t payload_length_error_count;

static bool GetExpectedPayloadLength(uint16_t command_id,
									 uint16_t *expected_length)
{
	switch (command_id)
	{
	case ID_game_state:
		*expected_length = LEN_game_state;
		return true;
	case ID_game_result:
		*expected_length = LEN_game_result;
		return true;
	case ID_game_robot_survivors:
		*expected_length = LEN_game_robot_HP;
		return true;
	case ID_event_data:
		*expected_length = LEN_event_data;
		return true;
	case ID_supply_projectile_action:
		*expected_length = LEN_supply_projectile_action;
		return true;
	case ID_game_robot_state:
		*expected_length = LEN_game_robot_state;
		return true;
	case ID_power_heat_data:
		*expected_length = LEN_power_heat_data;
		return true;
	case ID_game_robot_pos:
		*expected_length = LEN_game_robot_pos;
		return true;
	case ID_buff_musk:
		*expected_length = LEN_buff_musk;
		return true;
	case ID_aerial_robot_energy:
		*expected_length = LEN_aerial_robot_energy;
		return true;
	case ID_robot_hurt:
		*expected_length = LEN_robot_hurt;
		return true;
	case ID_shoot_data:
		*expected_length = LEN_shoot_data;
		return true;
	case ID_student_interactive:
		*expected_length = LEN_receive_data;
		return true;
	default:
		return false;
	}
}

static void HandleValidFrame(void *context,
							 uint16_t command_id,
							 const uint8_t *payload,
							 uint16_t payload_length,
							 uint8_t sequence)
{
	uint16_t expected_payload_length;
	const bool known_command =
		GetExpectedPayloadLength(command_id, &expected_payload_length);
	const bool extended_power_heat = command_id == ID_power_heat_data &&
		payload_length == REFEREE_POWER_HEAT_LEGACY_PAYLOAD_LENGTH;
	const bool extended_robot_state = command_id == ID_game_robot_state &&
		payload_length == REFEREE_ROBOT_STATE_EXTENDED_PAYLOAD_LENGTH;

	(void)context;
	referee_info.FrameHeader.Seq = sequence;
	referee_info.CmdID = command_id;

	if (known_command && payload_length != expected_payload_length &&
		!extended_power_heat && !extended_robot_state)
	{
		++payload_length_error_count;
	}
	else if (RefereePowerStateAcceptFrame(&referee_power_state,
										 command_id,
										 payload,
										 payload_length,
										 HAL_GetTick()))
	{
		if (command_id == ID_game_robot_state &&
			(payload_length == LEN_game_robot_state || extended_robot_state))
		{
			const uint16_t flags_offset = extended_robot_state ? 16U : 12U;
			uint8_t *const destination = (uint8_t *)&referee_info.GameRobotState;
			/* Extended frames insert four bytes before the output flags. */
			memcpy(destination, payload, LEN_game_robot_state - 1U);
			destination[LEN_game_robot_state - 1U] = payload[flags_offset];
		}
		else if (command_id == ID_power_heat_data)
		{
			memcpy(&referee_info.PowerHeatData, payload, LEN_power_heat_data);
			if (extended_power_heat)
			{
				/* In 16-byte frames, offset 12 is 17 mm heat; 42 mm is at 14. */
				memcpy(&referee_info.PowerHeatData.shooter_42mm_barrel_heat,
					   &payload[14],
					   sizeof(referee_info.PowerHeatData.shooter_42mm_barrel_heat));
			}
		}
	}
	else
	{
		switch (command_id)
		{
		case ID_game_state: // 0x0001
			if (payload_length == LEN_game_state)
				memcpy(&referee_info.GameState, payload, LEN_game_state);
			break;
		case ID_game_result: // 0x0002
			if (payload_length == LEN_game_result)
				memcpy(&referee_info.GameResult, payload, LEN_game_result);
			break;
		case ID_game_robot_survivors: // 0x0003
			if (payload_length == LEN_game_robot_HP)
				memcpy(&referee_info.GameRobotHP, payload, LEN_game_robot_HP);
			break;
		case ID_event_data: // 0x0101
			if (payload_length == LEN_event_data)
				memcpy(&referee_info.EventData, payload, LEN_event_data);
			break;
		case ID_supply_projectile_action: // 0x0102
			if (payload_length == LEN_supply_projectile_action)
				memcpy(&referee_info.SupplyProjectileAction, payload, LEN_supply_projectile_action);
			break;
		case ID_game_robot_pos: // 0x0203
			if (payload_length == LEN_game_robot_pos)
				memcpy(&referee_info.GameRobotPos, payload, LEN_game_robot_pos);
			break;
		case ID_buff_musk: // 0x0204
			if (payload_length == LEN_buff_musk)
				memcpy(&referee_info.BuffMusk, payload, LEN_buff_musk);
			break;
		case ID_aerial_robot_energy: // 0x0205
			if (payload_length == LEN_aerial_robot_energy)
				memcpy(&referee_info.AerialRobotEnergy, payload, LEN_aerial_robot_energy);
			break;
		case ID_robot_hurt: // 0x0206
			if (payload_length == LEN_robot_hurt)
				memcpy(&referee_info.RobotHurt, payload, LEN_robot_hurt);
			break;
		case ID_shoot_data: // 0x0207
			if (payload_length == LEN_shoot_data)
				memcpy(&referee_info.ShootData, payload, LEN_shoot_data);
			break;
		case ID_student_interactive: // 0x0301   syhtodo接收代码未测试
			if (payload_length == LEN_receive_data)
				memcpy(&referee_info.ReceiveData, payload, LEN_receive_data);
			break;
		default:
			break;
		}
	}

	if (referee_daemon != NULL)
		DaemonReload(referee_daemon);
}

/*裁判系统串口接收回调函数,解析数据 */
static void RefereeRxCallback(void)
{
	if (referee_usart_instance == NULL ||
		referee_usart_instance->recv_data_len == 0U)
	{
		return;
	}

	RefereeStreamParserFeed(&referee_stream_parser,
							referee_usart_instance->recv_buff,
							referee_usart_instance->recv_data_len);
}
// 裁判系统丢失回调函数,重新初始化裁判系统串口
static void RefereeLostCallback(void *arg)
{
	(void)arg;
	RefereeStreamParserReset(&referee_stream_parser);
	USARTServiceInit(referee_usart_instance);
	LOGWARNING("[rm_ref] lost referee data");
}

/* 裁判系统通信初始化 */
referee_info_t *RefereeInit(UART_HandleTypeDef *referee_usart_handle)
{
	RefereePowerStateReset(&referee_power_state);
	payload_length_error_count = 0U;
	(void)RefereeStreamParserInit(&referee_stream_parser,
								  HandleValidFrame,
								  NULL);

	USART_Init_Config_s conf;
	conf.module_callback = RefereeRxCallback;
	conf.usart_handle = referee_usart_handle;
	conf.recv_buff_size = RE_RX_BUFFER_SIZE; // mx 255(u8)
	referee_usart_instance = USARTRegister(&conf);

	Daemon_Init_Config_s daemon_conf = {
		.callback = RefereeLostCallback,
		.owner_id = referee_usart_instance,
		.reload_count = 30, // 0.3s没有收到数据,则认为丢失,重启串口接收
	};
	referee_daemon = DaemonRegister(&daemon_conf);

	return &referee_info;
}

bool RefereeGetPowerSnapshot(RefereePowerSnapshot_s *snapshot)
{
	RefereePowerState_s state;
	uint32_t now_ms;

	if (snapshot == NULL)
		return false;

	taskENTER_CRITICAL();
	state = referee_power_state;
	now_ms = HAL_GetTick();
	taskEXIT_CRITICAL();

	return RefereePowerBuildSnapshot(&state, now_ms, snapshot);
}

bool RefereeGetLinkDiagnostics(RefereeLinkDiagnostics_s *diagnostics)
{
	RefereeLinkDiagnostics_s current;

	if (diagnostics == NULL)
		return false;

	taskENTER_CRITICAL();
	current.transport = referee_stream_parser.stats;
	current.payload_length_error_count = payload_length_error_count;
	taskEXIT_CRITICAL();

	*diagnostics = current;
	return true;
}

/**
 * @brief 裁判系统数据发送函数
 * @param
 */
void RefereeSend(uint8_t *send, uint16_t tx_len)
{
	USARTSend(referee_usart_instance, send, tx_len, USART_TRANSFER_DMA);
	osDelay(115);
}
