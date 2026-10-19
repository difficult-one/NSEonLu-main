#include "chassis_power_capture.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static ChassisPowerCapture_s capture;
static ChassisPowerCaptureSample_s sample;
static void Tick(uint32_t tick)
{
    sample.tick_ms = tick;
    sample.meter_tick_ms = tick;
    ChassisPowerCaptureUpdate(&capture, &sample);
}
static void Prepare(uint32_t origin)
{
    ChassisPowerCaptureReset(&capture);
    memset(&sample, 0, sizeof(sample));
    sample.enabled_mask = 15;
    sample.budget_w = 20;
    for (uint32_t t = 0; t < 120; ++t) Tick(origin + t);
    for (unsigned i = 0; i < 4; ++i) sample.target_rpm[i] = 1000;
    Tick(origin + 120);
}
static void TestCaptureWindowFreezeAndDrain(void)
{
    Prepare(0);
    assert(capture.trigger_tick_ms == 120);
    for (uint32_t t = 121; t <= 380; ++t) Tick(t);
    assert(capture.frozen && capture.count == 150);
    uint8_t frame[CHASSIS_POWER_CAPTURE_FRAME_BYTES];
    assert(!ChassisPowerCaptureNextFrame(&capture, 400, false, frame));
    assert(ChassisPowerCaptureNextFrame(&capture, 400, true, frame));
    assert(memcmp(frame, "CPC1", 4) == 0);
    assert(ChassisPowerCaptureFrameIsValid(frame));
    /* Oldest pretrigger snapshot is 70 ms. */
    assert(frame[12] == 70 && frame[13] == 0);
    frame[50] ^= 1;
    assert(!ChassisPowerCaptureFrameIsValid(frame));
    ChassisPowerCaptureFrameAccepted(&capture, 400);
    assert(!ChassisPowerCaptureNextFrame(&capture, 420, true, frame));
    assert(ChassisPowerCaptureNextFrame(&capture, 421, true, frame));
    for (unsigned i = 1; i <= 150; ++i)
    {
        uint32_t now = 400 + i * 21;
        assert(ChassisPowerCaptureNextFrame(&capture, now, true, frame));
        ChassisPowerCaptureFrameAccepted(&capture, now);
    }
    assert(capture.frozen); /* still frozen until UART reports empty */
    ChassisPowerCaptureDrainComplete(&capture);
    assert(!capture.frozen);
}
static void TestT90HoldAndTargetChange(void)
{
    Prepare(0);
    for (unsigned i = 0; i < 4; ++i) sample.actual_rpm[i] = 900;
    for (uint32_t t = 121; t <= 141; ++t) Tick(t);
    assert(capture.t90_status == CHASSIS_POWER_T90_REACHED);
    assert(capture.t90_ms == 1); /* first reached time, after confirming 20 ms */
    Prepare(0);
    sample.target_rpm[0] = 900;
    Tick(121);
    assert(capture.t90_status == CHASSIS_POWER_T90_INVALID);
    Prepare(0);
    for (uint32_t t = 121; t <= 3120; ++t) Tick(t);
    assert(capture.t90_status == CHASSIS_POWER_T90_TIMEOUT);
}
static void TestWrapAndQuantization(void)
{
    Prepare(UINT32_MAX - 200);
    sample.raw_current[0] = 40000;
    sample.raw_current[1] = NAN;
    for (uint32_t t = 121; t <= 380; ++t) Tick(UINT32_MAX - 200 + t);
    assert(capture.frozen);
    uint8_t frame[CHASSIS_POWER_CAPTURE_FRAME_BYTES];
    for (unsigned i = 0; i <= 26; ++i)
    {
        assert(ChassisPowerCaptureNextFrame(&capture, 400 + 21 * i, true, frame));
        if (i == 26) assert((frame[11] & CHASSIS_POWER_CAPTURE_QUANTIZED) != 0);
        ChassisPowerCaptureFrameAccepted(&capture, 400 + 21 * i);
    }
}
int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "--dump") == 0) {
        Prepare(0);
        for (uint32_t t = 121; t <= 380; ++t) Tick(t);
        FILE *file = fopen(argv[2], "wb");
        assert(file != NULL);
        uint8_t frame[102];
        for (unsigned i = 0; i <= 150; ++i) {
            assert(ChassisPowerCaptureNextFrame(&capture, 400 + 21 * i, true, frame));
            assert(fwrite(frame, 1, sizeof(frame), file) == sizeof(frame));
            ChassisPowerCaptureFrameAccepted(&capture, 400 + 21 * i);
        }
        fclose(file);
        return 0;
    }
    TestCaptureWindowFreezeAndDrain();
    TestT90HoldAndTargetChange();
    TestWrapAndQuantization();
    puts("PASS: chassis power capture tests");
    return 0;
}
