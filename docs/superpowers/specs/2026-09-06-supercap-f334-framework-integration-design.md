# F334 超级电容板与 NSEonLu 框架分层接入设计

## 1. 目标

让 `NSEonLu-main` 的 F407 底盘主控通过 CAN 驱动独立运行的
`RM2024-SuperCapacitorController-master` F334 超级电容板，并把电容板给出的
可用功率、剩余能量和故障状态接入现有四电机功率管理链路。

接入完成后：

- F334 继续独立负责 ADC 采样、HRTIM、双向 DCDC 控制、充放电闭环和硬件保护；
- F407 负责裁判系统数据、DCDC 使能、底盘总功率预算和四个 M3508 的功率分配；
- F407 与 F334 使用现有 `0x061/0x051` CAN 协议通信；
- 超级电容离线、未使能或报错时，底盘立即撤销额外功率并退回裁判功率；
- 超级电容异常不直接清零四轮电流，避免在仍可由裁判系统供电时误停底盘；
- 第一阶段不修改 F334 的高频控制环。

## 2. 当前状态与问题

### 2.1 F334 电容板协议

F334 在 `Core/Src/Communication.cpp` 中：

- 接收标准帧 `0x061`；
- 发送标准帧 `0x051`；
- 两个方向的 DLC 均为 8；
- 使用 STM32 小端字节序；
- 状态帧包含一个从非对齐地址开始的原始 `float`。

### 2.2 F407 框架旧协议

框架 `modules/super_cap` 当前：

- 发送 `0x302`，接收 `0x301`；
- 把反馈解析为三个大端 `uint16_t`：电压、电流和功率；
- `SuperCapSend()` 无条件从调用者地址复制 8 字节；
- 没有在线监测、DLC 校验、故障字段或原子状态快照；
- `chassis.c` 只初始化了电容实例，没有周期发送命令，也没有使用反馈。

因此两端目前无法直接通信。

### 2.3 已有底盘功率链路

框架已经把纯 C 功率模型挂在 `DJIMotorControl()` 的 PID 输出与 CAN 打包之间：

1. 计算全部 DJI 电机的原始 PID 输出；
2. 对显式注册的四个底盘 M3508 进行功率分配和限流；
3. 统一执行启停、CAN 打包和发送。

本设计不替换这条链路，只为它提供经过超级电容策略计算的总功率预算。

## 3. 选定架构

采用“协议驱动、预算策略、电机限流”三层结构：

```text
裁判系统
  ├─ chassis_power_limit
  ├─ buffer_energy
  └─ chassis output enable
          │
          ▼
F407 super_cap 协议驱动 ── 0x061 ──► F334 电容板
          ▲                              │
          └────────── 0x051 ─────────────┘
                         │
                         ▼
              chassis_power_budget
                         │
                         ▼
              DJI 四电机功率管理组
                         │
                         ▼
                   DJI 电机 CAN
```

三个层次的职责边界：

- `super_cap`：只负责 CAN 编解码、最新状态快照和在线监测；
- `chassis_power_budget`：只根据裁判限制、电容状态和能量计算底盘预算；
- `dji_motor + power_model`：只根据总预算分配四轮功率并限制目标电流。

F334 的实际底盘功率只用于诊断，不作为 F407 上另一套闭环的反馈，避免两套控制器互相争夺。

## 4. CAN 协议

### 4.1 F407 到 F334：控制帧 `0x061`

| 字节 | 类型 | 含义 |
|---:|---|---|
| 0 | 位域 | bit0 `enableDCDC`；bit1 `systemRestart`；bit2～7 为 0 |
| 1～2 | `uint16_t` 小端 | 裁判系统底盘功率上限，单位 W |
| 3～4 | `uint16_t` 小端 | 裁判系统缓冲能量，单位 J |
| 5～7 | 保留 | 全部写 0 |

示例：使能 DCDC、功率上限 `100 W`、缓冲能量 `50 J`：

```text
01 64 00 32 00 00 00 00
```

`systemRestart` 首版始终写 0，不进入周期控制接口。若后续需要，只能由单独的一次性维护接口置位一帧。

### 4.2 F334 到 F407：状态帧 `0x051`

| 字节 | 类型 | 含义 |
|---:|---|---|
| 0 | 位域 | bit7 表示输出未使能；bit0～6 为错误码 |
| 1～4 | IEEE-754 `float` 小端 | 当前底盘功率，单位 W |
| 5～6 | `uint16_t` 小端 | F334 估计的可用底盘总功率，单位 W |
| 7 | `uint8_t` | 电容能量，`0～255` 对应 `0～100%` |

错误码定义与 F334 保持一致：

| 位 | 含义 |
|---:|---|
| 0 | 欠压 |
| 1 | 过压 |
| 2 | Buck-Boost 故障 |
| 3 | 短路 |
| 4 | 过温 |
| 5 | 无功率输入 |
| 6 | 电容故障 |
| 7 | DCDC 输出未使能 |

### 4.3 编解码规则

