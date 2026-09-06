# IMU 与惯导模块 -- BMI088 驱动与姿态估计

## 模块职责

IMU 模块负责读取 BMI088 惯性传感器数据（加速度计 + 陀螺仪），并通过四元数 EKF 进行姿态估计，为云台和底盘提供高精度的角度和角速度反馈。

## 设计思路

### 三层分离架构

IMU 模块分为三个层次：

1. **BMI088Middleware** -- 硬件抽象层，封装 SPI 通信和片选引脚操作
2. **BMI088driver** -- 传感器驱动层，负责寄存器配置、数据读取、零偏校准
3. **ins_task** -- 姿态解算层，运行四元数 EKF，输出 Roll/Pitch/Yaw

这种分层使得更换传感器或通信接口时，只需修改最底层。

### 为什么需要 EKF 而非互补滤波

互补滤波简单但精度有限，尤其在大机动和长时间运行后漂移明显。四元数 EKF 通过融合加速度计（提供重力参考）和陀螺仪（提供高频角速度）数据，同时在线估计陀螺仪零偏，能提供更稳定的姿态估计。

## 核心数据结构

### IMU_Data_t -- 原始 IMU 数据

```c
// BMI088driver.h
typedef struct {
    float Accel[3];           // 三轴加速度 (m/s^2)
    float Gyro[3];            // 三轴角速度 (rad/s)
    float TempWhenCali;       // 标定时的温度
    float Temperature;        // 当前温度
    float AccelScale;         // 加速度计标度因子 (9.81/gNorm)
    float GyroOffset[3];      // 陀螺仪零偏
    float gNorm;              // 当地重力加速度模值
} IMU_Data_t;
```

- `AccelScale`：加速度计读数乘以此系数，使得重力加速度精确等于 9.81 m/s^2
- `GyroOffset`：陀螺仪静态零偏，在标定时计算，读取时减去

### INS_t -- 惯导解算结果

```c
// ins_task.h
typedef struct {
    float q[4];               // 四元数估计值
    float MotionAccel_b[3];   // 机体坐标系下的运动加速度（去除重力）
    float MotionAccel_n[3];   // 导航坐标系下的运动加速度
    float AccelLPF;           // 加速度低通滤波系数

    float xn[3], yn[3], zn[3]; // 机体系基向量在导航系中的表示

    float Gyro[3];            // 角速度（经零偏补偿）
    float Accel[3];           // 加速度（经标度补偿）
    float Roll, Pitch, Yaw;   // 欧拉角
    float YawTotalAngle;      // Yaw 总角度（多圈，用于云台控制）

    uint8_t init;             // 初始化标志
} INS_t;
```

### attitude_t -- 简化的姿态输出

```c
// ins_task.h
typedef struct {
    float Gyro[3];            // 角速度
    float Accel[3];           // 加速度
    float Roll, Pitch, Yaw;   // 欧拉角
    float YawTotalAngle;      // Yaw 总角度
} attitude_t;
```

`INS_Init()` 返回 `attitude_t*`，供 APP 层直接使用。

### IMU_Param_t -- 安装误差修正参数

```c
typedef struct {
    uint8_t flag;             // 是否需要更新旋转矩阵
    float scale[3];           // 标度因数
    float Yaw, Pitch, Roll;   // 安装偏角（度）
} IMU_Param_t;
```

用于修正 IMU 安装轴与云台轴不对齐的问题。

## 函数详解

### BMI088Middleware -- 硬件抽象

```c
// BMI088Middleware.c
void BMI088_ACCEL_NS_L(void)  // 加速度计片选拉低（选中）
void BMI088_ACCEL_NS_H(void)  // 加速度计片选拉高（释放）
void BMI088_GYRO_NS_L(void)   // 陀螺仪片选拉低
void BMI088_GYRO_NS_H(void)   // 陀螺仪片选拉高
uint8_t BMI088_read_write_byte(uint8_t txdata) // SPI 收发一字节
```

SPI 通信的底层实现。片选引脚由 GPIO 控制，数据通过 `HAL_SPI_TransmitReceive` 收发。

### BMI088driver -- 传感器驱动

#### BMI088Init() -- 初始化传感器

```c
uint8_t BMI088Init(SPI_HandleTypeDef *bmi088_SPI, uint8_t calibrate)
```

**逻辑**：
1. 保存 SPI 句柄
2. 调用 `bmi088_accel_init()` 和 `bmi088_gyro_init()` 分别初始化加速度计和陀螺仪
3. 若 `calibrate == 1`，进行在线标定；否则使用离线参数（硬编码的零偏值）

#### bmi088_accel_init() / bmi088_gyro_init()

```c
static uint8_t BMI088_Accel_Init_Table[BMI088_WRITE_ACCEL_REG_NUM][3] = {
    {BMI088_ACC_PWR_CTRL, BMI088_ACC_ENABLE_ACC_ON, BMI088_ACC_PWR_CTRL_ERROR},
    {BMI088_ACC_PWR_CONF, BMI088_ACC_PWR_ACTIVE_MODE, BMI088_ACC_PWR_CONF_ERROR},
    {BMI088_ACC_CONF, BMI088_ACC_NORMAL | BMI088_ACC_800_HZ | BMI088_ACC_CONF_MUST_Set, ...},
    {BMI088_ACC_RANGE, BMI088_ACC_RANGE_6G, ...},
    {BMI088_INT1_IO_CTRL, ...},
    {BMI088_INT_MAP_DATA, ...}
};
```

初始化表为 `[寄存器地址, 写入值, 错误码]` 的三元组。依次写入并回读校验，失败则记录错误码。

