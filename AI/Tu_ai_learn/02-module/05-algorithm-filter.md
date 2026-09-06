# 滤波器与工具库 -- 卡尔曼滤波、用户工具与 CRC 校验

## 模块职责

算法工具库提供卡尔曼滤波器（EKF/UKF 的基础）、数学工具函数和 CRC 校验算法，是姿态估计和通信校验的底层支撑。

## 设计思路

### 为什么需要矩阵运算库

卡尔曼滤波的五个核心方程都是矩阵运算。本框架使用 ARM CMSIS-DSP 库（`arm_math.h`）提供的浮点矩阵运算接口，利用 STM32F407 的 FPU 硬件加速，比纯软件实现快数倍。

### 为什么卡尔曼滤波器设计得如此复杂

标准卡尔曼滤波器假设所有传感器的采样频率相同，但实际系统中 IMU（1kHz）、气压计（50Hz）、GPS（10Hz）的采样频率差异很大。本实现通过 `UseAutoAdjustment` 机制，动态调整观测矩阵 H、噪声矩阵 R 和增益矩阵 K 的维度，使得不同频率的传感器数据可以在同一个滤波器中融合。

## 核心数据结构

### KalmanFilter_t -- 卡尔曼滤波器实例

```c
// kalman_filter.h
typedef struct kf_t {
    float *FilteredValue;    // 滤波输出值（状态估计）
    float *MeasuredVector;   // 量测向量（传感器数据入口）
    float *ControlVector;    // 控制向量

    uint8_t xhatSize;        // 状态变量维度
    uint8_t uSize;           // 控制变量维度
    uint8_t zSize;           // 观测量维度

    uint8_t UseAutoAdjustment;        // 是否启用量测自动调整
    uint8_t MeasurementValidNum;      // 当前有效量测数量
    uint8_t *MeasurementMap;          // 量测与状态的对应关系
    float *MeasurementDegree;         // H 矩阵中每个量测对应的元素值
    float *MatR_DiagonalElements;     // R 矩阵的对角线元素（各量测方差）
    float *StateMinVariance;          // 状态最小方差（防止 P 过度收敛）

    uint8_t SkipEq1~5;               // 跳过标准 KF 五式中任意一式的标志

    // 核心矩阵（使用 arm_matrix_instance_f32 类型）
    mat xhat, xhatminus;     // 状态估计 x(k|k), 先验估计 x(k|k-1)
    mat u, z;                // 控制向量, 量测向量
    mat P, Pminus;           // 协方差 P(k|k), 先验协方差 P(k|k-1)
    mat F, FT;               // 状态转移矩阵及其转置
    mat B;                   // 控制矩阵
    mat H, HT;               // 观测矩阵及其转置
    mat Q;                   // 过程噪声协方差
    mat R;                   // 量测噪声协方差
    mat K;                   // 卡尔曼增益

    // 用户自定义函数指针，可用于扩展为 EKF/UKF/ESKF
    void (*User_Func0_f)(struct kf_t *kf);  // 替换量测更新
    void (*User_Func1_f)(struct kf_t *kf);  // 替换先验估计
    // ... 共 7 个用户函数指针 (User_Func0~6)

    // 矩阵数据存储空间指针
    float *xhat_data, *xhatminus_data;
    float *P_data, *Pminus_data;
    // ... 其余矩阵的 data 指针
} KalmanFilter_t;
```

**关键设计**：
- 所有矩阵的数据空间通过 `user_malloc`（FreeRTOS 环境下为 `pvPortMalloc`）动态分配
- `SkipEq1~5` 标志位允许跳过标准 KF 的任意环节，配合 `User_Func` 可实现 EKF 等变体
- `StateMinVariance` 防止 P 矩阵对角线元素过度收敛到零，确保滤波器始终能响应新的量测

### 矩阵类型别名

```c
// kalman_filter.h
#define mat arm_matrix_instance_f32
#define Matrix_Init arm_mat_init_f32
#define Matrix_Add arm_mat_add_f32
#define Matrix_Subtract arm_mat_sub_f32
#define Matrix_Multiply arm_mat_mult_f32
#define Matrix_Transpose arm_mat_trans_f32
#define Matrix_Inverse arm_mat_inverse_f32
```

直接将 ARM DSP 库的矩阵函数重命名为简洁的通用名称，方便在不同场景下替换底层实现。

## 函数详解

### Kalman_Filter_Init() -- 初始化滤波器

