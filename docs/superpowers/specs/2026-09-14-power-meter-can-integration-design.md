# C 板与功率计 CAN 单向通信设计

## 目标

在 NSEonLu 的跃鹿框架中新增独立的 `power_meter` 模块，使 STM32F407 C 板通过 CAN1 接收功率计周期上报的测量数据，保存一致的数据快照并提供读取接口，供调试和后续日志功能使用。

本次接入不修改底盘功率控制、超级电容控制或功率计固件。

## 范围

本次修改包括：

- 新增与 HAL、FreeRTOS 无关的功率计 CAN 协议解码单元；
- 新增符合跃鹿框架实例注册风格的功率计模块；
- 在 `ChassisInit()` 中把功率计注册到 CAN1；
- 在 `robot_def.h` 中提供是否启用功率计的编译期开关；
- 保存测量数据、在线状态和接收诊断计数；
- 提供线程安全的数据快照读取接口；
- 新增协议主机测试并接入现有 ARM 固件构建。

本次不包括：

- C 板向功率计发送命令；
- 修改功率计当前的 CAN 标识符或载荷格式；
- 将实测功率接入现有底盘功率控制闭环；
- 在高频 CAN 接收回调中输出日志；
- 修改超级电容通信协议或控制逻辑。

## 方案选择

### 采用：独立 `power_meter` 模块

按照跃鹿框架的 BSP、Module、App 三层结构，CAN 收发仍由 `bsp_can` 负责；`power_meter` 模块拥有自己的 `CANInstance` 和 `DaemonInstance`，负责协议解码、数据保存和在线判断；底盘应用只负责初始化模块。

协议解码拆分为纯 C 文件，使字节序、符号和缩放逻辑可以脱离硬件进行主机测试。

### 不采用：在 `chassis.c` 中直接解码

直接在底盘应用中注册 CAN 回调虽然文件更少，但会把硬件通信协议与底盘业务耦合，不利于复用和测试，也不符合跃鹿框架中外部硬件设备应封装在 Module 层的设计。

### 不采用：复用 `CANComm`

`CANComm` 使用帧头、数据长度、CRC8 和帧尾组成的分包协议。功率计当前发送固定 8 字节原始 CAN 帧，两者不兼容。为本次单向状态接收修改功率计协议没有必要。

## CAN 协议

### 总线配置

- 经典 CAN；
- 标准 11 位标识符；
- C 板使用 CAN1；
- 波特率为 1 Mbps；
- 功率计状态帧 ID 为 `0x605`；
- DLC 固定为 8；
- 多字节整数使用大端字节序。

C 板与功率计当前 CAN 时序都计算为 1 Mbps，因此本次不修改 CubeMX 生成的 CAN 初始化代码。

### 状态帧：功率计到 C 板

| 字节 | 原始格式 | 缩放 | 输出单位 |
|---:|---|---:|---|
| 0-1 | `int16_t`，大端 | 除以 1000 | 分流电压，V |
| 2-3 | `int16_t`，大端 | 除以 1000 | 总线电压，V |
| 4-5 | `int16_t`，大端 | 除以 1000 | 电流，A |
| 6-7 | `int16_t`，大端 | 除以 100 | 功率，W |

解码器仅接受 DLC 为 8 的帧。空指针或错误长度返回失败，不修改调用方已有状态。

现有协议使用 `int16_t` 保存缩放后的结果，因此总线电压可表示上限约为 `32.767 V`，功率可表示上限约为 `327.67 W`。本次保持与功率计固件兼容，不扩大协议范围。

## 模块结构

### `power_meter_protocol`

`modules/power_meter/power_meter_protocol.h` 定义：

- `POWER_METER_STATUS_CAN_ID`，值为 `0x605U`；
- `POWER_METER_CAN_DLC`，值为 `8U`；
- `PowerMeterMeasurement_s`，包含分流电压、总线电压、电流和功率四个 `float` 字段；
- `PowerMeterProtocolDecodeStatus()`，负责校验 DLC、读取大端有符号整数并应用缩放。

`modules/power_meter/power_meter_protocol.c` 只依赖 C 标准整数、布尔和空指针定义，不依赖 HAL 或 FreeRTOS。

### `power_meter`

`modules/power_meter/power_meter.h` 定义 `PowerMeterInstance`，实例拥有：

- `CANInstance *can_ins`；
- `DaemonInstance *daemon`；
- 最近一次有效 `PowerMeterStatus_s`；
- 成功帧计数；
- 拒绝帧计数；
- 最近一次有效接收的系统毫秒时刻；
- 由 daemon 配置换算得到的毫秒离线窗口。

`PowerMeterStatus_s` 包含：

- `online`；
- `shunt_voltage_v`；
- `bus_voltage_v`；
- `current_a`；
- `power_w`。

模块提供以下接口：

```c
PowerMeterInstance *PowerMeterInit(const PowerMeter_Init_Config_s *config);
bool PowerMeterGetStatus(PowerMeterInstance *instance,
                         PowerMeterStatus_s *status);
```

初始化接口复制调用方的 CAN 配置，覆盖回调函数和父实例指针，再通过 `CANRegister()` 注册。由于当前只有功率计到 C 板的单向通信，发送 ID 仅用于满足 `CANInstance` 的初始化结构，不调用 `CANTransmit()`。

## 数据流与并发

