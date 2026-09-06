# bsp_usart 完整源码解析

> 逐行分析 `bsp/usart/bsp_usart.h` 和 `bsp/usart/bsp_usart.c`

---

## 第一部分：头文件 `bsp_usart.h` — 接口定义

### 1.1 头文件保护与依赖

```c
#ifndef BSP_RC_H         // ← 注意：BSP_RC_H 而非 BSP_USART_H
#define BSP_RC_H         //    说明此文件最初是从遥控器（RC）的 bsp 拆分出来的

#include <stdint.h>       // 提供 uint8_t / uint16_t 等标准整数类型
#include "main.h"         // 提供 HAL 库的 UART_HandleTypeDef 等定义
```

---

### 1.2 宏常量

```c
#define DEVICE_USART_CNT  3      // C 板至多分配 3 个串口
#define USART_RXBUFF_LIMIT 256   // 接收缓冲区上限（字节）
```

| 宏 | 值 | 设计考量 |
|---|:---:|---|
| `DEVICE_USART_CNT` | 3 | STM32F407IG 有 USART1/2/3 接出来了（UART4/5 未引出） |
| `USART_RXBUFF_LIMIT` | 256 | 裁判系统一帧约 128 字节，视觉一帧可能超 200，256 留有余量 |

---

### 1.3 回调函数指针类型 `usart_module_callback`

```c
typedef void (*usart_module_callback)();
//     ↑            ↑                    ↑
//   返回void    类型名                  空参数列表（C 中表示未指定参数）
```

这是一个**函数指针类型**。任何满足"无参数、无返回值"的函数，其地址都可以赋值给此类型的变量。

> BSP 层用此类型保存上层模块的回调地址——BSP 不知道、也不关心具体是什么函数，只负责"到时候调用它"。

---

### 1.4 发送模式枚举 `USART_TRANSFER_MODE`

```c
typedef enum
{
    USART_TRANSFER_NONE     = 0,  // 未设置（未使用此枚举的值）
    USART_TRANSFER_BLOCKING,      // = 1 — HAL_UART_Transmit() 阻塞发送
    USART_TRANSFER_IT,            // = 2 — HAL_UART_Transmit_IT() 中断发送
    USART_TRANSFER_DMA,           // = 3 — HAL_UART_Transmit_DMA() DMA 发送
} USART_TRANSFER_MODE;
```

| 模式 | HAL 函数 | CPU 行为 | 适用场景 |
|------|---------|---------|---------|
| `BLOCKING` | `HAL_UART_Transmit()` | 全程占用，等每一位发完 | 调试、极短数据 |
| `IT` | `HAL_UART_Transmit_IT()` | 每字节触发一次中断 | 中等长度 |
| `DMA` | `HAL_UART_Transmit_DMA()` | 几乎不占 CPU，硬件自动搬运 | 长数据、高频发送 |

---

### 1.5 运行时实例结构体 `USARTInstance`

```c
typedef struct
{
    uint8_t recv_buff[USART_RXBUFF_LIMIT]; // [0] 256 字节接收缓冲区 — DMA 硬件直接写入
    uint8_t recv_buff_size;                // [1] 期望接收的单包字节数
    UART_HandleTypeDef *usart_handle;      // [2] HAL 库串口句柄指针（如 &huart1）
    usart_module_callback module_callback; // [3] 上层模块注册的回调函数指针
} USARTInstance;
```

| 字段 | 大小 | 作用 | 写者 | 读者 |
|------|:---:|------|------|------|
| `recv_buff` | 256 B | DMA 搬运目的地 | DMA 硬件 | 上层回调 |
| `recv_buff_size` | 1 B | DMA 传输长度配置 | 注册时 | `USARTServiceInit()` |
| `usart_handle` | 4 B | 标识物理串口 | 注册时 | 中断匹配 |
| `module_callback` | 4 B | 上层回调地址 | 注册时 | 中断服务 |

