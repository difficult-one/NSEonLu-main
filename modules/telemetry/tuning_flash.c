#include "tuning_flash.h"
#include "tuning_storage.h"
#include "runtime_tuning.h"
#include "chassis.h"
#include "dji_motor.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stddef.h>

#define BANK_WORDS (128U * 1024U / sizeof(uint32_t))
/* Provided by the linker: these sectors cannot overlap the application image. */
extern uint32_t __tuning_bank0_start__, __tuning_bank1_start__;
static TuningStorage storage;
static TuningStoredImage pending;
static void FlushDataCache(void)
{
    if (FLASH->ACR & FLASH_ACR_DCEN) {
        __HAL_FLASH_DATA_CACHE_DISABLE();
        __HAL_FLASH_DATA_CACHE_RESET();
        __HAL_FLASH_DATA_CACHE_ENABLE();
    }
    __DSB(); __ISB();
}
static bool Program(void *context,unsigned bank,unsigned word,uint32_t value)
{
    (void)context;
    if (bank>1 || word>=BANK_WORDS) return false;
    uint32_t address=(uint32_t)(uintptr_t)storage.io.banks[bank]+word*sizeof(uint32_t);
    HAL_StatusTypeDef result=HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,address,value);
    FlushDataCache();
    return result==HAL_OK;
}
static bool Erase(void *context,unsigned bank)
{
    (void)context;
    if (bank>1) return false;
    FLASH_EraseInitTypeDef erase={.TypeErase=FLASH_TYPEERASE_SECTORS,
        .Sector=bank ? FLASH_SECTOR_11 : FLASH_SECTOR_10,
        .NbSectors=1,.VoltageRange=FLASH_VOLTAGE_RANGE_3};
    uint32_t error=UINT32_MAX;
    HAL_StatusTypeDef result=HAL_FLASHEx_Erase(&erase,&error);
    FlushDataCache();
    return result==HAL_OK && error==UINT32_MAX;
}
void TuningFlashInit(void)
{
    const TuningStorageIO io={.banks={&__tuning_bank0_start__,&__tuning_bank1_start__},
        .bank_words=BANK_WORDS,.program=Program,.erase=Erase};
    TuningStorageInit(&storage,&io);
    RuntimeTuningPersistenceInit(storage.valid ? &storage.image : NULL);
}
void TuningFlashPoll(void)
{
    if (!RuntimeTuningPendingImage(&pending)) return;
    /* Keep the control tasks from re-enabling outputs between the gate and erase.
     * IRQs remain enabled; this MCU still stalls Flash instruction fetches while busy. */
    vTaskSuspendAll();
    ChassisTelemetrySnapshot_s chassis={0};
    uint32_t now=HAL_GetTick();
    bool safe=!RuntimeTuningTrialActive() && ChassisGetTelemetrySnapshot(&chassis) && chassis.mode==0 &&
        (uint32_t)(now-chassis.tick_ms)<=5 && DJIMotorCanSaveParameters(now);
    if (!safe) {(void)xTaskResumeAll(); return;}
    bool success=false;
    if (HAL_FLASH_Unlock()==HAL_OK) {
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP|FLASH_FLAG_OPERR|FLASH_FLAG_WRPERR|
            FLASH_FLAG_PGAERR|FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);
        success=TuningStorageSave(&storage,&pending);
        if (HAL_FLASH_Lock()!=HAL_OK) success=false;
    }
    (void)xTaskResumeAll();
    RuntimeTuningPersistenceComplete(&pending,success);
}
