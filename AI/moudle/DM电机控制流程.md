# DM 电机控制流程

> DM 电机与 DJI 电机架构完全不同：**每个 DM 电机实例拥有自己独立的 RTOS 任务**。

---

## 1. 与 DJI 电机的核心区别

| 维度 | DJI 电机 | DM 电机 |
|------|---------|--------|
| 控制架构 | **一组电机共享一个任务** `MotorControlTask()` | **每个电机实例独立一个 RTOS 任务** `DMMotorTask()` |
| 任务创建 | 编译时固定（`robot_task.h` 中的 `StartMOTORTASK`） | 运行时动态创建（`DMMotorControlInit()` 为每个实例 `osThreadCreate`） |
| 发送频率 | 1kHz（所有电机顺序计算完一起发） | 500Hz（`osDelay(2)`，每个实例独立发） |
| 发送协议 | DJI 协议（仅发 `int16_t` 电流值） | MIT 协议（发 pos/vel/torque/Kp/Kd 五元组） |
| 分组发送 | 4 电机拼 1 帧 CAN（`sender_assignment[6]`） | 每个电机独立组包发送 |
| PID 计算 | 集中式 `DJIMotorControl()` 三环串级 | 每个实例自己的 while(1) 中单独算 |

---

## 2. 完整控制流程

### 阶段 1：初始化（Application 调用）

```c
// gimbal.c 或 chassis.c 中：
Motor_Init_Config_s dm_config = {
    .can_init_config = {
        .can_handle = &hcan1,
        .tx_id = 1,
        .rx_id = 0x11,   // DM 电机默认回传 ID
    },
    .controller_setting_init_config = {
        .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
        .outer_loop_type = SPEED_LOOP,
        .close_loop_type = SPEED_LOOP,
    },
    .motor_type = DM_MOTOR,  // ← 注意：motor_def.h 中可能未定义此类型
};
DMMotorInstance *dm_motor = DMMotorInit(&dm_config);
```

### 阶段 2：`DMMotorInit()` 创建实例 (`dmmotor.c:66`)

```c
DMMotorInstance *DMMotorInit(Motor_Init_Config_s *config)
{
    // ① malloc 实例
    DMMotorInstance *motor = malloc(sizeof(DMMotorInstance));
    memset(motor, 0, sizeof(DMMotorInstance));

    // ② 保存配置 + 初始化 PID
    motor->motor_settings = config->controller_setting_init_config;
    PIDInit(&motor->current_PID, ...);
    PIDInit(&motor->speed_PID, ...);
    PIDInit(&motor->angle_PID, ...);
    motor->other_angle_feedback_ptr = ...;
    motor->other_speed_feedback_ptr = ...;

    // ③ 注册到 CAN 总线（接收反馈用）
    config->can_init_config.can_module_callback = DMMotorDecode;
    config->can_init_config.id = motor;
    motor->motor_can_instace = CANRegister(&config->can_init_config);

    // ④ 注册看门狗
    motor->motor_daemon = DaemonRegister(&conf);

    // ⑤ 使能 + 校准编码器
    DMMotorEnable(motor);                    // stop_flag = MOTOR_ENALBED
    DMMotorSetMode(DM_CMD_MOTOR_MODE, motor); // 发模式切换指令
    DMMotorCaliEncoder(motor);               // 发编码器零位校准指令

    // ⑥ 加入全局数组
    dm_motor_instance[idx++] = motor;
    return motor;
}
```

### 阶段 3：`DMMotorControlInit()` — 为每个实例创建独立 RTOS 任务 (`dmmotor.c:161`)

```c
void DMMotorControlInit()
{
    if (!idx) return;   // 没有注册电机就跳过

    for (size_t i = 0; i < idx; i++)
    {
        char task_name[8];
        sprintf(task_name, "dm%d", i);

        osThreadDef(dm_task, DMMotorTask, osPriorityNormal, 0, 128);
        dm_task_handle[i] = osThreadCreate(
            osThread(dm_task),
            dm_motor_instance[i]   // ★ 把实例指针作为参数传给任务
        );
    }
}
```

> ⚠️ **当前状态**：`DMMotorControlInit()` 已定义，但**项目中没有任何地方调用它**。这个 API 已经写好但未接入初始化流程。需要在 `OSTaskInit()` 或 `MotorControlTask()` 中添加调用。

### 阶段 4：`DMMotorTask()` — 每个实例独立的控制循环 (`dmmotor.c:120`)

