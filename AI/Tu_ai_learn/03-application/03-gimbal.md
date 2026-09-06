# Gimbal — 云台控制

## 模块职责

Gimbal 应用控制云台的两个电机（yaw/pitch），根据 cmd 发布的模式和参考值，通过串级 PID 实现云台的角度/速度闭环控制，并将 IMU 姿态和电机角度反馈给 robot_cmd。

---

## 设计思路

### 为什么云台需要独立应用？

云台是机器人最核心的稳定平台，它有以下特点需要独立管理：

1. **多反馈源切换** — 云台既可以用电机编码器反馈，也可以用 IMU 姿态反馈，不同模式需要动态切换
2. **串级 PID 控制** — 外环角度环 + 内环速度环，需要独立于底盘和发射的控制周期
3. **与底盘的耦合关系** — 底盘跟随模式需要云台的偏转角，但云台不应直接依赖底盘

### 双反馈源设计

云台的核心难题是：**用电机编码器反馈时，底盘转动会导致 yaw 角度变化；用 IMU 反馈时，云台相对惯性空间稳定。** 因此：

- `GIMBAL_FREE_MODE`：使用编码器反馈，云台相对底盘固定（用于调整姿态）
- `GIMBAL_GYRO_MODE`：使用 IMU 反馈，云台相对地面固定（用于正常操控和小陀螺）

两种模式的切换通过 `DJIMotorChangeFeed()` 实现，在运行时动态更改反馈来源。

---

## 核心数据结构

### 私有变量（gimbal.c）

```c
// ---- 硬件实例 ----
static attitude_t *gimba_IMU_data;                    // IMU 姿态数据指针，INS_Init() 返回
static DJIMotorInstance *yaw_motor, *pitch_motor;     // 两个 DJI 电机实例

// ---- 消息中心 ----
static Publisher_t *gimbal_pub;                       // 发布 "gimbal_feed"
static Subscriber_t *gimbal_sub;                      // 订阅 "gimbal_cmd"
static Gimbal_Upload_Data_s gimbal_feedback_data;     // 回传给 cmd 的反馈
static Gimbal_Ctrl_Cmd_s gimbal_cmd_recv;             // 从 cmd 接收的控制命令
```

### 关键数据流

```
robot_cmd 发布 "gimbal_cmd" (Gimbal_Ctrl_Cmd_s)
    |
    v
GimbalTask() 订阅并读取
    |
    +-- gimbal_cmd_recv.yaw        -> yaw 电机参考值
    +-- gimbal_cmd_recv.pitch      -> pitch 电机参考值
    +-- gimbal_cmd_recv.gimbal_mode -> 决定反馈来源和电机启停

gimbal 发布 "gimbal_feed" (Gimbal_Upload_Data_s)
    |
    +-- gimbal_imu_data                  -> IMU 姿态（robot_cmd 用于视觉发送等）
    +-- yaw_motor_single_round_angle     -> yaw 单圈角度（robot_cmd 用于计算偏转角）
```

---

## 函数详解

### GimbalInit() — 初始化

位于 `gimbal.c:18`。

```c
void GimbalInit()
{
    // 1. IMU 初始化，获取姿态数据指针
    gimba_IMU_data = INS_Init();

    // 2. YAW 电机配置
    Motor_Init_Config_s yaw_config = {
        .can_init_config = {
            .can_handle = &hcan1,     // CAN1
            .tx_id = 1,               // 电调 ID = 1
        },
        .controller_param_init_config = {
            .angle_PID = {
                .Kp = 8, .Ki = 0, .Kd = 0,
                .MaxOut = 500,       // 角度环输出限制
                .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit
                         | PID_Derivative_On_Measurement,
            },
            .speed_PID = {
                .Kp = 50, .Ki = 200, .Kd = 0,
                .MaxOut = 20000,     // 速度环输出限制
                .IntegralLimit = 3000,
            },
            // 关键：将 IMU 数据指针设为额外反馈源
            .other_angle_feedback_ptr = &gimba_IMU_data->YawTotalAngle,
            .other_speed_feedback_ptr = &gimba_IMU_data->Gyro[2],  // z轴角速度
        },
        .controller_setting_init_config = {
            .angle_feedback_source = OTHER_FEED,   // 使用 IMU 角度
            .speed_feedback_source = OTHER_FEED,   // 使用 IMU 角速度
            .outer_loop_type = ANGLE_LOOP,         // 最外层为角度环
            .close_loop_type = ANGLE_LOOP | SPEED_LOOP,  // 串级：角度+速度
        },
        .motor_type = GM6020,  // 大疆云台电机
    };

    // 3. PITCH 电机配置（类似，但反馈方向不同）
    // pitch 使用 &gimba_IMU_data->Pitch 和 &gimba_IMU_data->Gyro[0]

    // 4. 注册电机
    yaw_motor = DJIMotorInit(&yaw_config);
    pitch_motor = DJIMotorInit(&pitch_config);

    // 5. 注册消息中心
    gimbal_pub = PubRegister("gimbal_feed", sizeof(Gimbal_Upload_Data_s));
    gimbal_sub = SubRegister("gimbal_cmd", sizeof(Gimbal_Ctrl_Cmd_s));
}
```

