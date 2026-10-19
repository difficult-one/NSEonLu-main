#include "pid_autotune.h"
#include <math.h>

static bool Stationary(const PIDAutotuneFeedback *f)
{
    for (unsigned i=0;i<4;++i) if (!isfinite(f->rpm[i]) || fabsf(f->rpm[i])>50) return false;
    return true;
}
bool PIDAutotuneBegin(PIDAutotune *a,uint32_t session,const PIDAutotuneFeedback *f)
{
    if (a->active || !session || !f->permitted || !f->feedback_ok || !f->can_ok || !Stationary(f)) return false;
    *a=(PIDAutotune){.active=true,.session=session,.state=PID_AUTO_READY,
        .lease_ms=f->now_ms,.begin_ms=f->now_ms};
    return true;
}
bool PIDAutotuneStart(PIDAutotune *a,const PIDAutotuneTrial *t,const PIDAutotuneFeedback *f)
{
    if (!a->active || a->state==PID_AUTO_RUNNING || !Stationary(f) ||
        t->wheel>=4 || !isfinite(t->rpm) || t->rpm<100 || t->rpm>1500 ||
        t->hold_ms<500 || t->hold_ms>2000 || !isfinite(t->current_limit) ||
        t->current_limit<500 || t->current_limit>5000 ||
        !isfinite(t->kp) || t->kp<0 || t->kp>1000 ||
        !isfinite(t->ki) || t->ki<0 || t->ki>1000 ||
        !isfinite(t->kd) || t->kd<0 || t->kd>1000) return false;
    uint32_t session=a->session,trial=a->trial+1,begin=a->begin_ms;
    *a=(PIDAutotune){.active=true,.session=session,.trial=trial,.begin_ms=begin,
        .lease_ms=f->now_ms,.start_ms=f->now_ms,.test=*t,.state=PID_AUTO_RUNNING,
        .settle_ms=UINT32_MAX};
    return true;
}
void PIDAutotuneKeepAlive(PIDAutotune *a,uint32_t now) {a->lease_ms=now;}
void PIDAutotuneStop(PIDAutotune *a,PIDAutotuneReason reason)
{
    a->active=false; a->state=PID_AUTO_ABORTED;a->reason=reason;
}
float PIDAutotuneTarget(const PIDAutotune *a,uint32_t now)
{
    uint32_t elapsed=now-a->start_ms;
    return a->active && a->state==PID_AUTO_RUNNING && elapsed>=300 &&
        elapsed<300+a->test.hold_ms ? a->test.rpm : 0;
}
void PIDAutotuneTick(PIDAutotune *a,const PIDAutotuneFeedback *f)
{
    if (!a->active) return;
    if ((uint32_t)(f->now_ms-a->lease_ms)>1000 || (uint32_t)(f->now_ms-a->begin_ms)>180000)
        {PIDAutotuneStop(a,PID_AUTO_TIMEOUT);return;}
    if (!f->permitted) {PIDAutotuneStop(a,PID_AUTO_GUARD);return;}
    if (!f->feedback_ok) {PIDAutotuneStop(a,PID_AUTO_FEEDBACK);return;}
    if (!f->can_ok) {PIDAutotuneStop(a,PID_AUTO_CAN);return;}
    for (unsigned i=0;i<4;++i) {
        float maximum=a->state==PID_AUTO_RUNNING && i==a->test.wheel ? fmaxf(a->test.rpm*1.75f,a->test.rpm+300) : 100;
        if (!isfinite(f->rpm[i]) || fabsf(f->rpm[i])>maximum)
            {PIDAutotuneStop(a,PID_AUTO_OVERSPEED);return;}
    }
    if (a->state!=PID_AUTO_RUNNING || (uint32_t)(f->now_ms-a->start_ms)<a->test.hold_ms+600) return;
    a->state=PID_AUTO_DONE;
    if (a->samples<20 || !a->steady_samples) {a->reason=PID_AUTO_NO_RESPONSE;return;}
    a->rmse=(float)sqrt(a->error2/a->samples);
    a->overshoot=fmaxf(0,a->peak-a->test.rpm);
    a->steady=(float)(a->steady_sum/a->steady_samples);
    a->jitter=(float)(a->current_variation/a->samples);
    a->limited_pm=(uint32_t)(1000ULL*a->limited_samples/a->samples);
    a->settle_ms=a->last_outside+100<a->test.hold_ms ? a->last_outside : UINT32_MAX;
    if (a->peak<a->test.rpm*.1f) a->reason=PID_AUTO_NO_RESPONSE;
    else if (a->limited_pm>50) a->reason=PID_AUTO_LIMITED;
}
void PIDAutotuneObserve(PIDAutotune *a,uint32_t now,float rpm,float raw,float final,bool limited)
{
    uint32_t elapsed=now-a->start_ms;
    if (!a->active || a->state!=PID_AUTO_RUNNING || elapsed<300 || elapsed>=300+a->test.hold_ms) return;
    if (!isfinite(rpm) || !isfinite(raw) || !isfinite(final)) {PIDAutotuneStop(a,PID_AUTO_FEEDBACK);return;}
    float error=fabsf(a->test.rpm-rpm);
    a->error2+=(double)error*error;
    if (a->samples) a->current_variation+=fabsf(final-a->previous_current);
    a->previous_current=final;++a->samples;
    a->limited_samples+=limited || fabsf(raw-final)>fmaxf(5,fabsf(raw)*.05f);
    a->peak=fmaxf(a->peak,rpm);
    if (error>a->test.rpm*.05f) a->last_outside=elapsed-300+1;
    if (elapsed>=300+a->test.hold_ms*4/5) {a->steady_sum+=error;++a->steady_samples;}
}
