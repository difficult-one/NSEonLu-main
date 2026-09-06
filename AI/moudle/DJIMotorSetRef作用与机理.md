# DJIMotorSetRef 作用与底层机理

> `modules/motor/DJImotor/dji_motor.c:227-230`

---

## 1. 代码本身（极简）

```c
void DJIMotorSetRef(DJIMotorInstance *motor, float ref)
{
    motor->motor_controller.pid_ref = ref;
}
```

就一行赋值。但这一行是 **Application 层和电机控制层之间的唯一数据接口**。

---

## 2. 架构定位

```
Application 层（chassis / gimbal / shoot）
        │
        │  DJIMotorSetRef(motor, target_value)    ← 设定期望值
        ▼
┌─────────────────────────────┐
│  motor->motor_controller    │
│  .pid_ref  = target_value   │  ← 存起来
└─────────────────────────────┘
        │
        │  DJIMotorControl()                       ← 1kHz 定时调用
        ▼
┌─────────────────────────────┐
│  串级 PID 控制计算           │
│  pid_ref → Angle → Speed → Current → CAN TX     │
└─────────────────────────────┘
```

Application 层只关心"我要电机转到什么位置/什么速度/什么扭矩"，完全不需要知道底层有几个 PID 环、CAN 报文怎么组包——这就是 `DJIMotorSetRef` 封装的意义。

---

## 3. 底层机理：`pid_ref` 作为数据载体

`Motor_Controller_s`（`motor_def.h:86-98`）：

```c
typedef struct
{
    float *other_angle_feedback_ptr;   // 外部角度反馈来源
    float *other_speed_feedback_ptr;   // 外部速度反馈来源
    float *speed_feedforward_ptr;      // 速度前馈
    float *current_feedforward_ptr;    // 电流前馈

    PIDInstance current_PID;           // 电流环 PID
    PIDInstance speed_PID;             // 速度环 PID
    PIDInstance angle_PID;             // 角度环 PID

    float pid_ref;  // ★ 这是 DJIMotorSetRef 写入的变量 ★
} Motor_Controller_s;
```

`DJIMotorControl()` 中，`pid_ref` 顺次流过每个被启用的闭环（`dji_motor.c:233-312`）：

```
                     ┌──────────┐
DJIMotorSetRef ──→  │ pid_ref  │  (Application 写入的目标值)
                     └────┬─────┘
                          │
    ┌─────────────────────┼─────────────────────┐
    │  根据 outer_loop_type 决定从哪个环开始     │
    └─────────────────────┼─────────────────────┘
                          ▼
              ┌─────────────────────┐
              │  角度环 (最外层)      │  ← 仅当 outer_loop_type == ANGLE_LOOP
              │  pid_ref = PID(      │
              │    measure: 总角度   │
              │    ref:    目标角度  │
              │  )                   │
              │  输出 → 目标角速度    │
              └─────────┬───────────┘
                        ▼  (pid_ref 现在是角速度目标值)
              ┌─────────────────────┐
              │  速度环              │  ← 当 outer_loop_type 含 ANGLE/SPEED
              │  pid_ref = PID(      │
              │    measure: 角速度   │
              │    ref:    目标角速度│
              │  )                   │
              │  + 速度前馈          │
              │  输出 → 目标电流     │
              └─────────┬───────────┘
                        ▼  (pid_ref 现在是电流目标值)
              ┌─────────────────────┐
              │  电流环 (最内层)     │  ← 只要启用了电流环就算
              │  pid_ref = PID(      │
              │    measure: 实际电流 │
              │    ref:    目标电流  │
              │  )                   │
              │  + 电流前馈          │
              │  输出 → 最终扭矩值   │
              └─────────┬───────────┘
                        ▼
              ┌─────────────────────┐
              │  set = (int16_t)    │
              │  填入 CAN tx_buff   │  → 发送到电机
              └─────────────────────┘
```

关键源码（`dji_motor.c:251`）：

```c
pid_ref = motor_controller->pid_ref;  // ① 取出 Application 设的目标值

// ② 顺次流过三个环，pid_ref 不断被重写
if (角度环启用 && 外层是角度)
    pid_ref = PIDCalculate(&angle_PID,   实际角度, pid_ref);   // 输出 → 目标速度

if (速度环启用)
    pid_ref = PIDCalculate(&speed_PID,   实际速度, pid_ref);   // 输出 → 目标电流

if (电流环启用)
    pid_ref = PIDCalculate(&current_PID, 实际电流, pid_ref);   // 输出 → 最终扭矩

set = (int16_t)pid_ref;  // ③ 转成 CAN 报文格式
sender_assignment[group].tx_buff[2*num] = set >> 8;     // 填入 CAN 帧
sender_assignment[group].tx_buff[2*num+1] = set & 0xff;
```

---

## 4. 调用示例

### 位置控制（角度环）

```c
// chassis.c — 麦轮底盘控制，以 200Hz 调用
DJIMotorSetRef(motor_lf, target_speed);  // 直接给速度目标值
// 因为底盘电机配置的是 SPEED_LOOP，pid_ref 直接进入速度环
```

### 速度控制（速度环）

```c
// gimbal.c — 云台 Yaw 轴，双环控制
DJIMotorSetRef(yaw_motor, target_angle);  // 给角度目标值
// 因为云台电机配置的是 ANGLE_AND_SPEED_LOOP，pid_ref 先过角度环再过速度环
```

### 扭矩控制（电流环）

```c
// shoot.c — 摩擦轮，电流控制
DJIMotorSetRef(friction_motor, target_current);  // 给电流目标值
// 只开 CURRENT_LOOP，pid_ref 直达电流环
```

---

## 5. 一句话总结

| 概念 | 说明 |
|------|------|
| **作用** | Application 层设定期望值（角度/速度/电流）的唯一入口 |
| **机理** | 将值写入 `motor->motor_controller.pid_ref`，由 1kHz 的 `DJIMotorControl()` 取出，顺次流过启用的串级 PID 环，最终转换为 CAN 总线上的电流指令发给电机 |
| **设计意图** | 让 Application 把电机当作"传递函数为 1 的黑盒"——给多少值，电机就去执行，不需要关心内部几个环、PID 参数多少、CAN 协议格式 |
