# 底盘功率模型标定 RTT 设计

## 目标

为 M3508 底盘功率模型的 `k0` 至 `k5` 六个系数提供可直接筛选、
对齐和回归的 RTT CSV 数据。保留现有逐电机诊断信息，同时明确功率计
样本是否有效、何时接收，以及四个底盘电机是否真正允许输出。

本次只改观测链路，不修改功率模型方程、模型系数、四轮功率分配、外环
控制策略、控制周期或 CAN 协议。

## 现状与问题

现有 19 列 RTT CSV 已包含拟合所需的四轮限流后电流命令、四轮实际转速
和实测总功率。它存在三个标定风险：

1. 功率计离线时实测功率被写成 `0`，无法与真实零功率区分；
2. CSV 不包含功率计帧的接收时间，无法识别重复测量或离线对齐电机样本；
3. CSV 不包含四个电机的输出使能状态，单个电机离线或被停止的样本可能
   被错误用于含有 `4*k0` 的整车回归方程。

最后一列当前名为 `p_cap_cw`，但数据源已经是独立功率计，因此名称与实际
语义不一致。

## 方案选择

在现有 RTT 流上追加样本元数据，而不是新增第二套标定专用 RTT 流。
这样保留逐电机数据，便于离线检查异常和重新构造回归特征，也不增加运行时
模式或编译开关。

现有前 18 列的顺序和值保持不变。第 19 列只把表头从 `p_cap_cw` 更名为
`p_meter_cw`，值和百分之一瓦的缩放保持不变；随后追加三个整数列：

```text
meter_online,meter_tick_ms,motor_enabled_mask
```

最终固定 22 列表头为：

```text
tick,i_lf,i_rf,i_lb,i_rb,rpm_lf,rpm_rf,rpm_lb,rpm_rb,target_lf,target_rf,target_lb,target_rb,p_lf_cw,p_rf_cw,p_lb_cw,p_rb_cw,p_total_cw,p_meter_cw,meter_online,meter_tick_ms,motor_enabled_mask
```

## 字段语义

- `tick`：生成该电机遥测快照时的系统毫秒 tick；
- `i_*`：实际下发给四个底盘电机的限流后电流命令；
- `rpm_*`：四个底盘电机的实际转速；
- `target_*`：四个底盘电机的目标转速；
- `p_*_cw`：当前模型的逐电机预测功率，单位为百分之一瓦；
- `p_total_cw`：当前模型的四电机预测总功率，单位为百分之一瓦；
- `p_meter_cw`：独立功率计的最近一次实测总功率，单位为百分之一瓦；
- `meter_online`：该功率计样本是否仍在在线时限内，值为 `0` 或 `1`；
- `meter_tick_ms`：该功率计样本对应 CAN 帧的最后接收 tick；从未收到有效帧时为 `0`；
- `motor_enabled_mask`：四个底盘电机的输出使能位掩码。

电机掩码位定义为：

```text
bit0 = LF
bit1 = RF
bit2 = LB
bit3 = RB
```

因此 `motor_enabled_mask == 15` 表示四个电机均在线、功率模型有效且未被
`DJIMotorStop()` 停止。

## 数据流与一致性

`PowerMeterStatus_s` 公开最近有效功率帧的接收 tick。`PowerMeterGetStatus()`
继续在临界区内复制功率计状态，并把在线判定、功率值和接收 tick 作为同一份
快照返回。

底盘任务通过扩展后的功率测量 setter，把以下三个字段一次交给底盘功控实例：

```text
power_w, online, last_rx_tick_ms
```

setter 在临界区内整体更新测量快照。电机任务生成 100 Hz RTT 行前也在临界区
内整体复制该快照，避免功率值、在线标志和接收 tick 来自不同更新周期。

`ChassisPowerTelemetryBuild()` 在已有 `output_enabled[]` 输入基础上生成四位
`motor_enabled_mask`，不重复推导电机状态。RTT 格式器只负责把已构造好的
遥测和测量快照格式化，不访问硬件或 RTOS 状态。

