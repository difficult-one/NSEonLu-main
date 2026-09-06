# CANTransmit 函数逻辑分析

> `bsp/can/bsp_can.c:106-129`

---

## 源码

```c
uint8_t CANTransmit(CANInstance *_instance, float timeout)
{
    static uint32_t busy_count;                                   // ① 记录拥堵次数
    static volatile float wait_time __attribute__((unused));       // 消除编译器警告
    float dwt_start = DWT_GetTimeline_ms();                       // ② 记录开始时间

    // ③ 等待空闲邮箱
    while (HAL_CAN_GetTxMailboxesFreeLevel(_instance->can_handle) == 0)
    {
        if (DWT_GetTimeline_ms() - dwt_start > timeout)           // ④ 超时判断
        {
            LOGWARNING("[bsp_can] CAN MAILbox full! failed to add msg to mailbox. Cnt [%d]", busy_count);
            busy_count++;
            return 0;                                              // ⑤ 超时 → 发送失败
        }
    }
    wait_time = DWT_GetTimeline_ms() - dwt_start;

    // ⑥ 将报文投入硬件邮箱
    if (HAL_CAN_AddTxMessage(_instance->can_handle,
            &_instance->txconf, _instance->tx_buff,
            &_instance->tx_mailbox))
    {
        LOGWARNING("[bsp_can] CAN bus BUS! cnt:%d", busy_count);
        busy_count++;
        return 0;                                                  // ⑦ 仲裁失败 → 发送失败
    }
    return 1;                                                      // ⑧ 发送成功
}
```

---

## 逐步拆解

### ② 记时：`DWT_GetTimeline_ms()`

用 DWT（Data Watchpoint and Trace）硬件定时器记录当前时间（毫秒级）。不用 `HAL_GetTick()` 是因为 DWT 精度更高，且不受中断优先级影响。

### ③ 等待邮箱：`while (... == 0)`

STM32F4 的 BxCAN 外设只有 **3 个发送邮箱**（Mailbox 0/1/2）。如果 3 个邮箱都塞满了（之前的帧还在仲裁/发送中），就必须等待。

```
CAN 发送邮箱示意：
┌──────────┬──────────┬──────────┐
│ Mailbox0 │ Mailbox1 │ Mailbox2 │   ← 只有 3 个
│  空闲 ✅  │  占用 ❌  │  占用 ❌  │   ← FreeLevel = 1
└──────────┴──────────┴──────────┘
```

多个模块共享一条 CAN 总线时，大家同时往里面塞报文，3 个邮箱可能瞬间被占满。

### ④+⑤ 超时保护

等不到空闲邮箱 → 超过 `timeout` 毫秒 → 放弃本次发送，返回 0。

`timeout` 通常设得很小（1ms 甚至 0.2ms）。注释明确要求：

> 超时时间不应该超过调用此函数的任务的周期，否则会导致任务阻塞

在 RTOS 中，一个任务在 `CANTransmit` 里死等，整个任务的执行周期就被拖垮，影响实时性。

### ⑥ `HAL_CAN_AddTxMessage()` — 投递报文

参数全部来自 `CANInstance` 中预配的值：

| 参数 | 来源 | 说明 |
|------|------|------|
| `can_handle` | 注册时指定（`&hcan1` 或 `&hcan2`） | 走哪条 CAN 总线 |
| `txconf` | 注册时配置（ID、帧类型、DLC） | 报文元数据 |
| `tx_buff` | Module 调用前填入的数据 | 8 字节数据载荷 |
| `tx_mailbox` | 输出参数 | 实际占用了哪个邮箱号 |

### ⑦ 返回值判断

`HAL_CAN_AddTxMessage()` 返回 `HAL_OK`(0) = 成功，非零 = 失败。失败原因通常是 CAN 总线仲裁丢失，此时函数返回 0。

### ⑧ 成功

返回 1，数据已成功投递到硬件邮箱。CAN 控制器会在总线空闲时自动发出，不需要 CPU 干预。

---

## 返回值约定

| 返回值 | 含义 |
|:---:|------|
| `1` | 发送成功：报文已投入邮箱，硬件自动发出 |
| `0` | 发送失败：邮箱满超时 或 CAN 总线仲裁失败 |

调用方通常忽略返回值——CAN 发送失败在实时控制场景下没法补救（重发就过期了），只做日志记录。

---

## 调用前的准备工作

Module 必须先往 `CANInstance.tx_buff` 里填入要发送的数据，再调用 `CANTransmit()`：

### 简单单帧（super_cap.c）

```c
void SuperCapSend(SuperCapInstance *instance, uint8_t *data)
{
    memcpy(instance->can_ins->tx_buff, data, 8);  // 填数据
    CANTransmit(instance->can_ins, 1);            // 发送
}
```

### 多帧分包（can_comm.c）

CAN 单帧最多 8 字节。如果数据超过 8 字节，需要分包发送：

```c
void CANCommSend(CANCommInstance *instance, uint8_t *data)
{
    // 先组包（帧头 + 数据 + CRC8 + 帧尾）
    memcpy(instance->raw_sendbuf + 2, data, instance->send_data_len);
    instance->raw_sendbuf[2 + instance->send_data_len] = crc_8(data, instance->send_data_len);

    // 按 8 字节分包发送
    for (size_t i = 0; i < instance->send_buf_len; i += 8)
    {
        send_len = instance->send_buf_len - i >= 8 ? 8 : instance->send_buf_len - i;
        CANSetDLC(instance->can_ins, send_len);                                  // ① 设本包长度
        memcpy(instance->can_ins->tx_buff, instance->raw_sendbuf + i, send_len); // ② 填数据
        CANTransmit(instance->can_ins, 1);                                       // ③ 发送
    }
}
```

---

## 完整发送流程总结

```
Module 层
  → 往 can_ins->tx_buff 写入发送数据
  → CANSetDLC() 设定长度（可选，默认 8）
  → CANTransmit(instance, timeout)
      │
      ├─ 邮箱满？ → 等待（不超过 timeout）
      │             └─ 超时 → 返回 0
      │
      ├─ 邮箱空 → HAL_CAN_AddTxMessage()
      │             ├─ 成功 → 返回 1（硬件自动发出）
      │             └─ 失败（仲裁丢失）→ 返回 0
```

---

## 代码质量标注

源码注释中有一个 `@todo`，指出当前设计的潜在问题：

> 目前似乎封装过度，应该添加一个指向 tx_buff 的指针，tx_buff 不应该由 CAN instance 保存。如果让 CANInstance 保存 tx_buff，会增加一次复制的开销。

当前做法是 Module 用 `memcpy` 把数据拷到 `CANInstance.tx_buff` 再发送。改进方向是让 Module 直接传数据指针，省掉这次 `memcpy`。
