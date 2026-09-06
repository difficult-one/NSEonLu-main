# Application 层如何创建任务

> 从 `main()` 到 RTOS 任务运行的完整链路

---

## 1. 整体启动流程

```
main()                           [Src/main.c]
  │
  ├─ HAL_Init()                  [HAL 库初始化]
  ├─ SystemClock_Config()        [时钟 168MHz]
  ├─ MX_xxx_Init()               [外设初始化: GPIO/CAN/SPI/UART/I2C 等]
  │
  ├─ RobotInit()                 [★ 硬件无关的模块初始化 + 创建任务 ★]
  │   ├─ BSPInit()               [DWT + 日志]
  │   ├─ RobotCMDInit()
  │   ├─ ChassisInit()
  │   └─ OSTaskInit()            [★ 创建 5 个 RTOS 任务 ★]
  │
  ├─ MX_FREERTOS_Init()          [STM32CubeMX 生成的 FreeRTOS 初始化]
  │   └─ 创建 defaultTask
  │
  └─ osKernelStart()             [启动调度器，所有任务开始运行]
```

---

## 2. 任务创建核心：`OSTaskInit()`

`application/robot_task.h:37-55`：

```c
void OSTaskInit()
{
    // ① INS 姿态解算任务 — 最高优先级，1kHz
    osThreadDef(instask, StartINSTASK, osPriorityAboveNormal, 0, 1024);
    insTaskHandle = osThreadCreate(osThread(instask), NULL);

    // ② 电机控制任务 — 1kHz
    osThreadDef(motortask, StartMOTORTASK, osPriorityNormal, 0, 256);
    motorTaskHandle = osThreadCreate(osThread(motortask), NULL);

    // ③ 守护任务（看门狗监控）— 100Hz
    osThreadDef(daemontask, StartDAEMONTASK, osPriorityNormal, 0, 128);
    daemonTaskHandle = osThreadCreate(osThread(daemontask), NULL);

    // ④ 机器人核心任务（底盘/云台/发射）— 200Hz
    osThreadDef(robottask, StartROBOTTASK, osPriorityNormal, 0, 1024);
    robotTaskHandle = osThreadCreate(osThread(robottask), NULL);

    // ⑤ UI 裁判系统串口屏任务 — 低频率
    osThreadDef(uitask, StartUITASK, osPriorityNormal, 0, 512);
    uiTaskHandle = osThreadCreate(osThread(uitask), NULL);

    HTMotorControlInit(); // 电机校准（非 RTOS 任务）
}
```

---

## 3. `osThreadCreate` 的三个要素

CMSIS-RTOS v1 封装了两步操作：

```c
osThreadDef(instask, StartINSTASK, osPriorityAboveNormal, 0, 1024);
//          ↑名称     ↑入口函数       ↑优先级               ↑栈大小(字)
insTaskHandle = osThreadCreate(osThread(instask), NULL);
//                                       ↑参数（本项目中始终为 NULL）
```

| 参数 | 说明 | 本项目取值 |
|------|------|----------|
| **入口函数** | 任务函数，调度器启动后从此函数开始执行 | `StartINSTASK` 等 |
| **优先级** | 数值越大优先级越高 | `osPriorityAboveNormal` / `osPriorityNormal` |
| **栈大小** | 单位是**字**（4 字节），实际字节数 ×4 | INS: 1024 字 = 4KB，Daemon: 128 字 = 512B |

---

## 4. 任务线程函数的标准模式

所有 5 个任务函数都遵循同一模板，以 `StartROBOTTASK` 为例：

```c
__attribute__((noreturn))                    // ① 永不返回，帮助编译器优化
void StartROBOTTASK(void const *argument)
{
    static float robot_dt;                   // ② 执行耗时统计
    static float robot_start;

    LOGINFO("[freeRTOS] ROBOT core Task Start");

    for (;;)                                 // ③ 无限循环（RTOS 任务的标志）
    {
        robot_start = DWT_GetTimeline_ms();  // ④ 记录开始时间

        RobotTask();                         // ⑤ ★ 调用 Application 业务逻辑

        robot_dt = DWT_GetTimeline_ms() - robot_start;  // ⑥ 计算执行耗时

        if (robot_dt > 5)                    // ⑦ 超时告警（目标 200Hz → 5ms 内必须完成）
            LOGERROR("[freeRTOS] ROBOT core Task is being DELAY! dt = [%f]", &robot_dt);

        osDelay(5);                          // ⑧ 挂起 5ms → 执行频率 200Hz
    }
}
```

---

## 5. 五个任务一览