当 `POWER_METER_ENABLE == 0` 时，测量快照固定为：

```text
power_w = 0
online = false
last_rx_tick_ms = 0
```

## 拟合样本规则

六系数整车回归只使用满足以下条件的行：

```text
meter_online == 1
motor_enabled_mask == 15
p_meter_cw >= 0
sum(i_i * rpm_i) >= 0
```

定义：

```text
I_i = abs(i_i) / 1000
n_i = abs(rpm_i)
P_measured = p_meter_cw / 100
```

每行回归特征为：

```text
[4, sum(I_i), sum(n_i), sum(I_i*n_i), sum(I_i^2), sum(n_i^2)]
```

对应模型为：

```text
P_measured = 4*k0
           + k1*sum(I_i)
           + k2*sum(n_i)
           + k3*sum(I_i*n_i)
           + k4*sum(I_i^2)
           + k5*sum(n_i^2)
```

`meter_tick_ms` 用于识别同一功率计帧被多次打印的行，并允许离线分析时评估
功率测量相对电机快照的时间差。固件不在在线路径中插值或重采样。

## 格式与资源边界

RTT 继续每十个电机控制周期输出一次，即标称 100 Hz。每行仍由固定静态缓冲区
完整格式化后一次写出；缓冲区从 256 字节增至 320 字节，容纳新增字段及整数
极值，不允许输出截断的半行。

所有非有限浮点数继续按现有规则转换为 `0`，功率转换继续安全饱和到有符号
32 位整数。`meter_online` 只输出 `0` 或 `1`，电机掩码只使用低四位。

## 文件范围

修改：

- `modules/power_meter/power_meter.h`：在功率计状态中公开最后接收 tick；
- `modules/power_meter/power_meter.c`：在一致状态快照中返回该 tick；
- `application/chassis/chassis.c`：传递完整功率测量快照；
- `modules/motor/DJImotor/dji_motor.h/.c`：保存一致测量快照并生成电机使能掩码；
- `modules/algorithm/power_model.h/.c`：在结构化遥测中保存使能掩码；
- `modules/telemetry/rtt_telemetry.h/.c`：定义并格式化 22 列 CSV；
- 对应主机测试：固定接口、表头、代表性行、离线值和掩码行为。

不新增标定脚本，不修改模型参数，也不改变 RTT 输出频率。

## 测试与验证

按测试先行实施：

1. RTT 测试先断言新的 22 列表头和代表性行，确认旧实现失败；
2. 遥测构造测试覆盖 `0`、部分使能和 `15` 三种掩码；
3. 功率计测试覆盖从未收帧、有效收帧和超时离线时的最后接收 tick；
4. setter/输出路径测试确认功率、在线标志和 tick 同行输出；
5. 运行 `rtt_telemetry`、`power_model`、`power_meter` 及相关功率控制回归测试；
6. 分别以 `POWER_METER_ENABLE=1` 和 `POWER_METER_ENABLE=0` 构建 ARM 固件；
7. 检查 RTT 代表性实机行严格为 22 列，且正常运行时 `meter_online=1`、
   `motor_enabled_mask=15`。

## 完成标准

- 每个 RTT 数据行和表头严格包含 22 列；
- 现有前 18 列的顺序、缩放和数值行为不变；
- 第 19 列准确命名为 `p_meter_cw`；
- 离线功率值可通过 `meter_online` 明确剔除；
- 功率计样本可通过 `meter_tick_ms` 识别和对齐；
- 单个电机不可输出时，掩码准确指出对应轮位；
- 四轮均有效时掩码为 `15`，可直接作为六系数拟合筛选条件；
- RTT 满缓冲或格式化失败不影响功率控制和电机输出；
- 相关主机测试和两种功率计固件配置全部通过。
