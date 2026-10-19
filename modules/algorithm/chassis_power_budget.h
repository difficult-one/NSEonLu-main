#ifndef CHASSIS_POWER_BUDGET_H
#define CHASSIS_POWER_BUDGET_H

#include <stdbool.h>
#include <stdint.h>

#define CHASSIS_BUFFER_HISTORY_LENGTH 96U

typedef enum
{
    CHASSIS_POWER_BUDGET_OFFLINE = 0,
    CHASSIS_POWER_BUDGET_DEGRADED,
    CHASSIS_POWER_BUDGET_READY,
} ChassisPowerBudgetMode_e;

typedef struct
{
    float max_energy_j;
    float recovery_enter_j;
    float recovery_exit_j;
    float hard_floor_j;
    float prediction_window_s;
    float guard_window_s;
    float settlement_delay_s;
    float energy_margin_j;
    float power_margin_w;
    float max_boost_w;
    float recovery_power_w; /* Maximum recovery reduction, not a fixed decrement. */
    float recovery_target_time_s;
    float observer_max_step_s;
    float recovery_hold_s;
    float rise_rate_w_per_s;
    float safety_factor;
    float correction_rise_j_per_s;
} ChassisBufferBudgetConfig_s;

typedef struct
{
    float empty_energy_ratio;
    float full_energy_ratio;
    float max_boost_w;
    float rise_rate_w_per_s;
    float demand_margin_w; /* Physical watts above a valid load request. */
    ChassisBufferBudgetConfig_s buffer;
} ChassisPowerBudgetConfig_s;

typedef struct
{
    float referee_limit_w;
    bool cap_online;
    bool cap_output_enabled;
    uint8_t cap_error_code;
    float cap_energy_ratio;
    float cap_reported_limit_w;
    bool buffer_control_enabled;
    bool use_supercap;
    bool buffer_data_valid;
    float buffer_energy_j;
    uint32_t buffer_tick_ms;
    float buffer_age_s;
    bool input_power_valid;
    float measured_input_power_w;
    bool cap_input_control_ready;
    bool load_power_valid;
    bool demand_valid;
    float requested_load_power_w; /* Physical load power before current limiting. */
    float confirmed_input_target_w; /* Fresh 0x052 target, physical watts. */
} ChassisPowerBudgetInput_s;

typedef struct
{
    float elapsed_s;
    float loss_w;
} ChassisBufferHistorySample_s;

typedef struct
{
    float applied_budget_w;// 实际应用的功率预算,由算法计算得出,用于限制底盘输出
    ChassisPowerBudgetMode_e mode;
    float input_budget_w;
    float recovery_time_s;
    float buffer_lower_j;
    float last_buffer_age_s;
    float last_input_power_w;
    float last_referee_limit_w;
    uint32_t buffer_tick_ms;
    bool has_buffer_sample;
    bool buffer_estimate_valid;
    bool last_input_power_valid;
    bool buffer_initialized;
    bool recovering;
    ChassisBufferHistorySample_s history[CHASSIS_BUFFER_HISTORY_LENGTH];
    uint16_t history_head;
    uint16_t history_count;
} ChassisPowerBudgetState_s;

typedef struct
{
    float energy_factor;// 电容能量因子,0~1,0表示电容能量耗尽,1表示电容能量充满
    float target_budget_w;// 目标功率预算,由算法计算得出,用于限制底盘输出
    float applied_budget_w;// 实际应用的功率预算,由算法计算得出,用于限制底盘输出
    ChassisPowerBudgetMode_e mode;
    float input_target_w;
    float buffer_lower_j;
    float buffer_predicted_j;
    float recovery_reduction_w;
    bool recovering;
} ChassisPowerBudgetOutput_s;

bool ChassisPowerBudgetConfigIsValid(const ChassisPowerBudgetConfig_s *config);
bool ChassisBufferBudgetConfigIsValid(const ChassisBufferBudgetConfig_s *config);
void ChassisPowerBudgetReset(ChassisPowerBudgetState_s *state,
                             float initial_budget_w);
bool ChassisPowerBudgetUpdate(const ChassisPowerBudgetConfig_s *config,
                              const ChassisPowerBudgetInput_s *input,
                              float dt_s,
                              ChassisPowerBudgetState_s *state,
                              ChassisPowerBudgetOutput_s *output);

#endif
