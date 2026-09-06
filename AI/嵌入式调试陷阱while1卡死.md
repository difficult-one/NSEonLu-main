# 嵌入式调试陷阱：`while(1);` 卡死

> `USARTSend()` 中 `default` 块的 `while(1);` 是做什么的？

---

## 1. 源码

`bsp/usart/bsp_usart.c:62-80`：

```c
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
        while (1)
            ; // illegal mode! check your code context!
        break;
    }
}
```

---

## 2. 作用

`default` + `while(1);` 是一个**手工断言（hand-crafted assert）**，用于在开发阶段立刻捕获非法参数。

`mode` 只可能是 3 个值之一：

```c
USART_TRANSFER_BLOCKING = 1
USART_TRANSFER_IT       = 2
USART_TRANSFER_DMA      = 3
```

如果 `mode` 是其他任何值（0、4、255、随机值），说明出现了以下 bug 之一：

| 可能原因 | 说明 |
|---------|------|
| **野指针写入** | 某处指针越界，踩到了存放 `mode` 的栈位置 |
| **数组越界写入** | `arr[i]` 越界，改写了相邻栈变量 |
| **未初始化变量** | `mode` 对应的枚举变量声明后没赋值就被传入 |
| **类型转换错误** | 把其他类型的值强转成了 `USART_TRANSFER_MODE` |

---

## 3. 为什么用死循环而不是干别的

| 做法 | 效果 |
|------|------|
| **`while(1);` 死循环** | ✅ 程序**立刻卡死** → 调试器断下来 → 调用栈直指出问题的那次调用 |
| `break;` 静默跳过 | ❌ bug 被忽略，后续表现诡异，排查难度指数增长 |
| `return;` 返回 | ❌ 调用者以为发送成功，继续执行 → 数据丢了找不到原因 |
| `assert(0)` | ✅ 同样效果，但 Keil/IAR 的微控制器项目通常没标准 `assert` |

---

## 4. 调试流程

```
程序卡死在 while(1);
  → 调试器暂停
    → 查看调用栈 (Call Stack)
      → 找到是哪个调用者传了非法 mode 值
        → 检查那个调用者的局部变量
          → memory watch 观察 mode 附近的内存
            → 找到是谁越界写入 / 未初始化
```

---

## 5. 本项目中的同类用法

同样的模式在本项目中出现在多个 BSP 层的安全检查和断言中：

```c
// bsp_can.c:71 — 实例数超限
if (idx >= CAN_MX_REGISTER_CNT)
    while (1) LOGERROR("[bsp_can] CAN instance exceeded MAX num!");

// bsp_can.c:77 — 重复注册
if (can_instance[i]->rx_id == config->rx_id && /* ... */)
    while (1) LOGERROR("[bsp_can] CAN id crash!");

// bsp_can.c:135 — CAN DLC 非法值
if (length > 8 || length == 0)
    while (1) LOGERROR("[bsp_can] CAN DLC error! check your code");

// bsp_usart.c:41 — 实例数超限
if (idx >= DEVICE_USART_CNT)
    while (1) LOGERROR("[bsp_usart] USART exceed max instance count!");

// bsp_usart.c:46 — 重复注册
if (usart_instance[i]->usart_handle == init_config->usart_handle)
    while (1) LOGERROR("[bsp_usart] USART instance already registered!");
```

---

## 6. 本质

> 嵌入式 C 里的 **手工 assert**——用最小代价（几字节代码）换取一个**明确的崩溃点**，而不是让 bug 带着随机的行为继续传播到下游逻辑中。

配合 **看门狗（IWDG）** 使用：卡死后自动复位 → 系统重启 → 你知道出了问题。
配合 **调试器（JLink/DAP）** 使用：卡死瞬间暂停 → 调用栈精确定位 bug 来源。
