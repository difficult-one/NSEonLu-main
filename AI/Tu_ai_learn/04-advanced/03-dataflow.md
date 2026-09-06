# 完整数据流分析

## 模块职责

本篇用 ASCII 图表绘制框架从上电到运行的全过程数据流，包括启动序列、CAN 接收路径、消息中心传递、电机控制输出，帮助读者建立系统级的全局视角。

---

## 设计思路

理解一个嵌入式框架，最有效的方式是追踪数据从产生到消费的完整路径。本篇从三个维度绘制数据流：

1. **时间维度** — 从上电到各任务稳定运行的启动序列
2. **硬件到软件维度** — CAN 中断如何逐层传递到应用逻辑
3. **应用间维度** — message_center 如何连接各个应用

---

## 核心数据结构

本篇不引入新的数据结构，而是将前面各篇的数据结构在数据流图中串联起来。

---

## 函数详解

### 1. 启动序列

从上电到 FreeRTOS 调度器接管控制权的完整流程：

```
+---------------------+
|     上电 / 复位      |
+----------+----------+
           |
           v
+----------+----------+
|  startup_stm32f4xx.s |  向量表初始化，设置 SP 和 PC
|  Reset_Handler()     |  跳转到 SystemInit() + main()
+----------+----------+
           |
           v
+----------+----------+
|     main()           |  Src/main.c:78
+----------+----------+
           |
           +-- HAL_Init()                     HAL 库初始化
           +-- SystemClock_Config()           配置 168MHz 主频
           +-- MX_GPIO_Init()                 GPIO 初始化
           +-- MX_DMA_Init()                  DMA 初始化
           +-- MX_CAN1_Init()                 CAN1 硬件初始化（CubeMX 生成）
           +-- MX_CAN2_Init()                 CAN2 硬件初始化
           +-- MX_SPI1_Init()                 SPI 初始化（IMU 用）
           +-- MX_USART1_UART_Init()          串口初始化
           +-- MX_USART3_UART_Init()          DBUS 串口
           +-- MX_TIMx_Init()                 定时器初始化
           +-- ...其他外设初始化
           |
           v
+----------+----------+
|   RobotInit()        |  robot.c:23
|   [全局中断已关闭]    |
+----------+----------+
           |
           +-- BSPInit()                      bsp_init.h:16
           |     +-- DWT_Init(168)            高精度计时器，主频参数=168
           |     +-- BSPLogInit()             SEGGER RTT 日志初始化
           |
           +-- RobotCMDInit()                 robot_cmd.c:51
           |     +-- RemoteControlInit()       注册 DBUS 串口接收
           |     +-- VisionInit()              注册视觉串口
           |     +-- PubRegister("gimbal_cmd")  发布云台命令
           |     +-- SubRegister("gimbal_feed")  订阅云台反馈
           |     +-- PubRegister("shoot_cmd")
           |     +-- SubRegister("shoot_feed")
           |     +-- PubRegister("chassis_cmd") [单板]
           |     +-- SubRegister("chassis_feed") [单板]
           |     +-- CANCommInit()              [双板-云台板]
           |
           +-- ChassisInit()                  chassis.c:59
           |     +-- PowerControlInit() x4     四个 M3508 电机
           |     |     +-- DJIMotorInit()
           |     |           +-- MotorSenderGrouping()
           |     |           +-- CANRegister()        <-- 首次注册触发 CANServiceInit()
           |     |           +-- DaemonRegister()
           |     +-- UITaskInit()              裁判系统
           |     +-- SuperCapInit()            超级电容
           |     +-- SubRegister("chassis_cmd") [单板]
           |     +-- PubRegister("chassis_feed") [单板]
           |     +-- CANCommInit()              [双板-底盘板]
           |
           +-- OSTaskInit()                   robot_task.h:36
           |     +-- osThreadCreate(INSTASK)     优先级 AboveNormal, 1024 字节栈
           |     +-- osThreadCreate(MOTORTASK)   优先级 Normal, 256 字节栈
           |     +-- osThreadCreate(DAEMONTASK)  优先级 Normal, 128 字节栈
           |     +-- osThreadCreate(ROBOTTASK)   优先级 Normal, 1024 字节栈
           |     +-- osThreadCreate(UITASK)      优先级 Normal, 512 字节栈
           |     +-- HTMotorControlInit()
           |
           +-- __enable_irq()                 开启全局中断
           |
           v
+----------+----------+
| MX_FREERTOS_Init()   |  FreeRTOS 内部初始化
+----------+----------+
           |
           v
+----------+----------+
| osKernelStart()      |  启动调度器，此后不再返回
+---------------------+
```

