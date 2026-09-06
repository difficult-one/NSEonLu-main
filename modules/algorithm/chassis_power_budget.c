#include "chassis_power_budget.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static void ResetInvalid(ChassisPowerBudgetState_s *state,
                         ChassisPowerBudgetOutput_s *output)
{
    if (state != NULL)
    {
        state->applied_budget_w = 0.0f;
        state->mode = CHASSIS_POWER_BUDGET_OFFLINE;
    }
    if (output != NULL)
    {
        memset(output, 0, sizeof(*output));
        output->mode = CHASSIS_POWER_BUDGET_OFFLINE;
    }
}

static float ClampFloat(float value, float minimum, float maximum)
{
    return fminf(fmaxf(value, minimum), maximum);
}

bool ChassisPowerBudgetConfigIsValid(const ChassisPowerBudgetConfig_s *config)
{
    return config != NULL &&
           isfinite(config->empty_energy_ratio) &&
           isfinite(config->full_energy_ratio) &&
           isfinite(config->max_boost_w) &&
           isfinite(config->rise_rate_w_per_s) &&
           config->empty_energy_ratio >= 0.0f &&
           config->empty_energy_ratio < config->full_energy_ratio &&
           config->full_energy_ratio <= 1.0f &&
           config->max_boost_w > 0.0f &&
           config->rise_rate_w_per_s > 0.0f;
}

void ChassisPowerBudgetReset(ChassisPowerBudgetState_s *state,
                             float initial_budget_w)
{
    if (state == NULL)
        return;

    state->applied_budget_w = isfinite(initial_budget_w) && initial_budget_w >= 0.0f
                                  ? initial_budget_w
                                  : 0.0f;
    state->mode = CHASSIS_POWER_BUDGET_OFFLINE;
}

bool ChassisPowerBudgetUpdate(const ChassisPowerBudgetConfig_s *config,
                              const ChassisPowerBudgetInput_s *input,
                              float dt_s,
                              ChassisPowerBudgetState_s *state,
                              ChassisPowerBudgetOutput_s *output)
{
    if (!ChassisPowerBudgetConfigIsValid(config) || input == NULL ||
        state == NULL || output == NULL || !isfinite(input->referee_limit_w) ||
        input->referee_limit_w <= 0.0f || !isfinite(dt_s) || dt_s < 0.0f)
    {
        ResetInvalid(state, output);
        return false;
    }

    ChassisPowerBudgetMode_e mode = CHASSIS_POWER_BUDGET_READY;
    float energy_factor = 0.0f;
    if (!input->cap_online)
    {
        mode = CHASSIS_POWER_BUDGET_OFFLINE;
    }
    else if (!input->cap_output_enabled || input->cap_error_code != 0U ||
             !isfinite(input->cap_energy_ratio) ||
             input->cap_energy_ratio < 0.0f || input->cap_energy_ratio > 1.0f ||
             !isfinite(input->cap_reported_limit_w) ||
             input->cap_reported_limit_w < 0.0f ||
             input->cap_energy_ratio <= config->empty_energy_ratio)
    {
        mode = CHASSIS_POWER_BUDGET_DEGRADED;
    }
    else
    {
        energy_factor = ClampFloat(
            (input->cap_energy_ratio - config->empty_energy_ratio) /
                (config->full_energy_ratio - config->empty_energy_ratio),
            0.0f,
            1.0f);
    }

    float target_budget_w = input->referee_limit_w;
    if (mode == CHASSIS_POWER_BUDGET_READY)
    {
        const float reported_boost_w = ClampFloat(
            input->cap_reported_limit_w - input->referee_limit_w,
            0.0f,
            config->max_boost_w);
        target_budget_w += energy_factor * reported_boost_w;
    }

    float applied_budget_w = state->applied_budget_w;
    if (!isfinite(applied_budget_w) || applied_budget_w < input->referee_limit_w)
        applied_budget_w = input->referee_limit_w;

    if (target_budget_w < applied_budget_w)
    {
        applied_budget_w = target_budget_w;
    }
    else
    {
        const float maximum_rise_w = config->rise_rate_w_per_s * dt_s;
        applied_budget_w = fminf(target_budget_w,
                                 applied_budget_w + maximum_rise_w);
    }

    state->applied_budget_w = applied_budget_w;
    state->mode = mode;
    output->energy_factor = energy_factor;
    output->target_budget_w = target_budget_w;
    output->applied_budget_w = applied_budget_w;
    output->mode = mode;
    return true;
}
