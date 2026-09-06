# 电机控制模块 -- DJI/HT/LK/DM/Servo 电机家族

## 模块职责

电机控制模块封装了多种型号电机的通信协议、反馈解码和闭环控制逻辑，为 APP 层提供统一的"设定参考值-读取反馈"接口。

## 设计思路

### 统一抽象 + 个性实现

不同电机在通信协议和数据格式上差异很大，但控制逻辑（PID 闭环）高度相似。框架的做法是：

- **通用定义**集中在 `motor_def.h`（闭环类型、电机类型枚举、控制设置结构体等）
- **个性化实现**分散在各电机的 `.c/.h` 文件中（解码函数、发送格式、初始化流程）
- **统一调度**通过 `motor_task.c` 的 `MotorControlTask()` 实现

### "对应用而言，电机是传递函数为 1 的设备"

APP 层调用 `DJIMotorSetRef(motor, ref)` 设定参考值，电机的 PID 闭环、CAN 收发由模块自动处理。APP 不需要知道电机用了几个闭环、走的是什么通信协议。

## 核心数据结构

### 闭环类型与控制设置 -- `motor_def.h`

```c
// 闭环类型，可用或运算组合
typedef enum {
    OPEN_LOOP = 0b0000,
    CURRENT_LOOP = 0b0001,   // 电流环
    SPEED_LOOP   = 0b0010,   // 速度环
    ANGLE_LOOP   = 0b0100,   // 位置环
} Closeloop_Type_e;

// 电机控制设置
typedef struct {
    Closeloop_Type_e outer_loop_type;         // 最外层闭环类型
    Closeloop_Type_e close_loop_type;          // 启用的闭环组合
    Motor_Reverse_Flag_e motor_reverse_flag;   // 电机反转标志
    Feedback_Reverse_Flag_e feedback_reverse_flag; // 反馈反转标志
    Feedback_Source_e angle_feedback_source;   // 角度反馈来源（电机/其他）
    Feedback_Source_e speed_feedback_source;   // 速度反馈来源
    Feedfoward_Type_e feedforward_flag;        // 前馈标志
} Motor_Control_Setting_s;
```

### 电机控制器 -- `motor_def.h`

```c
typedef struct {
    float *other_angle_feedback_ptr;  // 外部角度反馈指针（如 IMU）
    float *other_speed_feedback_ptr;  // 外部速度反馈指针
    float *speed_feedforward_ptr;     // 速度前馈指针
    float *current_feedforward_ptr;   // 电流前馈指针

    PIDInstance current_PID;  // 电流环 PID
    PIDInstance speed_PID;    // 速度环 PID
    PIDInstance angle_PID;    // 位置环 PID

    float pid_ref;  // 参考值，在串级闭环中充当数据载体
} Motor_Controller_s;
```

`pid_ref` 是串级 PID 计算的核心变量：它先作为位置环的输入，位置环的输出覆盖它，再作为速度环的输入，以此类推。这种设计使得不同闭环类型的选择变得简单 -- 只需通过位掩码判断是否启用某个环。

### 电机初始化配置 -- `motor_def.h`

```c
typedef struct {
    Motor_Controller_Init_s controller_param_init_config; // PID 参数
    Motor_Control_Setting_s controller_setting_init_config; // 闭环设置
    Motor_Type_e motor_type;     // 电机型号（M3508/M2006/GM6020/...）
    CAN_Init_Config_s can_init_config; // CAN 配置
} Motor_Init_Config_s;
```

### DJI 电机实例 -- `dji_motor.h`

```c
typedef struct {
    DJI_Motor_Measure_s measure;            // 反馈量测值
    Motor_Control_Setting_s motor_settings; // 控制设置
    Motor_Controller_s motor_controller;    // 控制器

    CANInstance *motor_can_instance; // 拥有的 CAN 实例
    uint8_t sender_group;  // 发送分组号（0-5）
    uint8_t message_num;   // 组内编号（0-3）

    Motor_Type_e motor_type;
    Motor_Working_Type_e stop_flag;  // 启停标志
    DaemonInstance *daemon;          // 守护进程实例
    uint32_t feed_cnt;               // 喂狗计数
    float dt;                        // 两次反馈的时间间隔
} DJIMotorInstance;
```

