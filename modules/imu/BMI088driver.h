#ifndef BMI088DRIVER_H
#define BMI088DRIVER_H

#include "stdint.h"
#include "main.h"

#define BMI088_TEMP_FACTOR 0.125f
#define BMI088_TEMP_OFFSET 23.0f

#define BMI088_WRITE_ACCEL_REG_NUM 6
#define BMI088_WRITE_GYRO_REG_NUM 6

#define BMI088_GYRO_DATA_READY_BIT 0
#define BMI088_ACCEL_DATA_READY_BIT 1
#define BMI088_ACCEL_TEMP_DATA_READY_BIT 2

#define BMI088_LONG_DELAY_TIME 80
#define BMI088_COM_WAIT_SENSOR_TIME 150

#define BMI088_ACCEL_IIC_ADDRESSE (0x18 << 1)
#define BMI088_GYRO_IIC_ADDRESSE (0x68 << 1)

#define BMI088_ACCEL_3G_SEN 0.0008974358974f
#define BMI088_ACCEL_6G_SEN 0.00179443359375f
#define BMI088_ACCEL_12G_SEN 0.0035888671875f
#define BMI088_ACCEL_24G_SEN 0.007177734375f

#define BMI088_GYRO_2000_SEN 0.00106526443603169529841533860381f
#define BMI088_GYRO_1000_SEN 0.00053263221801584764920766930190693f
#define BMI088_GYRO_500_SEN 0.00026631610900792382460383465095346f
#define BMI088_GYRO_250_SEN 0.00013315805450396191230191732547673f
#define BMI088_GYRO_125_SEN 0.000066579027251980956150958662738366f

// 閿熸枻鎷烽敓琛楄鎷烽敓鐫潻鎷�
#if INFANTRY_ID == 0
#define GxOFFSET 0.00247530174f
#define GyOFFSET 0.000393082853f
#define GzOFFSET 0.000393082853f
#define gNORM 9.69293118f
#elif INFANTRY_ID == 1
#define GxOFFSET 0.0007222f
#define GyOFFSET -0.001786f
#define GzOFFSET 0.0004346f
#define gNORM 9.876785f
#elif INFANTRY_ID == 2
#define GxOFFSET 0.0007222f
#define GyOFFSET -0.001786f
#define GzOFFSET 0.0004346f
#define gNORM 9.876785f
#elif INFANTRY_ID == 3
#define GxOFFSET 0.00270364084f
#define GyOFFSET -0.000532632112f
#define GzOFFSET 0.00478090625f
#define gNORM 9.73574924f
#elif INFANTRY_ID == 4
#define GxOFFSET 0.0007222f
#define GyOFFSET -0.001786f
#define GzOFFSET 0.0004346f
#define gNORM 9.876785f
#endif

/* IMU閺佺増宓佺紒鎾寸€担锟� */
typedef struct
{
    float Accel[3];

    float Gyro[3];

    float TempWhenCali;
    float Temperature;

    float AccelScale;
    float GyroOffset[3];

    float gNorm;
} IMU_Data_t;

/* BMI088闁挎瑨顕ら惍浣圭亣娑擄拷 */
enum
{
    BMI088_NO_ERROR = 0x00,
    BMI088_ACC_PWR_CTRL_ERROR = 0x01,
    BMI088_ACC_PWR_CONF_ERROR = 0x02,
    BMI088_ACC_CONF_ERROR = 0x03,
    BMI088_ACC_SELF_TEST_ERROR = 0x04,
    BMI088_ACC_RANGE_ERROR = 0x05,
    BMI088_INT1_IO_CTRL_ERROR = 0x06,
    BMI088_INT_MAP_DATA_ERROR = 0x07,
    BMI088_GYRO_RANGE_ERROR = 0x08,
    BMI088_GYRO_BANDWIDTH_ERROR = 0x09,
    BMI088_GYRO_LPM1_ERROR = 0x0A,
    BMI088_GYRO_CTRL_ERROR = 0x0B,
    BMI088_GYRO_INT3_INT4_IO_CONF_ERROR = 0x0C,
    BMI088_GYRO_INT3_INT4_IO_MAP_ERROR = 0x0D,

    BMI088_SELF_TEST_ACCEL_ERROR = 0x80,
    BMI088_SELF_TEST_GYRO_ERROR = 0x40,
    BMI088_NO_SENSOR = 0xFF,
};

extern IMU_Data_t BMI088;

/**
 * @brief 閸掓繂顫愰崠鏈哅I088,娴肩姴鍙嗘潻鐐村复閻ㄥ嚪PI閹崵鍤巋andle,娴犮儱寮烽弰顖氭儊鏉╂稖顢戦崷銊у殠閺嶅洤鐣�
 * 
 * @param bmi088_SPI handle
 * @param calibrate  1娑撻缚绻樼悰灞芥躬缁炬寧鐖ｇ€癸拷,0娴ｈ法鏁ょ粋鑽ゅ殠閺佺増宓�
 * @return uint8_t   閹存劕濮涢崚娆掔箲閸ユ卡MI088_NO_ERROR
 */
extern uint8_t BMI088Init(SPI_HandleTypeDef *bmi088_SPI, uint8_t calibrate);

/**
 * @brief 閸旂娀鈧喕顓搁崚婵嗩潗閸栵拷
 * 
 * @return uint8_t 
 */
extern uint8_t bmi088_accel_init(void);

/**
 * @brief 闂勨偓閾昏桨鍗庨崚婵嗩潗閸栵拷
 * 
 * @return uint8_t 
 */
extern uint8_t bmi088_gyro_init(void);

/**
 * @brief 鐠囪褰囨稉鈧▎顡圡I088閻ㄥ嫭鏆熼幑锟�,閸栧懏瀚璯yro閸滃畮ccel
 * 
 * @param bmi088 娴肩姴鍙咮MI088鐎圭偘绶�(缂佹挻鐎担锟�)
 */
extern void BMI088_Read(IMU_Data_t *bmi088);

#endif
