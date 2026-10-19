#include "main.h"
#include "chassis.h"
#include "runtime_tuning.h"
#include "tuning_flash.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
uint32_t __tuning_bank0_start__[32768],__tuning_bank1_start__[32768];
FlashRegisters test_flash_registers={.ACR=1};
static unsigned suspended,writes;
static bool stopped,unlock_failed;
static uint32_t tick=1000;
static ChassisTelemetrySnapshot_s telemetry={.valid=true,.tick_ms=1000,.mode=1};
void vTaskSuspendAll(void) {assert(suspended==0); ++suspended;}
int xTaskResumeAll(void) {assert(suspended==1); --suspended; return 1;}
uint32_t HAL_GetTick(void) {return tick;}
bool ChassisGetTelemetrySnapshot(ChassisTelemetrySnapshot_s *s) {*s=telemetry; return s->valid;}
bool DJIMotorCanSaveParameters(uint32_t now) {assert(now==tick); return stopped;}
HAL_StatusTypeDef HAL_FLASH_Unlock(void) {assert(suspended);return unlock_failed ? HAL_ERROR : HAL_OK;}
HAL_StatusTypeDef HAL_FLASH_Lock(void) {return HAL_OK;}
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type,uint32_t address,uint64_t value)
{
    (void)type; assert(suspended);
    uint32_t *banks[]={__tuning_bank0_start__,__tuning_bank1_start__};
    for (unsigned i=0;i<2;++i) {
        uint32_t offset=address-(uint32_t)(uintptr_t)banks[i];
        if (offset<sizeof __tuning_bank0_start__) {
            assert((banks[i][offset/4]&(uint32_t)value)==(uint32_t)value);
            banks[i][offset/4]&=(uint32_t)value; ++writes; return HAL_OK;
        }
    }
    assert(0);return HAL_ERROR;
}
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *e,uint32_t *error)
{
    assert(suspended && e->NbSectors==1 && (e->Sector==10 || e->Sector==11));
    memset(e->Sector==10 ? __tuning_bank0_start__ : __tuning_bank1_start__,0xff,sizeof __tuning_bank0_start__);
    *error=UINT32_MAX; return HAL_OK;
}
static float restored;
static bool apply(const float *v) {restored=v[0];return true;}
static void set(float value,unsigned revision)
{
    char command[512],cell[16],reply[512];
    snprintf(command,sizeof command,"@T1,1,SET,pid,%u",revision);
    for (unsigned i=0;i<24;++i) {
        float v=i==0 ? value : 0; uint32_t bits; memcpy(&bits,&v,4);
        snprintf(cell,sizeof cell,",%08lx",(unsigned long)bits);strcat(command,cell);
    }
    RuntimeTuningSubmit(command);RuntimeTuningProcess(TUNING_PID,apply);
    assert(RuntimeTuningReply(reply,sizeof reply)>0);RuntimeTuningReplySent();
}
int main(void)
{
    memset(__tuning_bank0_start__,0xff,sizeof __tuning_bank0_start__);
    memset(__tuning_bank1_start__,0xff,sizeof __tuning_bank1_start__);
    TuningFlashInit(); float values[24]={0};
    RuntimeTuningRegister(TUNING_PID,values,0);set(.75f,1);
    TuningFlashPoll(); assert(!writes && !suspended);
    stopped=true;TuningFlashPoll();assert(!writes); /* Active mode. */
    telemetry.mode=0;telemetry.tick_ms=994;TuningFlashPoll();assert(!writes);
    telemetry.tick_ms=1000;TuningFlashPoll();assert(writes && !suspended);
    TuningStoredImage pending;
    assert(!RuntimeTuningPendingImage(&pending));
    TuningFlashInit();RuntimeTuningRegister(TUNING_PID,values,0);
    RuntimeTuningRestore(TUNING_PID,apply);assert(restored==.75f);
    set(1.5f,1);unlock_failed=true;TuningFlashPoll();
    assert(!RuntimeTuningPendingImage(&pending) && !suspended);
    unlock_failed=false;
    RuntimeTuningSubmit("@T1,2,SAVE,pid,2");
    char reply[512];RuntimeTuningReply(reply,sizeof reply);RuntimeTuningReplySent();
    TuningFlashPoll();assert(!RuntimeTuningPendingImage(&pending));
    TuningFlashInit();RuntimeTuningRegister(TUNING_PID,values,0);
    RuntimeTuningRestore(TUNING_PID,apply);assert(restored==1.5f);
    puts("Flash adapter gate/restore/retry tests passed");
}
