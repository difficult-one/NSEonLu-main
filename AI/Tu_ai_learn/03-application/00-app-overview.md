# Application 层总览

## 模块职责

Application 层实现机器人的控制逻辑，对机器人控制结构进行抽象——开发者只需思考逻辑并用硬件无关的 C 语言代码完成控制即可。

---

## 设计思路

### 为什么需要 APP 层？

BSP 层封装了硬件细节，Module 层提供了设备级接口，但如果每个应用都自己管理电机初始化、消息传递、模式切换，仍然会有大量重复逻辑。APP 层的职责是：

1. **组合 Module** — 将电机、IMU、遥控器等模块组装成具有完整功能的控制单元
2. **编排控制流程** — 模式判断、PID 参考值设定、反馈数据分发
3. **应用间通信** — 通过 message_center 实现松耦合的数据共享

### 核心设计原则

- **每个应用 = 一个初始化函数 + 一个任务函数** — 对应一个 FreeRTOS 任务
- **应用之间平行、不包含** — 所有通信走 message_center，禁止直接包含彼此
- **初始化在 RTOS 之外，运行在 RTOS 之内** — 先初始化再启动调度器
- **条件编译切换单板/双板** — 同一份代码适配不同硬件配置

---

## 核心数据结构

APP 层没有统一的"基类"，但每个应用都遵循相同的模式：

```c
// 每个 .c 文件中的典型私有变量（以 chassis.c 为例）

// 1. Module 实例指针 —— 应用"拥有"的硬件设备
static DJIMotorInstance *motor_lf, *motor_rf, *motor_lb, *motor_rb;

// 2. 消息中心发布者和订阅者 —— 与其他应用通信
static Publisher_t *chassis_pub;     // 发布底盘反馈
static Subscriber_t *chassis_sub;    // 订阅底盘控制命令

// 3. 收发数据结构 —— 来自 robot_def.h 的共享类型
static Chassis_Ctrl_Cmd_s chassis_cmd_recv;       // 接收的控制命令
static Chassis_Upload_Data_s chassis_feedback_data; // 发送的反馈数据
```

### FreeRTOS 任务架构

在 `robot_task.h` 中定义了系统中的所有 FreeRTOS 任务：

| 任务句柄 | 任务函数 | 优先级 | 栈大小 | 运行频率 | 职责 |
|----------|----------|--------|--------|----------|------|
| insTaskHandle | StartINSTASK | AboveNormal | 1024 | 1kHz | 姿态解算 + 视觉发送 |
| motorTaskHandle | StartMOTORTASK | Normal | 256 | 1kHz | 电机 PID 计算 + CAN 发送 |
| daemonTaskHandle | StartDAEMONTASK | Normal | 128 | 100Hz | 模块离线检测 + 蜂鸣器 |
| robotTaskHandle | StartROBOTTASK | Normal | 1024 | 200Hz | 机器人应用逻辑 |
| uiTaskHandle | StartUITASK | Normal | 512 | ~ | 裁判系统 UI 绘制 |

---

## 函数详解

### RobotInit() — 系统唯一入口

位于 `robot.c:23`，是 `main()` 中唯一需要手动调用的初始化函数。

```c
void RobotInit()
{
    __disable_irq();  // 关闭中断，防止初始化过程中发生中断

    BSPInit();  // BSP 层基础初始化（DWT + Log）

    // 根据板型条件编译，初始化对应的应用
#if defined(ONE_BOARD) || defined(GIMBAL_BOARD)
    RobotCMDInit();     // 指令处理（大脑）
    // GimbalInit();    // 当前被注释，调试时按需打开
    // ShootInit();
#endif

#if defined(ONE_BOARD) || defined(CHASSIS_BOARD)
    ChassisInit();      // 底盘控制
#endif

    OSTaskInit();       // 创建 FreeRTOS 任务

    __enable_irq();     // 初始化完成，开启中断
}
```

**关键点**：
- `__disable_irq()` 确保初始化期间不会被打断，避免未初始化完就被中断访问
- BSPInit() 最先调用，因为后续模块注册依赖 DWT 计时
- 应用初始化（如 ChassisInit）在内部会调用模块注册，模块注册又触发 BSP 注册

### OSTaskInit() — 创建 FreeRTOS 任务

位于 `robot_task.h:36`，创建所有持续运行的任务。

```c
void OSTaskInit()
{
    // INS 任务，1kHz，阻塞读取传感器，优先级较高
    osThreadDef(instask, StartINSTASK, osPriorityAboveNormal, 0, 1024);
    insTaskHandle = osThreadCreate(osThread(instask), NULL);

    // 电机控制任务，1kHz
    osThreadDef(motortask, StartMOTORTASK, osPriorityNormal, 0, 256);
    motorTaskHandle = osThreadCreate(osThread(motortask), NULL);

    // 守护任务，100Hz，检测模块离线
    osThreadDef(daemontask, StartDAEMONTASK, osPriorityNormal, 0, 128);
    daemonTaskHandle = osThreadCreate(osThread(daemontask), NULL);

    // 机器人核心任务，200Hz，运行应用逻辑
    osThreadDef(robottask, StartROBOTTASK, osPriorityNormal, 0, 1024);
    robotTaskHandle = osThreadCreate(osThread(robottask), NULL);

    // UI 任务
    osThreadDef(uitask, StartUITASK, osPriorityNormal, 0, 512);
    uiTaskHandle = osThreadCreate(osThread(uitask), NULL);

    HTMotorControlInit();  // HT 电机控制初始化（无注册则不执行）
}
```

