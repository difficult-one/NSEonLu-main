# `#pragma pack(1)` 与字节对齐原理

> 解释 `APP层应用编写指引.md` 中的这句话：
> **"包裹起来，取消字节对齐以防止出现访问 8-bit 地址而出现错误。"**

---

## 1. 这句话拆解

```
包裹起来 → 取消字节对齐 → 以防止 → 访问 8-bit 地址 → 而出现错误
  │              │            │          │               │
  │              │            │          │               └─ HardFault / 数据错位
  │              │            │          └─ 非 4 字节对齐的内存地址
  │              │            └─ 原因：不通信用 struct 直接收发
  │              └─ 手段：用 #pragma pack(1)
  └─ 做法：pack 包住 struct 定义
```

---

## 2. "8-bit 地址"是什么

这里指**非 4 字节对齐的内存地址**。STM32F407 是 32 位 ARM Cortex-M4 处理器，CPU 一次能处理 32 位数据。编译器默认做了**自然对齐**：

| 数据类型 | 自然对齐 | 地址要求 |
|---------|:---:|---------|
| `uint8_t` | 1 字节 | 任意地址 ✅ |
| `uint16_t` | 2 字节 | 地址末位必须是 0、2、4、6、8、A、C、E |
| `uint32_t` | 4 字节 | 地址末位必须是 0、4、8、C |
| `uint64_t` | 8 字节 | 地址末位必须是 0、8 |
| 指针（32 位） | 4 字节 | 地址末位必须是 0、4、8、C |

---

## 3. 为什么通信结构体要 `pack(1)`

### 不 pack 的问题

假设你要通过 CAN 总线发送这个结构体：

```c
// 没有 pack — 编译器自动添加填充
typedef struct {
    uint8_t  flag;    // 1 字节  (地址偏移 0)
    // ★ 编译器在这塞了 1 字节填充 ★  (地址偏移 1，填充)
    uint16_t value;   // 2 字节  (地址偏移 2 ← 对齐到偶数地址)
    uint8_t  tail;    // 1 字节  (地址偏移 4)
} Data;
// sizeof(Data) = 6？错！实际是 6（flag 后面有一个 padding byte）

// 发送：memcpy(tx_buff, &data, sizeof(Data));  // 发了 6 个字节
// 接收端收到 6 个字节，但 flag 后面那个 padding 是垃圾值！
// 接收端如果用同样的 struct 解析：
//   flag=正确, value=错位!, tail=错位!
```

**更严重的情况**（混合 1 字节和 4 字节）：

```c
typedef struct {
    uint8_t  cmd;      // 1 字节
    // ★ 编译器塞了 3 字节填充 ★
    float    position;  // 4 字节 — 必须 4 字节对齐
} CmdPacket;
// sizeof(CmdPacket) = 8（不是 5！）
// 发送方的 tx_buff 里 cmd 后面有 3 个垃圾字节
```

### pack(1) 修复

```c
#pragma pack(1)                    // ← 关闭对齐（紧密排列）
typedef struct {
    uint8_t  cmd;       // 1 字节  (偏移 0)
    float    position;   // 4 字节  (偏移 1 ← 紧贴 cmd 后面！)
} CmdPacket;
#pragma pack()                     // ← 恢复默认对齐
// sizeof(CmdPacket) = 5（正确！）
```

发送 5 个字节，接收端也按 5 个字节解析，完全一致。

---

## 4. "出现错误"是什么

有**两种错误**：

### 错误 ①：数据错位（最常见）

接收端收到的字节流按默认对齐去解析，字段位置全错：

```
发送方 (pack(1)):  [cmd][position byte0][position byte1][position byte2][position byte3]  ← 5 字节
                          ↓ 接收
接收方 (无 pack):   [cmd][!!填充!!][position byte0]...  ← 把 position 的第一字节当 padding 跳过去了
                   → 数据完全错位！
```

### 错误 ②：硬件层面的非对齐访问（ARM Cortex-M 特有）

当 `position` 在 pack(1) 后的偏移是 1（奇数地址），代码执行：

```c
CmdPacket *pkt = (CmdPacket *)rx_buff;  // rx_buff 地址可能是奇数
float pos = pkt->position;              // ★ 从奇数地址读 4 字节！
```

ARM Cortex-M4 **默认支持非对齐访问**，但：
- **更慢**：需要两次内存访问（因为跨越了两个 4 字节字边界）
- 某些指令**不支持**：LDM/STM（多寄存器加载/存储）、LDREX/STREX（互斥访问）遇到非对齐地址会 HardFault
- 编译器优化时可能生成这些指令

所以文档里说"防止出现访问 8-bit 地址而出现错误"——这个错误可以是数据逻辑错误（①），也可以是 HardFault（②）。

---

## 5. 本项目中的实际用法

### robot_def.h — 所有跨应用通信的结构体都 pack

```c
#pragma pack(1)  // 压缩结构体，取消字节对齐，下面的数据都可能被传输
typedef struct {
    uint8_t rest_heat;
    Bullet_Speed_e bullet_speed;
    Enemy_Color_e enemy_color;
} Chassis_Upload_Data_s;          // 这个结构体通过 message_center 跨模块传递

typedef struct {
    attitude_t gimbal_imu_data;              // 内含 float[3] 等
    uint16_t yaw_motor_single_round_angle;
} Gimbal_Upload_Data_s;

typedef struct {
    uint16_t vol;
    uint16_t current;
    uint16_t power;
} SuperCap_Msg_s;
#pragma pack()  // 恢复默认对齐
```

### bsp_can.h — 不使用 pack 会出错的反例

CANInstance 如果用默认对齐，`CAN_TxHeaderTypeDef` 内部的对齐可能导致 `tx_buff[8]` 偏移位置变化，CAN 协议就会发错数据。

---

## 6. 一句话记忆

> **凡是通过通信外设（CAN/SPI/UART/I2C）在芯片间或在模块间（pub-sub）传递的结构体，必须用 `#pragma pack(1)` 包裹。**

```c
#pragma pack(1)
typedef struct { /* 通信用的 struct */ } xxx_t;
#pragma pack()
```

不做这件事的后果：
1. 发送方和接收方的 `sizeof` 不一致 → 数据错位
2. 结构体内有填充垃圾字节 → 通信线路上传出无意义数据
3. 极端情况下，ARM 的非对齐访问触发 HardFault
