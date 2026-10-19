#ifndef CHASSIS_POWER_TRANSIENT_H
#define CHASSIS_POWER_TRANSIENT_H
#include "power_model.h"

typedef enum {
    CHASSIS_POWER_TRANSIENT_IDLE = 0,
    CHASSIS_POWER_TRANSIENT_STARTUP,
    CHASSIS_POWER_TRANSIENT_CLAMP,
    CHASSIS_POWER_TRANSIENT_RECOVERY,
    CHASSIS_POWER_TRANSIENT_RUN,
    CHASSIS_POWER_TRANSIENT_OFFLINE,
} ChassisPowerTransientMode_e;

typedef struct {
    float eta, fast_gain, release_w_per_s, stale_release_w_per_s;
    uint32_t meter_fresh_ms, feedback_fresh_ms, release_confirm_ms, stable_run_ms;
    float max_current_command, acceleration_tau_s, prediction_s, growth_time_s;
    float peak_ratio, low_rpm, low_current, target_step_rpm;
    float zero_enter_rpm, zero_exit_rpm, demand_enter_current, demand_exit_current;
    uint32_t low_hold_ms, startup_window_ms, startup_cooldown_ms;
    uint16_t guard_search_nodes;
} ChassisPowerTransientConfig_s;
typedef struct {
    float budget_w, target_w, slow_w, meter_w;
    float power_deadband_w;
    float power_ceiling_w; /* 0: legacy behavior; >0: source/energy hard ceiling. */
    float raw_current[4], actual_rpm[4], target_rpm[4];
    uint32_t now_ms, meter_tick_ms, feedback_tick_ms[4], feedback_sequence[4];
    uint8_t online_mask, enabled_mask;
    bool meter_enabled, meter_online, control_enabled;
    bool startup_enabled;
} ChassisPowerTransientInput_s;
typedef struct {
    float fast_w, sent_current[4], slow_applied_w;
    uint32_t meter_tick_ms, below_since_ms, run_since_ms;
    uint8_t fresh_samples, below_samples;
    bool meter_initialized, below_valid, run_valid;
    ChassisPowerTransientMode_e mode;
    float previous_target[4], previous_abs_rpm[4], acceleration_rpm_s[4];
    uint32_t feedback_sequence[4], feedback_tick_ms[4];
    uint8_t feedback_seen[4], near_zero_mask, demand_mask;
    uint32_t low_since_ms, startup_since_ms, last_startup_ms;
    float startup_budget_w, peak_w, excess_j, allowance_j;
    bool initialized, low_valid, startup_active, startup_seen;
    uint32_t overpower_since_ms;
    float overpower_threshold_w;
    float previous_meter_w;
    bool overpower_valid;
} ChassisPowerTransientState_s;
typedef struct {
    float fast_w, slow_applied_w, model_budget_w, effective_w;
    uint8_t drive_mask, brake_mask;
    bool learning_allowed, positive_allowed, meter_fresh, feedback_fresh, timing_valid, fault;
    bool negative_learning_allowed; /* Sustained fresh driving overpower, outside startup. */
    float negative_learning_threshold_w;
    ChassisPowerTransientMode_e mode;
    float guard_rpm[4], guarded_current[4], allowed_guard_w, guard_power_w, candidate_guard_w, guard_scale;
    float excess_j, allowance_j;
    bool startup_active, startup_triggered, guard_infeasible, growth_fallback;
    bool guard_search_exhausted;
    float dt_s;
} ChassisPowerTransientOutput_s;

ChassisPowerTransientConfig_s ChassisPowerTransientDefaultConfig(void);
void ChassisPowerTransientReset(ChassisPowerTransientState_s *state);
bool ChassisPowerTransientUpdate(const ChassisPowerTransientConfig_s *config,
    const ChassisPowerTransientInput_s *input, float dt_s,
    ChassisPowerTransientState_s *state, ChassisPowerTransientOutput_s *output);
void ChassisPowerTransientRecordSent(ChassisPowerTransientState_s *state, const float current[4]);
bool ChassisPowerTransientGuard(const ChassisPowerTransientConfig_s *config,
    const MotorPowerModelConfig_s models[4], const ChassisPowerTransientInput_s *input,
    const float candidate[4], const ChassisPowerTransientState_s *state,
    ChassisPowerTransientOutput_s *output);
#endif
