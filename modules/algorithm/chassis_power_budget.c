#include "chassis_power_budget.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static void ResetInvalid(ChassisPowerBudgetState_s *state,
                         ChassisPowerBudgetOutput_s *output)
{
    if (state != NULL)
    {
        memset(state, 0, sizeof(*state));
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

bool ChassisBufferBudgetConfigIsValid(const ChassisBufferBudgetConfig_s *config)
{
    if (config == NULL) return false;
    const float values[] = {config->max_energy_j, config->recovery_enter_j,
        config->recovery_exit_j, config->hard_floor_j, config->prediction_window_s,
        config->guard_window_s, config->settlement_delay_s, config->energy_margin_j,
        config->power_margin_w, config->max_boost_w, config->recovery_power_w,
        config->recovery_target_time_s, config->observer_max_step_s,
        config->recovery_hold_s, config->rise_rate_w_per_s, config->safety_factor,
        config->correction_rise_j_per_s};
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
        if (!isfinite(values[i]) || values[i] < 0)
            return false;
    return config->hard_floor_j < config->recovery_enter_j &&
           config->recovery_enter_j < config->recovery_exit_j &&
           config->recovery_exit_j <= config->max_energy_j &&
           config->prediction_window_s > 0 && config->guard_window_s > 0 &&
           config->rise_rate_w_per_s > 0 && config->recovery_power_w > 0 &&
           config->recovery_target_time_s > 0 && config->observer_max_step_s > 0 &&
           config->energy_margin_j < config->max_energy_j &&
           config->safety_factor > 0 && config->safety_factor <= 1 &&
           config->correction_rise_j_per_s > 0;
}

static float ApplyRise(float previous, float target, float rise)
{
    return target <= previous ? target : fminf(target, previous + rise);
}

static void RecordBufferHistory(ChassisPowerBudgetState_s *state, float elapsed_s,
                                float loss_w)
{
    if (elapsed_s <= 0)
        return;
    state->history[state->history_head] = (ChassisBufferHistorySample_s){
        .elapsed_s = elapsed_s, .loss_w = loss_w,
    };
    state->history_head = (state->history_head + 1U) % CHASSIS_BUFFER_HISTORY_LENGTH;
    if (state->history_count < CHASSIS_BUFFER_HISTORY_LENGTH)
        ++state->history_count;
}

/* Unknown history remains worst-case; observed charging cannot erase a drain. */
static float SettlementLoss(const ChassisPowerBudgetState_s *state, float window_s,
                             float unknown_loss_w)
{
    float remaining_s = window_s;
    float loss_j = 0;
    for (unsigned k = 0; k < state->history_count && remaining_s > 0; ++k)
    {
        const unsigned index = (state->history_head + CHASSIS_BUFFER_HISTORY_LENGTH -
            1U - k) % CHASSIS_BUFFER_HISTORY_LENGTH;
        const ChassisBufferHistorySample_s *sample = &state->history[index];
        const float used_s = fminf(remaining_s, sample->elapsed_s);
        loss_j += sample->loss_w * used_s;
        remaining_s = fmaxf(0, remaining_s - used_s);
    }
    return loss_j + remaining_s * unknown_loss_w;
}

static bool UpdateBufferEstimate(const ChassisBufferBudgetConfig_s *buffer,
                                 const ChassisPowerBudgetInput_s *input,
                                 bool meter_valid,
                                 ChassisPowerBudgetState_s *state,
                                 bool *continuous_meter,
                                 float *observed_elapsed_s)
{
    const uint32_t sample_delta = input->buffer_tick_ms - state->buffer_tick_ms;
    if (state->has_buffer_sample && sample_delta > UINT32_MAX / 2U)
    {
        state->last_input_power_valid = false;
        state->history_head = state->history_count = 0;
        return false; /* Keep the latest timestamp so repeated old frames stay rejected. */
    }
    const float elapsed_s = state->has_buffer_sample ?
        (float)sample_delta * 0.001f + input->buffer_age_s - state->last_buffer_age_s : 0;
    if (elapsed_s < -0.000001f)
    {
        state->buffer_estimate_valid = false;
        state->last_input_power_valid = false;
        state->history_head = state->history_count = 0;
        return false;
    }

    *observed_elapsed_s = fmaxf(0, elapsed_s);
    *continuous_meter = meter_valid && (!state->has_buffer_sample ||
        (state->last_input_power_valid &&
         *observed_elapsed_s <= buffer->observer_max_step_s + 0.000001f));
    const float measured_w = meter_valid ? fmaxf(0, input->measured_input_power_w) : 0;
    const float limit_w = fminf(state->last_referee_limit_w, input->referee_limit_w);
    const float power_w = fmaxf(state->last_input_power_w, measured_w);
    const float energy_rate_w = *continuous_meter ?
        limit_w - power_w - buffer->power_margin_w :
        -fmaxf(buffer->max_boost_w, power_w - limit_w) - buffer->power_margin_w;
    const float predicted_j = state->buffer_lower_j +
        energy_rate_w * *observed_elapsed_s;
    if (state->has_buffer_sample)
        RecordBufferHistory(state, *observed_elapsed_s,
            *continuous_meter ? fmaxf(0, power_w - limit_w) + buffer->power_margin_w :
                -energy_rate_w);
    if (!state->buffer_estimate_valid || sample_delta != 0U)
    {
        /* Unknown settlement phase and history before the first observation. */
        const float worst_excess_w = fmaxf(buffer->max_boost_w,
            measured_w - input->referee_limit_w);
        float lower_j = fmaxf(0, input->buffer_energy_j - buffer->energy_margin_j -
            SettlementLoss(state, input->buffer_age_s + buffer->settlement_delay_s,
                            worst_excess_w + buffer->power_margin_w));
        if (state->buffer_estimate_valid && sample_delta != 0U)
            lower_j = fminf(lower_j, fmaxf(0, predicted_j) +
                buffer->correction_rise_j_per_s * (float)sample_delta * 0.001f);
        state->buffer_lower_j = lower_j;
    }
    else
    {
        state->buffer_lower_j = predicted_j;
    }
    state->buffer_lower_j = ClampFloat(state->buffer_lower_j, 0,
        buffer->max_energy_j - buffer->energy_margin_j);
    state->buffer_tick_ms = input->buffer_tick_ms;
    state->last_buffer_age_s = input->buffer_age_s;
    state->last_input_power_w = measured_w;
    state->last_referee_limit_w = input->referee_limit_w;
    state->last_input_power_valid = meter_valid;
    state->has_buffer_sample = true;
    state->buffer_estimate_valid = true;
    return true;
}

static bool UpdateBufferBudget(const ChassisPowerBudgetConfig_s *config,
                              const ChassisPowerBudgetInput_s *input, float dt_s,
                              ChassisPowerBudgetState_s *state,
                              ChassisPowerBudgetOutput_s *output)
{
    const ChassisBufferBudgetConfig_s *buffer = &config->buffer;
    if (!ChassisBufferBudgetConfigIsValid(buffer))
    {
        ResetInvalid(state, output);
        return false;
    }
    bool buffer_valid = input->buffer_data_valid &&
        isfinite(input->buffer_energy_j) && input->buffer_energy_j >= 0 &&
        input->buffer_energy_j <= buffer->max_energy_j &&
        isfinite(input->buffer_age_s) && input->buffer_age_s >= 0;
    const bool meter_valid = input->input_power_valid &&
        isfinite(input->measured_input_power_w);
    bool continuous_meter = false;
    float observed_elapsed_s = 0;
    if (buffer_valid)
        buffer_valid = UpdateBufferEstimate(buffer, input, meter_valid, state,
                                            &continuous_meter, &observed_elapsed_s);
    else
    {
        state->buffer_estimate_valid = false;
        state->last_input_power_valid = false;
        state->history_head = state->history_count = 0;
    }
    const bool cap_healthy = input->cap_online && input->cap_output_enabled &&
        input->cap_error_code == 0 && isfinite(input->cap_energy_ratio) &&
        input->cap_energy_ratio >= 0 && input->cap_energy_ratio <= 1 &&
        isfinite(input->cap_reported_limit_w) && input->cap_reported_limit_w >= 0;
    const bool source_ready = !input->use_supercap ||
        (cap_healthy && input->cap_input_control_ready && input->load_power_valid &&
         isfinite(input->confirmed_input_target_w) && input->confirmed_input_target_w >= 0);
    const bool timing_valid = dt_s <= buffer->observer_max_step_s;
    const bool demand_valid = input->demand_valid &&
        isfinite(input->requested_load_power_w) && input->requested_load_power_w >= 0;
    const bool boost_ready = buffer_valid && continuous_meter && source_ready && timing_valid;
    const float lower = buffer_valid ? state->buffer_lower_j : 0;
    const float excess = meter_valid ? fmaxf(0,
        input->measured_input_power_w + buffer->power_margin_w -
            input->referee_limit_w) : 0;
    const float predicted = lower - excess * buffer->guard_window_s;

    if (buffer_valid && (lower <= buffer->recovery_enter_j ||
                         (meter_valid && predicted <= buffer->hard_floor_j)))
    {
        state->recovering = true;
        state->recovery_time_s = 0;
    }
    else if (state->recovering)
    {
        if (boost_ready && lower >= buffer->recovery_exit_j)
        {
            state->recovery_time_s += observed_elapsed_s;
            if (state->recovery_time_s >= buffer->recovery_hold_s)
            {
                state->recovering = false;
                state->recovery_time_s = 0;
            }
        }
        else
            state->recovery_time_s = 0;
    }

    float target_input_budget = input->referee_limit_w;
    float recovery_reduction_w = 0;
    if (state->recovering && buffer_valid)
    {
        recovery_reduction_w = ClampFloat(
            (buffer->recovery_exit_j - lower) / buffer->recovery_target_time_s,
            0, buffer->recovery_power_w);
        if (lower <= buffer->hard_floor_j || predicted <= buffer->hard_floor_j)
            recovery_reduction_w = buffer->recovery_power_w;
        target_input_budget = fmaxf(0, input->referee_limit_w - recovery_reduction_w);
    }
    else if (boost_ready && demand_valid &&
             input->requested_load_power_w > input->referee_limit_w * buffer->safety_factor)
    {
        const float energy_boost = fmaxf(0,
            (lower - buffer->recovery_enter_j) / buffer->prediction_window_s);
        const float safe_boost = fmaxf(0,
            (lower - buffer->hard_floor_j) / buffer->guard_window_s -
                buffer->power_margin_w);
        const float demand_boost = fmaxf(0,
            (input->requested_load_power_w + config->demand_margin_w) /
                buffer->safety_factor - input->referee_limit_w);
        target_input_budget += fminf(demand_boost, fminf(buffer->max_boost_w,
                                     fminf(energy_boost, safe_boost)));
    }

    if (!state->buffer_initialized || !isfinite(state->input_budget_w) ||
        !isfinite(state->applied_budget_w))
    {
        state->input_budget_w = fminf(input->referee_limit_w, target_input_budget);
        state->applied_budget_w = state->input_budget_w;
        state->buffer_initialized = true;
    }
    const float rise = timing_valid ? buffer->rise_rate_w_per_s * dt_s : 0;
    state->input_budget_w = ApplyRise(state->input_budget_w, target_input_budget, rise);

    float factor = 0;
    float target_load_budget = state->input_budget_w;
    if (input->use_supercap && source_ready &&
        isfinite(input->confirmed_input_target_w) && input->confirmed_input_target_w >= 0)
        target_load_budget = fminf(target_load_budget,
            input->confirmed_input_target_w / buffer->safety_factor);
    if (input->use_supercap && boost_ready && demand_valid)
    {
        factor = ClampFloat((input->cap_energy_ratio - config->empty_energy_ratio) /
            (config->full_energy_ratio - config->empty_energy_ratio), 0, 1);
        const float board_budget = input->cap_reported_limit_w / buffer->safety_factor;
        const float headroom = fmaxf(0, board_budget - target_load_budget);
        target_load_budget = fminf(board_budget, target_load_budget +
            factor * fminf(config->max_boost_w, headroom));
    }
    if (input->use_supercap && cap_healthy)
        target_load_budget = fminf(target_load_budget,
            input->cap_reported_limit_w / buffer->safety_factor);
    const float load_rise = input->use_supercap && timing_valid ?
        config->rise_rate_w_per_s * dt_s : rise;
    state->applied_budget_w = ApplyRise(state->applied_budget_w, target_load_budget, load_rise);
    state->mode = boost_ready ? CHASSIS_POWER_BUDGET_READY : CHASSIS_POWER_BUDGET_DEGRADED;
    *output = (ChassisPowerBudgetOutput_s){
        .energy_factor = factor,
        .target_budget_w = input->use_supercap ? target_load_budget : target_input_budget,
        .applied_budget_w = state->applied_budget_w,
        .mode = state->mode,
        .input_target_w = state->input_budget_w * buffer->safety_factor,
        .buffer_lower_j = lower,
        .buffer_predicted_j = predicted,
        .recovery_reduction_w = recovery_reduction_w,
        .recovering = state->recovering,
    };
    return true;
}

bool ChassisPowerBudgetConfigIsValid(const ChassisPowerBudgetConfig_s *config)
{
    return config != NULL &&
           isfinite(config->empty_energy_ratio) &&
           isfinite(config->full_energy_ratio) &&
           isfinite(config->max_boost_w) &&
           isfinite(config->rise_rate_w_per_s) &&
           isfinite(config->demand_margin_w) && config->demand_margin_w >= 0 &&
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

    memset(state, 0, sizeof(*state));

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

    if (input->buffer_control_enabled)
        return UpdateBufferBudget(config, input, dt_s, state, output);

    state->buffer_initialized = false;
    state->has_buffer_sample = false;
    state->buffer_estimate_valid = false;
    state->last_input_power_valid = false;
    state->history_head = state->history_count = 0;
    state->recovering = false;
    state->recovery_time_s = 0;
    memset(output, 0, sizeof(*output));

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
