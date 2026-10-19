#include "autotune_protocol.h"
#include "runtime_tuning.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static PIDAutotuneFeedback f={.now_ms=100,.permitted=true,.feedback_ok=true,.can_ok=true};
static AutotuneDrive drive;
static char reply[512];
static void command(const char *line,const char *expected)
{
    AutotuneSubmit(line);AutotuneMotorStep(&f,&drive);
    assert(AutotuneReply(reply,sizeof reply)>0);
    if(!strstr(reply,expected)) {printf("unexpected: %s expected: %s\n",reply,expected);fflush(stdout);}
    assert(strstr(reply,expected));AutotuneReplySent();
}
int main(void)
{
    float values[24]={0};RuntimeTuningRegister(TUNING_PID,values,0);
    command("@A1,1,BEGIN,2","ERR,conflict");
    command("@A1,2,BEGIN,1","OK,1,ready");assert(drive.owned && RuntimeTuningTrialActive());
    command("@A1,3,TRIAL,9,0,500,500,3000,3f400000,00000000,00000000","ERR,session");
    command("@A1,4,TRIAL,1,0,500,500,3000,3f400000,00000000,00000000","OK,1,running");
    assert(drive.running && drive.target_rpm==0);
    f.now_ms=400;command("@A1,5,STATUS,1","OK,1,running");assert(drive.target_rpm==500);
    f.feedback_ok=false;AutotuneMotorStep(&f,&drive);assert(!drive.owned && !RuntimeTuningTrialActive());
    command("@A1,6,STATUS,1","OK,1,aborted,0,1,feedback");
    command("@A1,7,TRIAL,1,0,500,500,3000,3f400000,00000000,00000000","ERR,expired");
    command("@A1,8,END,1","OK,1,aborted");
    f.feedback_ok=true;command("@A1,9,BEGIN,1","OK,2,ready");
    AutotuneCancel();AutotuneMotorStep(&f,&drive);assert(!drive.owned && !RuntimeTuningTrialActive());
    command("@A1,10,STATUS,2","OK,2,aborted,0,0,stopped");
    command("@A1,11,TRIAL,2,0,500,500,3000,7fc00000,00000000,00000000","ERR,format");
    puts("Autotune protocol/session/lock/cancellation tests passed");
}
