# BSP 层总览

## 模块职责

BSP（Board Support Package）层构建于 ST HAL 库之上，针对 RoboMaster 竞赛所用电控外设和模块的特点进行进一步封装，**是唯一允许直接出现 STM32 HAL 库函数的代码层**。

---

## 设计思路

### 为什么要 BSP 层？

如果 Module 层和 APP 层直接调用 HAL 函数，会导致：

1. **耦合严重** — 换芯片或换开发板时，上层代码需要大范围修改
2. **接口不统一** — HAL 函数风格多样（有的传句柄，有的传参数），上层使用心智负担大
3. **重复代码** — 多个模块都要写相同的过滤器配置、中断回调分发等逻辑

BSP 层的引入将硬件细节限制在最底层，上层只需关心"注册实例 -> 使用接口"。

### 两大核心模式

BSP 层的所有通信类外设（CAN、USART、IIC、SPI 等）都遵循两个核心模式：

#### 模式一：实例注册制

Module 层创建实例配置，调用 `XXXRegister()` 注册到 BSP 层。BSP 层自动完成初始化。

```
Module 层                        BSP 层
  |                                |
  |--- XXXRegister(&config) ------>|  1. 首次注册时初始化硬件服务
  |                                |  2. malloc 分配实例空间
  |<--- 返回 XXXInstance* ---------|  3. 配置实例参数
  |                                |  4. 添加过滤器/启动接收
  |                                |  5. 保存实例指针到数组
```

**设计原则**：只有当一个 Module 被 APP 实例化时，对应的 BSP 才会被初始化，避免未使用的模块被加载。

#### 模式二：回调分发制

BSP 层重载 HAL 的弱回调函数，在其中遍历所有已注册的实例，找到匹配的那一个，调用其注册时传入的回调函数。

```
硬件中断
  |
  v
HAL 弱回调（被 BSP 重载）
  |
  v
BSP 回调分发函数
  |  遍历 instance 数组
  |  匹配 can_handle / usart_handle / GPIO_Pin
  v
Module 层注册的回调函数
```

---

## 核心数据结构

BSP 层没有统一的"基类"，但所有通信类外设的实例结构体都包含这几个共同字段：

| 字段 | 类型 | 作用 |
|------|------|------|
| `xxx_handle` | `XXX_HandleTypeDef*` | 指向 CubeMX 生成的硬件句柄 |
| `xxx_module_callback` | 函数指针 | Module 层注册的回调 |
| `id` | `void*` | Parent Pointer，指向拥有此实例的 Module 层结构体 |

BSP 层的 `.c` 文件中都维护一个静态实例指针数组和索引：

```c
// 每个 BSP 模块都有这样的结构
static XXXInstance *xxx_instance[MAX_CNT] = {NULL};
static uint8_t idx; // 全局实例索引，注册时自增
```

---

## 初始化流程

BSP 的初始化分为"必须初始化"和"按需初始化"两部分：

### 必须初始化（BSPInit）

在 `bsp_init.h` 中定义，由 `RobotInit()` 在 RTOS 启动前调用：

```c
// basic_framework/bsp/bsp_init.h
void BSPInit()
{
    DWT_Init(168);    // 初始化 DWT 周期计数器，传入 CPU 频率（MHz）
    BSPLogInit();      // 初始化日志系统（SEGGER RTT）
}
```

**为什么 DWT 必须最先初始化？** 因为后续很多模块（如 CAN 的超时检测）依赖 DWT 计时，日志系统也可能用到时间戳。

### 按需初始化（首次 Register 时自动触发）

其他 BSP 模块（CAN、USART、IIC、GPIO、PWM）的硬件初始化，在第一次调用 `XXXRegister()` 时自动完成：

```c
// 以 CAN 为例
CANInstance *CANRegister(CAN_Init_Config_s *config)
{
    if (!idx) {
        CANServiceInit();  // 首次注册，启动 CAN 总线、开启中断通知
    }
    // ... 后续注册逻辑
}
```