## 函数详解

### DJI 电机 -- `dji_motor.c`

#### DJIMotorInit() -- 初始化 DJI 电机

```c
DJIMotorInstance *DJIMotorInit(Motor_Init_Config_s *config)
```

**逻辑**：
1. `malloc` 分配实例内存，`memset` 清零
2. 复制电机类型和控制设置
3. 初始化三环 PID（`PIDInit`），设置外部反馈指针和前馈指针
4. 调用 `MotorSenderGrouping()` 计算发送/接收 ID 和分组
5. 设置 CAN 回调为 `DecodeDJIMotor`，`id` 指向电机实例
6. 调用 `CANRegister()` 注册 CAN 实例
7. 注册 Daemon 守护进程（超时 20ms）
8. 使能电机，加入实例指针数组

#### DecodeDJIMotor() -- 反馈报文解码（static）

```c
static void DecodeDJIMotor(CANInstance *_instance)
```

**逻辑**：
1. 通过 `_instance->id` 获取 `DJIMotorInstance` 指针
2. 喂狗（`DaemonReload`），计算 `dt`
3. 解析 8 字节 CAN 反馈：
   - 字节 [0:1] -> 编码器值 `ecd`（0-8191）
   - 字节 [2:3] -> 转速（RPM），乘以 `RPM_2_ANGLE_PER_SEC` 转为度/秒，低通滤波
   - 字节 [4:5] -> 实际电流，低通滤波
   - 字节 [6]   -> 温度
4. 多圈角度计算：检测编码器跳变（>4096 说明跨越了 0/8191 边界），更新 `total_round`

#### DJIMotorControl() -- 串级 PID 计算与发送

```c
void DJIMotorControl()
```

**逻辑**：
1. 遍历 `dji_motor_instance[]` 数组
2. 对每个电机，按串级顺序计算 PID：
   - 位置环（启用且外层为位置环时计算）
   - 速度环（启用且外层为位置/速度环时计算，含前馈）
   - 电流环（启用时计算，含前馈）
3. 将 PID 输出填入 `sender_assignment[group].tx_buff` 对应位置
4. 若电机停止，将对应发送缓冲置零
5. 遍历 `sender_enable_flag[]`，对有电机注册的分组调用 `CANTransmit()`

#### MotorSenderGrouping() -- 电机分组（static）

DJI 电机 4 个一组共用一帧 CAN 控制报文。分组规则：
- M3508/M2006：ID 1-4 用发送 ID `0x200`，ID 5-8 用 `0x1FF`
- GM6020：ID 1-4 用发送 ID `0x2FF`，ID 5-8 用 `0x1FF`（与 M3508/M2006 的 ID 5-8 共用）

同时检查 ID 冲突（如 GM6020 的 ID 1 和 M3508 的 ID 5 反馈 ID 相同）。

### HT04 电机 -- `HT04.c`

HT04 与 DJI 电机的关键区别：

- **协议不同**：HT04 使用 MIT Mini Cheetah 协议，数据通过 `float_to_uint` / `uint_to_float` 进行浮点-整型映射
- **发送方式不同**：每个 HT04 电机创建独立的 FreeRTOS 任务（`HTMotorTask`），避免总线拥塞
- **模式控制**：需要发送特定命令帧（0xFC 使能、0xFD 停止、0xFE 校零）
- **反馈解码**：位置和速度用 16/12 位压缩编码，需要解映射

```c
// HT04 反馈解码核心
static void HTMotorDecode(CANInstance *motor_can) {
    HTMotorInstance *motor = (HTMotorInstance *)motor_can->id;
    // 解析位置（16位）
    tmp = (uint16_t)((rxbuff[1] << 8) | rxbuff[2]);
    measure->total_angle = uint_to_float(tmp, P_MIN, P_MAX, 16);
    // 解析速度（12位），过均值滤波
    tmp = (uint16_t)(rxbuff[3] << 4) | (rxbuff[4] >> 4);
    measure->speed_rads = AverageFilter(uint_to_float(tmp, V_MIN, V_MAX, 12), ...);
}
```

### LK9025 电机 -- `LK9025.c`

