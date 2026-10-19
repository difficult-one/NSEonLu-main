#ifndef TEST_BSP_CAN_H
#define TEST_BSP_CAN_H

#include <stdint.h>

typedef struct
{
    uint32_t marker;
} CAN_HandleTypeDef;

typedef struct CANInstance CANInstance;
struct CANInstance
{
    CAN_HandleTypeDef *can_handle;
    uint32_t tx_id;
    uint32_t rx_id;
    uint8_t tx_buff[8];
    uint8_t rx_buff[8];
    uint8_t rx_len;
    void (*can_module_callback)(CANInstance *);
    void *id;
};

typedef struct
{
    CAN_HandleTypeDef *can_handle;
    uint32_t tx_id;
    uint32_t rx_id;
    void (*can_module_callback)(CANInstance *);
    void *id;
} CAN_Init_Config_s;

CANInstance *CANRegister(CAN_Init_Config_s *config);
void CANSetDLC(CANInstance *instance, uint8_t length);
uint32_t HAL_GetTick(void);

#endif
