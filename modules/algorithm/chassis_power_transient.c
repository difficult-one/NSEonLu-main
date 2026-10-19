#include "chassis_power_transient.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

static float Clamp(float v, float lo, float hi) { return fminf(hi, fmaxf(lo, v)); }
static bool ConfigValid(const ChassisPowerTransientConfig_s *c)
{
    if (c == NULL) return false;
    const float values[] = {c->eta, c->fast_gain, c->release_w_per_s, c->stale_release_w_per_s,
        c->max_current_command, c->acceleration_tau_s, c->prediction_s, c->growth_time_s,
        c->peak_ratio, c->low_rpm, c->low_current, c->target_step_rpm, c->zero_enter_rpm,
        c->zero_exit_rpm, c->demand_enter_current, c->demand_exit_current};
    for (unsigned n = 0; n < sizeof(values) / sizeof(values[0]); ++n)
        if (!isfinite(values[n])) return false;
    return c->eta > 0 && c->eta <= 1 && c->fast_gain >= 0 && c->release_w_per_s > 0 &&
        c->stale_release_w_per_s > 0 && c->max_current_command > 0 && c->max_current_command <= 32767 &&
        c->acceleration_tau_s > 0 && c->prediction_s >= 0 && c->growth_time_s > 0 &&
        c->peak_ratio >= 1 && c->low_rpm > 0 && c->low_current > 0 && c->target_step_rpm > 0 &&
        c->zero_enter_rpm >= 0 && c->zero_exit_rpm > c->zero_enter_rpm &&
        c->demand_exit_current >= 0 && c->demand_enter_current > c->demand_exit_current &&
        c->meter_fresh_ms > 0 && c->feedback_fresh_ms > 0 && c->release_confirm_ms > 0 &&
        c->low_hold_ms > 0 && c->startup_window_ms > 0 && c->startup_cooldown_ms >= c->startup_window_ms &&
        c->guard_search_nodes > 0 && c->guard_search_nodes <= 1024;
}
ChassisPowerTransientConfig_s ChassisPowerTransientDefaultConfig(void)
{
    return (ChassisPowerTransientConfig_s){.eta = .95f, .fast_gain = .5f,
        .release_w_per_s = 60, .stale_release_w_per_s = 20,
        .meter_fresh_ms = 20, .feedback_fresh_ms = 5,
        .release_confirm_ms = 10, .stable_run_ms = 100,
        .max_current_command = 15000, .acceleration_tau_s = .010f, .prediction_s = .008f,
        .growth_time_s = .005f, .peak_ratio = 1.30f, .low_rpm = 100, .low_current = 500,
        .target_step_rpm = 300, .zero_enter_rpm = 50, .zero_exit_rpm = 100,
        .demand_enter_current = 200, .demand_exit_current = 100,
        .low_hold_ms = 100, .startup_window_ms = 30, .startup_cooldown_ms = 500,
        .guard_search_nodes = 256};
}
void ChassisPowerTransientReset(ChassisPowerTransientState_s *s)
{
    if (s != NULL) memset(s, 0, sizeof(*s));
}
static void ResetControlHistory(ChassisPowerTransientState_s *s)
{
    const uint32_t last = s->last_startup_ms;
    const bool seen = s->startup_seen;
    ChassisPowerTransientReset(s);
    s->last_startup_ms = last; s->startup_seen = seen;
}
void ChassisPowerTransientRecordSent(ChassisPowerTransientState_s *s, const float current[4])
{
    if (s != NULL && current != NULL) memcpy(s->sent_current, current, sizeof(s->sent_current));
}
bool ChassisPowerTransientUpdate(const ChassisPowerTransientConfig_s *c,
    const ChassisPowerTransientInput_s *i, float dt, ChassisPowerTransientState_s *s,
    ChassisPowerTransientOutput_s *o)
{
    if (o == NULL || s == NULL) return false;
    memset(o, 0, sizeof(*o));
    if (!ConfigValid(c) || i == NULL) {
        ResetControlHistory(s); o->fault = true; return false;
    }
    if (!i->control_enabled) {
        /* Preserve the refractory clock across stop/start; clear all control history. */
        ResetControlHistory(s);
        return true;
    }
    bool valid = isfinite(i->budget_w) && i->budget_w > 0 && isfinite(i->target_w) &&
                 i->target_w > 0 && isfinite(i->slow_w) && isfinite(s->fast_w) &&
                 isfinite(s->slow_applied_w) && isfinite(i->power_deadband_w) &&
                 i->power_deadband_w >= 0 && isfinite(i->power_ceiling_w) &&
                 i->power_ceiling_w >= 0;
    o->feedback_fresh = i->online_mask == 15 && i->enabled_mask == 15;
    float average_rpm = 0, maximum_sent = 0;
    bool target_step = false;
    for (unsigned w = 0; w < 4; ++w) {
        valid &= isfinite(i->raw_current[w]) && isfinite(i->actual_rpm[w]) && isfinite(i->target_rpm[w]);
        o->feedback_fresh &= i->feedback_sequence[w] != 0 &&
            (uint32_t)(i->now_ms - i->feedback_tick_ms[w]) <= c->feedback_fresh_ms;
        const uint8_t bit = (uint8_t)(1U << w);
        const float speed = fabsf(i->actual_rpm[w]);
        if (!s->initialized || speed <= c->zero_enter_rpm) s->near_zero_mask |= bit;
        if (speed >= c->zero_exit_rpm) s->near_zero_mask &= (uint8_t)~bit;
        if (fabsf(i->raw_current[w]) >= c->demand_enter_current) s->demand_mask |= bit;
        if (fabsf(i->raw_current[w]) <= c->demand_exit_current) s->demand_mask &= (uint8_t)~bit;
        const float direction = (s->near_zero_mask & bit) ? i->target_rpm[w] : i->actual_rpm[w];
        if (s->demand_mask & bit) {
            if (i->raw_current[w] * direction > 0) o->drive_mask |= bit;
            if (i->raw_current[w] * direction < 0) o->brake_mask |= bit;
        }
        const bool fresh = (i->online_mask & bit) && i->feedback_sequence[w] != 0 &&
            (uint32_t)(i->now_ms - i->feedback_tick_ms[w]) <= c->feedback_fresh_ms;
        if (!fresh) { s->feedback_seen[w] = 0; s->acceleration_rpm_s[w] = 0; }
        else if (s->feedback_seen[w] == 0 || i->feedback_sequence[w] != s->feedback_sequence[w]) {
            const uint32_t gap = i->feedback_tick_ms[w] - s->feedback_tick_ms[w];
            if (s->feedback_seen[w] != 0 && gap > 0 && gap <= c->feedback_fresh_ms) {
                const float feedback_dt = gap * .001f;
                const float acceleration = (speed - s->previous_abs_rpm[w]) / feedback_dt;
                if (s->feedback_seen[w] < 2) s->acceleration_rpm_s[w] = acceleration;
                else s->acceleration_rpm_s[w] += feedback_dt / (c->acceleration_tau_s + feedback_dt) *
                    (acceleration - s->acceleration_rpm_s[w]);
                s->feedback_seen[w] = 2;
            } else if (s->feedback_seen[w] == 0 || gap > c->feedback_fresh_ms) {
                s->feedback_seen[w] = 1; s->acceleration_rpm_s[w] = 0;
            }
            if (gap > 0 || s->feedback_seen[w] == 1) {
                s->feedback_tick_ms[w] = i->feedback_tick_ms[w]; s->previous_abs_rpm[w] = speed;
            }
            s->feedback_sequence[w] = i->feedback_sequence[w];
        }
        o->guard_rpm[w] = speed + (fresh && s->feedback_seen[w] >= 2 && (o->drive_mask & bit) ?
            fmaxf(s->acceleration_rpm_s[w], 0) * c->prediction_s : 0);
        valid &= isfinite(o->guard_rpm[w]) && isfinite(s->sent_current[w]);
        average_rpm += speed * .25f;
        maximum_sent = fmaxf(maximum_sent, fabsf(s->sent_current[w]));
        target_step |= fabsf(i->target_rpm[w] - s->previous_target[w]) >= c->target_step_rpm;
    }
    if (!valid) { ResetControlHistory(s); o->fault = true; return false; }
    o->timing_valid = isfinite(dt) && dt > 0 && dt <= .005f;
    if (!o->timing_valid) dt = 0;
    o->dt_s = dt;
    o->meter_fresh = i->meter_enabled && i->meter_online && isfinite(i->meter_w) &&
        (uint32_t)(i->now_ms - i->meter_tick_ms) <= c->meter_fresh_ms;
    const bool new_meter = o->meter_fresh && (!s->meter_initialized || s->meter_tick_ms != i->meter_tick_ms);
    const bool rising_power = new_meter && s->meter_initialized && i->meter_w > s->previous_meter_w;
    const float previous_fast_w = s->fast_w;
    const bool meter_recovered = o->meter_fresh &&
        (s->fresh_samples >= 2 || (s->fresh_samples == 1 && new_meter));
    const bool low = average_rpm < c->low_rpm && maximum_sent < c->low_current &&
        i->online_mask == 15 && i->enabled_mask == 15 && o->brake_mask == 0;
    const bool armed = s->low_valid && (uint32_t)(i->now_ms - s->low_since_ms) >= c->low_hold_ms;
    if (i->startup_enabled && i->meter_enabled && meter_recovered && o->feedback_fresh &&
        o->timing_valid && armed && target_step && o->drive_mask && !o->brake_mask &&
        s->fast_w == 0 && !s->startup_active &&
        (!s->startup_seen || (uint32_t)(i->now_ms - s->last_startup_ms) >= c->startup_cooldown_ms)) {
        s->startup_active = s->startup_seen = true;
        s->startup_since_ms = s->last_startup_ms = i->now_ms;
        s->startup_budget_w = i->budget_w; s->peak_w = c->peak_ratio * i->budget_w;
        s->allowance_j = (c->peak_ratio - 1) * i->budget_w * c->startup_window_ms * .001f;
        s->excess_j = 0; s->low_valid = false; o->startup_triggered = true;
    }
    if (s->startup_active && i->power_ceiling_w > 0)
        s->peak_w = fminf(s->peak_w, i->power_ceiling_w);
    if (s->startup_active) {
        if (o->timing_valid && o->meter_fresh) s->excess_j += fmaxf(i->meter_w - i->budget_w, 0) * dt;
        if (!i->startup_enabled || !o->timing_valid || !o->meter_fresh || !o->feedback_fresh ||
            o->brake_mask || !o->drive_mask || i->budget_w < s->startup_budget_w ||
            i->meter_w > s->peak_w || s->excess_j >= s->allowance_j ||
            (uint32_t)(i->now_ms - s->startup_since_ms) >= c->startup_window_ms)
            s->startup_active = false;
    }
    if (low && !s->startup_active && !o->startup_triggered) {
        if (!s->low_valid) { s->low_valid = true; s->low_since_ms = i->now_ms; }
    } else s->low_valid = false;
    memcpy(s->previous_target, i->target_rpm, sizeof(s->previous_target));
    s->initialized = true;
    if (!o->meter_fresh) {
        s->meter_initialized = false; s->fresh_samples = s->below_samples = 0; s->below_valid = false;
        s->mode = CHASSIS_POWER_TRANSIENT_OFFLINE;
        s->fast_w = i->meter_enabled ? fminf(0, s->fast_w + c->stale_release_w_per_s * dt) : 0;
    } else {
        if (new_meter) {
            s->meter_initialized = true; s->meter_tick_ms = i->meter_tick_ms;
            s->previous_meter_w = i->meter_w;
            if (s->fresh_samples < 2) ++s->fresh_samples;
        }
        const float meter_ceiling = i->power_ceiling_w > 0 ?
            fminf(s->startup_active ? s->peak_w : i->budget_w, i->power_ceiling_w) :
            (s->startup_active ? s->peak_w : i->budget_w);
        if (i->meter_w > meter_ceiling) {
            const float safe_target = i->power_ceiling_w > 0 ?
                fminf(i->target_w, i->power_ceiling_w) : i->target_w;
            const float request = Clamp(-c->fast_gain * (i->meter_w - safe_target) / c->eta, -i->budget_w, 0);
            s->fast_w = fminf(s->fast_w, request);
            s->below_valid = false; s->below_samples = 0;
            s->mode = CHASSIS_POWER_TRANSIENT_CLAMP;
            s->startup_active = false;
        } else if (s->fast_w < 0) {
            if (i->meter_w < i->target_w - .5f) {
                if (new_meter) {
                    if (!s->below_valid) { s->below_valid = true; s->below_since_ms = i->now_ms; }
                    if (s->below_samples < 2) ++s->below_samples;
                }
                if (s->below_valid && s->below_samples >= 2 &&
                    (uint32_t)(i->now_ms - s->below_since_ms) >= c->release_confirm_ms) {
                    s->mode = CHASSIS_POWER_TRANSIENT_RECOVERY;
                    s->fast_w = fminf(0, s->fast_w + c->release_w_per_s * dt);
                } else s->mode = CHASSIS_POWER_TRANSIENT_CLAMP;
            } else { s->below_valid = false; s->below_samples = 0; s->mode = CHASSIS_POWER_TRANSIENT_CLAMP; }
            if (s->fast_w == 0) s->mode = CHASSIS_POWER_TRANSIENT_RUN;
        } else s->mode = s->fresh_samples >= 2 ? CHASSIS_POWER_TRANSIENT_RUN : CHASSIS_POWER_TRANSIENT_OFFLINE;
    }
    if (s->startup_active && s->fast_w == 0) s->mode = CHASSIS_POWER_TRANSIENT_STARTUP;
    if (!o->feedback_fresh)
        s->mode = CHASSIS_POWER_TRANSIENT_OFFLINE;
    if (s->mode == CHASSIS_POWER_TRANSIENT_RUN && low && !o->drive_mask)
        s->mode = CHASSIS_POWER_TRANSIENT_IDLE;
    if (s->mode == CHASSIS_POWER_TRANSIENT_RUN) {
        if (!s->run_valid) { s->run_valid = true; s->run_since_ms = i->now_ms; }
    } else s->run_valid = false;
    o->positive_allowed = s->mode == CHASSIS_POWER_TRANSIENT_RUN &&
        o->feedback_fresh && o->drive_mask != 0 && o->brake_mask == 0 &&
        i->meter_w >= 0 && o->timing_valid;
    o->learning_allowed = o->positive_allowed &&
        (uint32_t)(i->now_ms - s->run_since_ms) >= c->stable_run_ms;
    const float overpower_threshold = i->target_w + i->power_deadband_w;
    const bool overpower = o->meter_fresh && o->feedback_fresh && o->timing_valid &&
        s->fresh_samples >= 2 && !s->startup_active && o->drive_mask != 0 &&
        o->brake_mask == 0 && i->meter_w > overpower_threshold &&
        (s->mode == CHASSIS_POWER_TRANSIENT_RUN || s->mode == CHASSIS_POWER_TRANSIENT_CLAMP);
    if (overpower) {
        if (!s->overpower_valid || (rising_power && s->fast_w < previous_fast_w) ||
            overpower_threshold > s->overpower_threshold_w) {
            s->overpower_valid = true; s->overpower_since_ms = i->now_ms;
        }
        s->overpower_threshold_w = overpower_threshold;
    } else s->overpower_valid = false;
    o->negative_learning_allowed = overpower &&
        (uint32_t)(i->now_ms - s->overpower_since_ms) >= c->stable_run_ms;
    o->negative_learning_threshold_w = overpower_threshold;
    const float slow_target = i->meter_enabled ? (o->positive_allowed ? i->slow_w : fminf(i->slow_w, 0)) : 0;
    s->slow_applied_w = slow_target > s->slow_applied_w ?
        fminf(slow_target, s->slow_applied_w + 20 * dt) : slow_target;
    if (!o->positive_allowed) s->slow_applied_w = fminf(s->slow_applied_w, 0);
    if (!i->meter_enabled) s->slow_applied_w = 0;
    o->slow_applied_w = s->slow_applied_w;
    s->fast_w = Clamp(s->fast_w, -i->budget_w, 0);
    o->fast_w = s->fast_w;
    o->model_budget_w = Clamp(i->budget_w + o->slow_applied_w + o->fast_w, 0, i->budget_w + 8);
    o->effective_w = c->eta * o->model_budget_w; o->mode = s->mode;
    o->startup_active = s->startup_active;
    o->excess_j = s->excess_j; o->allowance_j = s->allowance_j;
    o->allowed_guard_w = s->startup_active ? s->peak_w : o->effective_w;
    if (i->power_ceiling_w > 0)
        o->allowed_guard_w = fminf(o->allowed_guard_w, i->power_ceiling_w);
    return true;
}

