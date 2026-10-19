#ifndef CHASSIS_POWER_CAPTURE_H
#define CHASSIS_POWER_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

#define CHASSIS_POWER_CAPTURE_RECORDS 150U
#define CHASSIS_POWER_CAPTURE_RECORD_BYTES 96U
#define CHASSIS_POWER_CAPTURE_FRAME_BYTES 102U
#define CHASSIS_POWER_CAPTURE_QUANTIZED 0x80U

typedef enum {
    CHASSIS_POWER_T90_NONE = 0,
    CHASSIS_POWER_T90_PENDING,
    CHASSIS_POWER_T90_REACHED,
    CHASSIS_POWER_T90_INVALID,
    CHASSIS_POWER_T90_TIMEOUT,
} ChassisPowerT90Status_e;

typedef struct {
    uint32_t tick_ms, meter_tick_ms;
    float raw_current[4], final_current[4], feedback_current[4];
    float actual_rpm[4], target_rpm[4];
    float budget_w, measured_w, actual_model_w, guard_model_w;
    float fast_w, slow_w, excess_j, allowance_j;
    uint8_t motor_age_ms[4], meter_age_ms;
    uint8_t enabled_mask, drive_mask, brake_mask, state, flags;
} ChassisPowerCaptureSample_s;

typedef struct {
    uint8_t records[CHASSIS_POWER_CAPTURE_RECORDS][CHASSIS_POWER_CAPTURE_RECORD_BYTES];
    float previous_target[4], test_target[4];
    uint32_t trigger_tick_ms, low_tick_ms, last_sample_ms, last_trigger_ms;
    uint32_t reached_tick_ms, t90_ms, dropped_triggers, last_drain_ms;
    uint16_t head, start, count, post_count, drain_index, capture_id;
    ChassisPowerT90Status_e t90_status;
    bool initialized, low_valid, triggered, frozen, hold_valid, drained_once;
} ChassisPowerCapture_s;

void ChassisPowerCaptureReset(ChassisPowerCapture_s *capture);
void ChassisPowerCaptureUpdate(ChassisPowerCapture_s *capture,
                              const ChassisPowerCaptureSample_s *sample);
/* Peek is retryable; acknowledge only after the complete frame is queued. */
bool ChassisPowerCaptureNextFrame(ChassisPowerCapture_s *capture, uint32_t now_ms,
                                 bool control_stopped, uint8_t frame[102]);
void ChassisPowerCaptureFrameAccepted(ChassisPowerCapture_s *capture, uint32_t now_ms);
/* Release only after every frame has been queued AND the transport is empty. */
void ChassisPowerCaptureDrainComplete(ChassisPowerCapture_s *capture);
bool ChassisPowerCaptureFrameIsValid(const uint8_t frame[102]);

#endif
