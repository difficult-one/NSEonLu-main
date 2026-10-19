#include "chassis_spin.h"
#include "chassis_spin_config.h"

#include <math.h>
#include <stddef.h>

bool ChassisSpinGuardAllowsIncrease(const ChassisPowerTransientOutput_s *guard)
{
    return guard != NULL && guard->guard_scale == 1.0f && !guard->fault &&
           !guard->growth_fallback && !guard->guard_infeasible && !guard->guard_search_exhausted;
}

bool ChassisSpinTransientAllowsIncrease(const ChassisPowerTransientOutput_s *transient)
{
#if CHASSIS_SPIN_MODE == CHASSIS_SPIN_MODE_SINE
    /* A tracked low-load target can have no drive demand. Qualify that healthy
     * state so startup and a braking trough can initiate the next bounded rise. */
    return transient != NULL && !transient->fault && transient->meter_fresh &&
        transient->feedback_fresh && transient->timing_valid && transient->brake_mask == 0 &&
        (transient->mode == CHASSIS_POWER_TRANSIENT_RUN ||
         transient->mode == CHASSIS_POWER_TRANSIENT_IDLE) &&
        (transient->positive_allowed || transient->drive_mask == 0);
#else
    return transient != NULL && transient->positive_allowed && transient->brake_mask == 0;
#endif
}

bool ChassisSpinConfigIsValid(const ChassisSpinConfig_s *c)
{
    const bool common = c != NULL && isfinite(c->fixed_wz) && isfinite(c->min_wz) &&
           isfinite(c->max_wz) && isfinite(c->gain_wz_per_w_s) &&
           isfinite(c->rise_wz_per_s) && isfinite(c->fall_wz_per_s) &&
           isfinite(c->power_lpf_tau_s) && isfinite(c->power_deadband_w) &&
           isfinite(c->settle_time_s) && isfinite(c->budget_drop_reset_w) &&
           c->fixed_wz > 0 && c->min_wz > 0 && c->min_wz <= c->fixed_wz &&
           c->max_wz >= c->fixed_wz && c->gain_wz_per_w_s > 0 &&
           c->rise_wz_per_s > 0 && c->fall_wz_per_s > 0 &&
           c->power_lpf_tau_s > 0 && c->power_deadband_w >= 0 &&
           c->settle_time_s >= 0 && c->budget_drop_reset_w >= 0;
#if CHASSIS_SPIN_MODE == CHASSIS_SPIN_MODE_SINE
    return common && isfinite(c->sine_amplitude_wz) && c->sine_amplitude_wz >= 0 &&
        c->fixed_wz - c->sine_amplitude_wz >= c->min_wz &&
        c->fixed_wz + c->sine_amplitude_wz <= c->max_wz &&
        isfinite(c->sine_period_s) && c->sine_period_s > 0 &&
        isfinite(c->sine_ramp_s) && c->sine_ramp_s > 0 &&
        isfinite(c->sine_amplitude_wz / c->sine_ramp_s) &&
        isfinite(c->sine_rise_wz_per_s) && c->sine_rise_wz_per_s > 0 &&
        isfinite(c->sine_fall_wz_per_s) && c->sine_fall_wz_per_s > 0 &&
        (c->sine_direction == 1 || c->sine_direction == -1);
#else
    return common;
#endif
}

#if CHASSIS_SPIN_MODE != CHASSIS_SPIN_MODE_SINE
static void Reset(const ChassisSpinConfig_s *config, ChassisSpinState_s *state)
{
    *state = (ChassisSpinState_s){.target_wz = config->fixed_wz};
}
#else
static float Clamp(float value, float minimum, float maximum)
{
    return fminf(fmaxf(value, minimum), maximum);
}

