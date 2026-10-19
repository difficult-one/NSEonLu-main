#include "chassis_power_capture.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(float) == 4, "capture requires 32-bit float");
_Static_assert(sizeof(ChassisPowerCapture_s) < 15000, "capture RAM budget");

static void Put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}
static void Put32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i));
}
static void PutFloat(uint8_t *p, float v)
{
    uint32_t bits;
    memcpy(&bits, &v, 4);
    Put32(p, bits);
}
static uint16_t Crc16(const uint8_t *data, unsigned size)
{
    uint16_t crc = 0xffff;
    for (unsigned i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned b = 0; b < 8; ++b)
            crc = (uint16_t)((crc >> 1) ^ ((crc & 1) ? 0xa001 : 0));
    }
    return crc;
}
static void Encode(uint8_t *record, const ChassisPowerCaptureSample_s *s)
{
    memset(record, 0, 96);
    record[0] = 1; record[1] = 1; record[6] = s->state; record[7] = s->flags;
    Put32(record + 8, s->tick_ms); Put32(record + 12, s->meter_tick_ms);
    const float *arrays[5] = {s->raw_current, s->final_current, s->feedback_current,
                              s->actual_rpm, s->target_rpm};
    for (unsigned a = 0; a < 5; ++a) {
        for (unsigned i = 0; i < 4; ++i) {
            float value = arrays[a][i];
            if (!isfinite(value) || value < -32768 || value > 32767) {
                record[7] |= CHASSIS_POWER_CAPTURE_QUANTIZED;
                value = isfinite(value) ? fminf(32767, fmaxf(-32768, value)) : 0;
            }
            Put16(record + 16 + a * 8 + i * 2, (uint16_t)(int16_t)lroundf(value));
        }
    }
    const float scalar[8] = {s->budget_w, s->measured_w, s->actual_model_w,
        s->guard_model_w, s->fast_w, s->slow_w, s->excess_j, s->allowance_j};
    for (unsigned i = 0; i < 8; ++i) PutFloat(record + 56 + i * 4, scalar[i]);
    memcpy(record + 88, s->motor_age_ms, 4);
    record[92] = s->meter_age_ms; record[93] = s->enabled_mask;
    record[94] = s->drive_mask; record[95] = s->brake_mask;
}
void ChassisPowerCaptureReset(ChassisPowerCapture_s *c)
{
    if (c != NULL) memset(c, 0, sizeof(*c));
}
static void UpdateT90(ChassisPowerCapture_s *c, const ChassisPowerCaptureSample_s *s)
{
    if (c->t90_status != CHASSIS_POWER_T90_PENDING) return;
    bool reached = s->enabled_mask == 15;
    for (unsigned i = 0; i < 4; ++i) {
        if (!isfinite(s->actual_rpm[i]) || !isfinite(s->target_rpm[i]) ||
            s->target_rpm[i] != c->test_target[i] || s->enabled_mask != 15) {
            c->t90_status = CHASSIS_POWER_T90_INVALID;
            return;
        }
        if (c->test_target[i] != 0 &&
            s->actual_rpm[i] * copysignf(1, c->test_target[i]) <
                0.9f * fabsf(c->test_target[i])) reached = false;
        if (s->motor_age_ms[i] > 5) reached = false;
    }
    if (reached) {
        if (!c->hold_valid) { c->reached_tick_ms = s->tick_ms; c->hold_valid = true; }
        if ((uint32_t)(s->tick_ms - c->reached_tick_ms) >= 20) {
            c->t90_status = CHASSIS_POWER_T90_REACHED;
            c->t90_ms = c->reached_tick_ms - c->trigger_tick_ms;
        }
    } else c->hold_valid = false;
    if (c->t90_status == CHASSIS_POWER_T90_PENDING &&
        (uint32_t)(s->tick_ms - c->trigger_tick_ms) >= 3000)
        c->t90_status = CHASSIS_POWER_T90_TIMEOUT;
}
void ChassisPowerCaptureUpdate(ChassisPowerCapture_s *c, const ChassisPowerCaptureSample_s *s)
{
    if (c == NULL || s == NULL) return;
    float average = 0, max_current = 0;
    bool step = false, driving = false, finite = true;
    for (unsigned i = 0; i < 4; ++i) {
        finite &= isfinite(s->actual_rpm[i]) && isfinite(s->final_current[i]) &&
                  isfinite(s->target_rpm[i]);
        average += fabsf(s->actual_rpm[i]) * 0.25f;
        max_current = fmaxf(max_current, fabsf(s->final_current[i]));
        step |= fabsf(s->target_rpm[i] - c->previous_target[i]) >= 300;
        driving |= s->target_rpm[i] != 0;
    }
    const bool armed = c->low_valid && (uint32_t)(s->tick_ms - c->low_tick_ms) >= 100;
    const bool cooldown = !c->triggered || (uint32_t)(s->tick_ms - c->last_trigger_ms) >= 500;
    const bool trigger = c->initialized && armed && cooldown && step && driving &&
                         finite && s->enabled_mask == 15;
    if (trigger) {
        c->low_valid = false;
        c->last_trigger_ms = s->tick_ms;
        c->triggered = true;
        if (c->frozen || c->post_count != 0) ++c->dropped_triggers;
        else {
            c->trigger_tick_ms = s->tick_ms;
            c->start = (uint16_t)((c->head + 150 - c->count) % 150);
            c->post_count = 1; /* marker: the trigger sample is not written yet */
            ++c->capture_id;
            c->t90_status = CHASSIS_POWER_T90_PENDING;
            c->hold_valid = false; c->t90_ms = 0;
            memcpy(c->test_target, s->target_rpm, sizeof(c->test_target));
        }
    }
    if (!trigger && finite && average < 100 && max_current < 500 && s->enabled_mask == 15) {
        if (!c->low_valid) { c->low_valid = true; c->low_tick_ms = s->tick_ms; }
    } else c->low_valid = false;
    memcpy(c->previous_target, s->target_rpm, sizeof(c->previous_target));
    UpdateT90(c, s);
    if (!c->frozen && (!c->initialized || trigger ||
        (uint32_t)(s->tick_ms - c->last_sample_ms) >= 2)) {
        Encode(c->records[c->head], s);
        c->head = (uint16_t)((c->head + 1) % 150);
        c->last_sample_ms = s->tick_ms;
        if (c->post_count != 0) {
            ++c->count;
            if (++c->post_count == 126) {
                c->frozen = true; c->drain_index = 0; c->drained_once = false;
            }
        } else if (c->count < 25) ++c->count;
    }
    c->initialized = true;
}
bool ChassisPowerCaptureNextFrame(ChassisPowerCapture_s *c, uint32_t now,
                                 bool stopped, uint8_t frame[102])
{
    if (c == NULL || frame == NULL || !c->frozen || !stopped || c->drain_index > c->count ||
        (c->drained_once && (uint32_t)(now - c->last_drain_ms) < 21)) return false;
    memcpy(frame, "CPC1", 4);
    uint8_t *r = frame + 4;
    if (c->drain_index < c->count) {
        memcpy(r, c->records[(c->start + c->drain_index) % 150], 96);
    } else {
        memset(r, 0, 96); r[0] = 1; r[1] = 2;
        Put32(r + 8, c->trigger_tick_ms);
        Put32(r + 16, c->t90_status == CHASSIS_POWER_T90_PENDING ?
              CHASSIS_POWER_T90_INVALID : (uint32_t)c->t90_status);
        Put32(r + 20, c->t90_ms); Put32(r + 24, c->dropped_triggers);
        Put32(r + 28, c->count);
    }
    Put16(r + 2, c->capture_id); Put16(r + 4, c->drain_index);
    Put16(frame + 100, Crc16(frame, 100));
    return true;
}
void ChassisPowerCaptureFrameAccepted(ChassisPowerCapture_s *c, uint32_t now)
{
    if (c == NULL || !c->frozen) return;
    c->drained_once = true; c->last_drain_ms = now;
    ++c->drain_index;
}
void ChassisPowerCaptureDrainComplete(ChassisPowerCapture_s *c)
{
    if (c != NULL && c->frozen && c->drain_index > c->count) {
        c->frozen = false; c->post_count = c->count = c->head = c->start = 0;
        c->low_valid = false;
    }
}
bool ChassisPowerCaptureFrameIsValid(const uint8_t frame[102])
{
    return frame != NULL && memcmp(frame, "CPC1", 4) == 0 && frame[4] == 1 &&
           Crc16(frame, 100) == (uint16_t)(frame[100] | ((uint16_t)frame[101] << 8));
}