### 2. CAN 接收数据流

从硬件中断到应用层数据更新的完整路径：

```
                        硬件层
+-----------------------------------------------------------+
| CAN 总线信号 -> bxCAN 控制器 -> FIFO 溢出 -> NVIC 中断     |
+-----------------------------------------------------------+
           |
           v
                        BSP 层 (bsp_can.c)
+-----------------------------------------------------------+
| HAL_CAN_RxFifo0MsgPendingCallback(hcan)                   |
|   or                                                      |
| HAL_CAN_RxFifo1MsgPendingCallback(hcan)                   |
|                                                           |
| CANFIFOxCallback(_hcan, fifox)                            |
|   |                                                       |
|   +-- HAL_CAN_GetRxMessage() 取出报文                     |
|   +-- 遍历 can_instance[0..idx-1]                         |
|   |     匹配: hcan == can_instance[i]->can_handle         |
|   |           && StdId == can_instance[i]->rx_id          |
|   |                                                       |
|   +-- 匹配成功:                                           |
|         memcpy(rx_buff, can_rx_buff, DLC)                  |
|         can_instance[i]->can_module_callback(can_instance[i])
+-----------------------------------------------------------+
           |
           v
                    Module 层（多态分发）
+-----------------------------------------------------------+
|                                                           |
| [情况A] DJI 电机反馈                                      |
| DecodeDJIMotor(_instance)                                 |
|   +-- motor = (DJIMotorInstance*)_instance->id             |
|   +-- 解析编码器值、速度、电流、温度                        |
|   +-- 计算多圈角度 total_angle                             |
|   +-- DaemonReload(motor->daemon)                          |
|                                                           |
| [情况B] CAN 板间通信                                      |
| CANCommRxCallback(_instance)                              |
|   +-- comm = (CANCommInstance*)_instance->id               |
|   +-- 状态机拼包                                          |
|   +-- CRC8 校验                                           |
|   +-- memcpy(unpacked_recv_data)                           |
|   +-- update_flag = 1                                     |
|   +-- DaemonReload(comm->comm_daemon)                      |
|                                                           |
+-----------------------------------------------------------+
           |
           v
                    APP 层（在下次任务周期读取）
+-----------------------------------------------------------+
| ChassisTask():                                            |
|   +-- motor_lf->measure.speed_aps   (电机反馈被 PID 使用)  |
|                                                           |
| RobotCMDTask():                                           |
|   +-- CANCommGet(cmd_can_comm)      (板间通信数据)         |
|   +-- gimbal_fetch_data.yaw_motor_single_round_angle       |
+-----------------------------------------------------------+
```

### 3. 电机控制输出数据流

从应用层设定参考值到 CAN 发送控制报文的路径：

```
                    APP 层
+-----------------------------------------------------------+
| ChassisTask()                                             |
|   +-- DJIMotorSetRef(motor_lf, vt_lf)                     |
|         motor_lf->motor_controller.pid_ref = vt_lf        |
|                                                           |
| GimbalTask()                                              |
|   +-- DJIMotorSetRef(yaw_motor, yaw_ref)                  |
|         yaw_motor->motor_controller.pid_ref = yaw_ref     |
+-----------------------------------------------------------+
           |
           v
                    Module 层 (dji_motor.c)
+-----------------------------------------------------------+
| MotorControlTask()  [1kHz FreeRTOS 任务]                   |
|   |                                                       |
|   +-- DJIMotorControl()                                   |
|         遍历 dji_motor_instance[0..idx-1]                  |
|         |                                                 |
|         +-- 对每个 motor:                                  |
|         |     |                                           |
|         |     +-- 角度环 PID (若启用且外环为 ANGLE_LOOP)   |
|         |     |     measure = *other_angle_feedback_ptr   |
|         |     |           或 measure.total_angle          |
|         |     |     pid_ref = PIDCalculate(angle_PID)     |
|         |     |                                           |
|         |     +-- 速度环 PID (若启用)                      |
|         |     |     measure = *other_speed_feedback_ptr   |
|         |     |           或 measure.speed_aps            |
|         |     |     pid_ref = PIDCalculate(speed_PID)     |
|         |     |                                           |
|         |     +-- 电流环 PID (若启用)                      |
|         |     |     pid_ref = PIDCalculate(current_PID)   |
|         |     |                                           |
|         |     +-- 填入发送缓冲区                           |
|         |           sender_assignment[group].tx_buff[...]  |
|         |                                                 |
|         +-- 遍历 6 个 sender_assignment                    |
|               if (sender_enable_flag[i])                   |
|                   CANTransmit(&sender_assignment[i], 1)    |
+-----------------------------------------------------------+
           |
           v
                    BSP 层 (bsp_can.c)
+-----------------------------------------------------------+
| CANTransmit(_instance, timeout)                            |
|   |                                                       |
|   +-- 等待 CAN 邮箱空闲                                    |
|   +-- HAL_CAN_AddTxMessage(can_handle, txconf,             |
|                            tx_buff, &tx_mailbox)           |
+-----------------------------------------------------------+
           |
           v
                    硬件
+-----------------------------------------------------------+
| bxCAN 控制器 -> CAN 总线 -> 电调/电机                      |
+-----------------------------------------------------------+
```

