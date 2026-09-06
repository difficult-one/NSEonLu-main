# 双板通信机制

## 模块职责

双板通信通过 CAN 总线在云台板和底盘板之间传递控制命令和反馈数据，使同一份应用代码能通过条件编译在单板和双板两种硬件架构下工作。

---

## 设计思路

### 为什么需要双板？

RoboMaster 比赛中，部分机器人（如步兵、英雄）将控制板分为云台板和底盘板，原因包括：

1. **布线简化** — 云台部分电机和传感器较多，独立布线减少滑环负担
2. **功能隔离** — 云台控制（高频、低延迟）与底盘控制（功率管理、裁判系统）分开
3. **容错** — 一块板故障时另一块板可能仍能工作

### 单板 vs 双板的通信差异

```
单板模式 (ONE_BOARD):
  robot_cmd ──[message_center]──> chassis
  robot_cmd ──[message_center]──> gimbal
  robot_cmd ──[message_center]──> shoot
  通信路径: 进程内函数调用（PubPushMessage -> SubGetMessage）

双板模式:
  云台板:
    robot_cmd ──[message_center]──> gimbal     (板内)
    robot_cmd ──[message_center]──> shoot      (板内)
    robot_cmd ──[CANComm]──────────> 底盘板    (跨板)
    底盘板   ──[CANComm]──────────> robot_cmd  (跨板)

  底盘板:
    chassis   ──[CANComm]──────────> 云台板    (跨板)
    云台板    ──[CANComm]──────────> chassis    (跨板)
```

### 设计原则

应用层代码通过 `#ifdef` 切换通信方式，**业务逻辑完全相同**，只有数据通道不同：

```c
// 发送命令（robot_cmd.c:348-353）
#ifdef ONE_BOARD
    PubPushMessage(chassis_cmd_pub, &chassis_cmd_send);    // 消息中心
#endif
#ifdef GIMBAL_BOARD
    CANCommSend(cmd_can_comm, &chassis_cmd_send);          // CAN 通信
#endif
```

---

## 核心数据结构

### CANCommInstance — CAN 通信实例

位于 `can_comm.h:26-44`，负责将应用数据打包为 CAN 帧格式进行跨板传输。

```c
#pragma pack(1)
typedef struct {
    CANInstance *can_ins;                                // 底层 CAN 实例

    /* 发送部分 */
    uint8_t send_data_len;                               // 应用数据长度
    uint8_t send_buf_len;                                // 缓冲区长度 = 数据长度 + 帧头帧尾CRC
    uint8_t raw_sendbuf[CAN_COMM_MAX_BUFFSIZE + 4];      // 原始发送缓冲区

    /* 接收部分 */
    uint8_t recv_data_len;                               // 应用数据长度
    uint8_t recv_buf_len;                                // 缓冲区长度
    uint8_t raw_recvbuf[CAN_COMM_MAX_BUFFSIZE + 4];      // 原始接收缓冲区
    uint8_t unpacked_recv_data[CAN_COMM_MAX_BUFFSIZE];   // 解包后的数据

    /* 状态标志 */
    uint8_t recv_state;      // 接收状态机（0=等待帧头, 1=接收中）
    uint8_t cur_recv_len;    // 当前已接收长度
    uint8_t update_flag;     // 数据更新标志

    DaemonInstance *comm_daemon;   // 离线检测
} CANCommInstance;
#pragma pack()
```

### CANComm 帧格式

```
+--------+----------+==================+------+------+
| 帧头's' | datalen |   应用数据 N字节   | CRC8 | 帧尾'e' |
|  1字节  |  1字节  |     N字节         |1字节 | 1字节 |
+--------+----------+==================+------+------+
|<--        总长度 = N + 4 字节                      -->|

帧头: 's' (0x73)
datalen: 应用数据的长度（不含帧头帧尾CRC）
CRC8: 对 datalen 之后、CRC 之前的所有数据计算
帧尾: 'e' (0x65)
```

### CANComm_Init_Config_s — 初始化配置

```c
typedef struct {
    CAN_Init_Config_s can_config;    // CAN 底层配置（句柄、tx_id、rx_id）
    uint8_t send_data_len;           // 发送数据长度
    uint8_t recv_data_len;           // 接收数据长度
    uint16_t daemon_count;           // 离线检测计数
} CANComm_Init_Config_s;
```

---

