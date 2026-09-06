# DMA 中断类型与 HAL 库 DMA 中断陷阱

---

## 1. STM32F4 的 5 种 DMA 中断

定义在 `stm32f4xx_hal_dma.h:349-353`：

| 宏定义 | 全称 | 触发条件 | 寄存器位位置 |
|--------|------|---------|:---:|
| `DMA_IT_TC` | **T**ransfer **C**omplete | 数据传输**全部完成** | `DMA_SxCR.TCIE` |
| `DMA_IT_HT` | **H**alf **T**ransfer | 数据传输**完成一半**（50%） | `DMA_SxCR.HTIE` |
| `DMA_IT_TE` | **T**ransfer **E**rror | 总线错误 / 访问非法地址 | `DMA_SxCR.TEIE` |
| `DMA_IT_DME` | **D**irect **M**ode **E**rror | 直接模式下访问冲突 | `DMA_SxCR.DMEIE` |
| `DMA_IT_FE` | **F**IFO **E**rror | FIFO 溢出或下溢 | `DMA_SxFCR.FEIE`（第 7 位，0x80） |

**CR 寄存器和 FCR 寄存器**：

```
DMA_SxCR  (控制寄存器)
  ┌──────┬──────┬──────┬──────┐
  │ TCIE │ HTIE │ TEIE │ DMEIE│  ← 前 4 种使能位在 CR
  └──────┴──────┴──────┴──────┘

DMA_SxFCR (FIFO 控制寄存器)
  ┌──────┐
  │ FEIE │                         ← FIFO 错误使能位在 FCR
  └──────┘
```

---

## 2. `__HAL_DMA_DISABLE_IT` 宏的实现

`stm32f4xx_hal_dma.h:591`：

```c
#define __HAL_DMA_DISABLE_IT(__HANDLE__, __INTERRUPT__)  \
    (((__INTERRUPT__) != DMA_IT_FE) ?                     \
     ((__HANDLE__)->Instance->CR  &= ~(__INTERRUPT__)) :  \   // 前 4 种 → 清 CR
     ((__HANDLE__)->Instance->FCR &= ~(__INTERRUPT__)))      // FE → 清 FCR
```

因为 `DMA_IT_FE` 的使能位在 `FCR` 寄存器（不在 `CR`），所以宏内部做了判断分流。

---

## 3. HAL 库的 DMA 接收陷阱

### 问题

`HAL_UARTEx_ReceiveToIdle_DMA()` 开启 DMA 接收后，**三种事件**都会触发 `HAL_UARTEx_RxEventCallback()`：

| 事件 | 触发时机 |
|------|---------|
| `DMA_IT_HT` — 半传输完成 | 数据传了**一半**时 |
| `DMA_IT_TC` — 全传输完成 | **全部**数据传完 |
| UART IDLE — 总线空闲 | 数据帧之间出现空闲 |

### 后果

```c
// 假设一次收 18 字节：
HAL_UARTEx_ReceiveToIdle_DMA(huart, buf, 18);

// 数据到达 9 字节  → DMA_IT_HT 中断  → ① 触发回调！
// 数据到达 18 字节 → DMA_IT_TC 中断  → ② 又触发回调！
//                           IDLE      → ③ 也可能触发！
```

**一帧数据触发多次回调**，而用户只想收到完整数据后处理一次。

### 解决方法

`bsp_usart.c:35`（每次启动 DMA 后都执行）：

```c
HAL_UARTEx_ReceiveToIdle_DMA(_instance->usart_handle,
                             _instance->recv_buff,
                             _instance->recv_buff_size);

// 立刻关掉半传输中断，只留 TC 和 IDLE
__HAL_DMA_DISABLE_IT(_instance->usart_handle->hdmarx, DMA_IT_HT);
```

### 为什么每次都要关？

`HAL_UARTEx_ReceiveToIdle_DMA()` 内部会**重新使能所有 DMA 中断**，所以每次调用后都要再关一次 HT。这也是为什么 `bsp_usart.c` 中这个调用出现了 3 处（第 35、114、134 行）。

### 本项目中的中断最终配置

| DMA 中断 | 状态 | 用途 |
|---------|:---:|------|
| `DMA_IT_TC` | ✅ 保持开启 | 全部接收完 → 触发回调 |
| `DMA_IT_HT` | ❌ 手动关闭 | 不需要"收到一半"的通知 |
| `DMA_IT_TE` | ✅ 保持开启 | 出错 → `HAL_UART_ErrorCallback` 恢复 |
| `DMA_IT_DME` | 默认关闭 | 没用直接模式 FIFO |
| `DMA_IT_FE` | 默认关闭 | 没启用 FIFO |

---

## 4. 对应代码位置

```c
// bsp_usart.c:29 — 启动接收时
void USARTServiceInit(USARTInstance *_instance)
{
    HAL_UARTEx_ReceiveToIdle_DMA(/* ... */);
    __HAL_DMA_DISABLE_IT(_instance->usart_handle->hdmarx, DMA_IT_HT);  // ←
}

// bsp_usart.c:113 — 接收完成回调后重启
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    // ...
    HAL_UARTEx_ReceiveToIdle_DMA(/* ... */);       // 重启
    __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT); // 又关一次 ←
}

// bsp_usart.c:133 — 错误恢复后重启
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    // ...
    HAL_UARTEx_ReceiveToIdle_DMA(/* ... */);       // 重启
    __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT); // 又关一次 ←
}
```
