# robot_cmd — 指令处理（机器人"大脑"）

## 模块职责

robot_cmd 接收所有输入源（遥控器、视觉、键鼠）的原始数据，将其转化为定量的控制命令，分发给 gimbal、chassis、shoot 三个应用，并处理紧急停止逻辑。

---

## 设计思路

### 为什么需要一个"大脑"应用？

如果不集中处理输入，每个应用都自己读遥控器数据，会导致：
1. **重复读取** — 多个应用都解析遥控器拨杆
2. **协调困难** — 云台切到 GYRO_MODE 时，底盘必须知道以切换跟随方式
3. **紧急停止不统一** — 每个应用各自判断离线，无法确保所有执行机构同时停止

robot_cmd 作为唯一的输入入口，把"用户想做什么"翻译成"每个应用该做什么"。

### 输入到输出的映射

```
输入源                       robot_cmd 处理               输出
─────────                   ─────────────              ─────
遥控器拨杆                   增益系数 * 拨杆值          vx, vy, yaw_ref, pitch_ref
遥控器拨轮                   阈值判断                   摩擦轮开关、拨弹模式
鼠标位移                     归一化 * 增益              yaw_ref, pitch_ref 增量
键盘 W/S/A/D                 固定速度值                 vx, vy
键盘 Z/E/R/F/C               计数器取模                 弹速、拨弹模式、弹舱、摩擦轮、底盘速度
视觉数据                     （待实现）                 yaw/pitch 误差增量
遥控器拨轮急停 / 模块离线     EmergencyHandler()        所有模式设为 ZERO_FORCE / STOP
```

---

## 核心数据结构

### 私有变量（robot_cmd.c）

```c
// ---- 输入源 ----
static RC_ctrl_t *rc_data;              // 遥控器数据指针，初始化时返回
static Vision_Recv_s *vision_recv_data; // 视觉接收数据指针

// ---- 发布者和订阅者 ----
static Publisher_t *chassis_cmd_pub;    // 发布底盘控制命令
static Publisher_t *gimbal_cmd_pub;     // 发布云台控制命令
static Publisher_t *shoot_cmd_pub;      // 发布发射控制命令
static Subscriber_t *chassis_feed_sub;  // 订阅底盘反馈
static Subscriber_t *gimbal_feed_sub;   // 订阅云台反馈
static Subscriber_t *shoot_feed_sub;    // 订阅发射反馈

// ---- 发送/接收数据 ----
static Chassis_Ctrl_Cmd_s chassis_cmd_send;      // 发给底盘
static Gimbal_Ctrl_Cmd_s gimbal_cmd_send;        // 发给云台
static Shoot_Ctrl_Cmd_s shoot_cmd_send;          // 发给发射
static Chassis_Upload_Data_s chassis_fetch_data;  // 底盘反馈
static Gimbal_Upload_Data_s gimbal_fetch_data;    // 云台反馈
static Shoot_Upload_Data_s shoot_fetch_data;      // 发射反馈

// ---- 状态 ----
static Robot_Status_e robot_state;  // 机器人整体工作状态
```

### 双板通信变量（条件编译）

```c
#ifdef GIMBAL_BOARD
#include "can_comm.h"
static CANCommInstance *cmd_can_comm;  // 双板时用 CAN 通信替代消息中心
#endif
#ifdef ONE_BOARD
// 使用消息中心发布订阅
#endif
```

---

## 函数详解

### RobotCMDInit() — 初始化

位于 `robot_cmd.c:51`。

