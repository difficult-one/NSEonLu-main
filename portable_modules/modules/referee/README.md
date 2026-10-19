# 便携式裁判系统模块

本目录是从现有工程裁判系统代码移植出的独立 C11 模块。它不依赖 STM32 HAL、FreeRTOS、工程 BSP 或守护任务；RoboMaster A 板工程只需要提供 UART 发送、毫秒时钟以及可选的临界区回调。

> 本模块实现的是本仓库现有裁判协议版本。使用前应与当年官方裁判系统协议核对命令和字段；本模块不会自动适配新赛季协议。

## 1. 复制与编译

把整个 `referee` 目录复制到 A 板工程中，至少保留以下生产文件：

```text
referee/
├── referee.h
├── referee.c
├── referee_protocol.h
├── referee_crc.h
├── referee_crc.c
├── referee_ui.h
└── referee_ui.c
```

将 `referee.c`、`referee_crc.c` 和 `referee_ui.c` 加入工程编译，并把本目录加入头文件搜索路径。模块只使用标准 C 头文件，不要把原 C 板工程的 `bsp_usart`、Daemon、FreeRTOS 任务或 HAL 适配代码一起复制进来。

## 2. A 板 UART 配置

裁判系统串口参数为：

- 波特率：`115200`
- 数据位：8 位
- 校验：无
- 停止位：1 位
- 收发模式：TX/RX
- 推荐接收方式：DMA + UART IDLE（Receive-to-idle）

必须根据接收方 **A 板的原理图、实际接线和 CubeMX 工程**选择 UART 外设、GPIO 复用、DMA 通道和中断。不要照搬本仓库 C 板使用的 `huart1`、`huart6` 或对应引脚；不同 A 板工程的裁判串口分配可能不同。

## 3. 初始化和回调

应用需要长期保存一个 `Referee` 对象，并提供以下配置：

```c
typedef struct {
    RefereeUartSendFn uart_send;          /* 必填 */
    RefereeNowMsFn now_ms;                /* 必填 */
    RefereeCriticalFn enter_critical;     /* 可选，但必须与 exit 成对 */
    RefereeCriticalFn exit_critical;
    void *context;
    uint32_t offline_timeout_ms;          /* 0 使用默认 300 ms */
} RefereeConfig;
```

- `uart_send`：发送一段完整裁判帧，成功返回 `true`。
- `now_ms`：返回单调递增的 32 位毫秒计时；允许自然溢出。
- `enter_critical`/`exit_critical`：必须同时提供或同时为 `NULL`。
- `context`：原样传给以上所有回调，可指向 UART 句柄或应用自定义上下文。

`RefereeInit()` 会复制配置，但不会复制 `context` 指向的对象。`Referee` 实例、回调函数和 `context` 指向的对象必须在整个使用期间有效。模块不使用堆内存；建议把实例定义为静态/全局对象，初始化后不要按值复制。

最小初始化示例：

```c
#include "referee.h"
#include "referee_ui.h"
#include "usart.h"

static Referee g_referee;

static bool RefereeWrite(void *context,
                         const uint8_t *data,
                         size_t length)
{
    UART_HandleTypeDef *uart = (UART_HandleTypeDef *)context;

    /* 阻塞发送会在返回前消费完 data，适合作为最小示例。 */
    return HAL_UART_Transmit(uart, (uint8_t *)data,
                             (uint16_t)length, 20U) == HAL_OK;
}

static uint32_t BoardClock(void *context)
{
    (void)context;
    return HAL_GetTick();
}

void RefereeAppInit(void)
{
    /* 按 A 板原理图/CubeMX 替换为实际裁判串口，不能照搬 C 板。 */
    UART_HandleTypeDef *referee_uart = &huart6;
    const RefereeConfig config = {
        .uart_send = RefereeWrite,
        .now_ms = BoardClock,
        .enter_critical = NULL,
        .exit_critical = NULL,
        .context = referee_uart,
        .offline_timeout_ms = 300U,
    };

    if (!RefereeInit(&g_referee, &config)) {
        Error_Handler();
    }
}
```

