# Module 层总览 -- OOP-in-C 设计模式与实例管理

## 模块职责

Module 层基于 BSP 层的封装打造各种功能模块，为 APP 层提供硬件无关的接口，是框架中承上启下的核心抽象层。

## 设计思路

### 为什么需要 Module 层

如果 APP 层直接调用 BSP 函数，会出现大量重复的硬件操作代码（如 CAN 数据解析、PID 计算、离线检测）。Module 层将这些通用逻辑封装为模块，使得 APP 层只需关心控制逻辑本身。

### OOP-in-C 的设计哲学

C 语言没有类和对象，但可以通过结构体 + 函数指针 + 注册机制实现面向对象的核心思想：

| OOP 概念 | 本框架实现 | 示例 |
|----------|-----------|------|
| 类 | 结构体定义 | `DJIMotorInstance` |
| 构造函数 | `XXXRegister()` / `XXXInit()` | `DJIMotorInit()` |
| 公有方法 | `.h` 中声明的函数 | `DJIMotorSetRef()` |
| 私有方法 | `.c` 中 `static` 函数 | `DecodeDJIMotor()` |
| 多态 | 函数指针 / 回调 | `can_module_callback` |
| 实例管理 | 静态指针数组 | `dji_motor_instance[]` |

### 懒加载：Module 没有统一初始化

Module 层不存在一个统一的 `ModuleInit()` 函数。只有当 APP 层调用某个模块的 `XXXRegister()` 时，该模块才会被初始化。未注册的模块不会占用运行时资源（仅有少量代码段开销）。

这意味着：如果你在 APP 中注释掉了 `ChassisInit()`，底盘相关的电机、PID、CAN 实例都不会被创建。

## 核心数据结构

### 类型命名规范

| 类型 | 后缀 | 含义 | 示例 |
|------|------|------|------|
| 简单数据类型 | `_t` | 数据单一、结构不复杂 | `IMU_Data_t`, `PID_t`, `RC_ctrl_t` |
| 复杂结构体 | `_s` | 功能和内涵多的结构体 | `Motor_Controller_s`, `PID_Init_Config_s` |
| BSP/Module 实例 | `Instance` | 通过 Register 创建的实例 | `CANInstance`, `DJIMotorInstance` |

### 通用数值定义 -- `general_def.h`

```c
// modules/general_def.h
#define PI 3.1415926535f
#define PI2 (PI * 2.0f)
#define RAD_2_DEGREE 57.2957795f    // 180/pi
#define DEGREE_2_RAD 0.01745329252f // pi/180
#define RPM_2_ANGLE_PER_SEC 6.0f    // x360/60sec
#define RPM_2_RAD_PER_SEC 0.104719755f // x2pi/60sec
```

这些宏在电机、IMU 等模块中广泛使用，用于角度/角速度的单位换算。

### 实例指针数组模式

每个 Module 的 `.c` 文件中维护一个静态的实例指针数组，用于在控制任务中遍历所有实例：

```c
// dji_motor.c
static DJIMotorInstance *dji_motor_instance[DJI_MOTOR_CNT] = {NULL};
static uint8_t idx = 0; // 全局电机索引
```

`DJIMotorControl()` 遍历 `dji_motor_instance[]` 对每个电机计算 PID 并发送控制报文。这是 Module 层最核心的设计模式之一。

### Parent Pointer（`void* id`）模式

BSP 层的 `CANInstance` 有一个 `void *id` 成员，保存拥有该 CAN 实例的 Module 层实例地址：

```c
// bsp_can.h
typedef struct _ {
    CAN_HandleTypeDef *can_handle;
    // ...
    void (*can_module_callback)(struct _ *); // 回调函数指针
    void *id;                                // 指向拥有此实例的 Module 实例
} CANInstance;
```

当 BSP 层 CAN 中断触发时，回调函数通过 `id` 找到 Module 层实例：

```c
// dji_motor.c: DecodeDJIMotor()
DJIMotorInstance *motor = (DJIMotorInstance *)_instance->id;
```