static bool UpdateSine(const ChassisSpinConfig_s *c, const ChassisSpinInput_s *i,
                        float dt, ChassisSpinState_s *s, float *target)
{
    *target = 0;
    if (!i->spin_enabled || !i->control_enabled)
    {
        *s = (ChassisSpinState_s){0};
        return true;
    }
    if (!isfinite(dt) || dt <= 0 || dt > .020f ||
        !isfinite(i->target_power_w) || i->target_power_w <= 0 ||
        !isfinite(i->measured_power_w) ||
        (s->sine_direction_locked && s->sine_direction != 1 && s->sine_direction != -1) ||
        (s->initialized && (!isfinite(s->target_wz) || s->target_wz < c->min_wz ||
            s->target_wz > c->max_wz || !isfinite(s->filtered_power_w) ||
            !isfinite(s->settled_s) || s->settled_s < 0 ||
            !isfinite(s->sine_phase_s) ||
            s->sine_phase_s < 0 || !isfinite(s->sine_center_wz) ||
            s->sine_center_wz < c->min_wz || s->sine_center_wz > c->fixed_wz ||
            !isfinite(s->sine_amplitude_wz) || s->sine_amplitude_wz < 0 ||
            s->sine_amplitude_wz > c->sine_amplitude_wz)))
    {
        *s = (ChassisSpinState_s){0};
        return false;
    }
    const int8_t direction = s->sine_direction_locked ? s->sine_direction : c->sine_direction;
    if (!s->initialized || !i->feedback_valid)
    {
        *s = (ChassisSpinState_s){.target_wz = c->min_wz,
            .sine_center_wz = c->min_wz, .filtered_power_w = i->measured_power_w,
            .sine_direction = direction,
            .sine_direction_locked = true, .initialized = i->feedback_valid};
        if (!i->feedback_valid)
        {
            *target = (float)direction * c->min_wz;
            return true;
        }
    }
    const float alpha = dt / (c->power_lpf_tau_s + dt);
    s->filtered_power_w = (1 - alpha) * s->filtered_power_w + alpha * i->measured_power_w;
    s->sine_phase_s = fmodf(s->sine_phase_s + dt, c->sine_period_s);
    /* A demand-driven budget can fall when buffer boost is no longer needed.
     * Keep phase/envelope independent of that change; actual excess power and
     * protection flags below constrain the target through the normal slew. */
    const float excess_w = fmaxf(0, fmaxf(i->measured_power_w, s->filtered_power_w) -
        i->target_power_w - c->power_deadband_w);
    const bool restricted = i->energy_recovering || i->power_limited || excess_w > 0;
    const bool can_increase = i->increase_allowed && !restricted && i->measured_power_w >= 0;
    s->settled_s = can_increase ? fminf(s->settled_s + dt, c->settle_time_s) : 0;
    const bool stable = can_increase && s->settled_s >= c->settle_time_s;
    if (excess_w > 0)
        s->sine_center_wz = fmaxf(c->min_wz, s->sine_center_wz -
            fminf(c->gain_wz_per_w_s * excess_w, c->sine_fall_wz_per_s) * dt);
    else if (stable)
        s->sine_center_wz = fminf(c->fixed_wz, s->sine_center_wz + c->sine_rise_wz_per_s * dt);

    const float amplitude_step = c->sine_amplitude_wz / c->sine_ramp_s * dt;
    if (restricted)
        s->sine_amplitude_wz = fmaxf(0, s->sine_amplitude_wz - amplitude_step);
    else if (stable && s->sine_center_wz >= c->fixed_wz)
        s->sine_amplitude_wz = fminf(c->sine_amplitude_wz, s->sine_amplitude_wz + amplitude_step);
    const float requested = Clamp(s->sine_center_wz + s->sine_amplitude_wz *
        (s->sine_center_wz / c->fixed_wz) *
            sinf(6.28318530718f * (s->sine_phase_s / c->sine_period_s)), c->min_wz, c->max_wz);
    float change = requested - s->target_wz;
    if (change > 0 && !stable) change = 0;
    s->target_wz = Clamp(s->target_wz + Clamp(change,
        -c->sine_fall_wz_per_s * dt, c->sine_rise_wz_per_s * dt), c->min_wz, c->max_wz);
    *target = (float)direction * s->target_wz;
    return true;
}
#endif