模块会把完整发送帧保存在 `Referee` 实例内部的 `tx_buffer`。传给 `uart_send` 的指针在回调返回后仍有效，直到同一实例下一次有效发送尝试或重新执行 `RefereeInit()`。因此可以在回调中直接调用 `HAL_UART_Transmit_DMA()`，但必须在 DMA 发送完成前禁止同一实例再次发送。若应用无法保证串行发送，回调应自行复制数据并排队。发送失败应返回 `false`。

## 4. DMA + IDLE 接收与 `RefereeFeed`

`RefereeFeed()` 接受任意长度的 UART 字节块，能够处理一帧被拆成多块、一块内包含多帧、帧前噪声以及 CRC 错误。每次 DMA/IDLE 回调只需把本次实际收到的 `Size` 字节交给模块；不要补零，也不要假定一次回调就是一帧。

下面是 STM32 HAL 的最小单缓冲示例。缓冲区大小和重新启动接收的策略可以按工程调整：

```c
#include "referee.h"
#include "usart.h"

static uint8_t g_referee_rx[256];
static UART_HandleTypeDef *g_referee_uart;

static void RefereeStartReceive(void)
{
    if (HAL_UARTEx_ReceiveToIdle_DMA(g_referee_uart,
                                     g_referee_rx,
                                     sizeof(g_referee_rx)) != HAL_OK) {
        Error_Handler();
    }

    /* 本单缓冲示例在每次 IDLE/TC 事件后重启 DMA，因此必须关闭半传输中断。 */
    __HAL_DMA_DISABLE_IT(g_referee_uart->hdmarx, DMA_IT_HT);
}

void RefereeUartStart(UART_HandleTypeDef *uart)
{
    g_referee_uart = uart;
    RefereeStartReceive();
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart != g_referee_uart) {
        return;
    }

    if (Size > 0U) {
        (void)RefereeFeed(&g_referee, g_referee_rx, (size_t)Size);
    }
    RefereeStartReceive();
}
```

上例特意关闭 DMA 半传输（HT）中断，使回调只在 IDLE/TC 后投喂并重启这个单缓冲。如果保留 HT 事件，必须改为跟踪 DMA 写入偏移、只投喂新增数据的实现，不能直接照搬上述重启逻辑。实际工程还应在 UART 错误回调中清除错误并重新启动 DMA。若同一回调还服务其他串口，必须先按句柄分流。需要持续无缝接收时可使用双缓冲或环形 DMA，但最终仍是按实际新增字节调用 `RefereeFeed()`。

## 5. 对象并发规则

典型用法是只由 UART/DMA ISR 调用 `RefereeFeed()`，任务读取快照或发送 UI：

- `RefereeFeed()` 为了不在逐字节解析期间长时间关中断，不会主动调用临界区回调。
- 在典型 ISR 投喂方案中，`enter_critical`/`exit_critical` 必须能阻止执行 `RefereeFeed()` 的那个中断，从而保证任务获取一致快照。
- 若在任务中调用 `RefereeFeed()`，或有多个 Feed 生产者，调用方必须在模块外用同一个锁串行化所有 `RefereeFeed()` 调用；读取 API 的临界区回调也必须使用这把锁。
- 若不提供临界区回调，调用方必须保证投喂与读取不会并发。
- 不要从多个任务同时发送；建议由一个发送任务串行调用 UI/交互发送接口。
- 硬件发送回调不会被包在模块临界区内，避免长时间关中断。

临界区的具体实现取决于工程使用裸机还是 FreeRTOS，以及调用是否发生在 ISR 中，因此本模块不提供固定 HAL/RTOS 实现。

## 6. 完整接收数据查询

调用 `RefereeGetData()` 可获得一致的 `RefereeData` 快照：

```c
RefereeData data;

if (RefereeGetData(&g_referee, &data)) {
    if (data.has_robot_state) {
        uint8_t robot_id = data.robot_state.robot_id;
        uint16_t hp = data.robot_state.current_hp;
        (void)robot_id;
        (void)hp;
    }
}
```