static float QuantizedGuardPower(const MotorPowerModelConfig_s models[4],
    const float candidate[4], float scale, ChassisPowerTransientOutput_s *o)
{
    float total = 0;
    for (unsigned w = 0; w < 4; ++w) {
        const bool drive = (o->drive_mask & (1U << w)) != 0;
        o->guarded_current[w] = (float)(int16_t)(candidate[w] * (drive ? scale : 1));
        const float p = PowerModelPredict(&models[w], o->guarded_current[w], o->guard_rpm[w]);
        if (!isfinite(p)) return NAN;
        total += drive ? p : fmaxf(p, 0);
    }
    return total;
}

/* Step across the nearest command boundary, keeping a common drive scale. */
static float NextLowerCommandScale(const float candidate[4], float scale, uint8_t drive_mask)
{
    float boundary = 0;
    for (unsigned w = 0; w < 4; ++w) {
        if (!(drive_mask & (1U << w)) || candidate[w] == 0) continue;
        const float command = fabsf(candidate[w]);
        const float quantized = floorf(command * scale);
        if (quantized > 0) boundary = fmaxf(boundary, quantized / command);
    }
    return nextafterf(fminf(boundary, nextafterf(scale, 0)), 0);
}

static float IntegerIntervalMinimum(const MotorPowerModelConfig_s models[4],
    const float candidate[4], float lo, float hi, const ChassisPowerTransientOutput_s *o,
    bool *same_commands)
{
    float lower = 0;
    *same_commands = true;
    for (unsigned w = 0; w < 4; ++w) {
        const MotorPowerModelConfig_s *m = &models[w];
        const float n = o->guard_rpm[w];
        if (!(o->drive_mask & (1U << w))) {
            lower += fmaxf(0, PowerModelPredict(m, (float)(int16_t)candidate[w], n));
            continue;
        }
        const float qlo = floorf(fabsf(candidate[w]) * lo);
        const float qhi = floorf(fabsf(candidate[w]) * hi);
        *same_commands &= qlo == qhi;
        float minimum = fminf(PowerModelPredict(m, qlo, n), PowerModelPredict(m, qhi, n));
        if (m->k4 > 0) {
            const float vertex = -(m->k1 + m->k3 * n) / (2 * m->k4) * m->current_conversion;
            if (vertex > qlo && vertex < qhi) {
                minimum = fminf(minimum, PowerModelPredict(m, floorf(vertex), n));
                minimum = fminf(minimum, PowerModelPredict(m, ceilf(vertex), n));
            }
        }
        lower += minimum;
    }
    return lower;
}