- 不在 F407 上直接强制转换 `packed struct`；
- `uint16_t` 使用显式小端移位编解码；
- `float` 先从四个字节重建 `uint32_t`，再通过 `memcpy` 写入对齐的 `float`；
- 只有标准帧 `0x051`、DLC 为 8、浮点值有限时才接受状态并重载在线守护；
- CAN 回调先在局部变量中完成全部校验，再一次性更新实例状态；
- 任务读取状态时使用短临界区复制快照，避免 1 kHz 接收中断造成撕裂读取。

## 5. 协议驱动接口

用类型化接口替换旧的任意字节指针接口：

```c
typedef struct
{
    bool enable_dcdc;
    uint16_t referee_power_limit_w;
    uint16_t referee_buffer_energy_j;
} SuperCapCommand_s;

typedef struct
{
    bool online;
    bool output_enabled;
    uint8_t error_code;
    float chassis_power_w;
    uint16_t available_power_limit_w;
    float energy_ratio;
} SuperCapStatus_s;

SuperCapInstance *SuperCapInit(const SuperCap_Init_Config_s *config);
bool SuperCapSendCommand(SuperCapInstance *instance,
                         const SuperCapCommand_s *command);
bool SuperCapGetStatus(SuperCapInstance *instance,
                       SuperCapStatus_s *status);
```

约束：

- 超级电容仍为单实例，但 CAN 回调通过 `CANInstance.id` 找到所属实例，不依赖文件级全局指针；
- 初始化后状态默认为离线；
- `DaemonTask()` 当前为 100 Hz，守护重载值设为 20，对应 200 ms；
- 收到合法 `0x051` 状态帧时调用 `DaemonReload()`；
- 离线回调只把状态标记为离线，不发送控制帧、不阻塞、不停止电机。

## 6. 周期控制和使能

`ChassisTask()` 当前约 200 Hz，每次循环执行：

1. 读取裁判系统最新数据；
2. 构造并发送一帧 `0x061`；
3. 获取电容状态快照；
4. 计算底盘功率预算；
5. 把预算写入 DJI 底盘功率管理组；
6. 设置四轮速度参考值。

DCDC 使能条件：

```text
裁判数据有效
&& power_management_chassis_output == 1
&& chassis_mode != CHASSIS_ZERO_FORCE
```

其中“速度目标为零”不是关闭条件；机器人正常停车时仍允许电容充电。

当 `CHASSIS_POWER_BENCH_TEST == 1` 时，可使用现有 `40 W` 台架上限代替无效裁判上限；生产配置中该宏继续保持 0。

F407 发给 F334 的始终是裁判系统原始功率上限和原始缓冲能量，不能发送经过电容增益后的底盘预算，防止形成正反馈。

## 7. 底盘功率预算策略

### 7.1 输入

```c
typedef struct
{
    float referee_limit_w;
    bool cap_online;
    bool cap_output_enabled;
    uint8_t cap_error_code;
    float cap_energy_ratio;
    float cap_reported_limit_w;
} ChassisPowerBudgetInput_s;
```

### 7.2 状态

- `READY`：电容在线、输出已使能、错误码为 0、能量和功率字段合法；
- `DEGRADED`：电容在线，但未使能、低电量、报错或字段非法；
- `OFFLINE`：初始化后尚未收到合法状态，或连续 200 ms 未收到合法状态。

### 7.3 能量系数

初始映射采用分段线性函数：

```text
energy <= 0.10: factor = 0
0.10 < energy < 0.30: factor = (energy - 0.10) / 0.20
energy >= 0.30: factor = 1
```

### 7.4 预算公式

首版额外功率硬上限设为 `100 W`：

```text
reported_boost
  = clamp(cap_reported_limit - referee_limit, 0 W, 100 W)

target_budget
  = referee_limit + energy_factor × reported_boost
```

仅 `READY` 状态允许 `reported_boost` 非零；`DEGRADED` 和 `OFFLINE` 的目标预算均为裁判功率上限。

预算变化采用“快降慢升”：

- 目标预算下降时立即生效；
- 目标预算上升时最大斜率为 `200 W/s`；
- 从离线、报错或未使能进入降级时不经过斜率限制，立即撤销额外功率。

计算出的预算传入 `DJIChassisPowerSetLimit()`。功率模型现有 `0.95` 安全系数继续保留，超级电容能量不再通过 `DJIChassisPowerSetAttenuation()` 重复衰减。

后续把 `DJIChassisPowerSetLimit()` 更名为 `DJIChassisPowerSetBudget()`，使接口语义与实际输入一致；不改变内部算法。

## 8. 故障与异常处理

| 情况 | F407 行为 |
|---|---|
| 电容在线且正常 | 按能量系数使用额外功率 |
| 电容能量不高于 10% | 只用裁判功率 |
| 任一 F334 错误位置位 | 立即撤销额外功率，保留裁判功率 |
| F334 报告输出未使能 | 立即撤销额外功率 |
| 200 ms 无合法反馈 | 标记离线，立即撤销额外功率 |
| DLC 错误或 `float` 非有限数 | 丢弃该帧，不喂在线守护 |
| 报告功率低于裁判功率 | 额外功率按 0 处理 |
| 报告功率异常偏大 | 额外部分限制为 100 W |
| CAN 发送邮箱繁忙 | 本周期记失败并在下周期重试，不阻塞底盘任务 |
| 裁判数据无效且非台架模式 | DCDC 命令关闭，底盘功率管理组按无有效预算处理 |

