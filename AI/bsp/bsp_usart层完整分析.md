# bsp_usart 层完整分析

> `bsp/usart/bsp_usart.h` + `bsp/usart/bsp_usart.c`

---

## 1. 头文件：数据类型与宏定义

### 1.1 宏常量

```c
#define DEVICE_USART_CNT  3     // C 板至多分配 3 个串口（huart1/2/3 各一）
#define USART_RXBUFF_LIMIT 256  // 最大接收缓冲区大小
```

### 1.2 回调函数指针类型

```c
typedef void (*usart_module_callback)();
```

| 字段 | 含义 |
|------|------|
| 返回类型 | `void` — 无返回值 |
| 参数 | `()` — 无参数（回调通过全局实例访问数据） |
| 作用 | BSP 层用这个类型保存上层模块的回调函数地址 |

### 1.3 发送模式枚举

```c
typedef enum
{
    USART_TRANSFER_NONE    = 0,  // 未设置
    USART_TRANSFER_BLOCKING,     // 阻塞发送（CPU 等待）
    USART_TRANSFER_IT,           // 中断发送（非阻塞）
    USART_TRANSFER_DMA,          // DMA 发送（非阻塞，最高效）
} USART_TRANSFER_MODE;
```

### 1.4 运行时实例结构体 `USARTInstance`

```c
typedef struct
{
    uint8_t recv_buff[USART_RXBUFF_LIMIT]; // 256 字节接收缓冲区 — DMA 直接写入
    uint8_t recv_buff_size;                // 本模块期望接收的一包数据大小
    UART_HandleTypeDef *usart_handle;      // HAL 库串口句柄（如 &huart1）
    usart_module_callback module_callback; // 上层模块注册的回调函数指针
} USARTInstance;
```

| 成员 | 作用 | 谁写 | 谁读 |
|------|------|------|------|
| `recv_buff[256]` | DMA 接收数据的目的地 | DMA 硬件 | 上层回调 |
| `recv_buff_size` | 期望收多少字节（用于 DMA 配置） | 初始化时设定 | BSP 层 |
| `usart_handle` | 标识用哪个串口硬件 | 初始化时设定 | BSP 层（匹配中断） |
| `module_callback` | 上层回调函数地址 | 注册时存入 | BSP 层（中断时调用） |

### 1.5 初始化配置结构体 `USART_Init_Config_s`

```c
typedef struct
{
    uint8_t recv_buff_size;                // 期望接收大小
    UART_HandleTypeDef *usart_handle;      // 串口句柄
    usart_module_callback module_callback; // 回调函数
} USART_Init_Config_s;
```

> 不含 `recv_buff` 缓冲区——缓冲区由 `USARTRegister()` 通过 `malloc` 分配。Config 只负责"传参"，Instance 负责"运行"。

---

## 2. 源文件：私有变量与所有函数

### 2.1 私有全局变量

```c
static uint8_t idx;                                        // 当前已注册实例数
static USARTInstance *usart_instance[DEVICE_USART_CNT] = {NULL}; // 实例指针数组，最多 3 个
```

| 变量 | 作用 |
|------|------|
| `idx` | 注册计数器，每次 `USARTRegister()` 自增 |
| `usart_instance[3]` | 保存所有已注册实例的指针，中断时遍历匹配 |

---

## 3. 函数一览

| 函数 | 所在文件 | 类型 | 功能 |
|------|---------|------|------|
| `USARTServiceInit()` | `.c:29` | 公共（声明在 `.h`） | 启动 DMA 接收 |
| `USARTRegister()` | `.c:38` | 公共 | 注册一个串口实例 |
| `USARTSend()` | `.c:62` | 公共 | 发送数据（支持 3 种模式） |
| `USARTIsReady()` | `.c:83` | 公共 | 查询串口是否空闲 |
| `HAL_UARTEx_RxEventCallback()` | `.c:102` | HAL 回调（重写 `__weak`） | 接收完成回调 |
| `HAL_UART_ErrorCallback()` | `.c:127` | HAL 回调（重写 `__weak`） | 出错恢复 |