配置概要：
- 加速度计：6G 量程，800Hz 采样率，正常模式
- 陀螺仪：2000dps 量程，230Hz 带宽，正常模式

#### Calibrate_MPU_Offset() -- 在线零偏标定

```c
static void Calibrate_MPU_Offset(IMU_Data_t *bmi088)
```

**逻辑**：
1. 采集 6000 次数据，计算陀螺仪零偏的平均值
2. 同时计算重力加速度模值的平均值（`gNorm`）
3. 校验标准差：若零偏波动 > 0.15 rad/s 或 gNorm 偏离 9.8 > 0.5 m/s^2，说明标定过程中传感器被扰动，重新标定
4. 超时 12 秒后放弃，使用离线参数

**前提**：标定时机器人必须保持静止水平。

#### BMI088_Read() -- 读取传感器数据

```c
void BMI088_Read(IMU_Data_t *bmi088)
```

**逻辑**：
1. 通过 SPI 读取加速度计 6 字节，乘以 `AccelScale` 和灵敏度系数
2. 通过 SPI 读取陀螺仪 8 字节，减去 `GyroOffset`，乘以灵敏度系数
3. 读取温度传感器 2 字节，转换为摄氏度

### ins_task -- 姿态解算

#### INS_Init() -- 初始化惯导系统

```c
attitude_t *INS_Init(void)
```

**逻辑**：
1. 初始化 BMI088（含在线标定）
2. 读取 100 次加速度计数据，计算初始姿态（通过重力方向推算 Roll 和 Pitch）
3. 用初始四元数初始化 QEKF
4. 初始化温度控制 PID（维持 40 摄氏度恒温，减小温度漂移）
5. 返回 `attitude_t*`

#### INS_Task() -- 1kHz 姿态更新任务

```c
void INS_Task(void)
```

**逻辑**（每 1ms 执行一次）：
1. 读取 BMI088 数据
2. 执行 IMU 安装误差修正（`IMU_Param_Correction`）
3. **核心**：调用 `IMU_QuaternionEKF_Update()` 更新四元数
4. 从 QEKF 结果中获取 Roll/Pitch/Yaw
5. 计算运动加速度（去除重力分量）：重力从导航系转到机体系，加速度计读数减去重力
6. 每 2ms 执行一次温度 PID 控制

#### BodyFrameToEarthFrame() / EarthFrameToBodyFrame()

通过四元数实现的坐标系变换矩阵。用于将向量从一个坐标系转到另一个坐标系，核心是四元数对应的旋转矩阵。

## 调用链

### 从 SPI 到姿态角的完整路径

```
APP: GimbalInit()
  -> INS_Init()
     -> BMI088Init(&hspi1, 1)             // 初始化传感器
        -> bmi088_accel_init()             // 配置加速度计寄存器
        -> bmi088_gyro_init()              // 配置陀螺仪寄存器
        -> Calibrate_MPU_Offset()          // 在线标定
     -> InitQuaternion()                   // 计算初始四元数
     -> IMU_QuaternionEKF_Init()           // 初始化 EKF

APP: GimbalTask() [1kHz]
  -> INS_Task()
     -> BMI088_Read(&BMI088)              // SPI 读取原始数据
        -> BMI088_accel_read_muli_reg()    // 读加速度计
           -> BMI088_ACCEL_NS_L()          // 片选
           -> BMI088_read_muli_reg()       // SPI 连续读
           -> BMI088_ACCEL_NS_H()          // 释放
        -> BMI088_gyro_read_muli_reg()     // 读陀螺仪
     -> IMU_QuaternionEKF_Update()         // EKF 更新
        -> Kalman_Filter_Update()          // 卡尔曼滤波五式
     -> 从 QEKF_INS 读取 Roll/Pitch/Yaw
```

### IMU 数据到电机反馈的路径

```
INS_Task() 计算 YawTotalAngle
  -> APP 层通过 INS 返回的 attitude_t* 读取
  -> 设置电机的 other_angle_feedback_ptr = &attitude->Yaw
  -> DJIMotorControl() 中判断 angle_feedback_source == OTHER_FEED
     -> 使用 *motor_controller->other_angle_feedback_ptr 作为位置环反馈
```

这是小陀螺模式的关键：云台角度环不再使用电机编码器反馈，改用 IMU 的 Yaw 角。

## 注意事项

1. **INS_Init() 不要放入 RTOS 任务中** -- 初始化过程需要关闭中断（DWT_Delay 依赖精确计时），不应在 FreeRTOS 任务上下文中调用。

2. **温度控制的重要性** -- BMI088 的陀螺仪零偏随温度漂移显著。框架通过 PWM 加热片维持 40 摄氏度恒温，温度 PID 参数为 Kp=1000, Ki=20。

3. **在线标定的前提条件** -- 标定时机器人必须静止水平。代码中通过检查 gNorm 和零偏的标准差来判断是否被扰动，超时 12 秒后使用离线参数。

4. **离线参数按机器人编号硬编码** -- `GxOFFSET`, `GyOFFSET`, `GzOFFSET`, `gNORM` 通过 `INFANTRY_ID` 宏选择，更换机器人时需要更新。

5. **四元数初始化只确定 Roll 和 Pitch** -- Yaw 角在初始化时被设为 0（没有磁力计提供绝对方向参考）。这意味着每次上电，Yaw 角的零位是当前的朝向。

6. **attitude_t 指针的偷懒做法** -- `INS_Init()` 返回 `(attitude_t*)&INS.Gyro`，利用了结构体内存布局的巧合。代码注释中也标注了这个 `@todo`，后续可能修复。