电容故障不直接调用 `DJIMotorStop()`。底盘四轮是否停止仍由机器人急停、裁判底盘输出许可和电机自身离线逻辑决定。

## 9. 文件变更边界

### 9.1 NSEonLu 框架

修改：

- `modules/super_cap/super_cap.h`
  - 删除旧的电压/电流/功率报文结构；
  - 增加命令、状态、错误位和类型化接口。
- `modules/super_cap/super_cap.c`
  - 实现新协议编解码、DLC/数值校验、状态快照和在线守护。
- `application/chassis/chassis.c`
  - 使用 `0x061/0x051`；
  - 周期发送裁判数据；
  - 调用预算策略并更新 DJI 功率管理组。
- `modules/motor/DJImotor/dji_motor.h`
- `modules/motor/DJImotor/dji_motor.c`
  - 把总功率输入接口从 `SetLimit` 更名为 `SetBudget`。
- `Makefile`
  - 加入预算策略源文件和测试所需依赖。

新增：

- `modules/algorithm/chassis_power_budget.h`
- `modules/algorithm/chassis_power_budget.c`
- `tests/super_cap_protocol/test_super_cap_protocol.c`
- `tests/chassis_power_budget/test_chassis_power_budget.c`
- `docs/supercap-can-protocol.md`

保持不变：

- `modules/algorithm/power_model.c` 的预测、分配和限流公式；
- 裁判系统协议解析；
- F407 的 CAN BSP 公共行为。

### 9.2 F334 电容板

第一阶段不修改。

框架通信与台架验证通过后，只修改 `Core/Src/PowerManager.cpp` 的低频状态发送调度：保留 `updateStatus()` 和故障检查的 1 kHz 频率，把 `Communication::feedbackPowerData()` 改为每 5 个 TIM2 周期发送一次，即 200 Hz。HRTIM 电流/电压控制环完全不动。

## 10. 测试策略

### 10.1 主机协议测试

- `100 W / 50 J / enable` 必须编码为 `01 64 00 32 00 00 00 00`；
- 各故障位和 bit7 输出状态逐一解析正确；
- 正常正功率、零功率和负功率 `float` 解析正确；
- DLC 非 8、NaN 和 Infinity 状态帧被拒绝；
- 编解码不依赖结构体对齐。

### 10.2 主机预算测试

- 5%、20%、50% 能量分别得到 0%、50%、100% 的额外功率；
- 电容报告 `180 W`、裁判限制 `100 W` 时，三种能量分别得到 `100 W`、`140 W`、`180 W`；
- 报告超过裁判功率 100 W 以上时被硬限制；
- 离线、未使能和每一个错误位都立即退回裁判功率；
- 预算下降立即生效，预算上升不超过 `200 W/s`；
- NaN、Infinity、负功率和越界能量不会产生非有限输出。

### 10.3 固件与总线验证

- F407 ARM 全量构建通过且保持现有告警策略；
- F334 使用指定 `HARDWARE_ID` 构建通过；
- CAN 分析仪看到 F407 的 `0x061` 为 200 Hz、DLC 8；
- 第一阶段 F334 的 `0x051` 约为 1 kHz，第二阶段降为 200 Hz；
- CAN2 无持续邮箱拥塞、FIFO 溢出或电机通信异常。

### 10.4 实车验证顺序

1. 电容板单独上电，用 CAN 分析仪验证协议；
2. F407 与 F334 连接但不接电机，验证使能、缓冲能量和状态字段；
3. 架空四轮，功率上限固定为现有台架 `40 W`；
4. 低速直行，确认功率模型限流正常；
5. 电容高能量时逐步启用额外功率；
6. 电量从 30% 降至 10%，确认预算平滑回落；
7. 拔掉电容 CAN，确认 200 ms 内撤销增益；
8. 注入故障位，确认撤销增益但不误停整车；
9. 最后测试急加速、急刹车、横移和小陀螺。

## 11. 实施阶段

### 阶段一：建立通信

- 替换框架旧协议；
- 加入状态快照和在线守护；
- 用 CAN 分析仪完成双向报文验证；
- 底盘预算仍固定为裁判功率。

### 阶段二：接入功率预算

- 新增纯 C 预算策略；
- 接入电容能量、可用功率和故障状态；
- 完成主机测试、架空轮和低功率台架验证。

### 阶段三：降低总线负载

- F334 状态发送频率从 1 kHz 降到 200 Hz；
- 重跑掉线检测和 CAN 总线测试；
- 不修改 F334 高频控制环。

## 12. 非目标

本轮不做以下工作：

- 不把 F334 的 HRTIM、ADC 或 PID 控制移入 F407；
- 不重写 F334 的硬件保护；
- 不修改现有电机功率模型系数；
- 不增加多电容实例；
- 不新增第二套底盘电机发送路径；
- 不把电容实际功率接成 F407 上的第二个闭环；
- 不在首次接入时设计多帧版本化协议。

