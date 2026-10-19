#include "pid_autotune.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static PIDAutotuneFeedback feedback(uint32_t now)
{
    PIDAutotuneFeedback f={.now_ms=now,.permitted=true,.feedback_ok=true,.can_ok=true};
    return f;
}
static PIDAutotuneTrial trial={.wheel=0,.rpm=500,.hold_ms=500,.current_limit=3000,.kp=.75f};
int main(void)
{
    PIDAutotune a={0}; PIDAutotuneFeedback f=feedback(0);
    assert(PIDAutotuneBegin(&a,1,&f));
    assert(PIDAutotuneStart(&a,&trial,&f));
    assert(PIDAutotuneTarget(&a,299)==0);
    assert(PIDAutotuneTarget(&a,300)==500);
    for (unsigned t=0;t<=1100;++t) {
        f=feedback(t); f.rpm[0]=t>=300 && t<800 ? 400 : 0;
        if (t%200==0) PIDAutotuneKeepAlive(&a,t);
        PIDAutotuneTick(&a,&f);
        PIDAutotuneObserve(&a,t,f.rpm[0],1000,1000,false);
    }
    assert(a.state==PID_AUTO_DONE && a.reason==PID_AUTO_NONE);
    assert(fabsf(a.rmse-100)<.01f && fabsf(a.steady-100)<.01f);
    assert(a.samples==500 && a.settle_ms==UINT32_MAX);
    assert(PIDAutotuneStart(&a,&trial,&f));
    f.now_ms+=1001; PIDAutotuneTick(&a,&f);
    assert(!a.active && a.reason==PID_AUTO_TIMEOUT);
    a=(PIDAutotune){0};f=feedback(UINT32_MAX-100);
    assert(PIDAutotuneBegin(&a,2,&f));
    assert(PIDAutotuneStart(&a,&trial,&f));
    f.now_ms=200;PIDAutotuneTick(&a,&f);
    assert(a.active && PIDAutotuneTarget(&a,200)==500);
    f.feedback_ok=false;PIDAutotuneTick(&a,&f);
    assert(!a.active && a.reason==PID_AUTO_FEEDBACK);
    a=(PIDAutotune){0};f=feedback(0);f.permitted=false;
    assert(!PIDAutotuneBegin(&a,3,&f));
    f.permitted=true;assert(PIDAutotuneBegin(&a,3,&f));
    PIDAutotuneTrial bad=trial;bad.rpm=NAN;
    assert(!PIDAutotuneStart(&a,&bad,&f));
    assert(PIDAutotuneStart(&a,&trial,&f));
    for(unsigned t=0;t<=1100;++t) {
        f=feedback(t); f.rpm[0]=t>=300 && t<800 ? 500 : 0;
        PIDAutotuneKeepAlive(&a,t);PIDAutotuneTick(&a,&f);
        PIDAutotuneObserve(&a,t,f.rpm[0],1000,500,true);
    }
    assert(a.reason==PID_AUTO_LIMITED && a.limited_pm==1000);
    a=(PIDAutotune){0};f=feedback(0);assert(PIDAutotuneBegin(&a,4,&f));
    assert(PIDAutotuneStart(&a,&trial,&f));
    f=feedback(301);f.rpm[0]=1000;PIDAutotuneTick(&a,&f);
    assert(!a.active && a.reason==PID_AUTO_OVERSPEED);
    puts("PID autotune timing/metrics/limits/watchdog tests passed");
}