先检查对应的 `has_*` 标志，再使用该结构。模块保存每类命令最近一次长度正确、CRC 正确的数据：

| 命令 ID | `RefereeData` 字段 | 有效标志 | 内容 |
| --- | --- | --- | --- |
| `0x0001` | `game_state` | `has_game_state` | 比赛类型、阶段、剩余时间、同步时间戳 |
| `0x0002` | `game_result` | `has_game_result` | 比赛结果 |
| `0x0003` | `game_robot_hp` | `has_game_robot_hp` | 红蓝双方机器人、前哨站和基地血量 |
| `0x0101` | `event_data` | `has_event_data` | 场地事件位 |
| `0x0102` | `supply_action` | `has_supply_action` | 补给站动作 |
| `0x0201` | `robot_state` | `has_robot_state` | 本机 ID、等级、血量、热量限制、功率限制、输出使能 |
| `0x0202` | `power_heat` | `has_power_heat` | 电压/电流/功率保留字段、缓冲能量、枪口热量 |
| `0x0203` | `robot_position` | `has_robot_position` | `x/y/z/yaw` |
| `0x0204` | `buff` | `has_buff` | 恢复、冷却、防御、易伤、攻击增益 |
| `0x0205` | `aerial_energy` | `has_aerial_energy` | 空中机器人状态和剩余时间 |
| `0x0206` | `robot_hurt` | `has_robot_hurt` | 装甲板 ID 和伤害类型 |
| `0x0207` | `shoot_data` | `has_shoot_data` | 弹丸类型、发射机构、频率和弹速 |
| `0x0301` | `interaction` | `has_interaction` | 子命令、发送方、接收方及 0～113 字节数据 |

`last_command_id` 和 `last_sequence` 记录最近一帧通过 CRC 的外层命令和序号。已知命令长度错误或未知命令不会覆盖对应的最后有效业务字段，但会反映在诊断计数中。

## 7. 功率数据快捷查询

`RefereeGetPower()` 专门返回超级电容/底盘功率控制常用字段：

```c
RefereePower power;

if (RefereeGetPower(&g_referee, &power) && power.valid) {
    /* power.chassis_output_enabled */
    /* power.chassis_power_limit_w */
    /* power.buffer_energy_j */
}
```

只有 `0x0201` 机器人状态帧和 `0x0202` 功率热量帧都收过，且两者都未超过 `offline_timeout_ms`，`valid` 才为 `true`。无效时其余三个字段会清零，调用方不应沿用旧功率值。

## 8. 在线状态与诊断

```c
bool online = RefereeIsOnline(&g_referee);
RefereeDiagnostics diagnostics;

if (RefereeGetDiagnostics(&g_referee, &diagnostics)) {
    /* 查看 diagnostics 中各计数和最后有效帧时间 */
}
```

收到任意 CRC 正确、帧长不超限的完整裁判帧后，链路时间会刷新。超过 `offline_timeout_ms` 未收到有效帧时，`RefereeIsOnline()` 返回 `false`。

诊断字段包括：有效帧、CRC8 错误、CRC16 错误、超长帧、丢弃字节、业务载荷长度错误、不支持命令、发送失败等计数，以及 `has_valid_frame` 和 `last_valid_frame_ms`。这些计数适合排查接线、波特率、DMA 分块和协议版本问题。

## 9. 客户端 ID

UI 的发送方 ID 通常取本机 `robot_state.robot_id`，客户端接收方 ID 可由以下函数计算：

```c
uint16_t sender_id = data.robot_state.robot_id;
uint16_t client_id = RefereeClientId(sender_id); /* 0x0100 + robot_id */
```

请先确认 `data.has_robot_state`，不要在本机 ID 尚未收到时发送 UI。

## 10. UI 图形构建与发送

先使用构建函数生成普通 C 结构，再调用发送函数。构建函数不会发送数据：

