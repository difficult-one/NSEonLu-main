# `#pragma pack()` — 结构体字节对齐控制

---

## 1. 它干什么

控制结构体成员在内存中的**字节对齐方式**。

| 指令 | 含义 |
|------|------|
| `#pragma pack(1)` | 从这行开始，按 **1 字节对齐**（紧凑排列，成员之间无空隙） |
| `#pragma pack()` | 从这行开始，**恢复编译器默认对齐**（STM32F4 通常是 4 字节对齐） |

---

## 2. 为什么需要它？—— 问题演示

### 不用的后果

假设你要通过 CAN 发送一段数据：

```c
// 没有 #pragma pack(1) — 编译器会插入填充字节
typedef struct
{
    uint16_t vol;     // 2 字节
    uint16_t current; // 2 字节
    uint16_t power;   // 2 字节
} SuperCap_Msg_s;
// 表面上看是 6 字节，实际也是 6 字节（恰好对齐了）
```

这个例子里三个 `uint16_t` 恰好不需要填充。换个例子：

```c
typedef struct
{
    uint8_t  flag;    // 1 字节
    uint32_t value;   // 4 字节 — 编译器会在 flag 后面塞 3 个字节的空隙！
    uint16_t count;   // 2 字节
} DataPacket;
// 你以为的布局：
// ┌──────┬──────────────────────────┬──────────────┐
// │ flag │          value           │    count     │
// │ 1 字节│         4 字节           │   2 字节     │   = 7 字节
// └──────┴──────────────────────────┴──────────────┘
//
// 实际编译器的布局（4 字节对齐）：
// ┌──────┬──┬──┬──┬──────────────────────────┬──────────────┬──┬──┐
// │ flag │ PAD │ PAD │ PAD │          value           │    count     │ PAD │ PAD │
// │ 1 字节│  3 字节填充  │         4 字节           │   2 字节     │  2 字节填充  │  = 12 字节！
// └──────┴──┴──┴──┴──────────────────────────┴──────────────┴──┴──┘
```

### 出问题的场景

通过 DMA/CAN/SPI 发送结构体时，发送的是**内存里的实际字节**：

```c
DataPacket pkt = { .flag = 0x01, .value = 0x12345678, .count = 100 };

// DMA 发送 sizeof(DataPacket) = 12 个字节（包含了 5 字节无意义的填充）
HAL_UART_Transmit_DMA(&huart1, (uint8_t *)&pkt, sizeof(DataPacket));
```

接收方收到的 12 字节里夹杂了 5 字节的垃圾填充值，接收方按 7 字节去解析 → **数据全错位**。

---

## 3. 怎么用 — 本项目中的例子

### 示例 ①：CAN 实例 `CANInstance`（`bsp_can.h:14-29`）

```c
#pragma pack(1)                          // ← 开启紧凑模式
typedef struct _
{
    CAN_HandleTypeDef *can_handle;       // 4 字节（32位指针）
    CAN_TxHeaderTypeDef txconf;         // 结构体
    uint32_t tx_id;                      // 4 字节
    uint32_t tx_mailbox;                 // 4 字节
    uint8_t tx_buff[8];                  // 8 字节
    uint8_t rx_buff[8];                  // 8 字节
    uint32_t rx_id;                      // 4 字节
    uint8_t rx_len;                      // 1 字节
    void (*can_module_callback)(struct _ *); // 4 字节（函数指针）
    void *id;                            // 4 字节
} CANInstance;
#pragma pack()                           // ← 恢复默认对齐
```

`CANInstance` 包含各种大小混合的成员：指针（4 字节）、`uint32_t`（4 字节）、`uint8_t` 数组 + 单字节。不用 `pack(1)` 的话，`tx_buff[8]` 后面、`rx_len` 后面都可能被编译器插入填充字节。

### 示例 ②：超级电容报文 `SuperCap_Msg_s`（`super_cap.h:13-20`）

```c
#pragma pack(1)
typedef struct
{
    uint16_t vol;     // 2 字节 — 电压
    uint16_t current; // 2 字节 — 电流
    uint16_t power;   // 2 字节 — 功率
} SuperCap_Msg_s;
#pragma pack()
```

这个报文通过 CAN 总线发给底盘，接收方用 `sizeof(SuperCap_Msg_s)` 确定数据长度。如果不对齐，两边算出来的 `sizeof` 不一致 → 组包/解包错乱。

### 示例 ③：裁判系统协议 `referee_protocol.h:23-390`

裁判系统的通信协议包含几十个结构体，每个都严格控制字节对齐：

```c
#pragma pack(1)
typedef struct { /* ... 上百个字段 ... */ } ext_game_state_t;
typedef struct { /* ... */ } ext_robot_hurt_t;
// ...
#pragma pack()
```

因为裁判系统数据是通过串口 DMA 接收的，接收端收到的字节流必须和结构体字段完全一一对应，不能有任何填充。

---

## 4. 核心原则

> **凡是通过通信外设（串口、CAN、SPI、I2C）发送的结构体，声明时必须用 `#pragma pack(1)` 包裹。**

```c
// ✅ 正确
#pragma pack(1)
typedef struct { /* ... */ } MyMsg;
#pragma pack()

// ❌ 错误：sizeof 不可信，发送出去的数据混了填充字节
typedef struct { /* ... */ } MyMsg;
```

这也写在了项目的文档中：

> `ARM-GCC` 不支持 `__packed` 关键字，要使用 `#pragma pack(1)` —— `.Doc/VSCode+Ozone使用方法.md`

---

## 5. 范围控制

```c
// 只有被包裹的部分受影响
struct A { int x; char y; };          // ← 默认对齐，可能有填充

#pragma pack(1)                        // 紧凑开始
struct B { int x; char y; };          // ← 无填充，sizeof = 5
#pragma pack()                         // 恢复默认

struct C { int x; char y; };          // ← 默认对齐，可能有填充
```