---

## 4. 每个函数的实现逻辑

### 4.1 `USARTServiceInit()` — 启动 DMA 接收（`.c:29`）

```c
void USARTServiceInit(USARTInstance *_instance)
{
    // ① 启动 DMA + IDLE 接收
    HAL_UARTEx_ReceiveToIdle_DMA(
        _instance->usart_handle,    // 用哪个串口
        _instance->recv_buff,       // 数据写到哪
        _instance->recv_buff_size   // 期望收多少字节
    );

    // ② 关闭 DMA 半传输中断（防止一帧数据触发两次回调）
    __HAL_DMA_DISABLE_IT(_instance->usart_handle->hdmarx, DMA_IT_HT);
}
```

**逻辑**：
1. 调用 HAL 库函数，让 DMA 开始监听串口接收。数据到达时 DMA 自动将数据从 UART 数据寄存器搬运到 `recv_buff`
2. 立刻关闭 DMA 半传输中断——只在传输完成（TC）或总线空闲（IDLE）时触发回调

> 该函数注册后自动调用，也可在丢失回调后手动调用（如 daemon 模块检测到离线后重新启动接收）

---

### 4.2 `USARTRegister()` — 注册串口实例（`.c:38`）

```c
USARTInstance *USARTRegister(USART_Init_Config_s *init_config)
{
    // ① 安全检查
    if (idx >= DEVICE_USART_CNT)          // 超过 3 个串口 → 死循环 + 报错
        while (1) LOGERROR("[bsp_usart] USART exceed max instance count!");

    for (uint8_t i = 0; i < idx; i++)     // 同一个串口重复注册 → 死循环 + 报错
        if (usart_instance[i]->usart_handle == init_config->usart_handle)
            while (1) LOGERROR("[bsp_usart] USART instance already registered!");

    // ② 堆上分配实例空间，清零
    USARTInstance *instance = (USARTInstance *)malloc(sizeof(USARTInstance));
    memset(instance, 0, sizeof(USARTInstance));

    // ③ 从配置单抄数据到实例
    instance->usart_handle    = init_config->usart_handle;
    instance->recv_buff_size  = init_config->recv_buff_size;
    instance->module_callback = init_config->module_callback;

    // ④ 存入全局数组，启动 DMA 接收
    usart_instance[idx++] = instance;
    USARTServiceInit(instance);

    // ⑤ 返回实例指针给模块持有
    return instance;
}
```

**逻辑流程**：

```
Module 传入 Config → 安全检查 → malloc 分配 Instance
→ 抄配置到 Instance → 加入全局数组 → 启动 DMA 接收 → 返回 Instance*
```

---

### 4.3 `USARTSend()` — 发送数据（`.c:62`）

```c
void USARTSend(USARTInstance *_instance, uint8_t *send_buf,
               uint16_t send_size, USART_TRANSFER_MODE mode)
{
    switch (mode)
    {
    case USART_TRANSFER_BLOCKING:
        HAL_UART_Transmit(_instance->usart_handle, send_buf, send_size, 100);
        break;                                        // 阻塞：CPU 等待直到发完（超时 100ms）

    case USART_TRANSFER_IT:
        HAL_UART_Transmit_IT(_instance->usart_handle, send_buf, send_size);
        break;                                        // 中断发送：函数立即返回，后台发

    case USART_TRANSFER_DMA:
        HAL_UART_Transmit_DMA(_instance->usart_handle, send_buf, send_size);
        break;                                        // DMA 发送：函数立即返回，DMA 后台搬运

    default:   // 非法模式 → 死循环（方便调试定位）
        while (1) ;
    }
}
```

**三种发送模式对比**：

| 模式 | CPU 占用 | 速度 | 适用场景 |
|------|---------|------|---------|
| BLOCKING | 全程占用 | — | 调试、短数据 |
| IT | 中断打断 | 快 | 中等长度数据 |
| DMA | 几乎不占 | 最快 | 长数据、高频发送、项目默认 |

