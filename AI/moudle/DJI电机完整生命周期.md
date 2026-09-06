# DJI 电机 — 从创建到控制的完整生命周期

---

## 总览：两线一循环

```
                ┌───── 发送线 (1kHz) ────┐
                │                        │
  Application   │   DJIMotorControl()    │   CAN 总线 ───→ 电机
  DJIMotorSetRef│   计算 PID → 组包 → 发送   │
                │                        │
                └────────────────────────┘

                ┌───── 接收线 (中断驱动) ────┐
                │                          │
  电机 ──CAN──→ DecodeDJIMotor() ──→ 更新 measure
                │   解析报文，更新反馈数据       │
                └────────────────────────────┘

                ┌───── 守护线 ────┐
                │  Daemon 监控    │
                │  20ms 无数据    │
                │  → 告警 + 停止  │
                └────────────────┘
```

---

## 阶段 1：初始化 — 创建电机实例

### Step 1: Application 构造初始化配置

以云台 Yaw 电机为例（`gimbal.c:22-57`）：

```c
Motor_Init_Config_s yaw_config = {
    .can_init_config = {
        .can_handle = &hcan1,          // 挂在 CAN1 上
        .tx_id = 1,                     // 电机 ID = 1
    },
    .controller_param_init_config = {
        .angle_PID = {                  // 角度环 PID 参数
            .Kp = 8, .Ki = 0, .Kd = 0,
            .MaxOut = 500,
        },
        .speed_PID = {                  // 速度环 PID 参数
            .Kp = 50, .Ki = 200, .Kd = 0,
            .MaxOut = 20000,
        },
        .current_PID = { /* 未设置，不启用电流环 */ },
        .other_angle_feedback_ptr = &imu_data->YawTotalAngle,  // ← IMU 角度反馈
        .other_speed_feedback_ptr = &imu_data->Gyro[2],        // ← IMU 角速度反馈
    },
    .controller_setting_init_config = {
        .angle_feedback_source = OTHER_FEED,   // 使用 IMU 反馈，不用电机编码器
        .speed_feedback_source = OTHER_FEED,
        .outer_loop_type = ANGLE_LOOP,         // 最外层是角度环
        .close_loop_type = ANGLE_LOOP | SPEED_LOOP,  // 双环控制
    },
    .motor_type = GM6020,
};
yaw_motor = DJIMotorInit(&yaw_config);   // ← 创建电机实例
```

### Step 2: `DJIMotorInit()` — 分配 + 初始化 + 注册（`dji_motor.c:159`）

```c
DJIMotorInstance *DJIMotorInit(Motor_Init_Config_s *config)
{
    // ① 在堆上分配实例
    DJIMotorInstance *instance = malloc(sizeof(DJIMotorInstance));
    memset(instance, 0, sizeof(DJIMotorInstance));

    // ② 保存基本配置
    instance->motor_type = config->motor_type;          // GM6020 / M3508 / M2006
    instance->motor_settings = config->controller_setting_init_config;  // 闭环类型、反转等

    // ③ 初始化三个 PID 控制器
    PIDInit(&instance->motor_controller.angle_PID,   &config->...angle_PID);
    PIDInit(&instance->motor_controller.speed_PID,   &config->...speed_PID);
    PIDInit(&instance->motor_controller.current_PID, &config->...current_PID);

    // ④ 保存外部反馈指针（IMU 或编码器）
    instance->motor_controller.other_angle_feedback_ptr = config->...other_angle_feedback_ptr;
    instance->motor_controller.other_speed_feedback_ptr = config->...other_speed_feedback_ptr;

    // ⑤ 分组：4 个电机共享一帧 CAN 报文
    MotorSenderGrouping(instance, &config->can_init_config);
    //   → 计算 rx_id、分配 sender_group 和 message_num
    //   → 设置 sender_enable_flag[group] = 1

    // ⑥ 注册到 BSP CAN 层（★ 关键：注册回调函数）
    config->can_init_config.can_module_callback = DecodeDJIMotor; // 绑定接收回调
    config->can_init_config.id = instance;                        // 绑定实例指针
    instance->motor_can_instance = CANRegister(&config->can_init_config);

    // ⑦ 注册看门狗（Daemon），20ms 收不到数据 → 离线告警
    Daemon_Init_Config_s daemon_config = {
        .callback = DJIMotorLostCallback,
        .owner_id = instance,
        .reload_count = 2,  // 2 × 10ms = 20ms
    };
    instance->daemon = DaemonRegister(&daemon_config);

    // ⑧ 加入全局电机数组
    DJIMotorEnable(instance);               // 初始状态 = 允许控制
    dji_motor_instance[idx++] = instance;   // 加入全局数组
    return instance;
}
```

