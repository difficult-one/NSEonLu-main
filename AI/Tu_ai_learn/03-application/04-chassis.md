# Chassis — 底盘控制

## 模块职责

Chassis 应用接收 robot_cmd 发布的速度命令和模式指令，进行麦克纳姆轮运动学解算，将底盘坐标系下的速度映射到四个轮子的转速，并通过功率控制限制输出，最终由电机模块执行闭环控制。

---

## 设计思路

### 为什么底盘需要独立应用？

底盘控制看似简单——"给四个轮子设定速度"，但实际上涉及多个维度的处理：

1. **坐标变换** — 控制命令以云台指向为参考系（云台坐标系），但电机安装在底盘上（底盘坐标系），需要根据偏转角进行旋转变换
2. **运动学解算** — 麦克纳姆轮的逆运动学：三个速度分量 (vx, vy, wz) 分解到四个轮子
3. **模式管理** — 跟随云台、小陀螺自旋、不跟随三种模式，不同模式下 wz 的计算方式完全不同
4. **功率限制** — 裁判系统有功率上限，超级电容提供缓冲，需要动态调节输出

### 右手坐标系约定

```
        云台指向(前方)
            ^
            |  x正方向(vx)
            |
            +---> y正方向(vy)
           /
          底盘平面

    底盘逆时针旋转为 wz 正方向
    云台命令以云台指向为 x 轴，采用右手系
```

---

## 核心数据结构

### 私有变量（chassis.c）

```c
// ---- 电机实例 ----
static DJIMotorInstance *motor_lf, *motor_rf, *motor_lb, *motor_rb; // 四个轮毂电机

// ---- 消息中心（单板模式） ----
#ifdef ONE_BOARD
static Publisher_t *chassis_pub;                    // 发布 "chassis_feed"
static Subscriber_t *chassis_sub;                   // 订阅 "chassis_cmd"
#endif

// ---- 双板通信（底盘板模式） ----
#ifdef CHASSIS_BOARD
static CANCommInstance *chasiss_can_comm;           // CAN 板间通信
attitude_t *Chassis_IMU_data;                       // 底盘板 IMU
#endif

// ---- 控制数据 ----
static Chassis_Ctrl_Cmd_s chassis_cmd_recv;         // 接收的控制命令
static Chassis_Upload_Data_s chassis_feedback_data; // 发送的反馈数据

// ---- 辅助模块 ----
static PIDInstance buffer_PID;                       // 缓冲能量 PID
static referee_info_t *referee_data;                 // 裁判系统数据
static SuperCapInstance *cap;                        // 超级电容

// ---- 运动学中间变量 ----
static float chassis_vx, chassis_vy;                 // 底盘坐标系下的速度
static float vt_lf, vt_rf, vt_lb, vt_rb;            // 四轮目标速度
```

### 麦克纳姆轮参数宏

```c
// 由 robot_def.h 中的参数自动计算
#define HALF_WHEEL_BASE  (WHEEL_BASE / 2.0f)      // 半轴距 175mm
#define HALF_TRACK_WIDTH (TRACK_WIDTH / 2.0f)     // 半轮距 150mm
#define PERIMETER_WHEEL  (RADIUS_WHEEL * 2 * PI)  // 轮周长

// 各轮到云台中心的旋转半径（考虑偏移）
#define LF_CENTER ((HALF_TRACK_WIDTH + CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE - CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)
#define RF_CENTER ((HALF_TRACK_WIDTH - CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE - CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)
#define LB_CENTER ((HALF_TRACK_WIDTH + CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE + CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)
#define RB_CENTER ((HALF_TRACK_WIDTH - CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE + CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)
```

---

## 函数详解

### ChassisInit() — 初始化

位于 `chassis.c:59`。

```c
void ChassisInit()
{
    // 1. 电机配置模板（四个轮子参数相同，只改 tx_id 和方向）
    Motor_Init_Config_s chassis_motor_config = {
        .can_init_config.can_handle = &hcan1,
        .controller_param_init_config = {
            .speed_PID = {
                .Kp = 0.75, .Ki = 0, .Kd = 0,
                .MaxOut = 15000,
                .Output_LPF_RC = 0.3,     // 低通滤波系数
            },
        },
        .controller_setting_init_config = {
            .outer_loop_type = SPEED_LOOP,      // 外环为速度环
            .close_loop_type = SPEED_LOOP,       // 仅速度闭环
        },
        .motor_type = M3508,                     // 3508 电机
    };

    // 2. 逐个注册电机，并组成唯一的底盘功控组
    chassis_motor_config.can_init_config.tx_id = 1;
    motor_lf = DJIMotorInit(&chassis_motor_config);
    // ... motor_rf (tx_id=2), motor_lb (tx_id=4), motor_rb (tx_id=3)
    DJIChassisPowerRegister(&power_config);

    // 3. 裁判系统初始化
    referee_data = UITaskInit(&huart6, &ui_data);

    // 4. 缓冲能量 PID 和超级电容
    PIDInit(&buffer_PID, &Buffer_pid_conf);
    cap = SuperCapInit(&cap_conf);

    // 5. 通信方式（条件编译）
#ifdef ONE_BOARD
    chassis_sub = SubRegister("chassis_cmd", ...);
    chassis_pub = PubRegister("chassis_feed", ...);
#endif
#ifdef CHASSIS_BOARD
    Chassis_IMU_data = INS_Init();           // 底盘板有自己的 IMU
    chasiss_can_comm = CANCommInit(...);     // CAN 板间通信
#endif
}
```

