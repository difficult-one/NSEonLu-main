# 可移植超级电容通信模块

本目录提供一个不依赖 STM32 HAL、FreeRTOS、工程 BSP 或守护进程的超级电容通信模块。它保留当前工程使用的 Classic CAN 协议，可由 RoboMaster A 板或其他 MCU 工程调用。

## 需要复制的文件

将以下两个文件复制到目标工程并参与编译：

```text
super_cap.h
super_cap.c
```

在使用处包含：

```c
#include "super_cap.h"
```

`tests/` 只用于 PC 宿主测试，不需要复制进单片机工程。模块使用 C11 标准库头文件；`super_cap.c` 使用了 `math.h`，GCC 工程链接时可能需要加入 `-lm`。

> **A 板注意事项：** 本文中的 `hcan1`、CAN1、滤波器编号和引脚都只是示例。必须依据接收方 A 板的原理图、CAN 收发器连接和 CubeMX 工程选择实际 CAN 外设、GPIO 复用、时钟、位时序、中断及滤波器。不要直接照搬原 C 板工程的 `hcan`、引脚或波特率分频参数。

## 协议约定

- 总线类型：Classic CAN，不是 CAN FD。
- 帧类型：11 位标准数据帧（Standard ID、Data Frame）。
- 固定长度：DLC = 8。
- 默认命令 ID：`0x061`，A 板发送给电容。
- 默认状态 ID：`0x051`，电容发送给 A 板。
- 多字节整数和浮点数均为小端序。

将 `SuperCapConfig.command_id` 或 `status_id` 设为 `0` 时，模块分别使用上述默认 ID。非零自定义 ID 必须在 `0x001`～`0x7FF` 范围内；模块的实现也接受其他不大于 `0x7FF` 的非零标准 ID。

### 命令帧：默认 ID `0x061`

| 字节 | 长度 | 含义 | 编码 |
| --- | ---: | --- | --- |
| 0 | 1 | DCDC 输出命令 | `0x01`：使能；`0x00`：关闭 |
| 1～2 | 2 | 裁判系统底盘功率上限 | `uint16_t`，单位 W，小端序 |
| 3～4 | 2 | 裁判系统缓冲能量 | `uint16_t`，单位 J，小端序 |
| 5～7 | 3 | 保留 | 固定填 `0x00` |

例如，使能 DCDC、功率上限 100 W、缓冲能量 50 J 时，模块发送：

```text
01 64 00 32 00 00 00 00
```

### 状态帧：默认 ID `0x051`

| 字节 | 长度 | 含义 | 解码结果 |
| --- | ---: | --- | --- |
| 0 | 1 | 输出状态和故障位 | bit7 为输出关闭标志；bit0～bit6 为故障位 |
| 1～4 | 4 | 底盘实时功率 | 小端 IEEE-754 单精度浮点数，单位 W |
| 5～6 | 2 | 当前可用功率上限 | `uint16_t`，单位 W，小端序 |
| 7 | 1 | 剩余能量比例 | 原始值 `0～255`；API 返回 `原始值 / 255.0f` |

字节 0 的定义如下。多个故障可以同时置位，因此应按位判断 `error_code`：

| 位掩码 | 头文件常量 | 含义 |
| ---: | --- | --- |
| `0x01` | `SUPER_CAP_ERROR_UNDER_VOLTAGE` | 欠压 |
| `0x02` | `SUPER_CAP_ERROR_OVER_VOLTAGE` | 过压 |
| `0x04` | `SUPER_CAP_ERROR_BUCK_BOOST` | Buck/Boost 故障 |
| `0x08` | `SUPER_CAP_ERROR_SHORT_CIRCUIT` | 短路 |
| `0x10` | `SUPER_CAP_ERROR_HIGH_TEMPERATURE` | 高温 |
| `0x20` | `SUPER_CAP_ERROR_NO_POWER_INPUT` | 无功率输入 |
| `0x40` | `SUPER_CAP_ERROR_CAPACITOR` | 电容故障 |
| `0x80` | `SUPER_CAP_OUTPUT_DISABLED_MASK` | 输出关闭；不包含在 `error_code` 中 |

`SuperCapStatus.output_enabled` 在 bit7 为 0 时为 `true`；`error_code` 只保留 bit0～bit6。

## 对象、配置和回调生命周期

模块不分配堆内存，也没有全局单例。应用负责提供并长期保存 `SuperCap` 对象：

```c
static SuperCap cap;
```

