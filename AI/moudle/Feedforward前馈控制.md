# Feedfoward_Type_e — 前馈控制标志

> `modules/motor/motor_def.h:37-43`（注意代码中有拼写错误：应为 Feed**for**ward）

---

## 1. 定义

```c
typedef enum
{
    FEEDFORWARD_NONE             = 0b00,  // 无前馈
    CURRENT_FEEDFORWARD          = 0b01,  // 电流前馈
    SPEED_FEEDFORWARD            = 0b10,  // 速度前馈
    CURRENT_AND_SPEED_FEEDFORWARD = CURRENT_FEEDFORWARD | SPEED_FEEDFORWARD,  // 两者都有
} Feedfoward_Type_e;
```

用位掩码编码，可以同时启用多种前馈：

| 值 | 含义 |
|:---:|------|
| `0b00` | 纯 PID 反馈控制，无前馈 |
| `0b01` | 仅电流前馈 |
| `0b10` | 仅速度前馈 |
| `0b11` | 电流 + 速度前馈 |

---

## 2. 前馈是什么

反馈 vs 前馈的区别：

```
反馈控制 (PID):
  目标 → [PID] → 输出 → [电机] → 实际值
            ↑                    │
            └─── 传感器反馈 ──────┘
  // 必须等误差出现后才开始纠正 — 永远慢一拍

前馈控制:
  目标 → [前馈模型] → + → 输出 → [电机] → 实际值
                       ↑
  目标 → [PID 反馈] →──┘
  // 前馈根据物理模型提前算出需要的输出 — 不等误差出现
```

**前馈的本质**：根据期望值直接计算出一个预估的输出量，绕过反馈延迟，让响应更快。

---

## 3. 在电机控制中的位置

`DJIMotorControl()` 中的串级 PID 流程（`dji_motor.c:251-287`）：

```
Application 设 pid_ref (目标角度)
        │
        ▼
   角度环 PID  ← 位置反馈
        │
        ▼  pid_ref = 目标角速度
   速度环 PID  ← 速度反馈
        │          ↑
        │          │ ★ SPEED_FEEDFORWARD 在这里加：
        │          │   pid_ref += *motor->speed_feedforward_ptr
        │          │   (预判：要达到这个角速度，需要多给一点电流)
        ▼
       + ─────── 速度前馈值
        │
        ▼  pid_ref = 目标电流
   电流环 PID  ← 电流反馈
        │          ↑
        │          │ ★ CURRENT_FEEDFORWARD 在这里加：
        │          │   pid_ref += *motor->current_feedforward_ptr
        │          │   (预判：补偿重力/摩擦力矩)
        ▼
       + ─────── 电流前馈值
        │
        ▼
   int16_t → CAN 发送给电机
```

源码：

```c
// 速度环的 pid_ref 计算完之后，额外加上速度前馈
if (motor_setting->feedforward_flag & SPEED_FEEDFORWARD)
    pid_ref += *motor_controller->speed_feedforward_ptr;

// 电流环计算前，额外加上电流前馈
if (motor_setting->feedforward_flag & CURRENT_FEEDFORWARD)
    pid_ref += *motor_controller->current_feedforward_ptr;
```

---

## 4. 具体应用场景

### 重力前馈（电流前馈的典型应用）

Pitch 轴云台在非水平位置时受重力矩影响：

```c
// 根据 IMU 的 Roll 和 Pitch 计算需要补偿的力矩
float gravity_torque = mass * g * arm_length * sin(pitch_angle);

// 把计算出的重力力矩作为电流前馈
*speed_feedforward_ptr = gravity_torque;
// → 在 DJIMotorControl 中，pid_ref 自动加上这个值
// → 电机不等 PID 反应过来就直接抵消重力
```

### 速度前馈

知道电机在某个目标角速度下大概需要多少电流：

```c
// 目标转速 100°/s，查表/计算得出稳态电流约 500mA
*speed_feedforward_ptr = 500;
// → 速度环输出直接加 500，减少速度环的跟踪误差
```

---

## 5. 指针机制 — 零拷贝设计

前馈值不存储在电机内部，而是通过**外部指针**传入：

```c
typedef struct {
    float *speed_feedforward_ptr;    // 指向外部变量
    float *current_feedforward_ptr;  // 指向外部变量
    // ...
} Motor_Controller_s;
```

初始化时：

```c
float my_speed_ff = 0;
float my_current_ff = 0;

Motor_Init_Config_s config = {
    .controller_param_init_config = {
        .speed_feedforward_ptr   = &my_speed_ff,    // 指向外部变量
        .current_feedforward_ptr = &my_current_ff,
    },
};
```

之后 Gravity 补偿模块只需修改 `my_current_ff` 的值，电机 1kHz PID 计算时自动通过指针读到最新值——**零拷贝、零函数调用**。

---

## 6. 预留在代码中

`motor_def.h:91` 的注释也说明这是预留接口：

```c
float *speed_feedforward_ptr;    // 速度前馈数据指针
float *current_feedforward_ptr;  // 电流前馈数据指针
```

以及 `dji_motor.c:176`：

```c
// 后续增加电机前馈控制器(速度和电流)
```

实际上当前代码框架已支持，但 gravity compensation 等模块尚未接入，属于**架构层面的预留能力**。