### 4. 消息中心数据流

应用之间通过发布-订阅传递数据：

```
                    robot_cmd (发布者)
+-----------------------------------------------------------+
| RobotCMDTask()                                            |
|                                                           |
|   +-- PubPushMessage(gimbal_cmd_pub, &gimbal_cmd_send)   |
|   |                                                       |
|   |   PubPushMessage 内部:                                 |
|   |     iter = pub->first_subs;                            |
|   |     while (iter) {                                     |
|   |       memcpy(iter->queue[back_idx], data, data_len);   |
|   |       iter = iter->next_subs_queue;                    |
|   |     }                                                  |
|   |                                                       |
|   +-- PubPushMessage(chassis_cmd_pub, &chassis_cmd_send) |
|   +-- PubPushMessage(shoot_cmd_pub, &shoot_cmd_send)     |
+-----------------------------------------------------------+
           |                    |                |
           | "gimbal_cmd"      | "chassis_cmd"  | "shoot_cmd"
           v                    v                v
+------------------+  +------------------+  +------------------+
| GimbalTask()     |  | ChassisTask()    |  | ShootTask()      |
|                  |  |                  |  |                  |
| SubGetMessage    |  | SubGetMessage    |  | SubGetMessage    |
|  (gimbal_sub,    |  |  (chassis_sub,   |  |  (shoot_sub,     |
|   &cmd_recv)     |  |   &cmd_recv)     |  |   &cmd_recv)     |
|                  |  |                  |  |                  |
| 内部:             |  |                  |  |                  |
| memcpy(data_ptr, |  |                  |  |                  |
|   queue[front],  |  |                  |  |                  |
|   data_len)      |  |                  |  |                  |
+------------------+  +------------------+  +------------------+
           |                    |                |
           v                    v                v
     反馈发布              反馈发布           反馈发布
           |                    |                |
           +--------------------+----------------+
           | "gimbal_feed"  | "chassis_feed" | "shoot_feed"
           v                v                v
+-----------------------------------------------------------+
|                    robot_cmd (订阅者)                       |
| RobotCMDTask()                                            |
|   +-- SubGetMessage(gimbal_feed_sub, &gimbal_fetch_data)  |
|   +-- SubGetMessage(chassis_feed_sub, &chassis_fetch_data)|
|   +-- SubGetMessage(shoot_feed_sub, &shoot_fetch_data)    |
+-----------------------------------------------------------+
```

### 5. 完整控制回路（闭环视角）

将所有数据流串联，形成一个完整的闭环控制回路：