```c
void RobotCMDInit()
{
    // 1. 初始化输入源
    rc_data = RemoteControlInit(&huart3);    // 遥控器 DBUS 串口
    vision_recv_data = VisionInit(&huart1);  // 视觉通信串口

    // 2. 注册消息中心：发布控制命令
    gimbal_cmd_pub = PubRegister("gimbal_cmd", sizeof(Gimbal_Ctrl_Cmd_s));
    shoot_cmd_pub = PubRegister("shoot_cmd", sizeof(Shoot_Ctrl_Cmd_s));

    // 3. 注册消息中心：订阅反馈数据
    gimbal_feed_sub = SubRegister("gimbal_feed", sizeof(Gimbal_Upload_Data_s));
    shoot_feed_sub = SubRegister("shoot_feed", sizeof(Shoot_Upload_Data_s));

    // 4. 单板/双板切换
#ifdef ONE_BOARD
    chassis_cmd_pub = PubRegister("chassis_cmd", sizeof(Chassis_Ctrl_Cmd_s));
    chassis_feed_sub = SubRegister("chassis_feed", sizeof(Chassis_Upload_Data_s));
#endif
#ifdef GIMBAL_BOARD
    CANComm_Init_Config_s comm_conf = {
        .can_config = { .can_handle = &hcan1, .tx_id = 0x312, .rx_id = 0x311 },
        .recv_data_len = sizeof(Chassis_Upload_Data_s),
        .send_data_len = sizeof(Chassis_Ctrl_Cmd_s),
    };
    cmd_can_comm = CANCommInit(&comm_conf);
#endif

    // 5. 初始状态
    gimbal_cmd_send.pitch = 0;
    robot_state = ROBOT_READY;
}
```

**要点**：
- 发布和订阅的话题名必须与对应应用一致（如 `"gimbal_cmd"` 对应 gimbal.c 中的 `"gimbal_cmd"`）
- 双板模式下用 CANComm 替代消息中心与底盘板通信
- 遥控器串口必须选择带反相器的那个（DBUS 协议要求）

### RemoteControlSet() — 遥控器输入映射

位于 `robot_cmd.c:154`。

```c
static void RemoteControlSet()
{
    // 右侧开关控制模式
    if (switch_is_down(rc_data[TEMP].rc.switch_right)) {
        chassis_cmd_send.chassis_mode = CHASSIS_ROTATE;    // 小陀螺
        gimbal_cmd_send.gimbal_mode = GIMBAL_GYRO_MODE;   // IMU 反馈
    }
    else if (switch_is_mid(rc_data[TEMP].rc.switch_right)) {
        chassis_cmd_send.chassis_mode = CHASSIS_NO_FOLLOW; // 不跟随
        gimbal_cmd_send.gimbal_mode = GIMBAL_FREE_MODE;   // 编码器反馈
    }

    // 左侧开关：视觉 / 遥控器
    if (switch_is_mid(rc_data[TEMP].rc.switch_left)) {
        // 视觉模式（待实现）
    }
    if (switch_is_down(rc_data[TEMP].rc.switch_left) ||
        vision_recv_data->target_state == NO_TARGET) {
        // 遥控器拨杆 -> 角度增量
        gimbal_cmd_send.yaw   += 0.005f * rc_data[TEMP].rc.rocker_l_;
        gimbal_cmd_send.pitch += 0.001f * rc_data[TEMP].rc.rocker_l1;
    }

    // 底盘速度
    chassis_cmd_send.vx = 10.0f * rc_data[TEMP].rc.rocker_r_;
    chassis_cmd_send.vy = 10.0f * rc_data[TEMP].rc.rocker_r1;

    // 摩擦轮和拨弹
    if (rc_data[TEMP].rc.dial < -100)  shoot_cmd_send.friction_mode = FRICTION_ON;
    if (rc_data[TEMP].rc.dial < -500)  shoot_cmd_send.load_mode = LOAD_BURSTFIRE;
    shoot_cmd_send.shoot_rate = 8;  // 固定 8 发/秒
}
```

**关键设计**：云台控制使用**增量方式**（`yaw += ...`），而不是绝对值。这是因为云台对 total_angle 闭环，每周期叠加增量可实现平滑旋转。

### MouseKeySet() — 键鼠输入映射

位于 `robot_cmd.c:210`。

