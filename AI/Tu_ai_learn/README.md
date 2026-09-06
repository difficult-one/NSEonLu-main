# 跃鹿战队 Basic Framework 学习笔记

> 逐函数级别讲解，面向有嵌入式经验但本框架新手的开发者。

## 框架核心思想

一句话概括：**BSP 封装硬件 → Module 封装设备 → Application 编写逻辑**，三层解耦，应用层完全不碰硬件。

```
┌─────────────────────────────────────────────┐
│  Application 层                              │  机器人控制逻辑
│  chassis / gimbal / shoot / robot_cmd        │  只用 Module 接口
├─────────────────────────────────────────────┤
│  Module 层                                   │  设备封装
│  motor / imu / PID / message_center / ...    │  硬件无关接口
├─────────────────────────────────────────────┤
│  BSP 层                                      │  硬件抽象
│  CAN / USART / IIC / SPI / GPIO / PWM / ...  │  唯一允许用 HAL 的层
├─────────────────────────────────────────────┤
│  STM32 HAL + FreeRTOS                        │  平台层
└─────────────────────────────────────────────┘
```

## 两个核心设计模式

### 1. 实例注册制

每个外设/模块不是"初始化一次全局用"，而是"谁用谁注册，注册了才存在"：

```c
// Module 层想用 CAN？注册一个实例
CANInstance *can = CANRegister(&config);  // BSP 层自动完成初始化
```

### 2. 回调分发

中断来了，BSP 层遍历所有注册的实例，找到匹配的那个，调用它的回调：

```c
CAN中断 → HAL回调 → 遍历 can_instance[] → 匹配 rx_id → 调用 module_callback()
```

## 学习路线

建议按以下顺序阅读：

### 第一阶段：理解基础设施

| 顺序 | 笔记 | 为什么先学 |
|------|------|------------|
| 1 | [01-bsp/00-bsp-overview.md](01-bsp/00-bsp-overview.md) | 总览分层思想和实例注册模式 |
| 2 | [01-bsp/01-bsp-log.md](01-bsp/01-bsp-log.md) | 日志系统，调试必备 |
| 3 | [01-bsp/02-bsp-dwt.md](01-bsp/02-bsp-dwt.md) | 定时器，理解为什么不用 HAL_Delay |

### 第二阶段：理解通信机制

| 顺序 | 笔记 | 核心问题 |
|------|------|----------|
| 4 | [01-bsp/03-bsp-can.md](01-bsp/03-bsp-can.md) | CAN 是整个框架最核心的通信方式 |
| 5 | [01-bsp/04-bsp-usart.md](01-bsp/04-bsp-usart.md) | UART + DMA + IDLE 接收 |
| 6 | [01-bsp/05-bsp-iic.md](01-bsp/05-bsp-iic.md) | I2C 多模式通信 |
| 7 | [01-bsp/06-bsp-gpio.md](01-bsp/06-bsp-gpio.md) | GPIO + EXTI 中断分发 |
| 8 | [01-bsp/07-bsp-pwm.md](01-bsp/07-bsp-pwm.md) | PWM 输出控制 |

### 第三阶段：理解 Module 层

| 顺序 | 笔记 | 核心问题 |
|------|------|----------|
| 9 | [02-module/00-module-overview.md](02-module/00-module-overview.md) | OOP-in-C 设计模式 |
| 10 | [02-module/01-message-center.md](02-module/01-message-center.md) | 发布-订阅，应用间通信 |
| 11 | [02-module/02-motor.md](02-module/02-motor.md) | 电机控制全家族 |
| 12 | [02-module/03-imu.md](02-module/03-imu.md) | IMU 数据读取 |
| 13 | [02-module/04-algorithm-pid.md](02-module/04-algorithm-pid.md) | PID 控制器 |
| 14 | [02-module/05-algorithm-filter.md](02-module/05-algorithm-filter.md) | 滤波器 |

### 第四阶段：理解 Application 层

| 顺序 | 笔记 | 核心问题 |
|------|------|----------|
| 15 | [03-application/00-app-overview.md](03-application/00-app-overview.md) | 初始化流程和任务架构 |
| 16 | [03-application/01-robot-def.md](03-application/01-robot-def.md) | 机器人配置文件 |
| 17 | [03-application/02-robot-cmd.md](03-application/02-robot-cmd.md) | 指令处理，机器人的"大脑" |
| 18 | [03-application/03-gimbal.md](03-application/03-gimbal.md) | 云台控制 |
| 19 | [03-application/04-chassis.md](03-application/04-chassis.md) | 底盘控制 |
| 20 | [03-application/05-shoot.md](03-application/05-shoot.md) | 发射控制 |

### 第五阶段：进阶

| 顺序 | 笔记 | 核心问题 |
|------|------|----------|
| 21 | [04-advanced/01-dual-board.md](04-advanced/01-dual-board.md) | 双板通信 |
| 22 | [04-advanced/02-oop-in-c.md](04-advanced/02-oop-in-c.md) | C 语言面向对象深入 |
| 23 | [04-advanced/03-dataflow.md](04-advanced/03-dataflow.md) | 完整数据流分析 |

## 源码位置

```
basic_framework/
├── bsp/          ← BSP 层源码
├── modules/      ← Module 层源码
├── application/  ← Application 层源码
├── Src/          ← CubeMX 生成的外设初始化
├── Inc/          ← CubeMX 生成的头文件
├── Drivers/      ← HAL 驱动
└── Middlewares/  ← FreeRTOS, SEGGER RTT 等
```