---

### 4.4 `USARTIsReady()` — 查询是否空闲（`.c:83`）

```c
uint8_t USARTIsReady(USARTInstance *_instance)
{
    if (_instance->usart_handle->gState | HAL_UART_STATE_BUSY_TX)
        return 0;   // 发送忙 → 不可用
    else
        return 1;   // 空闲 → 可发送
}
```

> 注意：代码中用的是 `|`（位或）而非 `&`（位与）。当 `gState` 为 `BUSY_TX` 时结果非零，表达"忙"。这是一个逻辑小技巧，但实际意图清晰。

---

### 4.5 `HAL_UARTEx_RxEventCallback()` — 接收完成回调（`.c:102`）

这是**最核心**的函数，重写了 HAL 库的 `__weak`：

```c
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    for (uint8_t i = 0; i < idx; ++i)               // ① 遍历所有注册的串口实例
    {
        if (huart == usart_instance[i]->usart_handle) // ② 匹配：找出哪个实例的串口触发了中断
        {
            if (usart_instance[i]->module_callback != NULL)  // ③ 安全检查
            {
                usart_instance[i]->module_callback();        // ④ ★ 调用上层模块的回调 ★
                memset(usart_instance[i]->recv_buff, 0, Size); // ⑤ 清空缓冲区
            }

            // ⑥ 重新启动 DMA 接收（循环接收）
            HAL_UARTEx_ReceiveToIdle_DMA(
                usart_instance[i]->usart_handle,
                usart_instance[i]->recv_buff,
                usart_instance[i]->recv_buff_size
            );
            __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT); // ⑦ 再次关掉半传输中断
            return;
        }
    }
}
```

**完整执行流程**：

```
硬件中断
  → HAL 库检测到 DMA_TC 或 UART_IDLE 事件
    → HAL_UARTEx_RxEventCallback(huart, Size)
      → 遍历 usart_instance[0..idx-1]
        → 找到 usart_handle 匹配的那个实例
          → 调用 module_callback()          ← 上层模块处理数据
          → 清空 recv_buff
          → 重启 DMA 接收（准备收下一包）
          → 关掉 HT 中断
```

---

### 4.6 `HAL_UART_ErrorCallback()` — 错误恢复（`.c:127`）

```c
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < idx; ++i)               // 遍历匹配
    {
        if (huart == usart_instance[i]->usart_handle)
        {
            // 重启 DMA 接收（错误后 HAL 会停掉接收，需要手动恢复）
            HAL_UARTEx_ReceiveToIdle_DMA(/* ... */);
            __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);
            LOGWARNING("[bsp_usart] USART error callback triggered, instance idx [%d]", i);
            return;
        }
    }
}
```

**逻辑**：常见串口错误（奇偶校验错、溢出、帧错）发生后，HAL 会停止当前接收。这个回调做的唯一事情就是**重新启动 DMA 接收**，让串口恢复工作。

---

## 5. 与 Module 层的解耦原理

### 5.1 整体架构

```
┌───────────────────────────────────────────────────────┐
│                    Module 层                           │
│                                                       │
│  remote_control.c   vision.c   referee.c   ...        │
│  RemoteControlRx()  VisionRx() RefereeRx()            │
│       ↑                 ↑          ↑                  │
│       │ 注册回调 (函数指针传给 BSP)                       │
├───────┼─────────────────┼──────────┼──────────────────┤
│       │                 │          │                  │
│                     BSP 层 (bsp_usart)                 │
│                                                       │
│  ┌─────────────────────────────────────────┐          │
│  │ usart_instance[3]  全局实例指针数组       │          │
│  │  [0] → RemoteControl 的 USARTInstance   │          │
│  │  [1] → Vision 的 USARTInstance          │          │
│  │  [2] → Referee 的 USARTInstance         │          │
│  └─────────────────────────────────────────┘          │
│                                                       │
│  HAL_UARTEx_RxEventCallback()                         │
│    → 按 usart_handle 匹配                              │
│    → 调用匹配实例的 module_callback()                   │
│                                                       │
│           ↑ 重写 __weak 回调                           │
├───────────┼───────────────────────────────────────────┤
│           │                                           │
│       HAL 库 (stm32f4xx_hal_uart.c)                   │
│       DMA 中断 / IDLE 中断处理                          │
└───────────────────────────────────────────────────────┘
```