## 函数详解

### CANCommInit() — 初始化 CAN 通信实例

位于 `can_comm.c:80`。

```c
CANCommInstance *CANCommInit(CANComm_Init_Config_s *comm_config)
{
    CANCommInstance *ins = (CANCommInstance *)malloc(sizeof(CANCommInstance));
    memset(ins, 0, sizeof(CANCommInstance));

    // 计算缓冲区长度 = 数据长度 + 4（帧头+数据长度字节+CRC+帧尾）
    ins->recv_data_len = comm_config->recv_data_len;
    ins->recv_buf_len = comm_config->recv_data_len + CAN_COMM_OFFSET_BYTES;
    ins->send_data_len = comm_config->send_data_len;
    ins->send_buf_len = comm_config->send_data_len + CAN_COMM_OFFSET_BYTES;

    // 预填帧头帧尾，避免每次发送重复赋值
    ins->raw_sendbuf[0] = CAN_COMM_HEADER;           // 's'
    ins->raw_sendbuf[1] = comm_config->send_data_len;
    ins->raw_sendbuf[send_buf_len - 1] = CAN_COMM_TAIL;  // 'e'

    // CAN 实例注册
    comm_config->can_config.id = ins;   // parent pointer: CANInstance->id 指向 CANCommInstance
    comm_config->can_config.can_module_callback = CANCommRxCallback;
    ins->can_ins = CANRegister(&comm_config->can_config);

    // 注册离线检测守护进程
    Daemon_Init_Config_s daemon_config = {
        .callback = CANCommLostCallback,
        .owner_id = (void *)ins,
        .reload_count = comm_config->daemon_count,
    };
    ins->comm_daemon = DaemonRegister(&daemon_config);
    return ins;
}
```

### CANCommRxCallback() — 接收回调

位于 `can_comm.c:26`，当 CAN 中断收到数据时被 BSP 层调用。

```c
static void CANCommRxCallback(CANInstance *_instance)
{
    CANCommInstance *comm = (CANCommInstance *)_instance->id;  // parent pointer 获取

    // 状态机：等待帧头
    if (_instance->rx_buff[0] == CAN_COMM_HEADER && comm->recv_state == 0) {
        if (_instance->rx_buff[1] == comm->recv_data_len) {
            comm->recv_state = 1;  // 开始接收
        } else return;
    }

    // 状态机：接收中
    if (comm->recv_state) {
        // 溢出检查
        if (comm->cur_recv_len + _instance->rx_len > comm->recv_buf_len) {
            CANCommResetRx(comm);
            return;
        }
        // 拼接数据
        memcpy(comm->raw_recvbuf + comm->cur_recv_len,
               _instance->rx_buff, _instance->rx_len);
        comm->cur_recv_len += _instance->rx_len;

        // 接收完成检查
        if (comm->cur_recv_len == comm->recv_buf_len) {
            // 验证帧尾
            if (comm->raw_recvbuf[recv_buf_len - 1] == CAN_COMM_TAIL) {
                // 验证 CRC8
                if (comm->raw_recvbuf[recv_buf_len - 2]
                    == crc_8(comm->raw_recvbuf + 2, comm->recv_data_len)) {
                    // 校验通过，拷贝到 unpacked_recv_data
                    memcpy(comm->unpacked_recv_data,
                           comm->raw_recvbuf + 2, comm->recv_data_len);
                    comm->update_flag = 1;         // 标记数据更新
                    DaemonReload(comm->comm_daemon); // 重载离线检测
                }
            }
            CANCommResetRx(comm);  // 重置接收状态
        }
    }
}
```

**接收状态机**：
1. 等待帧头 `0x73`，确认 datalen 匹配
2. 逐包拼接数据（CAN 单帧最大 8 字节，大包需要分帧）
3. 长度达到预期后验证帧尾和 CRC8
4. 校验通过则复制到 `unpacked_recv_data`，设置 `update_flag`

### CANCommSend() — 发送数据

位于 `can_comm.c:106`。

