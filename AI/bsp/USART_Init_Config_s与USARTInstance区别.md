# USART_Init_Config_s 与 USARTInstance 的区别

> 一句话说清楚：**`USART_Init_Config_s` 是临时的"配置单"，用完就扔；`USARTInstance` 是永久的"运行时实例"，存数据、供后续操作。**

---

## 1. 定义对比

**`USART_Init_Config_s`**（`bsp_usart.h:33-38`）：

```c
typedef struct
{
    uint8_t recv_buff_size;                // 接收一包的大小
    UART_HandleTypeDef *usart_handle;      // 用哪个串口
    usart_module_callback module_callback; // 回调函数
} USART_Init_Config_s;                     // ← 只有 3 个初始化参数
```

**`USARTInstance`**（`bsp_usart.h:24-30`）：

```c
typedef struct
{
    uint8_t recv_buff[USART_RXBUFF_LIMIT]; // ★ 多了接收缓冲区（256 字节）
    uint8_t recv_buff_size;
    UART_HandleTypeDef *usart_handle;
    usart_module_callback module_callback;
} USARTInstance;                           // ← 有实际存储空间
```

唯一区别：`USARTInstance` 多了一个 **`recv_buff[USART_RXBUFF_LIMIT]`（256 字节）接收缓冲区**。

---

## 2. 为什么要分成两个？

`USARTRegister()` 的逻辑（`bsp_usart.c:38-59`）展示了原因：

```c
USARTInstance *USARTRegister(USART_Init_Config_s *init_config)
{
    // 1. 分配堆内存，创建永久的运行时实例
    USARTInstance *instance = (USARTInstance *)malloc(sizeof(USARTInstance));
    memset(instance, 0, sizeof(USARTInstance));

    // 2. 把临时配置单的内容抄到永久实例里
    instance->usart_handle   = init_config->usart_handle;
    instance->recv_buff_size = init_config->recv_buff_size;
    instance->module_callback = init_config->module_callback;
    // recv_buff 已经由 malloc + memset 分配好了（全零），不用从 config 抄

    // 3. 存入 BSP 层的全局数组，config 使命完成
    usart_instance[idx++] = instance;
    USARTServiceInit(instance);   // 启动 DMA 接收，DMA 就往 recv_buff 里写数据了
    return instance;              // 返回永久实例给上层模块持有
}
```

---

## 3. 生命周期与职责对比

| 维度 | `USART_Init_Config_s` | `USARTInstance` |
|------|----------------------|----------------|
| **用途** | 传参给注册函数 | 运行时实际使用的数据结构 |
| **生命周期** | 函数调用期间（通常是栈上的临时变量） | 永久（`malloc` 在堆上，程序结束前一直存在） |
| **包含缓冲区** | ❌ 没有 `recv_buff` | ✅ `recv_buff[256]` |
| **谁持有** | 没人持有，用完即销毁 | Module 持有指针；BSP 层全局数组也持有引用 |
| **类比** | 酒店入住登记表 | 实际的房间 + 房卡 |
| **数据流向** | 单向：调用者 → BSP 层 | 双向：BSP 层 ↔ 模块（BSP 写接收数据，模块读） |

---

## 4. 典型使用方式

```c
// 模块初始化时，先在栈上创建配置单
USART_Init_Config_s config = {
    .usart_handle   = &huart3,
    .recv_buff_size = 18,
    .module_callback = MyRxCallback,
};

// 注册，得到永久实例
USARTInstance *my_uart = USARTRegister(&config);
// config 现在可以安全销毁了（比如函数返回后栈帧弹出）

// 后续所有操作都用 my_uart 这个永久实例
USARTSend(my_uart, data, len, USART_TRANSFER_IT);
```

---

## 5. CAN 也是同样的模式

`CAN_Init_Config_s` vs `CANInstance` 的关系完全一样：

```
CAN_Init_Config_s  →  CANRegister()  →  CANInstance（含 tx_buff[8]、rx_buff[8]、txconf 等运行状态）
USART_Init_Config_s → USARTRegister() → USARTInstance（含 recv_buff[256]）
```

CAN 的 Instance 额外多了 `tx_buff[8]`、`rx_buff[8]`、`txconf`（发送配置）、`rx_len`（接收长度）等**运行时状态字段**，这些在 Config 里都不需要出现。

---

## 6. 设计模式总结

这是一种在嵌入式/系统编程中非常常见的惯用法：

| 模式 | 说明 |
|------|------|
| **Config 结构体** | 定义"想要什么"——静态的初始化需求 |
| **Instance 结构体** | 承载"实际有什么"——动态的运行时状态 |

**为什么这样设计？**

1. **接口清晰**：Config 只包含调用者需要关心的初始化参数，Instance 的内部实现细节（如缓冲区）对调用者透明
2. **内存由 BSP 管理**：Instance 通过 `malloc` 在堆上分配，BSP 层统一管理生命周期，调用者不关心内存从哪来
3. **单一职责**：Config 只负责"传参"，Instance 负责"运行"——职责分离
4. **易于扩展**：给 Instance 加字段不影响已有的初始化代码；给 Config 加参数只需在注册函数里多抄一行