```c
void Kalman_Filter_Init(KalmanFilter_t *kf, uint8_t xhatSize, uint8_t uSize, uint8_t zSize)
```

**逻辑**（`kalman_filter.c:143`）：
1. 保存维度信息
2. 为量测相关数组分配空间：`MeasurementMap`, `MeasurementDegree`, `MatR_DiagonalElements`, `StateMinVariance`
3. 为滤波数据分配空间：`FilteredValue`, `MeasuredVector`, `ControlVector`
4. 为所有矩阵分配空间并初始化（`xhat`, `xhatminus`, `P`, `Pminus`, `F`, `FT`, `B`, `H`, `HT`, `Q`, `R`, `K` 等）
5. 初始化临时矩阵和向量
6. 清零所有 `SkipEq` 标志

分配后需要用户手动设置矩阵值（如 F, Q, H, R），通常在调用 `Kalman_Filter_Init` 之后通过 `memcpy` 写入 `_data` 数组。

### Kalman_Filter_Update() -- 执行一次完整滤波

```c
float *Kalman_Filter_Update(KalmanFilter_t *kf)
```

**逻辑**（`kalman_filter.c:369`）-- 卡尔曼滤波"黄金五式"：

```
0. 量测获取:    Kalman_Filter_Measure()
   -> 若 UseAutoAdjustment，调用 H_K_R_Adjustment() 动态重构 H/R/K
   -> User_Func0_f (可选)

1. 先验估计:    xhat'(k) = F * xhat(k-1) + B * u
   -> Kalman_Filter_xhatMinusUpdate()
   -> User_Func1_f (可选)

2. 先验协方差:  P'(k) = F * P(k-1) * F^T + Q
   -> Kalman_Filter_PminusUpdate()
   -> User_Func2_f (可选)

3. 若有有效量测:
   a. 卡尔曼增益: K(k) = P'(k) * H^T * (H * P'(k) * H^T + R)^(-1)
      -> Kalman_Filter_SetK()
      -> User_Func3_f (可选)

   b. 状态更新:   xhat(k) = xhat'(k) + K(k) * (z(k) - H * xhat'(k))
      -> Kalman_Filter_xhatUpdate()
      -> User_Func4_f (可选)

   c. 协方差更新: P(k) = (I - K(k) * H) * P'(k)
      -> Kalman_Filter_P_Update()
      -> User_Func5_f (可选)

4. 若无有效量测:
   -> xhat(k) = xhat'(k)  (仅预测)
   -> P(k) = P'(k)

5. 防过度收敛: 对 P 对角线元素设下限
6. 复制滤波值到 FilteredValue
7. User_Func6_f (可选后处理)
```

### H_K_R_Adjustment() -- 量测自动调整（static）

```c
static void H_K_R_Adjustment(KalmanFilter_t *kf)
```

**逻辑**（`kalman_filter.c:437`）：
1. 从 `MeasuredVector` 中识别非零元素（0 表示该量测无效）
2. 用有效量测重构 `z_data`、`H_data`、`R_data`
3. 调整矩阵的行列数：`H`, `R`, `K`, `z` 的维度随有效量测数变化

这解决了不同传感器采样频率不同的问题：当某个传感器没有新数据时，其对应位置在 `MeasuredVector` 中保持为 0，滤波器自动忽略该量测。

### 用户工具函数 -- `user_lib.c`

#### AverageFilter() -- 均值滤波

```c
float AverageFilter(float new_data, float *buf, uint8_t len)
```

将 buffer 左移一位（丢弃最老的数据），填入新数据，返回平均值。HT04 电机的速度解码中使用了此函数。

#### Sqrt() -- 快速开方

```c
float Sqrt(float x)
```

使用牛顿迭代法，比 `sqrtf()` 快约 2 倍（不需要 IEEE 754 精度保证时可用）。

#### 限幅与格式化工具

| 函数 | 功能 |
|------|------|
| `float_constrain(val, min, max)` | 限幅到 [min, max] |
| `loop_float_constrain(val, min, max)` | 循环限幅（如角度 -180~180） |
| `theta_format(Ang)` | 角度格式化到 [-180, 180] |
| `float_deadband(val, min, max)` | 死区处理 |
| `sign(value)` | 符号函数 |

#### 三维向量运算

| 函数 | 功能 |
|------|------|
| `Norm3d(v)` | 归一化（原地） |
| `NormOf3d(v)` | 求模长 |
| `Cross3d(v1, v2, res)` | 叉乘 |
| `Dot3d(v1, v2)` | 点乘 |