### 5.2 解耦的 5 个机制

#### 机制 ①：BSP 不 include 任何 Module 头文件

```c
// bsp_usart.c 只引用了这些：
#include "bsp_usart.h"
#include "bsp_log.h"
#include "stdlib.h"
#include "memory.h"

// 没有任何 #include "remote_control.h"  之类的 Module 引用
```

BSP 层**完全不知道**上层有哪些模块。它只知道有一个 `module_callback` 函数指针，到时候调它就是。

#### 机制 ②：通过 Config 注册，而非硬编码

```c
// 模块初始化时：
USART_Init_Config_s my_config = {
    .usart_handle   = &huart3,
    .recv_buff_size = 18,
    .module_callback = MyRxCallback,   // ← 把自己的函数地址传进去
};
USARTInstance *my_uart = USARTRegister(&my_config);

// BSP 层只需要一行存下来：
instance->module_callback = init_config->module_callback;
// 存完就完事了，不关心 MyRxCallback 到底干什么
```

#### 机制 ③：`void*` 的缺失版本——通过实例指针间接访问

CAN 的版本用 `void* id` 让回调反查模块（因为 CAN 是一对多），USART 因为是点对点通信（一个串口只接一个外设），直接通过 `usart_handle` 做匹配就够了，不需要 `void* id`。

但原理一样：**BSP 层持有模块数据的引用，但它把数据视为不透明**。

#### 机制 ④：回调函数无参设计——通过全局实例访问数据

```c
typedef void (*usart_module_callback)();  // 无参数！

// 回调被调用时：
usart_instance[i]->module_callback();     // 不带任何参数
```

模块在上层通过自己持有的 `USARTInstance*` 指针来读取接收数据：

```c
// Module 层代码示例：
void MyRxCallback()
{
    // my_uart 是模块持有的全局指针，init 时赋值的
    uint8_t *data = my_uart->recv_buff;
    // 解析 data...
}
```

这样设计的好处：BSP 调用回调时不需要传参，减少耦合。

#### 机制 ⑤：O(n) 匹配，而非硬编码分发

```c
// ✅ 实际做法：遍历 + 匹配（对扩展开放）
for (uint8_t i = 0; i < idx; ++i)
{
    if (huart == usart_instance[i]->usart_handle)
        usart_instance[i]->module_callback();
}

// ❌ 如果不解耦的做法（对修改不开放）：
if (huart == &huart1)       RemoteControlRx();
else if (huart == &huart2)  VisionRx();
else if (huart == &huart3)  RefereeRx();
// 每加一个模块，就要改 BSP 代码 ← 紧耦合
```

### 5.3 两层解耦全景图

```
HAL __weak 回调              →     BSP 回调函数指针        →    Module 业务逻辑
(硬件 → 用户代码解耦)            (BSP → Module 解耦)           (纯业务)

HAL_CAN_RxFifo0...Callback  →    can_module_callback()  →    CANCommRxCallback()
HAL_UARTEx_RxEventCallback  →    module_callback()      →    RemoteControlRx()
```

- **第一层**（HAL → BSP）：通过 `__weak` 重写，STM32 的代码和项目代码各管各的
- **第二层**（BSP → Module）：通过函数指针注册，BSP 不知道 Module 是什么，只知道"到时候调这个地址"

> 两层解耦叠加：HAL 库升级不影响项目代码；新增一个串口模块不需要改 BSP 一行代码。