**内存布局**（pack 后约为 265 字节）：
```
┌──────────────────┬──────┬──────────┬──────────┐
│ recv_buff[256]   │ size │ handle*  │ callback*│
│     256 B        │ 1 B  │  4 B     │  4 B     │
└──────────────────┴──────┴──────────┴──────────┘
```

---

### 1.6 初始化配置结构体 `USART_Init_Config_s`

```c
typedef struct
{
    uint8_t recv_buff_size;                // 接收大小
    UART_HandleTypeDef *usart_handle;      // 串口句柄
    usart_module_callback module_callback; // 回调函数
} USART_Init_Config_s;
```

> 不含 `recv_buff[256]`——缓冲区在注册时由 `malloc` 分配。Config 只负责"传参"，Instance 负责"运行"。

---

### 1.7 公共函数声明

```c
USARTInstance *USARTRegister(USART_Init_Config_s *init_config);   // 注册，返回实例指针
void USARTServiceInit(USARTInstance *_instance);                  // 启动 DMA 接收
void USARTSend(USARTInstance *_instance, uint8_t *send_buf,
               uint16_t send_size, USART_TRANSFER_MODE mode);     // 发送数据
uint8_t USARTIsReady(USARTInstance *_instance);                   // 查询是否空闲
```

---

## 第二部分：源文件 `bsp_usart.c` — 实现

### 2.1 依赖

```c
#include "bsp_usart.h"   // 自身接口
#include "bsp_log.h"     // LOGERROR / LOGWARNING 宏
#include "stdlib.h"      // malloc / free（实际未使用 free）
#include "memory.h"      // memset
```

### 2.2 私有全局变量

```c
static uint8_t idx;                                         // [1] 当前已注册实例数
static USARTInstance *usart_instance[DEVICE_USART_CNT] = {NULL}; // [2] 实例指针数组
```

| 变量 | 作用 | 生命周期 |
|------|------|---------|
| `idx` | 计数器，注册时自增，用于遍历上限和越界保护 | 整个程序运行期 |
| `usart_instance[3]` | 保存所有已注册实例的指针 | 整个程序运行期 |

`static` 限定文件作用域——外部模块无法直接访问这两个变量，只能通过公开 API 操作。

---

### 2.3 `USARTServiceInit()` — 启动 DMA 接收

```c
void USARTServiceInit(USARTInstance *_instance)
{
```

#### 第 1 步：启动 DMA + IDLE 接收

```c
    HAL_UARTEx_ReceiveToIdle_DMA(
        _instance->usart_handle,    // 用哪个串口
        _instance->recv_buff,       // 数据目的地
        _instance->recv_buff_size   // 期望收多少字节
    );
```

**`HAL_UARTEx_ReceiveToIdle_DMA()` 内部做了什么**：

1. 配置 DMA 流：源地址 = UART 数据寄存器（`USARTx->DR`），目的地址 = `recv_buff`，传输长度 = `recv_buff_size`
2. 使能 DMA 的 TC（传输完成）、HT（半传输）、TE（传输错误）中断
3. 使能 UART 的 IDLE（总线空闲检测）中断
4. 启动 DMA 传输

**关键**：这个函数是**循环接收**的起点。调用一次后，DMA 持续监听串口——数据到达时硬件自动搬运到 `recv_buff`，**不占用 CPU**。

#### 第 2 步：关闭 DMA 半传输中断

```c
    __HAL_DMA_DISABLE_IT(_instance->usart_handle->hdmarx, DMA_IT_HT);
}
```

| 中断 | 触发条件 | 状态 |
|------|---------|:---:|
| `DMA_IT_TC` | 全部数据传输完成 | ✅ 保留 |
| `DMA_IT_HT` | 传输完成一半 | ❌ **关闭** |
| `DMA_IT_TE` | 传输错误 | ✅ 保留（用于错误恢复） |
| UART IDLE | 总线空闲 | ✅ 保留（由 UART 外设产生） |