```
            +---------- 传感器反馈 ----------+
            |                                |
            v                                |
+-------------------+              +--------------------+
|   IMU (BMI088)    |              |  电机编码器 (DJI)  |
|   SPI 1kHz        |              |  CAN 反馈          |
+--------+----------+              +--------+-----------+
         |                                  |
         v                                  v
+--------+----------+              +--------+-----------+
| INS_Task()        |              | DecodeDJIMotor()   |
| 姿态解算           |              | 解码+计算角度       |
| 1kHz              |              | CAN 中断中          |
+--------+----------+              +--------+-----------+
         |                                  |
         | attitude_t *                     | DJI_Motor_Measure_s
         v                                  v
+--------+--------------------------------------------+
|                    APP 层任务                          |
|                                                      |
| RobotCMDTask (200Hz):                                |
|   读遥控器/视觉 -> 设置 cmd_send                       |
|   CalcOffsetAngle() -> chassis_cmd_send.offset_angle |
|   PubPushMessage("gimbal_cmd/cmd")                   |
|                                                      |
| GimbalTask (200Hz):                                  |
|   SubGetMessage("gimbal_cmd")                        |
|   DJIMotorSetRef(yaw, yaw_ref)                       |
|   DJIMotorSetRef(pitch, pitch_ref)                   |
|                                                      |
| ChassisTask (200Hz):                                 |
|   SubGetMessage("chassis_cmd")                       |
|   坐标变换 + 运动学解算                                |
|   DJIMotorSetRef(motor_lf/rf/lb/rb, speed)           |
+--------+--------------------------------------------+
         |
         | pid_ref = ...
         v
+--------+--------------------------------------------+
|              MotorControlTask (1kHz)                  |
|                                                      |
| DJIMotorControl():                                   |
|   for each motor:                                    |
|     角度环 PID -> 速度环 PID -> 电流环 PID            |
|     输出填入 sender_assignment[group].tx_buff[]       |
|                                                      |
|   for each group:                                    |
|     CANTransmit()                                    |
+--------+--------------------------------------------+
         |
         | CAN 控制报文
         v
+--------+--------------------------------------------+
|              硬件执行                                  |
|                                                      |
| 电调接收电流指令 -> 驱动电机 -> 机械运动                |
|                                                      |
| 电机转动 -> 编码器变化 -> 反馈报文 -> CAN 中断         |
| IMU 运动 -> 加速度计/陀螺仪 -> SPI DMA -> INS 解算   |
+--------+--------------------------------------------+
         |                                  |
         +---------- 反馈回传感器 ----------+
```

---

## 调用链

### 关键时序约束

```
任务调度关系:

INS Task (1kHz)        优先级: AboveNormal
    |  提供 IMU 数据给 GimbalTask
    v
Motor Task (1kHz)      优先级: Normal
    |  读取 APP 层设定的 pid_ref，计算 PID，发送 CAN
    v
Robot Task (200Hz)     优先级: Normal
    |  RobotCMDTask -> GimbalTask -> ChassisTask -> ShootTask
    |  设定 pid_ref 供 Motor Task 下一周期使用
    v
Daemon Task (100Hz)    优先级: Normal
    |  检测各模块离线状态
    v
UI Task                优先级: Normal
       裁判系统 UI 刷新

时序关系:
  INS Task 每毫秒运行一次，提供最新的 IMU 数据
  Robot Task 每 5ms 运行一次，设定控制目标
  Motor Task 每毫秒运行一次，执行 PID 闭环
  一个完整的控制周期: Robot Task 设定 ref -> Motor Task 计算 PID -> CAN 发送
  延迟: 最大 1ms（Motor Task 周期）+ CAN 传输时间
```

---

## 注意事项

1. **控制回路存在一拍延迟** — APP 层设定 `pid_ref` 后，Motor Task 在下一个 1ms 周期才会计算 PID 并发送。这是离散控制系统的固有延迟，对于 1kHz 的电机控制影响可忽略。

2. **INS Task 优先级最高** — 因为姿态解算以阻塞方式读取 SPI 传感器，如果被低优先级任务抢占，可能导致数据丢失。但这也意味着 INS Task 执行时间不能超过 1ms。

3. **Motor Task 和 Robot Task 优先级相同** — FreeRTOS 同优先级任务按时间片轮转。Robot Task 的执行时间如果超过 5ms，会挤压 Motor Task 的执行时间，导致电机控制频率下降。

4. **CAN 中断可以打断任何任务** — `DecodeDJIMotor()` 在中断上下文中执行，会抢占所有任务。中断处理必须尽量简短，否则会导致任务调度异常。

5. **message_center 是同步通信** — `PubPushMessage()` 在发布者的任务上下文中执行，会遍历所有订阅者并 memcpy。如果订阅者很多或数据量很大，发布者任务的执行时间会增加。

6. **数据竞争风险** — Motor Task 读 `pid_ref` 的同时，Robot Task 可能正在写 `pid_ref`。由于 `pid_ref` 是 `float`（4 字节），在 Cortex-M4 上 float 读写不是原子操作。当前框架没有使用互斥锁保护，依赖"先写后读"的时序关系来避免竞争。如果任务优先级或频率调整，需要重新评估竞争风险。

7. **CAN 发送可能阻塞** — `CANTransmit()` 内部有邮箱等待逻辑，如果 CAN 总线负载过高，所有邮箱都满，会等待直到超时。这个等待发生在 Motor Task 中，可能导致控制周期不稳定。