### StartROBOTTASK() — 机器人核心任务入口

位于 `robot_task.h:111`，以 5ms 周期运行 RobotTask()。

```c
void StartROBOTTASK(void const *argument)
{
    for (;;)
    {
        robot_start = DWT_GetTimeline_ms();
        RobotTask();  // 调用 robot.c 中的 RobotTask()
        robot_dt = DWT_GetTimeline_ms() - robot_start;
        if (robot_dt > 5)
            LOGERROR("[freeRTOS] ROBOT core Task is being DELAY!");
        osDelay(5);  // 5ms = 200Hz
    }
}
```

### RobotTask() — 分发到各应用任务

位于 `robot.c:48`，内部调用各应用的 Task 函数。

```c
void RobotTask()
{
#if defined(ONE_BOARD) || defined(GIMBAL_BOARD)
    RobotCMDTask();
    // GimbalTask();
    // ShootTask();
#endif

#if defined(ONE_BOARD) || defined(CHASSIS_BOARD)
    ChassisTask();
#endif
}
```

---

## 调用链

完整的启动和运行流程：

```
main()                                          // Src/main.c:78
  |
  +-- HAL_Init()                                // HAL 库初始化
  +-- SystemClock_Config()                      // 时钟配置 168MHz
  +-- MX_xxx_Init()                             // CubeMX 生成的外设初始化
  +-- RobotInit()                               // robot.c:23
  |     |
  |     +-- __disable_irq()                     // 关闭全局中断
  |     +-- BSPInit()                           // bsp_init.h:16
  |     |     +-- DWT_Init(168)                 // 高精度定时器
  |     |     +-- BSPLogInit()                  // SEGGER RTT 日志
  |     |
  |     +-- RobotCMDInit()                      // 注册遥控器、视觉、消息
  |     +-- ChassisInit()                       // 注册4个电机、裁判系统
  |     +-- OSTaskInit()                        // 创建 FreeRTOS 任务
  |     +-- __enable_irq()                      // 开启全局中断
  |
  +-- MX_FREERTOS_Init()                        // FreeRTOS 初始化
  +-- osKernelStart()                           // 启动调度器，此后不再返回

--- 以下在 FreeRTOS 调度下运行 ---

StartROBOTTASK (200Hz)                          // robot_task.h:111
  |
  +-- RobotTask()                               // robot.c:48
        +-- RobotCMDTask()                      // 读取输入、发布控制命令
        +-- ChassisTask()                       // 订阅命令、运动学解算

StartMOTORTASK (1kHz)                           // robot_task.h:76
  |
  +-- MotorControlTask()                        // 电机 PID 计算 + CAN 发送

StartINSTASK (1kHz)                             // robot_task.h:57
  |
  +-- INS_Task()                                // 姿态解算
  +-- VisionSend()                              // 发送视觉数据

StartDAEMONTASK (100Hz)                         // robot_task.h:92
  |
  +-- DaemonTask()                              // 模块离线检测
  +-- BuzzerTask()                              // 蜂鸣器报警
```

---

## 注意事项

1. **初始化不得放入任务中** — 即使在 FreeRTOS 任务的死循环前也不行。因为 `__disable_irq()` 在 `RobotInit()` 中关闭了全局中断，而任务创建在 `OSTaskInit()` 中，此时中断仍处于关闭状态。初始化中不能使用依赖中断的延时。

2. **初始化期间禁用中断** — `RobotInit()` 内部 `__disable_irq()` / `__enable_irq()` 包裹。初始化过程中绝对不能使用依赖中断的功能（如 `HAL_Delay`）。若必须延时，只能用 `DWT_Delay()`。

3. **所有模块在 APP 层初始化** — Module 层只提供注册接口，不自行初始化。这确保了"不注册就不存在"的原则。

4. **应用之间禁止直接包含** — 它们必须是平行关系，所有通信走 message_center 或 CAN comm。

5. **任务执行时间不能超过周期** — 每个任务内部都有 dt 检测，超过周期会打印 ERROR 日志。对于实时性要求高的任务，务必保证计算量在周期内完成。

6. **条件编译控制板型** — `robot.c` 和各应用中大量使用 `#ifdef ONE_BOARD` / `#ifdef GIMBAL_BOARD` / `#ifdef CHASSIS_BOARD`。烧录前必须确认 `robot_def.h` 中的板型定义正确。

7. **osDelay vs vTaskDelayUntil** — 当前代码使用 `osDelay()`，对于实时性要求更高的场景，建议改用 `vTaskDelayUntil()` 以获得更精确的周期控制。