**为何关闭 HT 中断**：HAL 库设计失误——DMA 的 TC、HT 以及 UART 的 IDLE 三种事件都触发 `HAL_UARTEx_RxEventCallback()`。若不禁用 HT，一帧 18 字节的数据在收到 9 字节和 18 字节时各触发一次回调 → **一帧触发两次处理**。

---

### 2.4 `USARTRegister()` — 注册串口实例

```c
USARTInstance *USARTRegister(USART_Init_Config_s *init_config)
{
```

#### 第 1 步：安全门检查

```c
    // 安全门 ①：实例数不能超过硬件串口数
    if (idx >= DEVICE_USART_CNT)          // idx 达到 3 → 没有更多串口可用了
        while (1)
            LOGERROR("[bsp_usart] USART exceed max instance count!");
```

`while(1)` + `LOGERROR` 是一个**手工断言**（hand-crafted assert）。一旦触发：
- 有调试器：暂停执行 → 调用栈直接定位到调用者
- 没有调试器：程序卡死 → 看门狗复位 → 你知道出事了

```c
    // 安全门 ②：同一串口不允许重复注册
    for (uint8_t i = 0; i < idx; i++)
        if (usart_instance[i]->usart_handle == init_config->usart_handle)
            while (1)
                LOGERROR("[bsp_usart] USART instance already registered!");
```

| 安全检查 | 防止什么 | 后果 |
|---------|---------|------|
| `idx >= DEVICE_USART_CNT` | 超过硬件数量 | 数组越界 |
| `usart_handle` 重复 | 同一串口注册两次 | 数据分发错乱 |

#### 第 2 步：分配实例（堆）

```c
    USARTInstance *instance = (USARTInstance *)malloc(sizeof(USARTInstance));
    memset(instance, 0, sizeof(USARTInstance));  // 清零——recv_buff 全为 0
```

| 操作 | 说明 |
|------|------|
| `malloc` | 在堆上分配约 265 字节，返回指针。**永久**存在（直到程序结束） |
| `memset(..., 0, ...)` | 清零所有字段——尤其是 `recv_buff[256]`，避免 DMA 读到旧数据/随机值 |

> 为什么不放在栈上？——调用者的局部变量在函数返回后就销毁了，但 DMA 在后台持续往 `recv_buff` 写数据，必须保证缓冲区地址**始终有效**。

#### 第 3 步：从 Config 抄数据

```c
    instance->usart_handle    = init_config->usart_handle;
    instance->recv_buff_size  = init_config->recv_buff_size;
    instance->module_callback = init_config->module_callback;
```

三行赋值，将临时的 Config 配置"固化"到永久的 Instance 中。此后 Config 可以销毁（栈弹出），Instance 独立存活。

#### 第 4 步：加入全局数组 + 启动接收

```c
    usart_instance[idx++] = instance;  // [a] 指针存入全局数组，idx 自增
    USARTServiceInit(instance);        // [b] 立刻启动 DMA 接收
    return instance;                   // [c] 返回指针给 Module 持有
}
```

---

### 2.5 `USARTSend()` — 发送数据

```c
void USARTSend(USARTInstance *_instance, uint8_t *send_buf,
               uint16_t send_size, USART_TRANSFER_MODE mode)
{
    switch (mode)
    {
```

#### 分派到三种 HAL 发送函数

```c
    case USART_TRANSFER_BLOCKING:
        HAL_UART_Transmit(_instance->usart_handle, send_buf, send_size, 100);
        break;
        // ↑ timeout = 100ms，CPU 死等直到发完或超时
```

**阻塞模式内部机制**：
```
for (i = 0; i < send_size; i++) {
    while (TXE == 0);        // 等发送数据寄存器为空
    USART->DR = send_buf[i]; // 逐字节写入
}
// CPU 被占用 ~ send_size × 位时间
```

```c
    case USART_TRANSFER_IT:
        HAL_UART_Transmit_IT(_instance->usart_handle, send_buf, send_size);
        break;
```

**中断模式内部机制**：
```
使能 TXE（发送数据寄存器空中断）
→ 函数立即返回
→ 每发送完一字节，硬件触发 TXE 中断
→ ISR 中写入下一字节
→ 全部发完 → 触发 TC（传输完成）中断
```