```c
void CANCommSend(CANCommInstance *instance, uint8_t *data)
{
    // 拷贝应用数据到发送缓冲区
    memcpy(instance->raw_sendbuf + 2, data, instance->send_data_len);
    // 计算 CRC8 并填入
    instance->raw_sendbuf[2 + instance->send_data_len]
        = crc_8(data, instance->send_data_len);

    // 分帧发送：CAN 单帧最大 8 字节
    for (size_t i = 0; i < instance->send_buf_len; i += 8) {
        send_len = instance->send_buf_len - i >= 8 ? 8 : instance->send_buf_len - i;
        CANSetDLC(instance->can_ins, send_len);        // 修改 DLC
        memcpy(instance->can_ins->tx_buff,
               instance->raw_sendbuf + i, send_len);   // 填入数据
        CANTransmit(instance->can_ins, 1);             // 发送
    }
}
```

### CANCommGet() — 获取接收数据

位于 `can_comm.c:125`。

```c
void *CANCommGet(CANCommInstance *instance)
{
    instance->update_flag = 0;  // 读取后清除更新标志
    return instance->unpacked_recv_data;
}
```

---

## 调用链

### 单板模式消息流

```
robot_cmd (发布)                              chassis (订阅)
    |                                              |
    |  PubPushMessage("chassis_cmd", &cmd)         |
    |--------------------------------------------->|
    |                                              SubGetMessage(chassis_sub, &cmd_recv)
    |
    |  PubPushMessage("gimbal_cmd", &cmd)          gimbal (订阅)
    |--------------------------------------------->SubGetMessage(gimbal_sub, &cmd_recv)
```

### 双板模式消息流

```
云台板                                         底盘板
robot_cmd                                      chassis
    |                                              |
    |  CANCommSend(cmd_can_comm, &cmd)             |
    |----[CAN 帧 's'|len|data|crc|'e']---------->|
    |                                              CANCommRxCallback() 拼包+校验
    |                                              CANCommGet(chasiss_can_comm)
    |                                              -> chassis_cmd_recv
    |
    |  CANCommRxCallback() 拼包+校验              CANCommSend(chasiss_can_comm, &feedback)
    |<--[CAN 帧 's'|len|data|crc|'e']------------|
    |  CANCommGet(cmd_can_comm)                    |
    |  -> chassis_fetch_data                       |
```

### 完整的 CAN ID 配对

```
云台板:
  cmd_can_comm:  can_handle=&hcan1, tx_id=0x312, rx_id=0x311
  发送 0x312 -> 底盘板接收 0x312（底盘板的 rx_id）
  接收 0x311 <- 底盘板发送 0x311

底盘板:
  chasiss_can_comm: can_handle=&hcan2, tx_id=0x311, rx_id=0x312
  发送 0x311 -> 云台板接收 0x311（云台板的 rx_id）
  接收 0x312 <- 云台板发送 0x312
```

---

## 注意事项

1. **tx_id 和 rx_id 是交叉配对的** — 云台板的 tx_id 是底盘板的 rx_id，反之亦然。配置时容易搞反，导致通信失败。

2. **双板时两个 CAN 句柄可能不同** — 云台板用 `&hcan1`，底盘板用 `&hcan2`，这是因为两条 CAN 总线分别连接不同的硬件。如果两块板通过同一条 CAN 总线通信，则句柄应相同。

3. **CAN 单帧最大 8 字节，大数据需分帧** — `CANCommSend()` 内部自动处理分帧，但接收端 `CANCommRxCallback()` 通过状态机拼包。如果中间丢帧，状态机会出错，需要重置。当前没有超时重置机制，只有溢出重置。

4. **`#pragma pack(1)` 对通信结构体至关重要** — `CANCommInstance` 本身也使用了 `pack(1)`，因为其中的缓冲区需要紧密排列。通信数据结构（如 `Chassis_Ctrl_Cmd_s`）如果没有 `pack(1)`，字节对齐会导致发送方和接收方的结构体布局不一致。

5. **双板通信增加了延迟** — 消息中心是进程内直接 memcpy，延迟微秒级；CAN 通信需要打包、分帧、传输、拼包、校验，延迟在毫秒级。对于高频控制环路（如底盘 500Hz），这个延迟需要考虑。

6. **离线检测通过 Daemon 实现** — CANComm 注册了一个守护进程，每次成功接收数据时 `DaemonReload()`。如果超过 `daemon_count` 个周期未收到数据，触发 `CANCommLostCallback()`，重置接收状态并打印警告。

7. **后续规划** — 设计文档提到考虑将双板通信纳入 message_center 的实现中，根据 `robot_def.h` 的板型定义自动选择通信方式，使应用层完全不需要 `#ifdef`。
