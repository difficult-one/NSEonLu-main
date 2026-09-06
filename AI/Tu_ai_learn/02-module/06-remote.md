# 遥控器与关键辅助模块 -- 遥控器、守护进程、CAN 通信、裁判系统

## 模块职责

遥控器模块解析 DBUS 协议提供操控输入；守护进程模块检测设备离线；CAN 通信模块实现板间数据传输；裁判系统模块与官方裁判系统交互。

## 设计思路

### 为什么这些模块需要放在一起理解

在完整的控制数据流中，遥控器是输入源，守护进程是安全保障，CAN 通信是双板架构的桥梁，裁判系统提供比赛状态。它们虽然功能不同，但都是"不直接控制执行机构，却不可或缺"的辅助模块。

## 核心数据结构

### RC_ctrl_t -- 遥控器数据

```c
// remote_control.h
typedef struct {
    struct {
        int16_t rocker_l_;     // 左摇杆水平（-660~660）
        int16_t rocker_l1;     // 左摇杆竖直
        int16_t rocker_r_;     // 右摇杆水平
        int16_t rocker_r1;     // 右摇杆竖直
        int16_t dial;          // 侧边拨轮
        uint8_t switch_left;   // 左开关（1=上, 3=中, 2=下）
        uint8_t switch_right;  // 右开关
    } rc;
    struct {
        int16_t x, y;          // 鼠标位移
        uint8_t press_l;       // 鼠标左键
        uint8_t press_r;       // 鼠标右键
    } mouse;
    Key_t key[3];              // 键盘状态 [0:按键, 1:Ctrl组合, 2:Shift组合]
    uint8_t key_count[3][16];  // 按键计数（上升沿检测）
} RC_ctrl_t;
```

- `rocker_l_` 命名：`l` = left，`_` = 水平方向，`1` = 竖直方向。偏置 1024 已在解析时减去。
- `key[3]`：`key[0]` 为原始按键，`key[1]` 为 Ctrl 组合键，`key[2]` 为 Shift 组合键。
- `key_count[3][16]`：记录每个键被按下的次数（上升沿），用于检测"按下一次"事件。

### Key_t -- 键盘位域

```c
// remote_control.h
typedef union {
    struct {
        uint16_t w : 1;     // W 键
        uint16_t s : 1;
        uint16_t d : 1;
        uint16_t a : 1;
        uint16_t shift : 1;
        uint16_t ctrl : 1;
        uint16_t q : 1;
        uint16_t e : 1;
        uint16_t r : 1;
        uint16_t f : 1;
        uint16_t g : 1;
        uint16_t z : 1;
        uint16_t x : 1;
        uint16_t c : 1;
        uint16_t v : 1;
        uint16_t b : 1;
    };
    uint16_t keys;  // 整体访问，用于位运算
} Key_t;
```

使用 `union` + 位域，既可以通过名称访问单个键（`key.press.w`），也可以通过 `keys` 进行位运算批量操作。空间仅 2 字节（相比之前 16 字节数组），速度提升约 16 倍。

### DaemonInstance -- 守护进程实例

```c
// daemon.h
typedef struct daemon_ins {
    uint16_t reload_count;       // 重载值（超时阈值）
    offline_callback callback;   // 离线回调函数
    uint16_t temp_count;         // 当前倒计数值
    void *owner_id;              // 拥有此守护的模块实例指针
} DaemonInstance;
```

- `reload_count`：单位为 `DaemonTask()` 的调用周期（通常 10ms）。例如 `reload_count = 2` 表示 20ms 无数据视为离线。
- `owner_id`：传入 Module 实例的地址，回调函数中可强转回具体类型。
- `temp_count`：每次"喂狗"（`DaemonReload`）重置为 `reload_count`，`DaemonTask()` 中每周期减 1。

### CANCommInstance -- CAN 板间通信实例

```c
// can_comm.h (pack(1))
typedef struct {
    CANInstance *can_ins;       // 底层 CAN 实例

    uint8_t send_data_len;     // 发送数据长度
    uint8_t send_buf_len;      // 发送缓冲区长度（= send_data_len + 4）
    uint8_t raw_sendbuf[CAN_COMM_MAX_BUFFSIZE + 4]; // 发送缓冲（含帧头帧尾CRC）

    uint8_t recv_data_len;     // 接收数据长度
    uint8_t recv_buf_len;      // 接收缓冲区长度
    uint8_t raw_recvbuf[CAN_COMM_MAX_BUFFSIZE + 4]; // 接收缓冲
    uint8_t unpacked_recv_data[CAN_COMM_MAX_BUFFSIZE]; // 解包后的数据

    uint8_t recv_state;        // 接收状态机（0:等待帧头, 1:接收中）
    uint8_t cur_recv_len;      // 当前已接收字节数
    uint8_t update_flag;       // 数据更新标志

    DaemonInstance *comm_daemon; // 守护进程实例
} CANCommInstance;
```

