# BSP USART — UART 通信封装（DMA + IDLE）

## 模块职责

基于 HAL 库的 UART 扩展接收接口（`HAL_UARTEx_ReceiveToIdle_DMA`），实现 DMA + IDLE 线空闲检测的不定长数据接收，并提供多种发送模式。

---

## 设计思路

### 为什么用 DMA + IDLE？

- **DMA 接收** — CPU 不参与数据搬运，降低中断负担
- **IDLE 线空闲检测** — 当 UART 总线空闲一个字符时间后触发中断，自动获得本次接收的数据长度，解决了"不知道一包数据多长"的问题
- **组合效果** — 不需要事先知道包长度，也不需要轮询，收到一包数据自动触发回调

### 为什么关闭 DMA Half Transfer 中断？

这是 HAL 库的一个设计缺陷：

- `HAL_UARTEx_ReceiveToIdle_DMA()` 会使能三种中断：DMA 传输完成、DMA 半传输完成、UART IDLE
- 三种中断都会调用 `HAL_UARTEx_RxEventCallback()`
- 半传输完成（收到一半数据时）也会触发回调，这不是我们想要的
- 解决方案：注册后立即 `__HAL_DMA_DISABLE_IT(handle, DMA_IT_HT)` 关闭半传输中断

---

## 核心数据结构

### USART_TRANSFER_MODE 枚举

```c
// basic_framework/bsp/usart/bsp_usart.h:15-20
typedef enum
{
    USART_TRANSFER_NONE = 0,
    USART_TRANSFER_BLOCKING,  // 阻塞发送，等待发送完成
    USART_TRANSFER_IT,        // 中断发送，非阻塞
    USART_TRANSFER_DMA,       // DMA 发送，非阻塞，CPU 开销最小
} USART_TRANSFER_MODE;
```

### USARTInstance

```c
// basic_framework/bsp/usart/bsp_usart.h:23-30
typedef struct
{
    uint8_t recv_buff[USART_RXBUFF_LIMIT]; // 接收缓冲区，默认 256 字节
    uint8_t recv_buff_size;                // 一包数据的最大长度
    UART_HandleTypeDef *usart_handle;      // UART 句柄
    usart_module_callback module_callback; // 接收回调函数
} USARTInstance;
```

逐字段说明：

| 字段 | 作用 | 备注 |
|------|------|------|
| `recv_buff[256]` | DMA 接收的目标缓冲区 | IDLE 中断时此缓冲区已包含数据 |
| `recv_buff_size` | 告诉 DMA 一次最多接收多少字节 | 通常设为协议最大帧长 |
| `usart_handle` | 区分 UART1/2/3 等 | 回调分发时用于匹配 |
| `module_callback` | 接收完成后的回调 | 注意：无参数！回调内需自行从 instance 读取数据 |

### USART_Init_Config_s

```c
// basic_framework/bsp/usart/bsp_usart.h:33-38
typedef struct
{
    uint8_t recv_buff_size;
    UART_HandleTypeDef *usart_handle;
    usart_module_callback module_callback;
} USART_Init_Config_s;
```

### 回调类型

```c
// basic_framework/bsp/usart/bsp_usart.h:11
typedef void (*usart_module_callback)();  // 注意：无参数！
```

**重要**：与 CAN 的回调（传入 `CANInstance*`）不同，USART 回调没有任何参数。Module 层需要自行保存实例指针，在回调中通过全局/静态变量访问接收缓冲区。

---

## 函数详解

### USARTServiceInit()

```c
// basic_framework/bsp/usart/bsp_usart.c:29-36
void USARTServiceInit(USARTInstance *_instance)
{
    HAL_UARTEx_ReceiveToIdle_DMA(_instance->usart_handle,
                                  _instance->recv_buff,
                                  _instance->recv_buff_size);
    // 关闭 DMA 半传输中断，防止收到一半数据就触发回调
    __HAL_DMA_DISABLE_IT(_instance->usart_handle->hdmarx, DMA_IT_HT);
}
```

- **功能**：启动 DMA 接收 + IDLE 检测
- **调用时机**：每个实例注册后自动调用
- **关键**：第二行关闭 DMA_HT 中断是必须的，否则回调会被调用两次