**常见错误**：直接将 `CANInstance*` 强转为 Module 实例类型（应该通过 `_instance->id` 间接获取），会导致数据覆写或野指针。

### 配置结构体模式

Module 的初始化函数接受一个配置结构体指针，集中设置所有参数：

```c
// motor_def.h
typedef struct {
    Motor_Controller_Init_s controller_param_init_config; // PID 参数
    Motor_Control_Setting_s controller_setting_init_config; // 闭环类型等
    Motor_Type_e motor_type;    // 电机型号
    CAN_Init_Config_s can_init_config; // CAN 通信配置
} Motor_Init_Config_s;
```

## 函数详解

### 命名规范

- **公共函数**：PascalCase，动宾短语，不超过 4 个单词
  - `DJIMotorInit()`, `PIDCalculate()`, `CANRegister()`
- **私有函数**：`.c` 文件中 `static` 修饰
  - `DecodeDJIMotor()`, `MotorSenderGrouping()`
- **变量**：snake_case，全小写
  - `motor_settings`, `pid_ref`, `total_angle`

### 统一接口风格

所有模块的初始化接口统一命名为 `XXXRegister()`（部分旧代码仍为 `XXXInit()`，后续会统一）：

| 模块 | 注册函数 | 说明 |
|------|---------|------|
| DJI 电机 | `DJIMotorInit()` | 旧命名，待统一 |
| HT 电机 | `HTMotorInit()` | 旧命名 |
| LK 电机 | `LKMotorInit()` | 旧命名 |
| CAN 通信 | `CANCommInit()` | 旧命名 |
| 遥控器 | `RemoteControlInit()` | 单例，Init 合理 |
| Daemon | `DaemonRegister()` | 已统一 |

## 调用链

### 从 APP 到硬件的典型路径（以电机控制为例）

```
APP: ChassisTask()
  -> DJIMotorSetRef(motor, ref)           // 设置参考值
  -> MotorControlTask()                    // RTOS 定时调用
     -> DJIMotorControl()                  // 遍历所有 DJI 电机实例
        -> PIDCalculate()                  // 计算串级 PID
        -> 填充 sender_assignment[].tx_buff // 分组写入发送缓冲
        -> CANTransmit()                   // BSP 层发送 CAN 报文
           -> HAL_CAN_Transmit()           // HAL 层
```

### 从硬件到 APP 的反馈路径（以电机反馈为例）

```
CAN 总线中断
  -> HAL_CAN_RxFifo0MsgPendingCallback()   // HAL 中断
     -> BSP_CAN_RxHandler()                // BSP 层处理
        -> 遍历已注册的 CAN 实例，匹配 rx_id
        -> instance->can_module_callback()  // 调用 Module 层回调
           -> DecodeDJIMotor()              // 解析反馈报文
              -> DaemonReload()             // 喂狗（标记在线）
              -> 更新 measure 结构体
              -> APP 层通过 motor->measure 读取数据
```

## 注意事项

1. **不要在非 BSP 层直接调用 HAL 函数** -- 应使用 BSP 封装。Module 层通过 BSP 层的接口（如 `CANRegister()`, `CANTransmit()`）访问硬件。

2. **`void* id` 是连接 BSP 和 Module 的桥梁** -- 注册 BSP 实例时必须正确设置 `id` 指针，否则回调函数无法找到 Module 实例。

3. **实例指针数组的索引 `idx` 是文件作用域的静态变量** -- 不存在跨文件访问的问题，但也意味着无法从外部获取已注册的实例数量。

4. **Module 层不需要自行初始化** -- 所有模块的注册由 APP 层在初始化阶段调用。Module 的 `.c` 文件中没有自动执行的初始化代码。

5. **malloc 的使用** -- 实例通过 `malloc()` 动态分配，在嵌入式环境中需要关注堆空间是否足够，避免碎片化问题。

6. **回调函数中禁止复杂逻辑** -- CAN 接收回调运行在中断上下文中，应仅做数据解析和标志位设置，不得放入耗时操作。