/* Right-first search: an independent wheel minimum is a safe interval lower bound.
 * The fixed stack and node budget bound control-task work. Exhaustion is separate
 * from proving every interval infeasible. No recursion or dynamic allocation. */
static bool SearchIntegerScale(const ChassisPowerTransientConfig_s *c,
    const MotorPowerModelConfig_s models[4], const float candidate[4], float maximum,
    float *scale, ChassisPowerTransientOutput_s *o)
{
    struct { float lo, hi; } stack[48];
    unsigned pending = 1, nodes = 0;
    stack[0].lo = 0; stack[0].hi = maximum;
    while (pending != 0) {
        if (++nodes > c->guard_search_nodes) { o->guard_search_exhausted = true; return false; }
        const float lo = stack[--pending].lo, hi = stack[pending].hi;
        o->guard_power_w = QuantizedGuardPower(models, candidate, hi, o);
        if (!isfinite(o->guard_power_w)) { o->fault = true; return false; }
        if (o->guard_power_w <= o->allowed_guard_w + 1e-4f) { *scale = hi; return true; }
        bool same;
        const float lower = IntegerIntervalMinimum(models, candidate, lo, hi, o, &same);
        if (!isfinite(lower)) { o->fault = true; return false; }
        if (same || lower > o->allowed_guard_w + 1e-4f) continue;
        const float mid = lo + (hi - lo) * .5f;
        if (mid <= lo || mid >= hi) {
            o->guard_power_w = QuantizedGuardPower(models, candidate, lo, o);
            if (o->guard_power_w <= o->allowed_guard_w + 1e-4f) { *scale = lo; return true; }
            continue;
        }
        if (pending + 2 > sizeof(stack) / sizeof(stack[0])) {
            o->guard_search_exhausted = true; return false;
        }
        stack[pending].lo = lo; stack[pending++].hi = nextafterf(mid, 0);
        stack[pending].lo = mid; stack[pending++].hi = hi;
    }
    return false;
}

