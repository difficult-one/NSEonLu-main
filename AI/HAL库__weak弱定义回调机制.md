# HAL 库的 `__weak` 弱定义回调机制

> 通过弱定义函数与用户重写的方式，将硬件事件与用户逻辑解耦。

---

## 1. 什么是 `__weak`

`__weak` 是一个编译器属性（ARM-GCC 支持）：

```
链接规则：
  - 如果用户定义了同名函数 → 链接用户的（强的覆盖弱的）
  - 如果用户没定义            → 链接 HAL 库的（默认空实现）
```

---

## 2. 完整调用链（以 CAN 为例）

```
硬件中断 (CAN 控制器收到数据)
  │
  ▼
CAN1_RX0_IRQHandler()              [启动文件，汇编]
  │
  ▼
HAL_CAN_IRQHandler(&hcan1)         [stm32f4xx_hal_can.c — HAL 库]
  │  读取寄存器、清中断标志、判断是哪个 FIFO
  ▼
HAL_CAN_RxFifo0MsgPendingCallback(hcan)  [__weak 声明，默认空函数]
  │                                        ↑
  │                            bsp_can.c 中重写了同名函数 ←
  ▼
CANFIFOxCallback(hcan, CAN_RX_FIFO0)      [bsp_can.c — BSP 层]
  │  遍历已注册实例，匹配 hcan + rx_id
  ▼
can_module_callback(instance)             [函数指针 — BSP 调 Module]
  │
  ▼
CANCommRxCallback() / DecodeDJIMotor()    [Module 层 — 业务逻辑]
```

---

## 3. 为什么要这样设计？三层含义

### 3.1 解耦——不改 HAL 源码

没有 `__weak` 时，只能去改 `stm32f4xx_hal_can.c` 才能响应中断：

```c
// ❌ 反模式：直接改 HAL 源码
void HAL_CAN_IRQHandler(CAN_HandleTypeDef *hcan)
{
    // ... HAL 的寄存器操作 ...
    DecodeDJIMotor(...);  // 硬编码在 HAL 里
}
// HAL 库一升级，你的修改全没了
```

有了 `__weak`，在**自己的文件**里写同名函数即可：

```c
// ✅ 在 bsp_can.c 里重写
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CANFIFOxCallback(hcan, CAN_RX_FIFO0);
}
// HAL 库随便升级，你的代码不受影响
```

### 3.2 异步事件处理——不阻塞主循环

数据随时可能到达，不能轮询等它：

```c
// 同步（❌ 浪费 CPU）：
while (1) { if (数据到了) 处理();  做其他事(); }

// 异步（✅ 回调）：
// 主循环正常跑，数据到了 → 中断 → 回调自动执行 → 处理完回到主循环
```

### 3.3 分层职责清晰

```
HAL 层：   我帮你处理了寄存器，数据准备好了，你自己取。
           （怎么用我不管——我调你的回调就行）

BSP 层：   我帮你维护了实例注册，匹配到了就分发给对应模块。
           （模块拿数据做什么我不管——我调模块的回调就行）

Module 层：我拿到数据了，解析、计算、控制。
           （我不管数据怎么来的）
```

---

## 4. 本项目重写的 HAL `__weak` 回调

| HAL 回调函数 | 重写位置 | 触发事件 |
|-------------|---------|---------|
| `HAL_CAN_RxFifo0MsgPendingCallback` | `bsp_can.c:184` | CAN FIFO0 收到数据 |
| `HAL_CAN_RxFifo1MsgPendingCallback` | `bsp_can.c:194` | CAN FIFO1 收到数据 |
| `HAL_UARTEx_RxEventCallback` | `bsp_usart.c:102` | UART DMA/IDLE 接收完成 |
| `HAL_UART_ErrorCallback` | `bsp_usart.c:127` | UART 接收出错 |

---

## 5. 与 BSP 层回调函数指针的区别

这是**两层不同的回调**，不要混淆：

```
第一层：HAL __weak 重写    第二层：BSP 函数指针注册
─────────────────────    ─────────────────────
硬件 → HAL 中断服务      HAL 回调 → 具体模块
(编译链接时覆盖)          (运行时注册)
```

```c
// 第一层（编译期绑定）
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)  // ← 重写 __weak
{
    CANFIFOxCallback(hcan, CAN_RX_FIFO0);
}

// 第二层（运行时绑定）
// CANFIFOxCallback 内部：
can_instance[i]->can_module_callback(can_instance[i]);  // ← 函数指针调用
```

| 维度 | `__weak` 重写 | 函数指针注册 |
|------|-------------|------------|
| 绑定时机 | 编译/链接期 | 运行时 |
| 关系 | 一对一（一个 HAL 函数只能重写一次） | 一对多（一个 CAN 总线挂多个模块） |
| 解决的问题 | HAL 库代码和用户代码分离 | 一个外设复用多个模块 |