```c
static void MouseKeySet()
{
    // WASD -> 底盘速度
    chassis_cmd_send.vx = rc_data[TEMP].key[KEY_PRESS].w * 300
                        - rc_data[TEMP].key[KEY_PRESS].s * 300;
    chassis_cmd_send.vy = rc_data[TEMP].key[KEY_PRESS].a * 300
                        - rc_data[TEMP].key[KEY_PRESS].d * 300;

    // 鼠标 -> 云台角度增量
    gimbal_cmd_send.yaw   += (float)rc_data[TEMP].mouse.x / 660 * 10;
    gimbal_cmd_send.pitch += (float)rc_data[TEMP].mouse.y / 660 * 10;

    // 按键计数器取模实现循环切换
    switch (rc_data[TEMP].key_count[KEY_PRESS][Key_Z] % 3) { /* 弹速 */ }
    switch (rc_data[TEMP].key_count[KEY_PRESS][Key_E] % 4) { /* 拨弹模式 */ }
    switch (rc_data[TEMP].key_count[KEY_PRESS][Key_F] % 2) { /* 摩擦轮 */ }
    switch (rc_data[TEMP].key_count[KEY_PRESS][Key_C] % 4) { /* 底盘速度 */ }
}
```

**关键设计**：`key_count` 是按键被按下的次数计数器，取模实现循环切换（如 Z 按一次=15m/s，两次=18m/s，三次=30m/s，四次回到15m/s）。

### EmergencyHandler() — 紧急停止

位于 `robot_cmd.c:297`。

```c
static void EmergencyHandler()
{
    // 拨轮向下超过阈值 或 机器人已在急停状态
    if (rc_data[TEMP].rc.dial > 300 || robot_state == ROBOT_STOP) {
        robot_state = ROBOT_STOP;
        gimbal_cmd_send.gimbal_mode = GIMBAL_ZERO_FORCE;    // 云台断电
        chassis_cmd_send.chassis_mode = CHASSIS_ZERO_FORCE; // 底盘断电
        shoot_cmd_send.shoot_mode = SHOOT_OFF;              // 停止发射
        shoot_cmd_send.friction_mode = FRICTION_OFF;        // 关摩擦轮
        shoot_cmd_send.load_mode = LOAD_STOP;               // 停止拨弹
        LOGERROR("[CMD] emergency stop!");
    }
    // 右侧开关拨到[上]，恢复
    if (switch_is_up(rc_data[TEMP].rc.switch_right)) {
        robot_state = ROBOT_READY;
        shoot_cmd_send.shoot_mode = SHOOT_ON;
        LOGINFO("[CMD] reinstate, robot ready");
    }
}
```

### CalcOffsetAngle() — 计算云台偏转角

位于 `robot_cmd.c:128`，根据云台当前 yaw 电机单圈角度和零位编码器值，计算底盘与正前方的偏转角。

```c
static void CalcOffsetAngle()
{
    static float angle;
    angle = gimbal_fetch_data.yaw_motor_single_round_angle;

#if YAW_ECD_GREATER_THAN_4096  // 对齐值 > 180度
    if (angle > YAW_ALIGN_ANGLE && angle <= 180.0f + YAW_ALIGN_ANGLE)
        chassis_cmd_send.offset_angle = angle - YAW_ALIGN_ANGLE;
    else if (angle > 180.0f + YAW_ALIGN_ANGLE)
        chassis_cmd_send.offset_angle = angle - YAW_ALIGN_ANGLE - 360.0f;
    else
        chassis_cmd_send.offset_angle = angle - YAW_ALIGN_ANGLE;
#else  // 对齐值 <= 180度
    if (angle > YAW_ALIGN_ANGLE)
        chassis_cmd_send.offset_angle = angle - YAW_ALIGN_ANGLE;
    else if (angle <= YAW_ALIGN_ANGLE && angle >= YAW_ALIGN_ANGLE - 180.0f)
        chassis_cmd_send.offset_angle = angle - YAW_ALIGN_ANGLE;
    else
        chassis_cmd_send.offset_angle = angle - YAW_ALIGN_ANGLE + 360.0f;
#endif
}
```