bool ChassisPowerTransientGuard(const ChassisPowerTransientConfig_s *c,
    const MotorPowerModelConfig_s models[4], const ChassisPowerTransientInput_s *i,
    const float candidate[4], const ChassisPowerTransientState_s *s,
    ChassisPowerTransientOutput_s *o)
{
    if (!ConfigValid(c) || models == NULL || i == NULL || candidate == NULL || s == NULL || o == NULL) return false;
    float a = 0, b = 0, constant = 0, upper = 1;
    o->guard_infeasible = o->guard_search_exhausted = false; o->growth_fallback = false;
    for (unsigned w = 0; w < 4; ++w) {
        if (!PowerModelConfigIsValid(&models[w]) || !isfinite(candidate[w]) ||
            !isfinite(o->guard_rpm[w]) || fabsf(candidate[w]) > c->max_current_command) {
            memset(o->guarded_current, 0, sizeof(o->guarded_current));
            o->fault = o->guard_infeasible = true; return false;
        }
        const uint8_t bit = (uint8_t)(1U << w);
        if (o->drive_mask & bit) {
            const float u = fabsf(candidate[w] / models[w].current_conversion);
            const float n = o->guard_rpm[w];
            a += models[w].k4 * u * u;
            b += (models[w].k1 + models[w].k3 * n) * u;
            constant += models[w].k0 + models[w].k2 * n + models[w].k5 * n * n;
            const bool fallback = s->feedback_seen[w] < 2 ||
                (uint32_t)(i->now_ms - i->feedback_tick_ms[w]) > c->feedback_fresh_ms;
            o->growth_fallback |= fallback;
            if ((fallback || !o->timing_valid) && candidate[w] != 0) {
                const float previous = fmaxf(0, s->sent_current[w] * copysignf(1, candidate[w]));
                const float increase = o->timing_valid ? c->max_current_command / c->growth_time_s * o->dt_s : 0;
                upper = fminf(upper, Clamp((previous + increase) / fabsf(candidate[w]), 0, 1));
            }
        } else constant += fmaxf(0, PowerModelPredict(&models[w], candidate[w], i->actual_rpm[w]));
    }
    o->candidate_guard_w = a + b + constant;
    float scale = 0;
    bool feasible = PowerModelFindFeasibleScale(a, b, constant - o->allowed_guard_w, upper, &scale);
    o->guard_power_w = QuantizedGuardPower(models, candidate, feasible ? scale : 0, o);
    /* Bounded local search avoids a full scan of up to 60000 integer boundaries. */
    for (unsigned attempt = 0; feasible && attempt < 8 &&
        o->guard_power_w > o->allowed_guard_w + 1e-4f && scale > 0; ++attempt) {
        scale = NextLowerCommandScale(candidate, scale, o->drive_mask);
        o->guard_power_w = QuantizedGuardPower(models, candidate, scale, o);
    }
    if (!feasible || o->guard_power_w > o->allowed_guard_w + 1e-4f)
        feasible = SearchIntegerScale(c, models, candidate, upper, &scale, o);
    o->guard_scale = feasible ? scale : 0;
    o->fault |= !isfinite(o->guard_power_w) || !isfinite(o->candidate_guard_w);
    o->guard_infeasible = (!feasible && !o->guard_search_exhausted) || o->fault;
    if (!feasible || o->fault) {
        o->guard_scale = 0;
        o->guard_power_w = QuantizedGuardPower(models, candidate, 0, o);
        if (o->fault) memset(o->guarded_current, 0, sizeof(o->guarded_current));
    }
    return feasible && !o->fault;
}