这些函数在 INS 姿态解算中大量使用（如初始四元数计算、重力向量转换等）。

#### MatInit() -- 矩阵初始化

```c
void MatInit(mat *m, uint8_t row, uint8_t col)
```

使用 `zmalloc`（malloc + memset 清零）分配矩阵数据空间并设置行列数。

### CRC 校验 -- `crc8.c` / `crc16.c`

#### crc_8()

```c
uint8_t crc_8(const uint8_t *input_str, uint16_t num_bytes)
```

基于查表法的 CRC-8 计算，用于 CAN 通信的帧校验（`can_comm.c` 中的数据包校验）。

#### crc_16() / crc_modbus()

```c
uint16_t crc_16(const uint8_t *input_str, uint16_t num_bytes)
uint16_t crc_modbus(const uint8_t *input_str, uint16_t num_bytes)
```

CRC-16 和 Modbus CRC-16，用于裁判系统通信协议的帧校验。首次调用时通过 `init_crc16_tab()` 生成查找表。

## 调用链

### 卡尔曼滤波在姿态估计中的使用

```
APP: INS_Init()
  -> Kalman_Filter_Init(&IMU_QuaternionEKF, 7, 0, 6)  // 7个状态(4四元数+3零偏), 6个量测(3加速度+3角速度)
  -> 设置 F, Q, H, R, P 矩阵

APP: INS_Task() [1kHz]
  -> BMI088_Read() -> 获取 Accel[3], Gyro[3]
  -> IMU_QuaternionEKF_Update()
     -> 设置 MeasuredVector
     -> Kalman_Filter_Update()
        -> 先验估计（四元数传播）
        -> 量测更新（加速度计修正）
        -> 协方差更新
     -> 从 FilteredValue 获取四元数和零偏估计
  -> 从 QEKF_INS 读取 Roll/Pitch/Yaw
```

### 均值滤波在电机解码中的使用

```
CAN 中断 -> HTMotorDecode()
  -> measure->speed_rads = AverageFilter(uint_to_float(tmp, ...), measure->speed_buff, SPEED_BUFFER_SIZE)
```

### CRC8 在 CAN 通信中的使用

```
APP: CANCommSend(instance, data)
  -> memcpy(send_buf + 2, data, send_data_len)
  -> crc8 = crc_8(data, send_data_len)           // 计算 CRC
  -> send_buf[2 + send_data_len] = crc8          // 写入校验和

接收端: CANCommRxCallback()
  -> if (raw_recvbuf[recv_buf_len - 2] == crc_8(raw_recvbuf + 2, recv_data_len))
     -> 校验通过，解包数据
```

## 注意事项

1. **KalmanFilter_t 的内存占用较大** -- 一个 7 状态 6 量测的滤波器，所有矩阵数据约需 `(7*7 + 7*7 + 7*7 + 7*7 + 7*6 + 6*7 + 6*6 + 7*6 + ...) * 4` 字节，加上多个临时矩阵，总计约 2-3KB。在内存受限的 MCU 上需注意。

2. **量测向量中 0 表示无效** -- `MeasuredVector` 中为 0 的元素被视为无效量测，会被 `H_K_R_Adjustment` 移除。如果某个传感器的真实读数恰好为 0，会被误判为无效。这是一个已知的局限性。

3. **矩阵运算使用 ARM DSP 库** -- 所有矩阵运算通过 `arm_math.h` 的 FPU 加速实现。移植到没有 FPU 的 MCU 时需要替换为软件实现。

4. **User_Func 扩展机制** -- 通过 `User_Func0~6` 和 `SkipEq1~5`，可以将标准 KF 扩展为 EKF（非线性状态转移）、UKF（无迹变换）等。当前框架中的 `QuaternionEKF` 就是通过 `User_Func` 实现的非线性扩展。

5. **CRC 查找表是 static 的** -- `crc8.c` 中的 `sht75_crc_table` 和 `crc16.c` 中的 `crc_tab16` 都是文件作用域的静态数组，不占用全局命名空间，但每个使用 CRC 的编译单元都会有一份副本。

6. **Sqrt() 不处理负数** -- 输入 <= 0 时直接返回 0，不设置错误标志。在需要错误检测的场合应使用标准 `sqrtf()`。

7. **AverageFilter 的 buffer 移动效率** -- 每次调用都进行 `len-1` 次数组元素移动（`buf[i] = buf[i+1]`），当 `len` 较大时效率不高。可以考虑环形缓冲区优化，但对于电机控制中 `len=5` 的场景影响可忽略。