```c
RefereeGraphic line;

if (RefereeUiMakeLine(&line, "ln1",
                      REFEREE_UI_ADD, 1U, REFEREE_UI_GREEN,
                      3U, 100U, 200U, 500U, 200U)) {
    (void)RefereeUiSendGraphics(&g_referee,
                                sender_id, client_id,
                                &line, 1U);
}
```

可用构建函数：

- `RefereeUiMakeLine()`：直线
- `RefereeUiMakeRectangle()`：矩形
- `RefereeUiMakeCircle()`：圆
- `RefereeUiMakeEllipse()`：椭圆
- `RefereeUiMakeArc()`：圆弧
- `RefereeUiMakeFloat()`：浮点数（传入按小数位缩放后的整数）
- `RefereeUiMakeInteger()`：整数
- `RefereeUiMakeText()`：文本，最多 30 字节

图形名必须提供 3 个字节。同一图形后续修改/删除时使用相同名称，并把操作设置为相应的增加、修改或删除常量。`RefereeUiSendGraphics()` 只接受裁判协议支持的图形数量：`1`、`2`、`5` 或 `7`。

文本示例：

```c
RefereeText text;

if (RefereeUiMakeText(&text, "txt",
                      REFEREE_UI_ADD, 2U, REFEREE_UI_WHITE,
                      20U, 2U, 300U, 400U, "CAP READY")) {
    (void)RefereeUiSendText(&g_referee,
                            sender_id, client_id, &text);
}
```

删除图层或全部 UI：

```c
(void)RefereeUiDelete(&g_referee, sender_id, client_id,
                      REFEREE_UI_DELETE_LAYER, 2U);

(void)RefereeUiDelete(&g_referee, sender_id, client_id,
                      REFEREE_UI_DELETE_ALL, 0U);
```

参数超出协议位宽、指针为空、图形数量不为 1/2/5/7，或底层发送失败时，接口返回 `false`。

### 发送频率

模块只负责构帧和调用发送回调，**没有发送队列、内部 115 ms 延时或自动限频任务**。调用方必须依据当年裁判系统协议规定调度 UI/交互发送，处理 UART busy，并保证上一次 DMA 使用 `tx_buffer` 结束后再发起下一次发送；不要在循环中无间隔连续发送。

## 11. 机器人交互发送

`RefereeSendInteraction()` 用于发送 `0x0301` 机器人交互帧：

```c
static const uint8_t payload[] = {0x01U, 0x02U, 0x03U};

bool ok = RefereeSendInteraction(&g_referee,
                                 sender_id,
                                 receiver_id,
                                 0x0200U,
                                 payload,
                                 sizeof(payload));
```

`data_length` 可为 `0`～`113` 字节。长度为 0 时不需要数据内容；长度大于 0 时 `data` 必须有效。模块不会解释自定义 `data_command_id` 或数据内容，发送双方应自行约定，并确认当年协议允许使用的子命令 ID、接收方 ID和发送频率。

收到的交互数据通过 `RefereeGetData()` 的 `interaction` 字段读取，其 `data_length` 同样可能为 0～113。

## 12. 宿主机测试

测试不依赖 STM32 HAL。在仓库根目录运行：

```powershell
make -C portable_modules/modules/referee/tests clean
make -C portable_modules/modules/referee/tests test
```

编译选项包含 `-std=c11 -Wall -Wextra -Werror -pedantic`。测试通过时会输出裁判接收与 UI 测试的 `PASS` 信息。Windows 环境若没有 `make`，可使用提供兼容命令的 MinGW/MSYS2 终端。

## 13. 常见检查项

- 一直离线：核对 A 板原理图、UART 实例、TX/RX 是否交叉、地线、115200 8N1、DMA/IDLE 回调是否实际触发。
- CRC 错误持续增加：优先检查波特率、数据位/校验/停止位、DMA 新增数据长度以及是否重复 Feed 同一段数据。
- 功率 `valid` 为假：确认 `0x0201` 与 `0x0202` 都在超时窗口内更新。
- UI 没显示：确认本机 robot ID、客户端 ID、图形操作、图层、协议年份和发送频率。
- 发送接口返回假：检查 UART busy、发送回调返回值和 `transmit_failure_count`。