**注意**：连续多次调用 `IT` 或 `DMA` 发送会导致上一次未完成的发送被覆盖。如需连续发送，用 `USARTIsReady()` 判断或自行实现发送队列。

```c
    case USART_TRANSFER_DMA:
        HAL_UART_Transmit_DMA(_instance->usart_handle, send_buf, send_size);
        break;
```

**DMA 模式内部机制**：
```
配置 DMA 流：源地址 = send_buf，目的地址 = USART->DR，长度 = send_size
→ 函数立即返回
→ DMA 硬件逐字节搬运到 UART，不产生中断
→ 全部发完 → DMA_TC 中断（可选）
```

```c
    default:
        while (1)
            ; // 非法模式 → 卡死 → 调试器定位 bug
    }
}
```

#### `USARTIsReady()` — 查询空闲

```c
uint8_t USARTIsReady(USARTInstance *_instance)
{
    if (_instance->usart_handle->gState | HAL_UART_STATE_BUSY_TX)
        return 0;  // 发送忙
    else
        return 1;  // 空闲可发
}
```

`gState` 是 HAL 库的状态字。发送时被设为 `BUSY_TX`，发完恢复 `READY`。调用者用它判断是否可以发起下一次 IT/DMA 发送。

---

### 2.6 `HAL_UARTEx_RxEventCallback()` — 接收完成回调（核心！）

这是整个 bsp_usart 的**心脏**——重写了 HAL 库的 `__weak` 函数。

```c
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
```

#### 第 1 步：遍历匹配

```c
    for (uint8_t i = 0; i < idx; ++i)           // O(n) 线性遍历
    {
        if (huart == usart_instance[i]->usart_handle)  // 指针比较——极其轻量
        {
```

因最多只有 3 个实例，O(n) 遍历完全可接受。

#### 第 2 步：调用上层回调

```c
            if (usart_instance[i]->module_callback != NULL)  // [a] 安全检查：防止空指针调用
            {
                usart_instance[i]->module_callback();        // [b] ★ 通过函数指针调用 Module 层
                memset(usart_instance[i]->recv_buff, 0, Size); // [c] 清空缓冲（变长数据必需）
            }
```

| 步骤 | 说明 |
|------|------|
| `[a]` | 如果 Module 注册时没传回调，跳过——防御性编程 |
| `[b]` | BSP 层完全不知道调的是哪个模块的什么函数，只知道"调它" |
| `[c]` | 清零防止下次 DMA 收的数据和旧数据混淆（尤其变长数据场景） |

#### 第 3 步：重启 DMA 接收

```c
            HAL_UARTEx_ReceiveToIdle_DMA(
                usart_instance[i]->usart_handle,
                usart_instance[i]->recv_buff,
                usart_instance[i]->recv_buff_size
            );
            __HAL_DMA_DISABLE_IT(usart_instance[i]->usart_handle->hdmarx, DMA_IT_HT);
```

**为什么要重启**：`HAL_UARTEx_ReceiveToIdle_DMA()` 配置的是一次性传输。DMA 传完 `recv_buff_size` 个字节后自动停止。必须在回调中重新启动，形成循环接收。

**为什么要再关 HT 中断**：`HAL_UARTEx_ReceiveToIdle_DMA()` 内部会重新使能所有 DMA 中断（包括 HT），所以每次重调后必须再关一次。

#### 第 4 步：提前退出

```c
            return;  // 找到匹配的实例，不必再遍历
        }
    }
}
```

---

### 2.7 `HAL_UART_ErrorCallback()` — 错误恢复

```c
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < idx; ++i)           // 同样 O(n) 遍历匹配
    {
        if (huart == usart_instance[i]->usart_handle)
        {
            HAL_UARTEx_ReceiveToIdle_DMA(/* ... */);  // ① 重启 DMA 接收
            __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);    // ② 再关 HT
            LOGWARNING("[bsp_usart] USART error callback triggered, instance idx [%d]", i);
            return;
        }
    }
}
```

