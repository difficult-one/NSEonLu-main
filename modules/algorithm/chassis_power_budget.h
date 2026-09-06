#ifndef CHASSIS_POWER_BUDGET_H
#define CHASSIS_POWER_BUDGET_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    CHASSIS_POWER_BUDGET_OFFLINE = 0,
    CHASSIS_POWER_BUDGET_DEGRADED,
    CHASSIS_POWER_BUDGET_READY,
} ChassisPowerBudgetMode_e;

typedef struct
{
    float empty_energy_ratio;
    float full_energy_ratio;
    float max_boost_w;
    float rise_rate_w_per_s;
} ChassisPowerBudgetConfig_s;

typedef struct
{
    float referee_limit_w;
    bool cap_online;
    bool cap_output_enabled;
    uint8_t cap_error_code;
    float cap_energy_ratio;
    float cap_reported_limit_w;
} ChassisPowerBudgetInput_s;

typedef struct
{
    float applied_budget_w;
    ChassisPowerBudgetMode_e mode;
} ChassisPowerBudgetState_s;

typedef struct
{
    float energy_factor;
    float target_budget_w;
    float applied_budget_w;
    ChassisPowerBudgetMode_e mode;
} ChassisPowerBudgetOutput_s;

bool ChassisPowerBudgetConfigIsValid(const ChassisPowerBudgetConfig_s *config);
void ChassisPowerBudgetReset(ChassisPowerBudgetState_s *state,
                             float initial_budget_w);
bool ChassisPowerBudgetUpdate(const ChassisPowerBudgetConfig_s *config,
                              const ChassisPowerBudgetInput_s *input,
                              float dt_s,
                              ChassisPowerBudgetState_s *state,
                              ChassisPowerBudgetOutput_s *output);

#endif