**要点**：
- `other_angle_feedback_ptr` 和 `other_speed_feedback_ptr` 是指向 IMU 数据的指针，电机控制器在 PID 计算时会读取这些指针指向的值
- yaw 的角速度取 `Gyro[2]`（z 轴），pitch 取 `Gyro[0]`（x 轴），这是由 IMU 坐标系与云台安装方向决定的
- 串级 PID：外环角度环输出作为速度环输入，速度环输出作为电机电流设定值

### GimbalTask() — 核心任务

位于 `gimbal.c:103`。

```c
void GimbalTask()
{
    // 1. 订阅控制命令
    SubGetMessage(gimbal_sub, &gimbal_cmd_recv);

    // 2. 根据模式执行不同控制逻辑
    switch (gimbal_cmd_recv.gimbal_mode)
    {
    case GIMBAL_ZERO_FORCE:
        DJIMotorStop(yaw_motor);    // 电机断电
        DJIMotorStop(pitch_motor);
        break;

    case GIMBAL_GYRO_MODE:
        DJIMotorEnable(yaw_motor);
        DJIMotorEnable(pitch_motor);
        // 切换到 IMU 反馈
        DJIMotorChangeFeed(yaw_motor, ANGLE_LOOP, OTHER_FEED);
        DJIMotorChangeFeed(yaw_motor, SPEED_LOOP, OTHER_FEED);
        DJIMotorChangeFeed(pitch_motor, ANGLE_LOOP, OTHER_FEED);
        DJIMotorChangeFeed(pitch_motor, SPEED_LOOP, OTHER_FEED);
        // 设定参考值
        DJIMotorSetRef(yaw_motor, gimbal_cmd_recv.yaw);
        DJIMotorSetRef(pitch_motor, gimbal_cmd_recv.pitch);
        break;

    case GIMBAL_FREE_MODE:
        DJIMotorEnable(yaw_motor);
        DJIMotorEnable(pitch_motor);
        // 同样使用 OTHER_FEED（当前实现）
        DJIMotorChangeFeed(yaw_motor, ANGLE_LOOP, OTHER_FEED);
        DJIMotorChangeFeed(yaw_motor, SPEED_LOOP, OTHER_FEED);
        DJIMotorChangeFeed(pitch_motor, ANGLE_LOOP, OTHER_FEED);
        DJIMotorChangeFeed(pitch_motor, SPEED_LOOP, OTHER_FEED);
        DJIMotorSetRef(yaw_motor, gimbal_cmd_recv.yaw);
        DJIMotorSetRef(pitch_motor, gimbal_cmd_recv.pitch);
        break;
    }

    // 3. 填充反馈数据
    gimbal_feedback_data.gimbal_imu_data = *gimba_IMU_data;
    gimbal_feedback_data.yaw_motor_single_round_angle
        = yaw_motor->measure.angle_single_round;

    // 4. 发布反馈
    PubPushMessage(gimbal_pub, (void *)&gimbal_feedback_data);
}
```