`SuperCapInit()` 会清零对象并复制一份 `SuperCapConfig`，所以初始化函数中的局部配置变量可以在调用后销毁。但是：

- `config.context` 指向的对象必须在模块使用期间始终有效；
- `can_send` 和 `now_ms` 是必需回调，缺少任意一个都会导致初始化失败；
- `enter_critical` 和 `exit_critical` 必须同时提供或同时为 `NULL`；
- 每次调用回调时都会传入同一个 `context`；
- `SuperCapReceive()` 只在调用期间读取接收缓冲区，不会保存 `data` 指针；
- `can_send` 收到的数据指针只保证在回调执行期间有效。若自定义驱动异步保存待发指针，回调必须先复制全部 8 字节。

可以创建多个 `SuperCap` 对象，每个对象配置不同的上下文或 CAN ID。

## 并发规则

典型场景是 CAN 接收中断调用 `SuperCapReceive()`，任务调用 `SuperCapGetStatus()`。此时应二选一：

1. 提供适合目标 MCU/RTOS、可同时从中断和任务调用的成对临界区回调；
2. 在 CAN 回调中只复制报文，再由同一个任务串行调用模块的全部 API。

如果两个临界区回调都省略，应用必须保证模块 API 不会并发访问同一实例。临界区回调用于保护内部状态和诊断快照；模块不会在 `can_send` 执行期间保持临界区。因此，多个任务同时发送时，仍需由应用或 CAN 驱动负责串行化发送。

不要不加区分地在中断中使用只适用于任务上下文的 RTOS 临界区宏。临界区实现还应正确保留调用前的中断状态，并保持执行时间尽可能短。

## A 板 CubeMX/HAL 负责的内容

本模块不初始化硬件。A 板工程需要自行完成：

1. 根据 A 板原理图选择实际 CAN 外设和 TX/RX 引脚，并确认外部 CAN 收发器、终端电阻及总线供电。
2. 在 CubeMX 中配置 Classic CAN 位时序，使波特率和采样点与超级电容端一致。
3. 配置一个接收默认 `0x051`（或自定义 `status_id`）11 位标准数据帧的滤波器。
4. 调用 `HAL_CAN_Start()`，并启用对应 RX FIFO 的消息挂起通知。
5. 在 HAL 接收回调中检查 CAN 实例、IDE、RTR、ID 和 DLC，再调用 `SuperCapReceive()`。
6. 实现 CAN 发送和毫秒时钟回调；如有并发，再实现成对临界区回调。

以下是 STM32 bxCAN HAL 的滤波器配置示意。`FilterBank`、`SlaveStartFilterBank`、FIFO 和 CAN 句柄必须按目标工程调整：

```c
static void CapCanStart(void)
{
    CAN_FilterTypeDef filter = {0};

    filter.FilterBank = 0U; /* 按 MCU 的滤波器分配修改 */
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh =
        (uint16_t)(SUPER_CAP_DEFAULT_STATUS_ID << 5);
    filter.FilterIdLow = 0U;
    filter.FilterMaskIdHigh = (uint16_t)(0x7FFU << 5);
    filter.FilterMaskIdLow = 0U;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = 14U; /* 仅按目标芯片/CAN 分配填写 */

    if (HAL_CAN_ConfigFilter(&hcan1, &filter) != HAL_OK ||
        HAL_CAN_Start(&hcan1) != HAL_OK ||
        HAL_CAN_ActivateNotification(
            &hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
    {
        Error_Handler();
    }
}
```

即使硬件滤波器已经限制 ID，接收回调仍应检查标准帧和数据帧类型。若把总线上的所有其他 ID 都传给 `SuperCapReceive()`，这些帧会被当作拒绝帧并增加 `rejected_frame_count`。

## HAL 回调示例

发送回调将模块生成的标准数据帧交给 HAL：

```c
static bool CapCanWrite(void *context, uint32_t id,
                        const uint8_t *data, uint8_t dlc)
{
    CAN_HandleTypeDef *can = context;
    CAN_TxHeaderTypeDef header = {
        .StdId = id,
        .IDE = CAN_ID_STD,
        .RTR = CAN_RTR_DATA,
        .DLC = dlc,
    };
    uint32_t mailbox;

    return HAL_CAN_AddTxMessage(can, &header,
                                (uint8_t *)data, &mailbox) == HAL_OK;
}

static uint32_t BoardClock(void *context)
{
    (void)context;
    return HAL_GetTick();
}
```

CAN FIFO0 接收回调示例：