### USARTRegister()

```c
// basic_framework/bsp/usart/bsp_usart.c:38-59
USARTInstance *USARTRegister(USART_Init_Config_s *init_config)
{
    if (idx >= DEVICE_USART_CNT)   // 最多 3 个串口
        while (1) LOGERROR("...");

    for (uint8_t i = 0; i < idx; i++)  // 检查重复注册
        if (usart_instance[i]->usart_handle == init_config->usart_handle)
            while (1) LOGERROR("...");

    USARTInstance *instance = (USARTInstance *)malloc(sizeof(USARTInstance));
    memset(instance, 0, sizeof(USARTInstance));

    instance->usart_handle = init_config->usart_handle;
    instance->recv_buff_size = init_config->recv_buff_size;
    instance->module_callback = init_config->module_callback;

    usart_instance[idx++] = instance;
    USARTServiceInit(instance);  // 注册后立即启动接收
    return instance;
}
```

- **重复注册检测** — 同一个 UART 句柄不能注册两次（UART 是独占资源）
- **自动启动接收** — 注册完就进入 DMA 接收模式

### USARTSend()

```c
// basic_framework/bsp/usart/bsp_usart.c:62-80
void USARTSend(USARTInstance *_instance, uint8_t *send_buf,
               uint16_t send_size, USART_TRANSFER_MODE mode)
{
    switch (mode)
    {
    case USART_TRANSFER_BLOCKING:
        HAL_UART_Transmit(_instance->usart_handle, send_buf, send_size, 100);
        break;
    case USART_TRANSFER_IT:
        HAL_UART_Transmit_IT(_instance->usart_handle, send_buf, send_size);
        break;
    case USART_TRANSFER_DMA:
        HAL_UART_Transmit_DMA(_instance->usart_handle, send_buf, send_size);
        break;
    default:
        while (1);  // 非法模式
    }
}
```

- **三种模式对比**：
  - `BLOCKING`：简单，但会阻塞当前任务直到发送完成
  - `IT`：中断驱动，非阻塞，但短时间连续调用会丢失前一次
  - `DMA`：最高效，适合大数据量，但同样不能连续调用
- **100ms 超时** — 阻塞模式的超时时间硬编码为 100ms

### USARTIsReady()

```c
// basic_framework/bsp/usart/bsp_usart.c:83-89
uint8_t USARTIsReady(USARTInstance *_instance)
{
    if (_instance->usart_handle->gState | HAL_UART_STATE_BUSY_TX)
        return 0;
    else
        return 1;
}
```

**这里有 Bug！** 应该用 `&`（位与）而非 `|`（位或）：

- `gState | HAL_UART_STATE_BUSY_TX` 的结果永远为真（非零），因为 `gState` 至少为 `HAL_UART_STATE_READY`（0x20）或 `HAL_UART_STATE_BUSY_TX`（0x21）
- 正确写法应为 `gState & HAL_UART_STATE_BUSY_TX`

**影响**：此函数永远返回 0，表示串口始终忙。如果 Module 层用此函数判断是否可以发送，将永远无法发送。

### HAL_UARTEx_RxEventCallback()

```c
// basic_framework/bsp/usart/bsp_usart.c:102-118
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    for (uint8_t i = 0; i < idx; ++i)
    {
        if (huart == usart_instance[i]->usart_handle)  // 匹配 UART 句柄
        {
            if (usart_instance[i]->module_callback != NULL)
            {
                usart_instance[i]->module_callback();  // 调用 Module 回调
                memset(usart_instance[i]->recv_buff, 0, Size); // 清空缓冲区
            }
            // 重新启动 DMA 接收
            HAL_UARTEx_ReceiveToIdle_DMA(usart_instance[i]->usart_handle,
                                          usart_instance[i]->recv_buff,
                                          usart_instance[i]->recv_buff_size);
            __HAL_DMA_DISABLE_IT(usart_instance[i]->usart_handle->hdmarx, DMA_IT_HT);
            return;
        }
    }
}
```