帧格式：`[0x73('s')] [data_len] [data...] [crc8] [0x65('e')]`，共额外 4 字节开销。

### Daemon_Init_Config_s -- 守护进程配置

```c
typedef struct {
    uint16_t reload_count;     // 超时阈值
    uint16_t init_count;       // 初始等待时间
    offline_callback callback; // 离线回调
    void *owner_id;            // 拥有者实例指针
} Daemon_Init_Config_s;
```

## 函数详解

### 遥控器 -- `remote_control.c`

#### RemoteControlInit() -- 初始化遥控器

```c
RC_ctrl_t *RemoteControlInit(UART_HandleTypeDef *rc_usart_handle)
```

**逻辑**（`remote_control.c:111`）：
1. 配置串口参数：`module_callback = RemoteControlRxCallback`, `recv_buff_size = 18`
2. 调用 `USARTRegister()` 注册串口实例
3. 注册守护进程：`reload_count = 10`（100ms 超时），回调为 `RCLostCallback`
4. 返回 `rc_ctrl` 数组指针（APP 层通过此指针读取遥控器数据）

#### sbus_to_rc() -- DBUS 协议解析（static）

```c
static void sbus_to_rc(const uint8_t *sbus_buf)
```

**逻辑**（`remote_control.c:35`）：
1. 解析 4 个摇杆通道：11 位分辨率，偏置 1024，有效范围 [-660, 660]
2. 调用 `RectifyRCjoystick()` 校正超限值
3. 解析 2 个开关：2 位编码（1=上, 3=中, 2=下）
4. 解析鼠标：X/Y 位移（int16），左右键
5. 解析键盘：直接 `memcpy` 到位域结构体
6. 处理组合键：Ctrl 按下时复制到 `key[KEY_PRESS_WITH_CTRL]`，Shift 同理
7. 检测按键上升沿：当前按下 && 上次未按下 && 无组合键 -> `key_count` 加 1
8. 保存当前数据到 `rc_ctrl[LAST]`

**DBUS 帧格式**：18 字节，包含 4 通道摇杆 + 2 开关 + 鼠标 + 键盘，位域紧密编码。

#### RCLostCallback() -- 遥控器离线处理（static）

```c
static void RCLostCallback(void *id)
```

清空所有遥控器数据（`memset(rc_ctrl, 0, ...)`），尝试重新启动串口接收（`USARTServiceInit`），输出警告日志。

### 守护进程 -- `daemon.c`

#### DaemonRegister() -- 注册守护进程

```c
DaemonInstance *DaemonRegister(Daemon_Init_Config_s *config)
```

**逻辑**（`daemon.c:11`）：
1. `malloc` 分配实例，`memset` 清零
2. 设置 `owner_id`、`reload_count`、`callback`
3. `temp_count = reload_count`（初始值等于重载值）
4. 加入 `daemon_instances[]` 数组

#### DaemonReload() -- 喂狗

```c
void DaemonReload(DaemonInstance *instance)
{
    instance->temp_count = instance->reload_count;
}
```

在 Module 层的回调函数中调用（如 `DecodeDJIMotor` 收到数据时喂狗），表示设备在线。

#### DaemonTask() -- 守护进程周期任务

```c
void DaemonTask()
```

**逻辑**（`daemon.c:37`）：
1. 遍历 `daemon_instances[]` 数组
2. 若 `temp_count > 0`：减 1（倒计时）
3. 若 `temp_count == 0` 且有回调：调用 `callback(owner_id)`

此函数应在 FreeRTOS 中以 10ms 周期运行。各模块的超时时间 = `reload_count * 10ms`。

#### DaemonIsOnline() -- 检查是否在线

```c
uint8_t DaemonIsOnline(DaemonInstance *instance)
{
    return instance->temp_count > 0;
}
```

### CAN 板间通信 -- `can_comm.c`

#### CANCommInit() -- 初始化 CAN 通信

```c
CANCommInstance *CANCommInit(CANComm_Init_Config_s *comm_config)
```

**逻辑**（`can_comm.c:80`）：
1. 分配实例，设置收发数据长度
2. 预设发送缓冲的帧头（`0x73`）和数据长度
3. 预设发送缓冲的帧尾（`0x65`）
4. 设置 CAN 回调为 `CANCommRxCallback`，`id` 指向 CANComm 实例
5. 注册 CAN 实例和守护进程

#### CANCommSend() -- 发送数据

```c
void CANCommSend(CANCommInstance *instance, uint8_t *data)
```

**逻辑**（`can_comm.c:106`）：
1. 将用户数据复制到 `raw_sendbuf + 2`（跳过帧头和长度）
2. 计算 CRC8 并写入 `raw_sendbuf[2 + send_data_len]`
3. 分包发送：每包最多 8 字节，不足 8 字节的最后一包修改 DLC

#### CANCommRxCallback() -- 接收回调（static）

```c
static void CANCommRxCallback(CANInstance *_instance)
```