**要点**：
- 底盘和其他 DJI 电机都使用 `DJIMotorInit()`；底盘四电机通过 `DJIChassisPowerRegister()` 在 PID 输出后挂接功率管理
- M3508 只用速度环，不需要角度环（底盘不关心轮子转了多少圈，只关心当前转速）
- `Output_LPF_RC = 0.3` 对速度环输出做低通滤波，减少机械冲击

### MecanumCalculate() — 麦克纳姆轮正运动学

位于 `chassis.c:151`。

```c
static void MecanumCalculate()
{
    vt_lf = -chassis_vx - chassis_vy - chassis_cmd_recv.wz * LF_CENTER;
    vt_rf = -chassis_vx + chassis_vy - chassis_cmd_recv.wz * RF_CENTER;
    vt_lb =  chassis_vx - chassis_vy - chassis_cmd_recv.wz * LB_CENTER;
    vt_rb =  chassis_vx + chassis_vy - chassis_cmd_recv.wz * RB_CENTER;
}
```

**麦克纳姆轮运动学公式解读**：

每个轮子的线速度由三部分叠加：
- `vx` 分量 — 前后运动，对角线方向的轮子符号相反（麦轮的特性）
- `vy` 分量 — 左右平移，同侧轮子符号相同
- `wz * R` 分量 — 旋转运动，每个轮子到云台中心的距离不同（考虑了云台偏移量）

注意正负号取决于麦轮辊子的安装方向（X 型还是 O 型）。

### LimitChassisOutput() — 功率限制和参考值设定

位于 `chassis.c:163`。

```c
static void LimitChassisOutput()
{
    // 功率限制待添加（通过裁判系统数据和超级电容状态）
    // ...

    // 设定电机参考值
    DJIMotorSetRef(motor_lf, vt_lf);
    DJIMotorSetRef(motor_rf, vt_rf);
    DJIMotorSetRef(motor_lb, vt_lb);
    DJIMotorSetRef(motor_rb, vt_rb);
}
```

**当前状态**：功率限制部分标注为"待添加"，暂时直接设定参考值。完整的功率控制需要结合裁判系统的 `chassis_power` 和 `chassis_power_buffer`，以及超级电容的剩余能量。

### ChassisTask() — 核心任务

位于 `chassis.c:189`。

```c
void ChassisTask()
{
    // 1. 获取控制命令
#ifdef ONE_BOARD
    SubGetMessage(chassis_sub, &chassis_cmd_recv);
#endif
#ifdef CHASSIS_BOARD
    chassis_cmd_recv = *(Chassis_Ctrl_Cmd_s *)CANCommGet(chasiss_can_comm);
#endif

    // 2. 设置功率限制
    DJIChassisPowerSetLimit((float)referee_data->GameRobotState.chassis_power_limit);

    // 3. 根据模式决定是否使能电机
    if (chassis_cmd_recv.chassis_mode == CHASSIS_ZERO_FORCE) {
        DJIMotorStop(motor_lf);  // 急停：四个电机全部断电
        DJIMotorStop(motor_rf);
        DJIMotorStop(motor_lb);
        DJIMotorStop(motor_rb);
    } else {
        DJIMotorEnable(motor_lf); // 正常：使能电机
        // ...
    }

    // 4. 根据模式计算旋转速度 wz
    switch (chassis_cmd_recv.chassis_mode) {
    case CHASSIS_NO_FOLLOW:
        chassis_cmd_recv.wz = 0;   // 不旋转
        break;
    case CHASSIS_FOLLOW_GIMBAL_YAW:
        // 以偏转角平方为速度输出，符号由绝对值保留
        chassis_cmd_recv.wz = -1.5f * chassis_cmd_recv.offset_angle
                             * abs(chassis_cmd_recv.offset_angle);
        break;
    case CHASSIS_ROTATE:
        chassis_cmd_recv.wz = 4000;  // 固定速度自旋
        break;
    }

    // 5. 坐标变换：云台系 -> 底盘系
    float cos_theta = arm_cos_f32(chassis_cmd_recv.offset_angle * DEGREE_2_RAD);
    float sin_theta = arm_sin_f32(chassis_cmd_recv.offset_angle * DEGREE_2_RAD);
    chassis_vx = chassis_cmd_recv.vx * cos_theta - chassis_cmd_recv.vy * sin_theta;
    chassis_vy = chassis_cmd_recv.vx * sin_theta + chassis_cmd_recv.vy * cos_theta;

    // 6. 运动学解算
    MecanumCalculate();

    // 7. 功率限制 + 设定参考值
    LimitChassisOutput();

    // 8. 逆运动学估算真实速度
    EstimateSpeed();

    // 9. 发布反馈
#ifdef ONE_BOARD
    PubPushMessage(chassis_pub, (void *)&chassis_feedback_data);
#endif
#ifdef CHASSIS_BOARD
    CANCommSend(chasiss_can_comm, (void *)&chassis_feedback_data);
#endif
}
```