**为什么分两种情况？** 单圈角度范围 0~360 度，跨越 0/360 边界时需要特殊处理，否则会得到 350 度的误差而非 10 度的误差。`YAW_ECD_GREATER_THAN_4096` 决定了零位在圆周上的位置，影响边界判断逻辑。

### RobotCMDTask() — 核心任务

位于 `robot_cmd.c:320`，以 200Hz 频率运行。

```c
void RobotCMDTask()
{
    // 1. 获取各应用的反馈数据
#ifdef ONE_BOARD
    SubGetMessage(chassis_feed_sub, &chassis_fetch_data);
#endif
#ifdef GIMBAL_BOARD
    chassis_fetch_data = *(Chassis_Upload_Data_s *)CANCommGet(cmd_can_comm);
#endif
    SubGetMessage(shoot_feed_sub, &shoot_fetch_data);
    SubGetMessage(gimbal_feed_sub, &gimbal_fetch_data);

    // 2. 计算偏转角
    CalcOffsetAngle();

    // 3. 根据输入源选择控制模式
    if (switch_is_down(rc_data[TEMP].rc.switch_left))
        RemoteControlSet();     // 遥控器
    else if (switch_is_up(rc_data[TEMP].rc.switch_left))
        MouseKeySet();          // 键鼠

    // 4. 紧急情况处理
    EmergencyHandler();

    // 5. 发布控制命令
#ifdef ONE_BOARD
    PubPushMessage(chassis_cmd_pub, &chassis_cmd_send);
#endif
#ifdef GIMBAL_BOARD
    CANCommSend(cmd_can_comm, &chassis_cmd_send);
#endif
    PubPushMessage(shoot_cmd_pub, &shoot_cmd_send);
    PubPushMessage(gimbal_cmd_pub, &gimbal_cmd_send);
}
```

---

## 调用链

```
StartROBOTTASK (200Hz)                      // robot_task.h:111
  |
  +-- RobotTask()                            // robot.c:48
        +-- RobotCMDTask()                   // robot_cmd.c:320
              |
              +-- SubGetMessage()            // 获取反馈
              |     +-- chassis_feed
              |     +-- gimbal_feed
              |     +-- shoot_feed
              |
              +-- CalcOffsetAngle()          // 计算偏转角
              |
              +-- RemoteControlSet() 或 MouseKeySet()  // 输入映射
              |     |
              |     +-- 读取 rc_data / vision_recv_data
              |     +-- 设置 chassis_cmd_send / gimbal_cmd_send / shoot_cmd_send
              |
              +-- EmergencyHandler()         // 急停检查
              |
              +-- PubPushMessage()           // 发布命令
                    +-- "chassis_cmd" -> ChassisTask 订阅
                    +-- "gimbal_cmd"  -> GimbalTask 订阅
                    +-- "shoot_cmd"   -> ShootTask 订阅
```

---

## 注意事项

1. **任务频率必须高于视觉发送频率** — 注释要求 200Hz 以上，否则视觉数据可能积压。

2. **紧急停止的阈值 `300` 需根据实际调整** — 遥控器拨轮范围 0~665，当前以 300 为急停阈值，可能误触发。

3. **`TEMP` 宏** — `rc_data[TEMP]` 取的是当前帧的遥控器数据，框架内部维护了一个缓冲区防止数据竞争。

4. **增量控制可能导致累积漂移** — 云台 `yaw += ...` 方式在长时间运行后可能因浮点精度产生偏移，但实际影响可忽略。

5. **视觉模式尚未实现** — `RemoteControlSet()` 中 `switch_is_mid` 分支为空，视觉自动瞄准功能预留但未完成。

6. **双板通信的 tx_id/rx_id 方向** — 云台板发送 0x312 / 接收 0x311，底盘板发送 0x311 / 接收 0x312，两者交叉配对。

7. **缺少模块离线检测** — `EmergencyHandler()` 当前仅检查拨轮急停，注释提到后续需添加重要模块离线的判断（通过 daemon 机制）。