**逻辑**（`can_comm.c:26`）：
1. 状态机：等待帧头（`0x73`） -> 接收数据 -> 校验帧尾和 CRC
2. 收到帧头且 `data_len` 匹配 -> 进入接收状态
3. 逐包拼接 `raw_recvbuf`
4. 收满后检查帧尾和 CRC8
5. 校验通过：复制数据到 `unpacked_recv_data`，设置 `update_flag`，喂狗
6. 任何异常：重置接收状态（`CANCommResetRx`）

#### CANCommGet() -- 获取接收数据

```c
void *CANCommGet(CANCommInstance *instance)
```

返回 `unpacked_recv_data` 指针并清除 `update_flag`。调用者需自行强转为对应类型。

### 裁判系统 -- `rm_referee.c` / `referee_task.c`

裁判系统模块通过 UART 与官方裁判系统通信，接收比赛状态、血量、功率限制等数据，发送自定义 UI 绘制指令。

**核心结构**：`referee_info_t` 包含所有裁判系统反馈数据（`GameRobotState`, `PowerHeatData`, `ShootData` 等），通过 `RefereeInit()` 初始化并返回指针。

**关键约束**：裁判系统接收 CMD 数据速率最高 10Hz，UI 绘制需要控制发送频率。

## 调用链

### 遥控器数据流

```
DBUS 串口中断
  -> BSP_USART_RxHandler()
     -> RemoteControlRxCallback()          // remote_control.c:94
        -> DaemonReload(rc_daemon)          // 喂狗
        -> sbus_to_rc(recv_buff)            // 解析协议
           -> 解析摇杆/开关/鼠标/键盘
           -> 检测按键上升沿
           -> 保存 LAST 数据

APP: RobotCmdTask() [100Hz]
  -> 读取 rc_ctrl[TEMP].rc.rocker_l_ 等    // 直接访问全局数据
```

### 守护进程工作流

```
APP: DaemonTask() [10Hz, 放入 RTOS]
  -> 遍历 daemon_instances[]
     -> temp_count > 0 ? temp_count-- : callback(owner_id)

各模块中断回调:
  -> DecodeDJIMotor() -> DaemonReload(motor->daemon)   // 20ms 超时
  -> HTMotorDecode()  -> DaemonReload(motor->daemon)   // 50ms 超时
  -> RemoteControlRxCallback() -> DaemonReload(rc_daemon) // 100ms 超时
```

### CAN 板间通信数据流

```
发送端 (GIMBAL_BOARD):
  APP: RobotCmdTask()
    -> CANCommSend(gimbal_can_comm, (uint8_t*)&cmd_data)

接收端 (CHASSIS_BOARD):
  CAN 中断 -> CANCommRxCallback()
    -> 拼接 raw_recvbuf
    -> CRC 校验 -> 复制到 unpacked_recv_data

  APP: ChassisTask()
    -> data = CANCommGet(chassis_can_comm)
    -> cmd = *(ChassisCmd_t*)data
```

## 注意事项

1. **遥控器数据是双缓冲的** -- `rc_ctrl[0]` 为当前数据（TEMP），`rc_ctrl[1]` 为上一次数据（LAST）。按键上升沿检测依赖两个缓冲的比较，`sbus_to_rc()` 末尾会执行 `memcpy` 保存。

2. **DBUS 帧长 18 字节** -- `REMOTE_CONTROL_FRAME_SIZE = 18`，串口接收缓冲必须设为此值。DJI 遥控器实际发送频率约 70Hz（1000/14Hz）。

3. **守护进程的 `owner_id` 用法** -- 回调函数签名是 `void (*callback)(void *)`，Module 层在回调内将 `owner_id` 强转为具体类型。例如 `DJIMotorLostCallback` 中 `(DJIMotorInstance*)motor_ptr`。

4. **CAN 通信不支持动态包长** -- 接收端在初始化时设定 `recv_data_len`，帧头中的 `data_len` 必须与此匹配才会开始接收。无法接收不同长度的数据包。

5. **CAN 通信的最大数据长度** -- `CAN_COMM_MAX_BUFFSIZE = 60` 字节。CAN 单帧最多 8 字节，60 字节数据需要 8 帧传输（(60+4+7)/8 = 8.75，取 9 帧）。注意总线负载。

6. **遥控器离线时的安全性** -- `RCLostCallback` 会清空所有遥控器数据。APP 层的 `robot_cmd` 应检测遥控器在线状态（`RemoteControlIsOnline()`），离线时停止所有执行机构。

7. **DaemonTask 的调用频率** -- 所有模块的超时时间都基于 10ms 周期。如果 `DaemonTask()` 的实际调用周期不是 10ms，超时时间会偏离预期值。

8. **裁判系统的 `pack(1)` 要求** -- `referee_info_t` 使用 `#pragma pack(1)` 包裹，确保与裁判系统协议的字节对齐一致。通过 CAN 通信传输的数据结构同样必须使用 `pack(1)`。
