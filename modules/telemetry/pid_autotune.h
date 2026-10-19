#ifndef PID_AUTOTUNE_H
#define PID_AUTOTUNE_H
#include <stdbool.h>
#include <stdint.h>

typedef enum {PID_AUTO_IDLE,PID_AUTO_READY,PID_AUTO_RUNNING,PID_AUTO_DONE,PID_AUTO_ABORTED} PIDAutotuneState;
typedef enum {PID_AUTO_NONE,PID_AUTO_STOPPED,PID_AUTO_TIMEOUT,PID_AUTO_GUARD,
    PID_AUTO_FEEDBACK,PID_AUTO_OVERSPEED,PID_AUTO_LIMITED,PID_AUTO_NO_RESPONSE,
    PID_AUTO_CONFIG,PID_AUTO_CAN} PIDAutotuneReason;
typedef struct {unsigned wheel; float rpm; uint32_t hold_ms; float current_limit,kp,ki,kd;} PIDAutotuneTrial;
typedef struct {uint32_t now_ms; bool permitted,feedback_ok,can_ok; float rpm[4];} PIDAutotuneFeedback;
typedef struct {
    bool active;
    uint32_t session,trial,lease_ms,begin_ms,start_ms,samples,limited_samples,steady_samples,last_outside;
    PIDAutotuneState state;
    PIDAutotuneReason reason;
    PIDAutotuneTrial test;
    double error2,steady_sum,current_variation;
    float peak,previous_current,rmse,overshoot,steady,jitter;
    uint32_t settle_ms,limited_pm;
} PIDAutotune;
bool PIDAutotuneBegin(PIDAutotune *a,uint32_t session,const PIDAutotuneFeedback *f);
bool PIDAutotuneStart(PIDAutotune *a,const PIDAutotuneTrial *test,const PIDAutotuneFeedback *f);
void PIDAutotuneKeepAlive(PIDAutotune *a,uint32_t now);
void PIDAutotuneStop(PIDAutotune *a,PIDAutotuneReason reason);
void PIDAutotuneTick(PIDAutotune *a,const PIDAutotuneFeedback *f);
float PIDAutotuneTarget(const PIDAutotune *a,uint32_t now);
void PIDAutotuneObserve(PIDAutotune *a,uint32_t now,float rpm,float raw_current,float final_current,bool limited);
#endif
