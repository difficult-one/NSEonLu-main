# 底盘功率闭环遥测精简设计

## 目标

将现有面向功率模型标定的 22 列底盘功率 CSV，替换为面向日常功率闭环整定的 17 列 CSV。新格式应直接显示预算生成、外环纠偏、模型限流和功率计反馈之间的关系，同时减少串口带宽和离线分析中的冗余字段。

本次只修改诊断输出，不改变功率模型、功率预算、外环算法、电机 PID、控制周期或 CAN 输出。

## CSV 格式

列顺序固定如下：

```text
tick,
nominal_budget_cw,target_power_cw,corrected_budget_cw,effective_limit_cw,
p_unlimited_cw,p_limited_cw,p_meter_cw,p_filtered_cw,
error_cw,correction_cw,
min_scale_pm,limited_mask,outer_mode,
meter_online,meter_age_ms,motor_enabled_mask
```

实际输出为一行逗号分隔文本，不包含上述换行。

### 字段语义

- `tick`：生成电机控制快照时的系统毫秒 tick。
- `nominal_budget_cw`：功率预算模块输出、进入外环前的基准预算，单位为百分之一瓦。
- `target_power_cw`：外环控制目标，单位为百分之一瓦。
- `corrected_budget_cw`：外环修正后传给功率模型的预算，单位为百分之一瓦。
- `effective_limit_cw`：功率模型应用衰减系数和安全系数后的有效限制，单位为百分之一瓦。
- `p_unlimited_cw`：四电机限流前预测功率之和，单位为百分之一瓦。
- `p_limited_cw`：使能电机的限流后预测功率之和；未使能电机按零计，单位为百分之一瓦。
- `p_meter_cw`：独立功率计最近一次实测总功率，单位为百分之一瓦，保留负值。
- `p_filtered_cw`：外环使用的一阶低通功率，单位为百分之一瓦。
- `error_cw`：外环输出的误差（已应用死区，死区内为零），单位为百分之一瓦。
- `correction_cw`：外环叠加到基准预算上的修正量，单位为百分之一瓦。
- `min_scale_pm`：四电机 `current_scale` 的最小值，使用千分比，`1000` 表示没有电流缩放。
- `limited_mask`：四电机限流位掩码；`current_scale < 1.0` 的电机对应位为 `1`。
- `outer_mode`：外环模式数值，`0=OFFLINE`、`1=STANDBY`、`2=ACTIVE`。
- `meter_online`：功率计在线标志，值为 `0` 或 `1`。
- `meter_age_ms`：当前电机快照与功率计样本之间的时间差。功率计从未收到有效帧时输出 `0`；否则使用无符号 tick 相减以支持回绕。
- `motor_enabled_mask`：四个底盘电机的输出使能位掩码。

删除以下串口字段：四路限流后电流、四路实际转速、四路目标转速和四路单电机预测功率。这些字段继续保留在内部控制快照中，不改变现有控制或其他诊断接口，只是不再进入功率闭环 CSV。

## 数据流与接口

在 `rtt_telemetry.h` 中增加只包含外环诊断量的 `ChassisPowerControlTelemetry_s`：

```c
typedef struct
{
    float nominal_budget_w;
    float target_power_w;
    float corrected_budget_w;
    float filtered_power_w;
    float error_w;
    float correction_w;
    uint8_t outer_mode;
} ChassisPowerControlTelemetry_s;
```

`application/chassis/chassis.c` 在每次外环更新完成后构造该结构体，并调用新的 `DJIChassisPowerSetControlTelemetry()`。该 setter 在临界区内将整个结构体复制到唯一的底盘功控实例，避免不同字段来自不同更新周期。

外环诊断、功率计状态与模型预算通过各自的现有接口依次发布；任务切换恰好发生在这些调用之间时，单行 CSV 可能短暂组合相邻周期的数据。完整性保证针对外环结构体和功率计三元组各自成立，不保证它们与预算跨接口原子同步。

`DJIMotorControl()` 仍以现有约 1 ms 周期执行，且仍通过现有分频器以约 100 Hz 输出遥测。当前电机周期的 `ChassisPowerOutput_s` 与 `ChassisPowerTelemetry_s` 是该任务的局部完整结果；输出前再在同一个临界区复制以下共享状态：

- 最近的外环诊断结构体；
- 功率计功率、在线状态和接收 tick；

CSV 格式化函数使用这些快照计算总未限流功率、最小缩放值、限流掩码和功率计样本年龄。CSV 继续通过 `BSPLogWrite*` 输出，因此同时遵守当前 RTT/UART 日志出口和整行非阻塞丢弃策略。

## 数值和异常处理

- 所有功率字段继续用百分之一瓦的有符号整数输出。
- 非有限浮点数输出 `0`；功率换算继续饱和到 `int32_t`。
- `min_scale_pm` 限制在 `0..1000`；功率模型状态无效时输出 `0`，以区别于有效且未限流的 `1000`。
- 外环诊断尚未收到首次更新时，其数值保持为零，`outer_mode` 为 `OFFLINE`。
- `meter_tick_ms == 0` 时 `meter_age_ms` 输出 `0`。
- 格式化缓冲区不足时清空首字节并返回 `0`，不发送截断行。
- RTT/UART 缓冲区满时继续丢弃整行，不得阻塞或改变电机输出。

## 测试

`tests/rtt_telemetry` 覆盖：

- 17 列表头和代表性数据行的精确文本；
- 未限流、部分限流和无效模型状态下的 `min_scale_pm` 与 `limited_mask`；
- 功率计样本年龄、tick 回绕和从未收帧；
- 负功率、非有限值、整数饱和和小缓冲区拒绝。

`tests/dji_chassis_power_rtt` 覆盖：

- 新 setter 的外环快照进入下一条输出行；
- 同一行中的模型状态、外环状态和功率计状态来自完整快照；
- 现有 100 Hz 分频行为不变。

最后运行相关遥测、功率模型、外环和 DJI 底盘功控主机测试，并执行 ARM 全量构建，确认接口和包含关系没有破坏目标固件。

## 不在本次范围

- 不保留可切换的“模型标定版”第二套 CSV。
- 不修改功率模型系数或驱动/制动建模方式。
- 不修改安全系数、外环增益、低通时间常数或功率分配策略。
- 不改变串口波特率、DMA 队列大小、RTT 缓冲区或打印频率。