---

## 阶段 2：发送线 — 1kHz PID 控制

### Step 3: RTOS 任务 `MotorTask` 调用 `DJIMotorControl()`

`robot_task.h:76-89`：

```c
void StartMOTORTASK(void const *argument)
{
    for (;;)
    {
        MotorControlTask();  // → DJIMotorControl()
        osDelay(1);          // 1kHz 频率
    }
}
```

### Step 4: `DJIMotorControl()` — 遍历电机，计算 PID，发送 CAN（`dji_motor.c:233`）

```c
void DJIMotorControl()
{
    for (size_t i = 0; i < idx; ++i)          // 遍历所有电机实例
    {
        motor = dji_motor_instance[i];
        pid_ref = motor->motor_controller.pid_ref;  // ★ 取出 Application 设的目标值 ★

        // ★ 串级 PID 计算 ★
        if (角度环启用)  pid_ref = PID(实际角度,  pid_ref);  // 输出=目标角速度
        if (速度环启用)  pid_ref = PID(实际角速度, pid_ref);  // 输出=目标电流
        if (电流环启用)  pid_ref = PID(实际电流,   pid_ref);  // 输出=最终扭矩值

        // 转为 int16_t 填入 CAN 发送帧
        set = (int16_t)pid_ref;
        group = motor->sender_group;     // 哪个发送组 (0-5)
        num   = motor->message_num;      // 组内第几个电机 (0-3)
        sender_assignment[group].tx_buff[2*num]   = set >> 8;      // 高 8 位
        sender_assignment[group].tx_buff[2*num+1] = set & 0xff;    // 低 8 位

        // 若电机被停止，发送电流置零
        if (motor->stop_flag == MOTOR_STOP)
            memset(sender_assignment[group].tx_buff + 2*num, 0, 2);
    }

    // 发送：只发有电机注册的组
    for (size_t i = 0; i < 6; ++i)
        if (sender_enable_flag[i])
            CANTransmit(&sender_assignment[i], 1);  // → 投递到 CAN 邮箱
}
```

**CAN ID 分组机制**（`dji_motor.c:22-29`）：

```
sender_assignment[6] 预定义了 6 组 CAN 发送实例：
   CAN1: [0] ID=0x1FF  [1] ID=0x200  [2] ID=0x2FF
   CAN2: [3] ID=0x1FF  [4] ID=0x200  [5] ID=0x2FF

每组 tx_buff[8] 可容纳 4 个电机的电流指令（每个 2 字节）
  motor 0 → buff[0:1]
  motor 1 → buff[2:3]
  motor 2 → buff[4:5]
  motor 3 → buff[6:7]
```

---

## 阶段 3：接收线 — CAN 中断 → 反馈解析

### Step 5: 硬件触发 CAN 接收中断

```
CAN 控制器收到电机发回的反馈帧
  → NVIC 中断
    → HAL_CAN_IRQHandler()
      → HAL_CAN_RxFifo0MsgPendingCallback()  [bsp_can.c]
        → CANFIFOxCallback()
          → 遍历 can_instance[] 数组
            → 匹配 hcan + rx_id
              → ★ 调用注册的回调 ★
```

### Step 6: `DecodeDJIMotor()` — 解析电机反馈数据（`dji_motor.c:122`）

```c
static void DecodeDJIMotor(CANInstance *_instance)
{
    // ① 反查电机实例（通过注册时设置的 void* id）
    DJIMotorInstance *motor = (DJIMotorInstance *)_instance->id;
    DJI_Motor_Measure_s *measure = &motor->measure;

    // ② 喂狗：通知 daemon "这个电机还活着"
    DaemonReload(motor->daemon);
    motor->dt = DWT_GetDeltaT(&motor->feed_cnt);  // 计算两帧间隔

    // ③ 解析 CAN 报文 → 填充 measure 结构体
    measure->ecd = (rxbuff[0] << 8) | rxbuff[1];   // 编码器值 (0-8191)
    measure->angle_single_round = ECD_ANGLE_COEF * measure->ecd;  // → 角度 0-360°

    measure->speed_aps = 低通滤波(rxbuff[2:3]);     // 角速度 (度/秒)
    measure->real_current = 低通滤波(rxbuff[4:5]);  // 实际电流
    measure->temperature = rxbuff[6];               // 温度

    // ④ 多圈角度计算（检测编码器溢出）
    if (measure->ecd - measure->last_ecd > 4096)    // 正向转过大半圈
        measure->total_round--;
    else if (measure->ecd - measure->last_ecd < -4096)  // 反向转过大半圈
        measure->total_round++;
    measure->total_angle = measure->total_round * 360 + measure->angle_single_round;
}
```