```c
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[SUPER_CAP_FRAME_DLC];

    if (hcan != &hcan1)
        return;

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0,
                             &rx_header, rx_data) != HAL_OK)
        return;

    if (rx_header.IDE != CAN_ID_STD ||
        rx_header.RTR != CAN_RTR_DATA ||
        rx_header.StdId != SUPER_CAP_DEFAULT_STATUS_ID ||
        rx_header.DLC != SUPER_CAP_FRAME_DLC)
        return;

    (void)SuperCapReceive(&cap,
                          rx_header.StdId,
                          rx_data,
                          (uint8_t)rx_header.DLC);
}
```

若配置了自定义 `status_id`，接收滤波器和回调中的 ID 检查也必须使用同一值。

## 最小使用示例

先在 CubeMX 生成的 CAN 初始化完成后初始化本模块，再启动滤波器、CAN 和接收通知：

```c
static SuperCap cap;

static const SuperCapConfig cap_config = {
    .can_send = CapCanWrite,
    .now_ms = BoardClock,
    .enter_critical = NULL,
    .exit_critical = NULL,
    .context = &hcan1,
    .command_id = 0U,       /* 使用默认 0x061 */
    .status_id = 0U,        /* 使用默认 0x051 */
    .offline_timeout_ms = 0U, /* 使用默认 200 ms */
};

void AppSuperCapInit(void)
{
    if (!SuperCapInit(&cap, &cap_config))
        Error_Handler();

    CapCanStart();
}
```

应用按自身控制周期发送裁判系统数据；模块本身不会创建定时任务：

```c
void AppSuperCapSend(uint16_t power_limit_w,
                     uint16_t buffer_energy_j,
                     bool enable_dcdc)
{
    const SuperCapCommand command = {
        .enable_dcdc = enable_dcdc,
        .referee_power_limit_w = power_limit_w,
        .referee_buffer_energy_j = buffer_energy_j,
    };

    if (!SuperCapSendCommand(&cap, &command))
    {
        /* CAN 邮箱忙或底层发送失败：由应用决定重试/降级策略 */
    }
}
```

读取最近一次有效状态：

```c
void AppSuperCapUpdate(void)
{
    SuperCapStatus status;

    if (!SuperCapGetStatus(&cap, &status))
        return;

    if (!status.online)
    {
        /* 尚未收到有效状态，或状态已超时 */
        return;
    }

    if (status.error_code != 0U || !status.output_enabled)
    {
        /* 按项目安全策略处理故障或输出关闭 */
    }

    /* 可使用 status.chassis_power_w、status.available_power_limit_w
       和 status.energy_ratio。 */
}
```

若接收回调与读取任务并发执行，上述配置中的临界区回调不能保持为 `NULL`；应按“并发规则”一节提供安全实现，或将接收帧转交给同一任务处理。

## 在线判定和诊断

`now_ms` 应返回单调递增的 `uint32_t` 毫秒计数，例如 `HAL_GetTick()`。模块使用无符号减法计算经过时间，可正确处理 32 位计数回绕。

- `offline_timeout_ms == 0` 时默认使用 200 ms。
- 只有 ID、DLC 和浮点功率都有效的状态帧才更新接收时间。
- 尚未收到有效状态帧时，`online` 为 `false`。
- 经过时间小于或等于超时值时为在线；超过后为离线。
- 模块没有后台定时器；每次 `SuperCapGetStatus()` 都会按当前时间重新计算 `online`。
- 离线只会改变返回快照中的 `online`，不会清除上次有效的其他状态字段。

可通过 `SuperCapGetDiagnostics()` 读取：

| 字段 | 含义 |
| --- | --- |
| `received_frame_count` | 成功解码的状态帧数 |
| `rejected_frame_count` | 因 ID、空指针、DLC 或非有限浮点值而拒绝的帧数 |
| `transmit_failure_count` | `can_send` 返回 `false` 的次数 |
| `last_receive_tick_ms` | 最近一次有效状态帧的毫秒时间戳 |

`SuperCapSendCommand()` 直接返回发送回调的结果。错误参数会返回 `false`；只有实际调用 `can_send` 后发送失败才增加 `transmit_failure_count`。

## 宿主测试

在仓库根目录执行：

```powershell
make -C portable_modules/modules/super_cap/tests clean
make -C portable_modules/modules/super_cap/tests test
```

需要支持 C11 的 GCC、GNU Make 和数学库。成功时输出：

```text
PASS: portable supercapacitor tests
```

