#ifndef CHASSIS_H
#define CHASSIS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool valid;
    uint32_t tick_ms;
    uint8_t mode;
    float vx_cmd, vy_cmd, wz_cmd;
    float nominal_budget_w, target_power_w;
    bool buffer_control_enabled, buffer_estimate_valid, buffer_recovering;
    float buffer_lower_j, buffer_predicted_j;
} ChassisTelemetrySnapshot_s;

bool ChassisGetTelemetrySnapshot(ChassisTelemetrySnapshot_s *snapshot);

/**
 * @brief 底盘应用初始化,请在开启rtos之前调用(目前会被RobotInit()调用)
 * 
 */
void ChassisInit();

/**
 * @brief 底盘应用任务,放入实时系统以一定频率运行
 * 
 */
void ChassisTask();

#endif // CHASSIS_H