**关键逻辑**：
- `GIMBAL_ZERO_FORCE` 模式下调用 `DJIMotorStop()`，电机会立刻断电（电流归零），不是将参考值设为 0
- `GIMBAL_GYRO_MODE` 和 `GIMBAL_FREE_MODE` 在当前实现中都使用 `OTHER_FEED`，区别在于 robot_cmd 设置的底盘跟随策略不同
- 参考值 `gimbal_cmd_recv.yaw` 是 robot_cmd 中通过增量方式累积的 total_angle

---

## 调用链

### 初始化调用链

```
RobotInit()                                    // robot.c:23
  |
  +-- GimbalInit()                             // gimbal.c:18
        |
        +-- INS_Init()                         // 返回 IMU 姿态数据指针
        +-- DJIMotorInit(&yaw_config)          // 注册 yaw 电机
        |     |
        |     +-- MotorSenderGrouping()        // CAN 发送分组
        |     +-- CANRegister()                // 注册 CAN 实例
        |     +-- DaemonRegister()             // 注册离线检测
        |
        +-- DJIMotorInit(&pitch_config)        // 注册 pitch 电机
        +-- PubRegister("gimbal_feed")         // 发布反馈
        +-- SubRegister("gimbal_cmd")          // 订阅命令
```

### 运行时控制链

```
StartROBOTTASK (200Hz)
  |
  +-- GimbalTask()
        |
        +-- SubGetMessage("gimbal_cmd")        // 获取 cmd 的控制命令
        |
        +-- DJIMotorSetRef(yaw, yaw_ref)       // 设定参考值
        |     |
        |     v  （下一周期由 MotorControlTask 执行）
        |   MotorControlTask()                  // motor_task.c
        |     +-- DJIMotorControl()             // 遍历所有 DJI 电机
        |           |
        |           +-- PIDCalculate(angle_PID) // 角度环 PID
        |           +-- PIDCalculate(speed_PID) // 速度环 PID
        |           +-- CANTransmit()           // 发送控制报文
        |
        +-- PubPushMessage("gimbal_feed")      // 发布反馈

--- CAN 接收路径 ---

CAN FIFO 中断
  |
  +-- HAL_CAN_RxFifo0MsgPendingCallback()      // bsp_can.c
        +-- CANFIFOxCallback()
              +-- 遍历 can_instance[] 找到匹配的实例
              +-- 调用 DecodeDJIMotor()         // dji_motor.c
                    |
                    +-- 解析编码器值、速度、电流
                    +-- 计算多圈角度 total_angle
                    +-- DaemonReload()          // 重载离线检测
```

---

## 注意事项

1. **IMU 坐标系与云台坐标系的方向映射** — `Gyro[2]` 对应 yaw 角速度，`Gyro[0]` 对应 pitch 角速度，这不是随意选择的，而是由 IMU 安装方向和 `robot_def.h` 中 `GYRO2GIMBAL_DIR_*` 宏决定的。更换 IMU 安装方式时必须重新确认。

2. **`GIMBAL_FREE_MODE` 当前实现与 `GIMBAL_GYRO_MODE` 相同** — 注释提到 FREE_MODE 应使用编码器反馈，但代码中两者都使用了 `OTHER_FEED`（IMU）。后续会修改 FREE_MODE 为 `MOTOR_FEED`。

3. **pitch 重力补偿未实现** — 注释标记了 `@todo`，需要根据当前 pitch 角度计算重力矩，作为前馈叠加到 PID 输出上，否则 pitch 在不同仰角下的响应特性会不同。

4. **云台参考值是增量累积的** — robot_cmd 中 `yaw += 0.005f * rocker`，每次叠加一个小增量。这意味着 yaw 电机对 `total_angle` 闭环，而不是单圈角度。上电时 total_angle 为 0，云台会保持静止直到收到控制命令。

5. **GM6020 的 ID 冲突** — GM6020 的反馈 ID 为 `0x204 + id`，而 M3508/M2006 的反馈 ID 为 `0x200 + id`。如果同一 CAN 总线上 6020 的 ID 和 3508 的 ID 存在 4 的偏移关系，会导致冲突。dji_motor.c 中有冲突检测。

6. **PID 参数调参** — 角度环 Kp=8 速度环 Kp=50/Ki=200 是 GM6020 的典型值，但需要根据具体机械结构（负载惯量、摩擦力）调整。Ki 过大会导致振荡，过小会导致稳态误差。
