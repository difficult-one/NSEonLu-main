#ifndef TEST_BSP_CAN_H
#define TEST_BSP_CAN_H

#include <stdint.h>

typedef struct
{
    uint32_t marker;
} CAN_HandleTypeDef;

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

typedef struct
{
    uint32_t StdId;
    uint32_t IDE;
    uint32_t RTR;
    uint32_t DLC;
} CAN_TxHeaderTypeDef;

typedef struct CANInstance CANInstance;
struct CANInstance
{
    CAN_HandleTypeDef *can_handle;
    uint32_t tx_id;
    uint32_t rx_id;
    CAN_TxHeaderTypeDef txconf;
    uint8_t tx_buff[8];
    uint8_t rx_buff[8];
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

#define CAN_ID_STD 0U
#define CAN_RTR_DATA 0U

CANInstance *CANRegister(CAN_Init_Config_s *config);
uint8_t CANTransmit(CANInstance *instance, float timeout);
uint32_t HAL_GetTick(void);

#endif