| 任务 | 入口 | 频率 | 优先级 | 栈 | 职责 |
|------|------|:---:|:---:|---:|------|
| **INS** | `StartINSTASK` → `INS_Task()` | 1kHz | AboveNormal | 4KB | BMI088 传感器读取 + 姿态解算 + 视觉数据发送 |
| **Motor** | `StartMOTORTASK` → `MotorControlTask()` | 1kHz | Normal | 1KB | 遍历所有电机并发送控制报文 |
| **Daemon** | `StartDAEMONTASK` → `DaemonTask()` | 100Hz | Normal | 0.5KB | 监控模块在线状态 + 蜂鸣器控制 |
| **Robot** | `StartROBOTTASK` → `RobotTask()` | 200Hz | Normal | 4KB | 底盘运动学解算 + 云台控制 + 发射控制 |
| **UI** | `StartUITASK` → `UITask()` | ~10Hz | Normal | 2KB | 裁判系统串口屏 UI 刷新 |

---

## 6. `RobotTask()` 的分层聚合

`application/robot.c:48-60`：

```c
void RobotTask()
{
#if defined(ONE_BOARD) || defined(GIMBAL_BOARD)
    RobotCMDTask();   // 遥控器/视觉指令解析
    // GimbalTask();
    // ShootTask();
#endif

#if defined(ONE_BOARD) || defined(CHASSIS_BOARD)
    ChassisTask();    // 底盘运动学解算
#endif
}
```

通过 `robot_def.h` 中的宏进行**编译时裁剪**：

```c
// robot_def.h 中定义：
#define ONE_BOARD      // 全功能板（底盘 + 云台合一）
// #define CHASSIS_BOARD // 仅底盘板
// #define GIMBAL_BOARD  // 仅云台板
```

| 宏 | RobotTask 包含 |
|----|---------------|
| `ONE_BOARD` | `RobotCMDTask()` + `ChassisTask()` — 全部功能 |
| `CHASSIS_BOARD` | 仅 `ChassisTask()` |
| `GIMBAL_BOARD` | 仅 `RobotCMDTask()` |

---

## 7. 任务的执行时序

```
osKernelStart() 之后，所有任务进入就绪态：

时间轴 →
│
├─ 1kHz ──────────────────────────────────────────────
│  t=0    t=1ms   t=2ms   t=3ms   t=4ms   t=5ms
│  [INS]  [INS]   [INS]   [INS]   [INS]   [INS]    ← INS (最高优先级，抢占)
│  [Motor][Motor] [Motor] [Motor] [Motor] [Motor]   ← Motor
│
├─ 200Hz ─────────────────────────────────────────────
│  t=0            t=5ms           t=10ms
│  [Robot·········]               [Robot·········]  ← Robot (200Hz)
│
├─ 100Hz ─────────────────────────────────────────────
│  t=0                    t=10ms
│  [Daemon···············][Daemon···············]   ← Daemon (100Hz)
│
└─ ~10Hz ─────────────────────────────────────────────
   [UI········································]      ← UI (按需刷新)
```

- INS 优先级最高（`osPriorityAboveNormal`），任何其他任务运行时 INS 的 1ms 到期都会**抢占**执行
- 其余四个任务同优先级（`osPriorityNormal`），按时间片轮转

---

## 8. 设计原则总结

| 原则 | 表现 |
|------|------|
| **隔离** | 每个子系统独立一个任务，一个崩了不影响其他（除非共享内存越界） |
| **周期驱动** | 使用 `osDelay()` 精确控制执行频率，配合 DWT 监控是否超时 |
| **编译时裁剪** | 通过 `#if defined(BOARD_TYPE)` 进行代码裁剪，一套代码支持多块板子 |
| **统一模板** | 所有任务函数结构一致：计时 → 执行 → 检查超时 → 挂起 |
| **分层调用** | RTOS 任务层只做调度，业务逻辑全在 `xxx_Task()` / `xxxTask()` 函数中 |

---

## 9. 添加新任务的步骤

假设要添加一个"功率控制"任务：

### Step 1：在 `robot_task.h` 中声明

```c
osThreadId powerTaskHandle;

void StartPOWERTASK(void const *argument);
```

### Step 2：在 `OSTaskInit()` 中创建

```c
osThreadDef(powertask, StartPOWERTASK, osPriorityNormal, 0, 256);
powerTaskHandle = osThreadCreate(osThread(powertask), NULL);
```

### Step 3：实现任务函数

```c
__attribute__((noreturn)) void StartPOWERTASK(void const *argument)
{
    for (;;)
    {
        PowerControlTask();
        osDelay(10);  // 100Hz
    }
}
```

### Step 4：实现业务逻辑

```c
// 在 application 目录下创建对应应用的 .h/.c 文件
void PowerControlTask(void)
{
    // 读取功率 → 计算限制 → 调整输出
}
```
