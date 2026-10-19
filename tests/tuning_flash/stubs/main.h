#ifndef TEST_FLASH_MAIN_H
#define TEST_FLASH_MAIN_H
#include <stdint.h>
typedef enum {HAL_OK,HAL_ERROR} HAL_StatusTypeDef;
typedef struct {uint32_t TypeErase,Sector,NbSectors,VoltageRange;} FLASH_EraseInitTypeDef;
typedef struct {uint32_t ACR;} FlashRegisters;
extern FlashRegisters test_flash_registers;
#define FLASH (&test_flash_registers)
#define FLASH_ACR_DCEN 1U
#define FLASH_TYPEPROGRAM_WORD 0U
#define FLASH_TYPEERASE_SECTORS 0U
#define FLASH_VOLTAGE_RANGE_3 3U
#define FLASH_SECTOR_10 10U
#define FLASH_SECTOR_11 11U
#define FLASH_FLAG_EOP 1U
#define FLASH_FLAG_OPERR 2U
#define FLASH_FLAG_WRPERR 4U
#define FLASH_FLAG_PGAERR 8U
#define FLASH_FLAG_PGPERR 16U
#define FLASH_FLAG_PGSERR 32U
#define __HAL_FLASH_CLEAR_FLAG(flags) ((void)(flags))
#define __HAL_FLASH_DATA_CACHE_DISABLE() ((void)0)
#define __HAL_FLASH_DATA_CACHE_RESET() ((void)0)
#define __HAL_FLASH_DATA_CACHE_ENABLE() ((void)0)
#define __DSB() ((void)0)
#define __ISB() ((void)0)
uint32_t HAL_GetTick(void);
HAL_StatusTypeDef HAL_FLASH_Unlock(void);
HAL_StatusTypeDef HAL_FLASH_Lock(void);
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type,uint32_t address,uint64_t value);
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *erase,uint32_t *error);
#endif
