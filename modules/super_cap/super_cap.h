#ifndef SUPER_CAP_H
#define SUPER_CAP_H

#include "bsp_can.h"
#include "daemon.h"
#include "super_cap_protocol.h"

typedef struct
{
    CANInstance *can_ins;
    CANInstance *input_can_ins;
    DaemonInstance *daemon;
    SuperCapStatus_s status;
    SuperCapInputPower_s input_power;

    /* 临时诊断计数,用于确认 CAN 链路是否真的在收帧。问题定位完就删。
       全部在 CAN 接收中断里更新,32 位对齐读写是原子的,读侧不用加锁。 */
    volatile uint32_t received_frame_count; // 成功解码的状态帧累计数
    volatile uint32_t rejected_frame_count; // 收到但被解码拒绝的帧数(长度不对/浮点非法)
    volatile uint32_t last_rx_tick_ms;      // 最近一次成功解码时的 HAL_GetTick()
} SuperCapInstance;

typedef struct
{
    CAN_Init_Config_s can_config;
    uint16_t offline_reload_count;
    bool input_power_feedback_enabled;
} SuperCap_Init_Config_s;

bool SuperCapInit(const SuperCap_Init_Config_s *config);
bool SuperCapSendCommand(const SuperCapCommand_s *command);
bool SuperCapGetStatus(SuperCapStatus_s *status);
bool SuperCapGetInputPower(SuperCapInputPower_s *power);

#endif