**关键设计**：

**跟随模式的非线性控制** — `wz = -1.5f * offset_angle * |offset_angle|`，这是角度误差的平方再乘以符号。效果是：偏转角越大，旋转速度越快，但方向由符号保证正确。相比线性控制（`wz = Kp * angle`），平方控制在大角度时响应更迅速，小角度时更平滑。

**坐标变换** — 使用 ARM CMSIS-DSP 的 `arm_cos_f32` / `arm_sin_f32` 而非标准库 `cosf` / `sinf`，因为前者针对 Cortex-M4 的 FPU 做了优化。

---

## 调用链

### 初始化调用链

```
RobotInit()                                       // robot.c:23
  |
  +-- ChassisInit()                               // chassis.c:59
        |
        +-- DJIMotorInit(motor_lf/rf/lb/rb)       // 注册四个底盘电机
        |     +-- CANRegister()
        |     +-- DaemonRegister()
        +-- DJIChassisPowerRegister()             // 组成四电机功控组
        +-- UITaskInit()                          // 裁判系统
        +-- SuperCapInit()                        // 超级电容
        +-- SubRegister("chassis_cmd")            // 订阅控制命令
        +-- PubRegister("chassis_feed")           // 发布反馈
```

### 运行时控制链

```
RobotCMDTask() 发布 "chassis_cmd"
    |
    v
ChassisTask()                                   // chassis.c:189
    |
    +-- SubGetMessage("chassis_cmd")             // 获取命令
    +-- DJIChassisPowerSetLimit()                // 设置功率上限
    +-- DJIMotorStop / DJIMotorEnable            // 电机启停
    +-- wz 计算（跟随/自旋/不跟随）
    +-- 坐标变换（云台系 -> 底盘系）
    +-- MecanumCalculate()                       // 四轮速度解算
    +-- LimitChassisOutput()                     // 设置四轮速度参考值
    |     +-- DJIMotorSetRef(motor_lf, vt_lf * 6)
    |     +-- DJIMotorSetRef(motor_rf, vt_rf * 6)
    |     +-- DJIMotorSetRef(motor_lb, vt_lb * 6)
    |     +-- DJIMotorSetRef(motor_rb, vt_rb * 6)
    +-- EstimateSpeed()                          // 逆运动学估算
    +-- PubPushMessage("chassis_feed")           // 发布反馈

--- 电机 PID 计算路径 ---

MotorControlTask() (1kHz)                        // motor_task.c
  |
  +-- DJIMotorControl()                          // PID -> 底盘功控 -> 统一 CAN 发送
```

---

## 注意事项

1. **四个电机的 tx_id 不完全按顺序** — lf=1, rf=2, lb=4, rb=3。这与电调拨码开关的物理安装位置有关，不是代码错误，但容易引起困惑。

2. **电机方向标志未正确设置** — 注释提到当前 `motor_reverse_flag` 全部为 `MOTOR_DIRECTION_NORMAL`，需要根据实际安装方向调整，否则某些轮子的转向可能相反，导致底盘运动异常。

3. **功率限制尚未完整实现** — `LimitChassisOutput()` 中的功率限制标注为"待添加"。当前直接将运动学解算结果设为电机参考值，没有根据裁判系统功率反馈进行动态限幅，比赛时可能触发功率超限。

4. **自旋模式使用固定 wz=4000** — 后续应增加变速策略（如正弦变速、随机变速），避免被对手预测旋转方向。

5. **跟随模式使用平方控制而非 PID** — `wz = -1.5 * angle * |angle|` 是一种简化方案，没有积分项。如果底盘存在持续偏转力矩（如一侧摩擦力大），会有稳态误差。

6. **EstimateSpeed() 尚未实现** — 逆运动学函数体为空，当前无法获取底盘的真实速度反馈，只能使用电机编码器速度做间接估算。

7. **底盘板有独立 IMU** — 双板模式下底盘板调用 `INS_Init()` 初始化自己的 IMU，这与云台板的 IMU 是独立的。底盘 IMU 可用于更好的角速度反馈和底盘姿态检测。