1. 功率计每约 2 ms 发送一次 `0x605` 状态帧；
2. `bsp_can` 根据 CAN1 和 `0x605` 找到功率计实例；
3. CAN 接收中断调用功率计模块回调；
4. 回调校验 DLC 并调用纯协议解码器；
5. 解码成功后更新状态、成功帧计数和最后接收时刻，并调用 `DaemonReload()`；
6. 解码失败只增加拒绝帧计数，不覆盖最后一次有效状态，也不刷新 daemon；
7. 调试或上层代码通过 `PowerMeterGetStatus()` 获取一次性快照。

中断回调写状态，任务上下文读取状态。`PowerMeterGetStatus()` 使用与现有超级电容模块相同的 FreeRTOS 临界区模式复制完整快照，避免读取到一半新、一半旧的数据。

## 在线与错误处理

跃鹿框架的 daemon 任务以 100 Hz 运行。功率计正常上报频率约为 500 Hz，模块使用 10 个 daemon 计数，对应约 100 ms 的离线窗口。模块继续注册和重载 daemon，以保持跃鹿框架的一致性；同时以锁存的在线状态和 `HAL_GetTick()` 与最近一次有效接收时刻的无符号差值作为权威在线判据，避免 CAN 中断重载与 daemon 任务递减并发时丢失重载。无符号差值也覆盖 32 位 tick 回绕，锁存状态避免长时间离线后因完整 tick 周期回绕而瞬时误报在线。

- 初始化后、尚未收到有效帧时，状态为离线；
- 每个有效帧把状态置为在线、更新时间戳并重载 daemon；
- 距最后有效帧小于 100 ms 时保持在线，到达或超过 100 ms 时离线回调和 getter 均报告离线；
- 错误 DLC 不会刷新在线状态；
- getter 的空实例或空输出指针返回 `false`；
- 在线状态只用于观察，不影响底盘电机或超级电容控制。

## 应用集成

`application/robot_def.h` 新增编译期开关：

```c
#ifndef POWER_METER_ENABLE
#define POWER_METER_ENABLE 1
#endif

#if POWER_METER_ENABLE != 0 && POWER_METER_ENABLE != 1
#error POWER_METER_ENABLE must be 0 or 1.
#endif
```

默认值为 `1`，使本次新增功能在当前工程中生效；设置为 `0` 时完全跳过功率计实例创建和 CAN 注册。使用 `#ifndef` 保留通过编译参数覆盖配置的能力，取值检查可防止拼写或非布尔配置被静默接受。

`chassis.c` 中与功率计有关的头文件、静态实例和初始化代码均受 `#if POWER_METER_ENABLE` 保护。启用时，`ChassisInit()` 创建一个静态功率计实例，CAN 配置为：

- `can_handle = &hcan1`；
- `rx_id = POWER_METER_STATUS_CAN_ID`；
- 不发送任何功率计 CAN 帧；
- `offline_reload_count = 10U`。

关闭宏时不注册 `0x605` CAN 过滤器、不创建功率计 daemon，也不分配功率计实例内存。协议和模块源码仍保留在构建中，避免切换宏时还需同步维护构建文件。

不修改 `Src/main.c`。跃鹿框架的主入口只调用 `RobotInit()`，实际硬件模块应由所属 App 的初始化函数创建；功率计属于底盘功率观测设备，因此在 `ChassisInit()` 中初始化。

本次不在 `ChassisTask()` 中轮询、打印或参与运算。有效测量数据由 CAN 回调直接保存在模块实例内，需要时通过 getter 读取。

## 构建集成

CMake 使用递归源文件和头文件目录搜索，新目录会自动加入构建。仓库的 Makefile 使用显式列表，因此需要增加：

- `modules/power_meter/power_meter_protocol.c`；
- `modules/power_meter/power_meter.c`；
- `-Imodules/power_meter`。

不修改 CubeMX `.ioc` 和生成的 CAN 初始化文件。

## 测试与验证

新增 `tests/power_meter_protocol/test_power_meter_protocol.c`，主机测试至少覆盖：

- 已知正数帧正确解码和缩放；
- 负分流电压、负电流和负功率保持符号；
- 大端边界值正确解码；
- DLC 不为 8 时拒绝；
- 输入或输出空指针时拒绝；
- 拒绝帧不修改调用方预先保存的测量结果。

自动验证包括：

- 先运行新增协议测试并确认在实现前因缺少接口而失败；
- 实现最小协议代码后确认测试通过；
- 运行仓库现有主机测试，确认无回归；
- 使用现有 ARM 工具链以 `POWER_METER_ENABLE=1` 构建完整固件，确认无新增错误或警告；
- 再以 `POWER_METER_ENABLE=0` 编译相关应用代码，确认关闭配置不会引用功率计实例或注册 CAN 设备。

上板验证步骤：

1. 将功率计接入 C 板 CAN1，并确认两端共地和总线终端电阻正确；
2. 观察成功帧计数持续增加、拒绝帧计数保持不变；
3. 检查电压、电流和功率快照与功率计串口 VOFA 输出一致；
4. 断开功率计，确认约 100 ms 后 `online` 变为 `false`；
5. 恢复连接，确认收到第一帧有效数据后自动恢复在线。

## 完成标准

- C 板 CAN1 能持续接收并正确解码 `0x605`、DLC 8 的功率计状态帧；
- 测量结果以 V、A、W 为单位保存在功率计模块中；
- 调用方能够通过快照接口读取数据和在线状态；
- `POWER_METER_ENABLE=1` 时创建模块，设为 `0` 时不创建实例、不注册 CAN 设备；
- 错误帧不会覆盖最后一次有效数据或刷新在线状态；
- 功率计掉线约 100 ms 后能够被识别，恢复后自动上线；
- 不改变现有底盘功率控制和超级电容行为；
- 新增协议测试、现有相关测试和 ARM 固件构建全部通过。