```c
void DMMotorTask(void const *argument)
{
    DMMotorInstance *motor = (DMMotorInstance *)argument;  // 接收自己的实例
    DMMotor_Send_s motor_send_mailbox;
    float pid_ref, set;

    while (1)
    {
        // ① 读取目标值（Application 通过 DMMotorSetRef 写入）
        pid_ref = motor->pid_ref;

        // ② 反转处理
        set = pid_ref;
        if (motor->motor_settings.motor_reverse_flag == MOTOR_DIRECTION_REVERSE)
            set *= -1;

        // ③ 限幅
        LIMIT_MIN_MAX(set, DM_T_MIN, DM_T_MAX);  // -18 ~ +18 N·m

        // ④ 组包 MIT 协议（5 个字段）
        motor_send_mailbox.position_des = float_to_uint(0,        DM_P_MIN, DM_P_MAX, 16);  // 位置=0（力控）
        motor_send_mailbox.velocity_des = float_to_uint(0,        DM_V_MIN, DM_V_MAX, 12);  // 速度=0（力控）
        motor_send_mailbox.torque_des   = float_to_uint(pid_ref,  DM_T_MIN, DM_T_MAX, 12);  // ★ 实际控制量 ★
        motor_send_mailbox.Kp = 0;
        motor_send_mailbox.Kd = 0;

        // ⑤ 停止检测
        if (motor->stop_flag == MOTOR_STOP)
            motor_send_mailbox.torque_des = 0;

        // ⑥ 填入 CAN tx_buff（8 字节 MIT 协议帧）
        motor->motor_can_instace->tx_buff[0] = (position_des >> 8);
        motor->motor_can_instace->tx_buff[1] = (position_des & 0xFF);
        // ... buff[2:3] = velocity_des
        // ... buff[4:5] = Kp
        // ... buff[6:7] = Kd | torque_des

        // ⑦ 发送
        CANTransmit(motor->motor_can_instace, 1);

        osDelay(2);  // 500Hz
    }
}
```

### 阶段 5：Application 设目标

```c
DMMotorSetRef(dm_motor, 5.0f);  // 设 5 N·m 扭矩
// → motor->pid_ref = 5.0f
// → 500Hz 任务在下一循环读到 → 转为 uint → CAN 发出
```

### 阶段 6：接收反馈

CAN 中断 → `DMMotorDecode()` (`dmmotor.c:35`)：

```c
static void DMMotorDecode(CANInstance *motor_can)
{
    DMMotorInstance *motor = (DMMotorInstance *)motor_can->id;
    DM_Motor_Measure_s *measure = &motor->measure;

    DaemonReload(motor->motor_daemon);

    // 解析 DM 电机 MIT 协议回传帧
    measure->position = uint_to_float(rxbuff[1:2],  DM_P_MIN, DM_P_MAX, 16);
    measure->velocity = uint_to_float(rxbuff[3:4],  DM_V_MIN, DM_V_MAX, 12);
    measure->torque   = uint_to_float(rxbuff[4:5],  DM_T_MIN, DM_T_MAX, 12);
    measure->T_Mos    = rxbuff[6];   // MOS 管温度
    measure->T_Rotor  = rxbuff[7];   // 转子温度
}
```

---

## 3. DM vs DJI 架构全景

```
DJI 电机架构：
  StartMOTORTASK (1个任务, robot_task.h)
    → MotorControlTask() (motor_task.c)
      → DJIMotorControl() → 遍历 dji_motor_instance[]
        → PID 计算 → 分组拼帧 → CANTransmit()

DM 电机架构：
  DMMotorControlInit() (需手动调用)
    → osThreadCreate × N  → 为每个电机创建独立任务
      → DMMotorTask(motor0)  (500Hz)  // 只管自己
      → DMMotorTask(motor1)  (500Hz)  // 只管自己
      → DMMotorTask(motor2)  (500Hz)
      → DMMotorTask(motor3)  (500Hz)
        → 自己组包 → CANTransmit()
```

---

## 4. 当前未接入

`DMMotorControlInit()` 已经写好了，但**全局没有任何代码调用它**。要启用 DM 电机控制，需在初始化流程中加入：

```c
// 例如在 robot_task.h 的 OSTaskInit() 最后：
void OSTaskInit()
{
    // ... 创建其他任务 ...
    DMMotorControlInit();  // ← 添加这一行
}
```

或者更优雅的方式：在 `MotorControlTask()` 中检测 `dm_motor_instance` 是否非空再初始化。

---

## 5. 控制对比总结

| 步骤 | DJI | DM |
|------|-----|-----|
| 创建 | `DJIMotorInit()` 加入全局数组 | `DMMotorInit()` 加入全局数组 |
| 启动控制 | 编译时创建 1 个 `StartMOTORTASK` | 运行时 `DMMotorControlInit()` 创建 N 个任务 |
| 设目标 | `DJIMotorSetRef(m, ref)` | `DMMotorSetRef(m, ref)`（接口一样） |
| 控制循环 | `DJIMotorControl()` 集中遍历 | `DMMotorTask()` 每个实例独立运行 |
| 发送 | 4 电机拼帧 → 统一发 | 每个实例自己组包独立发 |
| PID | 三环串级 | 当前仅扭矩直驱（pid_ref 直接当扭矩） |
| 频率 | 1kHz | 500Hz |