- **编码器精度**：16 位（65536），远高于 DJI 的 13 位（8192）
- **多电机发送**：使用第一个电机的 CAN 实例的 `tx_buff` 拼接所有电机数据，以 `0x280` 作为发送 ID
- **反馈 ID**：`0x140 + tx_id`
- **角度跨越检测阈值**：32768（16 位编码器，半量程）

### DM 电机 -- `dmmotor.c`

- 与 HT04 类似的 MIT 协议（`float_to_uint` / `uint_to_float`）
- 每个电机创建独立 FreeRTOS 任务
- 目前只实现了力控模式（`@Todo: 更多位控 PID 等请自行添加`）
- 支持 `DM_CMD_CLEAR_ERROR`（0xFB）清除过热错误

### 舵机 -- `servo_motor.c`

- 支持 PWM 舵机和总线舵机两种类型
- PWM 舵机：通过 `PWMSetDutyRatio()` 控制角度
- 总线舵机：通过 UART 串口通信，发送帧格式 `[0x55, 0x55, len, cmd, id, ...]`

### motor_task.c -- 统一调度

```c
void MotorControlTask() {
    DJIMotorControl();
    PowerControl();
    LKMotorControl();
    // HTMotorControlInit() 在 APP 初始化时调用，创建独立任务
}
```

### power_control.c -- 功率限制

`PowerControl()` 对底盘电机进行功率限制：
1. 计算每个电机的初始输出功率（基于电机功率模型：`P = K1*T^2 + K2*w^2 + C*T*w + constant`）
2. 若总功率超过 `chassis_max_power`，按比例缩放每个电机的输出力矩
3. 通过求解二次方程计算限制后的力矩值

## 调用链

### DJI 电机完整控制流程

```
APP: ChassisInit()
  -> DJIMotorInit(&config)
     -> PIDInit() x3                    // 初始化三环 PID
     -> MotorSenderGrouping()           // 计算 ID 和分组
     -> CANRegister()                   // 注册 CAN 实例
     -> DaemonRegister()                // 注册守护进程

APP: ChassisTask() [1kHz]
  -> DJIMotorSetRef(motor, ref)        // 设置参考值
  -> MotorControlTask()
     -> DJIMotorControl()
        -> PIDCalculate(&angle_PID)    // 位置环
        -> PIDCalculate(&speed_PID)    // 速度环
        -> PIDCalculate(&current_PID)  // 电流环
        -> CANTransmit()               // 发送控制报文

CAN 中断:
  -> DecodeDJIMotor()
     -> DaemonReload()                 // 喂狗
     -> 解析 ecd/speed/current/temp
     -> 计算多圈角度
```

## 注意事项

1. **DJI 电机 ID 冲突** -- GM6020 的反馈 ID（0x204+id）可能与 M3508/M2006 的 ID 5-8 冲突。注册时会检测并报错，需要合理分配拨码开关 ID。

2. **HT04 不支持多电机指令** -- 每个 HT04 电机需要独立的 CAN 报文，占用带宽较大。500Hz 控制频率下 2ms 内需完成多次 CAN 报文发送，总线负载需注意。

3. **LK9025 的多电机发送实现** -- 当前使用第一个电机的 CAN 实例发送，`tx_id` 被修改为 `0x280`。其他电机的 `tx_id` 仅用于计算偏移量，设计上有待优化。

4. **电机停止逻辑** -- `DJIMotorStop()` 设置 `stop_flag = MOTOR_STOP`，`DJIMotorControl()` 中检测到后将对应发送缓冲置零，而非将 PID 输出置零。这意味着 PID 内部状态（积分项）不会被清零，重新使能后可能产生突变。

5. **编码器多圈角度假设** -- 多圈角度计算假设两次采样间电机转过的角度小于半圈（DJI 为 4096/8192，LK 为 32768/65536）。如果控制频率过低，此假设可能不成立。

6. **PowerControl 与 DJIMotorControl 的关系** -- `PowerControl()` 维护自己独立的实例指针数组和发送分组，与 `DJIMotorControl()` 中的是分开的。底盘电机需通过 `PowerControlInit()` 注册而非 `DJIMotorInit()`。