| 常见错误 | 原因 |
|---------|------|
| 奇偶校验错 | 线路干扰 |
| 溢出错误 | 数据速率超过处理速率 |
| 帧错误 | 停止位不符 / 波特率不匹配 |

发生这些错误后，HAL 库会停掉当前 DMA 接收。此回调做的唯一事情：**重新启动接收**，让串口从错误中恢复。

---

## 第三部分：完整数据流（从硬件到应用）

### 接收方向

```
[硬件] UART 引脚收到数据 → 移位寄存器 → UART 数据寄存器
   │
[DMA] 自动搬运：UART_DR → recv_buff[]
   │  (每收到一个完整字节就搬一次，不需要 CPU 参与)
   │
[条件触发]之一：
   ├─ DMA 传完了 recv_buff_size 个字节  → DMA_TC 中断
   └─ UART 线路上出现空闲（一个字节时间内无数据）→ UART_IDLE 中断
   │
[HAL 库] HAL_UART_IRQHandler() → 清标志 → 调用 __weak 回调
   │
[bsp_usart] HAL_UARTEx_RxEventCallback(huart, Size)
   → 遍历 usart_instance[] 匹配 huart
   → module_callback()  ← 调 Module 层
   → memset 清缓冲
   → HAL_UARTEx_ReceiveToIdle_DMA()  ← 重启接收
   → __HAL_DMA_DISABLE_IT(..., DMA_IT_HT)  ← 关 HT
   │
[Module 层] MyRxCallback()
   → 读 recv_buff  → 解析协议 → 更新状态
```

### 发送方向

```
[Module 层]
   → 准备好数据
   → USARTSend(instance, data, len, USART_TRANSFER_DMA)
   │
[bsp_usart]
   → switch(mode) → HAL_UART_Transmit_DMA(huart, data, len)
   │
[HAL + 硬件]
   → DMA 配置：mem → USART_DR，长度 len
   → DMA 逐字节搬运
   → UART 逐位输出到 TX 引脚
```

---

## 第四部分：全部函数速查表

| 函数 | 位置 | 类别 | 功能 | 关键内部操作 |
|------|------|------|------|------------|
| `USARTServiceInit()` | `.c:29` | 公开 | 启动 DMA 循环接收 | `HAL_UARTEx_ReceiveToIdle_DMA()` + 关 HT 中断 |
| `USARTRegister()` | `.c:38` | 公开 | 注册实例 | `malloc` → 抄 Config → 存数组 → `USARTServiceInit()` |
| `USARTSend()` | `.c:62` | 公开 | 发送数据 | `switch(mode)` → 三种 HAL 发送函数 |
| `USARTIsReady()` | `.c:83` | 公开 | 查询空闲 | 读 `gState` 寄存器 |
| `HAL_UARTEx_RxEventCallback()` | `.c:102` | HAL `__weak` 重写 | 接收完成分发 | 遍历 → 调 `module_callback()` → 重启 DMA |
| `HAL_UART_ErrorCallback()` | `.c:127` | HAL `__weak` 重写 | 错误恢复 | 遍历 → 重启 DMA |

---

## 第五部分：全部数据类型速查表

| 类型 | 位置 | 类别 | 作用 |
|------|------|------|------|
| `usart_module_callback` | `.h:11` | 函数指针类型 | 统一回调签名 |
| `USART_TRANSFER_MODE` | `.h:14` | 枚举 | 三种发送模式 |
| `USARTInstance` | `.h:24` | 结构体 | 永久运行实例（含 `recv_buff[256]`） |
| `USART_Init_Config_s` | `.h:33` | 结构体 | 临时配置参数（仅 3 个字段） |
| `DEVICE_USART_CNT` | `.h:7` | 宏 | 最大串口数 = 3 |
| `USART_RXBUFF_LIMIT` | `.h:8` | 宏 | 最大缓冲 = 256 字节 |