---

## 阶段 4：应用层设目标

### Step 7: Application 调用 `DJIMotorSetRef()`

`gimbal.c:126-127`：

```c
DJIMotorSetRef(yaw_motor,   gimbal_cmd_recv.yaw);     // 设 Yaw 目标角度
DJIMotorSetRef(pitch_motor, gimbal_cmd_recv.pitch);   // 设 Pitch 目标角度
```

实现极简（`dji_motor.c:227`）：

```c
void DJIMotorSetRef(DJIMotorInstance *motor, float ref)
{
    motor->motor_controller.pid_ref = ref;  // 一行！写入目标值
}
```

---

## 完整时序图

```
时间 →

t=0ms    Application: DJIMotorSetRef(yaw, 90°)  → pid_ref = 90
t=0ms    Application: DJIMotorSetRef(pitch, 10°)
         ...
t=1ms    MotorTask: DJIMotorControl() 被 RTOS 唤醒
           → 遍历 dji_motor_instance[]
           → pid_ref = 90 → Angle PID → Speed PID → set = int16_t
           → tx_buff 填好 → CANTransmit() → 邮箱投递
         (与此同时)
         硬件: CAN 控制器把上一帧发出去，电机收到
         中断: 电机回传反馈 → DecodeDJIMotor() → measure 更新
t=2ms    MotorTask: 同上，用最新的 measure 做 PID
t=3ms    同上
         ...
```

**同一个电机的控制闭环**：

```
OSC 1kHz 循环                            CAN 中断随时发生
────────────                            ────────────
DJIMotorSetRef(目标)                    DecodeDJIMotor(反馈)
    │                                       │
    ▼                                       ▼
pid_ref 写入                          measure 更新
    │                                       │
    ▼                                       ▼
DJIMotorControl()                     PID 下次计算时
  → 读取 measure                       读到最新值
  → PID 计算
  → CAN 发送
```

---

## 关键数据结构关系

```
DJIMotorInstance
├── measure: DJI_Motor_Measure_s        ← 中断更新 (DecodeDJIMotor)
│   ├── ecd, total_angle              ← 位置反馈
│   ├── speed_aps                     ← 速度反馈
│   └── real_current                  ← 电流反馈
│
├── motor_settings: Motor_Control_Setting_s
│   ├── outer_loop_type               ← 角度 / 速度
│   ├── close_loop_type               ← 单环 / 双环 / 三环
│   └── feedback_source               ← MOTOR_FEED / OTHER_FEED
│
├── motor_controller: Motor_Controller_s
│   ├── pid_ref                       ← Application 写入 (DJIMotorSetRef)
│   ├── angle_PID / speed_PID / current_PID
│   └── other_*_feedback_ptr          ← 指向 IMU 的内存 (零拷贝)
│
├── motor_can_instance: CANInstance*  ← CAN 通信管道
│   ├── tx_buff[8]                    ← 发送缓冲区
│   ├── rx_buff[8]                    ← 接收缓冲区
│   └── can_module_callback           ← = DecodeDJIMotor
│
├── sender_group / message_num        ← 发送分组信息
└── daemon: DaemonInstance*           ← 离线监控
```

---

## 总结：六步创建到控制

| 步骤 | 谁做 | 做了什么 |
|:---:|------|---------|
| ① | **Application** | 构造 `Motor_Init_Config_s`，设定 ID、PID 参数、闭环类型、反馈来源 |
| ② | **DJIMotorInit** | `malloc` 实例 → 初始化 PID → 分组 → `CANRegister` 注册中断回调 → 加入全局数组 |
| ③ | **MotorTask** | RTOS 1kHz 死循环调用 `DJIMotorControl()` |
| ④ | **DJIMotorControl** | 遍历所有电机 → 读 `pid_ref` → 串级 PID → 组包填 `tx_buff` → `CANTransmit` 发送 |
| ⑤ | **CAN 中断** | 电机回传 → `DecodeDJIMotor()` 解析 → 更新 `measure` → `DaemonReload` 喂狗 |
| ⑥ | **Application** | 调用 `DJIMotorSetRef(motor, target)` 设目标 → 下一周期 ④ 自动闭环 |