static bool UpdateTarget(const ChassisSpinConfig_s *config,
                       const ChassisSpinInput_s *input,
                       float dt_s,
                       ChassisSpinState_s *state,
                       float *target_wz)
{
    if (target_wz != NULL)
        *target_wz = 0;
    if (!ChassisSpinConfigIsValid(config) || input == NULL || state == NULL || target_wz == NULL)
    {
#if CHASSIS_SPIN_MODE == CHASSIS_SPIN_MODE_SINE
        if (state != NULL) *state = (ChassisSpinState_s){0};
#endif
        return false;
    }
    *target_wz = config->fixed_wz;

#if CHASSIS_SPIN_MODE == CHASSIS_SPIN_MODE_FIXED
    (void)dt_s;
    Reset(config, state);
    return true;
#elif CHASSIS_SPIN_MODE == CHASSIS_SPIN_MODE_SINE
    return UpdateSine(config, input, dt_s, state, target_wz);
#else
    if (!input->spin_enabled || !input->control_enabled || !input->feedback_valid)
    {
        Reset(config, state);
        return true;
    }
    if (!isfinite(dt_s) || dt_s <= 0 || dt_s > .020f ||
        !isfinite(input->target_power_w) || input->target_power_w <= 0 ||
        !isfinite(input->measured_power_w) ||
        (state->initialized &&
         (!isfinite(state->target_wz) || !isfinite(state->filtered_power_w) ||
          !isfinite(state->settled_s) || state->settled_s < 0 ||
          !isfinite(state->previous_target_power_w))))
    {
        Reset(config, state);
        return false;
    }
    if (!state->initialized)
    {
        Reset(config, state);
        state->filtered_power_w = input->measured_power_w;
        state->initialized = true;
    }
    else
    {
        state->filtered_power_w += dt_s / (config->power_lpf_tau_s + dt_s) *
            (input->measured_power_w - state->filtered_power_w);
    }
    if (state->previous_target_power_w >
        input->target_power_w + config->budget_drop_reset_w)
    {
        state->target_wz = fminf(state->target_wz, config->fixed_wz);
        state->settled_s = 0;
    }
    state->previous_target_power_w = input->target_power_w;
    const bool can_increase = input->increase_allowed && input->measured_power_w >= 0;
    state->settled_s = can_increase ?
        fminf(state->settled_s + dt_s, config->settle_time_s) : 0;

    const float error = input->target_power_w - state->filtered_power_w;
    const float outside_deadband = copysignf(
        fmaxf(fabsf(error) - config->power_deadband_w, 0), error);
    float rate = config->gain_wz_per_w_s * outside_deadband;
    if (rate > 0 && (!can_increase || state->settled_s < config->settle_time_s))
        rate = 0;
    rate = fminf(fmaxf(rate, -config->fall_wz_per_s), config->rise_wz_per_s);
    state->target_wz = fminf(fmaxf(state->target_wz + rate * dt_s, config->min_wz),
                            config->max_wz);
    *target_wz = state->target_wz;
    return true;
#endif
}

bool ChassisSpinRetune(const ChassisSpinConfig_s *config, ChassisSpinState_s *state)
{
    if (!state || !ChassisSpinConfigIsValid(config)) return false;
#if CHASSIS_SPIN_MODE == CHASSIS_SPIN_MODE_SINE
    if (state->sine_direction_locked && config->sine_direction != state->sine_direction)
        return false;
#endif
    const float previous = state->applied_wz;
    const int8_t direction = state->sine_direction;
    const bool locked = state->sine_direction_locked;
    *state = (ChassisSpinState_s){.applied_wz = previous, .retuning = previous != 0,
        .sine_direction = direction, .sine_direction_locked = locked};
    return true;
}

bool ChassisSpinUpdate(const ChassisSpinConfig_s *config,
                       const ChassisSpinInput_s *input, float dt_s,
                       ChassisSpinState_s *state, float *target_wz)
{
    const float previous = state ? state->applied_wz : 0;
    const bool retuning = state && state->retuning;
    bool ok = UpdateTarget(config, input, dt_s, state, target_wz);
    if (!state || !target_wz) return ok;
    state->retuning = false;
    if (ok && retuning && input->spin_enabled && input->control_enabled &&
        input->feedback_valid && isfinite(dt_s) && dt_s > 0 && dt_s <= .020f) {
#if CHASSIS_SPIN_MODE == CHASSIS_SPIN_MODE_SINE
        float rate = fabsf(*target_wz) > fabsf(previous) ?
            config->sine_rise_wz_per_s : config->sine_fall_wz_per_s;
#else
        float rate = fabsf(*target_wz) > fabsf(previous) ?
            config->rise_wz_per_s : config->fall_wz_per_s;
#endif
        if (fabsf(*target_wz) > fabsf(previous) &&
            (!input->increase_allowed || input->power_limited || input->energy_recovering ||
             input->measured_power_w > input->target_power_w + config->power_deadband_w))
            rate = 0;
        const float change = *target_wz - previous;
        const float step = fminf(fmaxf(change, -rate * dt_s), rate * dt_s);
        state->retuning = fabsf(change - step) > .0001f;
        *target_wz = previous + step;
    }
    state->applied_wz = *target_wz;
    return ok;
}
