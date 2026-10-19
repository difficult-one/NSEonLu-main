# 底盘功率控制与标定数据采集

当前外环误差死区为 **0.3 W**。目标仍是基准预算的 95%，PI 增益为 Kp=0.5、Ki=1，功率低通时间常数为 30 ms。缩小死区让持续的 0.3～1 W 误差继续产生纠偏；实际收敛精度、波动和超调需通过上板数据确认。

## 切换输出模式

`modules/telemetry/rtt_telemetry.h` 中的 `CHASSIS_POWER_CALIBRATION_ENABLE` 默认为 `0`，普通输出仍为原来的 17 列。设为 `1` 后，输出变为 26 列，前 17 列保持原字段、顺序和单位。只能设置为 0 或 1。

也可以通过构建参数选择模式。两种模式必须使用不同构建目录，避免切换宏后复用旧对象。在仓库根目录运行：

```powershell
# 标定模式
New-Item -ItemType Directory -Force build/power_calibration | Out-Null
make -j8 BUILD_DIR=build/power_calibration CHASSIS_POWER_CALIBRATION_ENABLE=1

# 普通 17 列模式
New-Item -ItemType Directory -Force build/power_control | Out-Null
make -j8 BUILD_DIR=build/power_control CHASSIS_POWER_CALIBRATION_ENABLE=0
```

烧录所选目录内的 `basic_framework.elf`、`.hex` 或 `.bin`。CMake 用户可以在配置时加入 `-DCHASSIS_POWER_CALIBRATION_ENABLE=1`，恢复普通模式时使用 `=0`，同样分别使用独立构建目录。

当前 USART1 日志初始化为 **250000 baud、8N1、无流控**，串行监视器需要匹配。日志通过固定队列非阻塞发送；队列满时整行丢弃，可以检查 `tick` 间隔和 `BSPUARTLogGetDiagnostics()` 的丢弃计数。

## CSV 字段

前 17 列详见 `superpowers/specs/2026-09-22-chassis-power-control-telemetry-design.md`。所有 `_cw` 字段除以 100 得到 W；`min_scale_pm` 除以 1000 得到电流缩放比例。

新增字段如下，轮序统一为左前、右前、左后、右后：

| 列 | 字段 | 含义与单位 |
|---:|---|---|
| 18 | `i_lf_cmd` | 左前限流后的输出电流命令 |
| 19 | `i_rf_cmd` | 右前限流后的输出电流命令 |
| 20 | `i_lb_cmd` | 左后限流后的输出电流命令 |
| 21 | `i_rb_cmd` | 右后限流后的输出电流命令 |
| 22 | `n_lf_rpm` | 左前实际转速，rpm |
| 23 | `n_rf_rpm` | 右前实际转速，rpm |
| 24 | `n_lb_rpm` | 左后实际转速，rpm |
| 25 | `n_rb_rpm` | 右后实际转速，rpm |
| 26 | `meter_tick_ms` | 本行功率计样本接收时间，ms |

电流与转速均保留正负号，按最近整数输出。电流是 DJI 原始命令量，不是实测安培；停止或离线电机的输出命令为 0。转速来自模型本次计算所用的实际反馈，内部的角度每秒值已除以 6 转成 rpm。

完整表头：

```text
tick,nominal_budget_cw,target_power_cw,corrected_budget_cw,effective_limit_cw,p_unlimited_cw,p_limited_cw,p_meter_cw,p_filtered_cw,error_cw,correction_cw,min_scale_pm,limited_mask,outer_mode,meter_online,meter_age_ms,motor_enabled_mask,i_lf_cmd,i_rf_cmd,i_lb_cmd,i_rb_cmd,n_lf_rpm,n_rf_rpm,n_lb_rpm,n_rb_rpm,meter_tick_ms
```

## 采集与使用

1. 使用标定模式固件，保存表头及连续数据。分别采集自旋、直行、横移，以及不同速度和负载；每个稳定工况保留连续多秒数据。
2. 优先使用 `meter_online=1` 且 `motor_enabled_mask=15` 的样本。检查 `meter_age_ms` 和 `meter_tick_ms`，识别离线、重复的功率计样本及数据间断。
3. 用第 7 列 `p_limited_cw` 对比第 8 列 `p_meter_cw` 评估模型误差；第 6 列是限流前需求，不代表电机实际获得的电流。
4. 用第 3 列 `target_power_cw` 对比第 9 列 `p_filtered_cw` 评估外环跟踪。第 10 列误差已应用死区，不能据其为 0 就认为原始误差严格为零。
5. 拟合现有正向耗电模型时，使用 `I_i=abs(i_i_cmd)/1000`、`n_i=abs(n_i_rpm)` 和 `P=p_meter_cw/100`。剔除负功率样本，并逐轮检查 `i_i_cmd*n_i_rpm`：任一轮为负的制动样本都应剔除或单独分析，不能仅用四轮乘积之和判断，否则会保留部分驱动、部分制动的混合工况。近零且方向不确定的样本单独处理，保留合适的空载/静止样本以识别基础功率。
6. `tick` 是电机快照时间；`meter_tick_ms` 是接收时间，传感器内部采样可能更早。动态分析需核对时间对齐，标定优先选取稳定片段，避免把测量延迟拟合进模型系数。
7. 六个模型参数应通过多个工况联合拟合，并使用留出的独立片段验证。当前 340 ms 自旋片段不能独立确定全部系数，也不能证明新参数在其他工况的精度。

## 主机回归

```powershell
make -B -C tests/rtt_telemetry CC=gcc CHASSIS_POWER_CALIBRATION_ENABLE=0 test
make -B -C tests/rtt_telemetry CC=gcc CHASSIS_POWER_CALIBRATION_ENABLE=1 test
make -B -C tests/dji_chassis_power_rtt CC=gcc CHASSIS_POWER_CALIBRATION_ENABLE=0 test
make -B -C tests/dji_chassis_power_rtt CC=gcc CHASSIS_POWER_CALIBRATION_ENABLE=1 test
make -B -C tests/chassis_power_outer_loop CC=gcc test
make -B -C tests/power_model CC=gcc test
```

本次未进行模型重新拟合；六个系数维持原值。采集后的数据用于分别验证外环目标跟踪和模型预测精度。