**初始化顺序总结**：

```
RobotInit()
  |
  +-- BSPInit()
  |     |
  |     +-- DWT_Init(168)      // 必须最先，提供时间基准
  |     +-- BSPLogInit()        // 日志系统
  |
  +-- APP 层初始化
        |
        +-- Module1Register() --> CANRegister() --> CANServiceInit()  // 首次触发
        +-- Module2Register() --> USARTRegister() --> USARTServiceInit()
        +-- Module3Register() --> IICRegister()
        +-- ...后续注册不再触发 ServiceInit
```

---

## 各模块速查

| 模块 | 源文件 | 注册函数 | 回调类型 | 特点 |
|------|--------|----------|----------|------|
| DWT | bsp_dwt.c/.h | DWT_Init() | 无 | 必须初始化，提供时间基准 |
| Log | bsp_log.c/.h | BSPLogInit() | 无 | 必须初始化，基于 SEGGER RTT |
| CAN | bsp_can.c/.h | CANRegister() | `void (*)(CANInstance*)` | 实例注册 + 回调分发 + 过滤器自动配置 |
| USART | bsp_usart.c/.h | USARTRegister() | `void (*)()` | DMA + IDLE 接收，回调无参数 |
| IIC | bsp_iic.c/.h | IICRegister() | `void (*)(IICInstance*)` | 支持 blocking/IT/DMA 三种模式 |
| GPIO | bsp_gpio.c/.h | GPIORegister() | `void (*)(GPIOInstance*)` | EXTI 中断分发 |
| PWM | bsp_pwm.c/.h | PWMRegister() | `void (*)(PWMInstance*)` | 自动计算时钟频率，支持 DMA 波形 |

---

## 调用链

典型的 BSP 模块使用流程（以 CAN 为例）：

```
APP 层                    Module 层                     BSP 层                硬件
  |                         |                            |                    |
  |--- DJIMotorRegister -->|                            |                    |
  |                        |--- CANRegister(&config) -->|                    |
  |                        |                            |-- CANServiceInit() |
  |                        |                            |-- CANAddFilter()   |
  |                        |<-- CANInstance* -----------|                    |
  |                        |                            |                    |
  |  (控制循环中)           |                            |                    |
  |--- SetMotorControl -->|                            |                    |
  |                        |--- CANTransmit() --------->|-- HAL_CAN_AddTxMessage --> CAN 总线
  |                        |                            |                    |
  |                        |     (中断发生)              |                    |
  |                        |                            |<-- HAL_CAN_RxFifo0Callback
  |                        |                            |    CANFIFOxCallback()
  |                        |<-- DJIMotorDecode() -------|    遍历匹配 + 回调  |
```

---

## 注意事项

1. **禁止在非 BSP 层直接调用 HAL 函数** — 应使用 BSP 封装。如有同功能的 BSP 函数，必须优先使用。
2. **禁止在 CubeMX 生成的文件中把代码放在 usercode 块外** — 重新生成时会被覆盖。
3. **初始化期间全局中断被关闭** — `BSPInit()` 在 `RobotInit()` 中调用，此时中断尚未开启，不能使用依赖中断的功能（如 HAL_Delay）。
4. **首次 Register 触发 ServiceInit** — 这个设计意味着如果没有任何模块使用某个外设，该外设不会被初始化，节省资源。
5. **实例指针数组是静态的** — 存放在 `.c` 文件中，外部无法直接访问，保证了封装性。
6. **DWT 初始化参数因 MCU 而异** — C 板（F407）传 168，A 板（F427）传 180，搞错会导致所有计时都不准。
7. **BSP 层的 while(1) 死循环** — 很多错误处理用 `while(1)` 卡死 + 日志输出，这在嵌入式调试中是常见做法，但正式产品应考虑更优雅的错误恢复。