- **匹配方式** — 只按 `usart_handle` 匹配（串口是独占的，一个句柄只对应一个实例）
- **回调无参数** — Module 层在回调中需要自行读取 `recv_buff`（通过保存的实例指针）
- **memset 清空** — 对变长数据协议是必要的，防止旧数据残留
- **重启 DMA** — 回调结束后必须重新启动 DMA 接收，否则后续数据丢失
- **再次关闭 HT** — 重启 DMA 后 HT 中断又被使能了，需要再次关闭

### HAL_UART_ErrorCallback()

```c
// basic_framework/bsp/usart/bsp_usart.c:127-139
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < idx; ++i)
    {
        if (huart == usart_instance[i]->usart_handle)
        {
            // 错误恢复：重新启动 DMA 接收
            HAL_UARTEx_ReceiveToIdle_DMA(usart_instance[i]->usart_handle,
                                          usart_instance[i]->recv_buff,
                                          usart_instance[i]->recv_buff_size);
            __HAL_DMA_DISABLE_IT(usart_instance[i]->usart_handle->hdmarx, DMA_IT_HT);
            LOGWARNING("[bsp_usart] USART error callback triggered, instance idx [%d]", i);
            return;
        }
    }
}
```

- **常见错误**：奇偶校验错误、溢出错误（Overrun）、帧错误
- **恢复策略**：直接重启 DMA 接收，丢弃错误状态
- **不重启的后果**：UART 进入错误状态后停止接收，数据永久丢失

---

## 调用链

### 接收路径

```
UART 硬件接收到数据
  +-- DMA 自动搬运到 recv_buff
  +-- IDLE 线空闲 或 DMA 传输完成 触发中断
  +-- HAL 中断处理
  +-- HAL_UARTEx_RxEventCallback(huart, Size)
        +-- 遍历 usart_instance[] 匹配 huart
        +-- module_callback()              // 回调到 Module 层
        |     +-- 如视觉协议解析/遥控器解析
        +-- memset(recv_buff, 0, Size)     // 清空缓冲区
        +-- HAL_UARTEx_ReceiveToIdle_DMA() // 重启 DMA
        +-- __HAL_DMA_DISABLE_IT(HT)       // 再次关闭 HT 中断
```

### 发送路径

```
Module 层
  +-- USARTSend(instance, buf, len, USART_TRANSFER_DMA)
        +-- HAL_UART_Transmit_DMA(handle, buf, len)
              +-- DMA 控制器自动搬运数据到 UART 发送寄存器
```

### 注册路径

```
Module 层
  +-- USARTRegister(&config)
        +-- malloc + memset
        +-- 配置 handle / buff_size / callback
        +-- USARTServiceInit()
        |     +-- HAL_UARTEx_ReceiveToIdle_DMA()
        |     +-- __HAL_DMA_DISABLE_IT(HT)
        +-- return instance
```

---

## 注意事项

1. **回调函数无参数** — 与 CAN/IIC 不同，USART 回调是 `void (*)()` 类型，Module 层需要自行保存实例指针来读取 `recv_buff`。这是一个设计上的不一致，使用时需特别注意。
2. **USARTIsReady() 有 Bug** — 使用了 `|` 而非 `&`，导致函数永远返回 0。如果需要判断发送是否完成，需自行实现或修复此函数。
3. **DMA 发送不保证连续** — 短时间内连续调用 `USARTSend()` 使用 IT/DMA 模式，前一次未完成时新的发送会被取消。需要配合 `USARTIsReady()`（修复后）或自行实现发送队列。
4. **memset 清空的必要性** — 对于定长协议可能不需要，但对于变长数据（如视觉协议），不清空可能导致旧数据残留被误读。
5. **错误恢复必须重启 DMA** — 如果不处理 `HAL_UART_ErrorCallback`，UART 出错后将永久停止接收。这是 HAL 库的行为，不是框架的 Bug。
6. **串口是独占资源** — 一个 UART 外设只能注册一个实例，与 CAN 不同（CAN 一个外设可注册多个实例）。
7. **recv_buff_size 设置** — 应设为协议规定的最大帧长。设置过小会截断数据，过大会浪费内存。
