# 底盘启动保护与快速恢复

控制依据为 `superpowers/specs/2026-10-09-chassis-power-startup-recovery-design.md`。
第一阶段保留现有模型、PID、基础分配和超级电容预算来源。新阈值是调参起点，实际峰值、恢复和加速性能须上板验证。

## 配置

`CHASSIS_POWER_CONTROL_STAGE` 默认 0：0 观察、1 快恢复、2 启动保护、3 完整慢环。
`CHASSIS_POWER_CAPTURE_ENABLE` 默认 0，诊断固件置 1；与现有 17/26 列开关独立。
Make 和 CMake 均支持这两个配置及 `POWER_METER_ENABLE`。每个组合使用独立目录，避免复用不同宏的对象文件。

PowerShell 构建诊断基线：

```powershell
New-Item -ItemType Directory -Force build/power-observe | Out-Null
make -j8 BUILD_DIR=build/power-observe CHASSIS_POWER_CONTROL_STAGE=0 CHASSIS_POWER_CAPTURE_ENABLE=1 POWER_METER_ENABLE=1 UART1_USE=1
```

CMake 接入同样使用独立目录，例如：

```powershell
cmake -S . -B build/cmake-power-observe -G Ninja -DCHASSIS_POWER_CONTROL_STAGE=0 -DCHASSIS_POWER_CAPTURE_ENABLE=1 -DPOWER_METER_ENABLE=1 -DUART1_USE=1
cmake --build build/cmake-power-observe
```

## 记录与排空

四轮低速、低电流持续 100 ms 后，至少 300 rpm 目标阶跃触发一次记录。
记录频率为 500 Hz，25 条前窗与 125 条后窗，共 150 条固定 96 字节记录。
记录完成后冻结整段历史；未排空期间的新触发只增加丢失计数。达到 t90 或 3 s 超时后停止本轮测试，再置零力模式，低优先级任务向 USART1 日志通道排空。
通道采用现有 250000 波特率，排空每帧至少间隔 21 ms，最多约 4.86 kB/s；队列满时保留当前帧重试。裁判串口模式不提供该通道。
串口原始数据保存为二进制，混入的普通日志不影响帧搜索：

```powershell
./scripts/decode-chassis-power-capture.ps1 -InputPath capture.bin -OutputPath capture.csv
```

工具检测 CRC、版本、序号、重复与缺帧，完整性失败时拒绝导出。一个文件内 capture_id 必须唯一；长时间记录应分文件保存。

## 二进制版本 1

每帧 102 字节：ASCII `CPC1`、96 字节载荷、CRC16/MODBUS。CRC 初值 0xffff，多项式 0xa001，覆盖前 100 字节。
整数、IEEE754 float32 和 CRC 均为小端；载荷通过逐字段编码产生，不依赖结构体布局。

| 载荷偏移 | 字段 |
|---|---|
| 0..7 | version:u8、kind:u8、capture_id:u16、sequence:u16、state:u8、flags:u8 |
| 8..15 | 控制 tick:u32、功率计接收 tick:u32 |
| 16..55 | raw_current、final_current、feedback_current、actual_rpm、target_rpm；各四个 int16，轮序 LF/RF/LB/RB |
| 56..87 | B、实测功率、实际转速模型功率、预测转速功率、C_fast、C_slow、E_excess、E_max；各 float32 |
| 88..95 | 四轮反馈年龄:u8、功率计年龄:u8、使能/驱动/制动掩码:u8 |

年龄饱和到 255 ms；flags 的 0x80 表示 int16 量化溢出或非有限数组值。电流使用原始命令/反馈单位，不解释为安培。
flags 的低四位依次为非有限需求、预测约束不可行、增长兜底、整数搜索预算耗尽；第 4/5 位记录控制阶段。
记录的 guard_model_w 使用实际最终命令与预测转速计算；建议命令、候选预测功率和建议预测功率通过 DJI 诊断快照读取。观察模式的建议值不冒充实际发送值。
kind=1 为控制快照；kind=2 为末尾摘要。摘要偏移 8 为启动 tick，16/20/24/28 分别为 t90 状态、t90 毫秒、丢失触发数、记录数，均 u32。
t90 状态：0 未开始、1 待完成、2 达到、3 无效、4 超时。t90 报告首次达到 90% 的时刻，经连续保持 20 ms 确认；目标变化、停机使尚未完成的指标无效。
接收功率通过零阶保持积分，重复 meter_tick 不代表独立测量；500 Hz 记录不提高功率计带宽。

## 启动与快恢复初值

启动须四轮在线使能，平均绝对转速低于 100 rpm、上次最终电流最大值低于 500，持续 100 ms；随后至少 300 rpm 阶跃且存在驱动需求。
反馈容忍最多 30 ms，阈值为 1.30 B_start，能量额度为 0.30 B_start × 0.030 s；模型预算不增加。
预算下降、制动、陈旧或离线结束容忍；500 ms 冷却在零力切换和异常降级中保留，防止重新满足低速条件后反复领取额度。显式重新初始化清空状态。测量恢复至少两个不同时间戳的新鲜样本后才允许领取额度。
转速差分只使用不同序号且接收时间间隔有效的 CAN 反馈；10 ms 加速度滤波、8 ms 正加速度预测。首两个有效反馈前的增长上限为 max_current_command / 5 ms。
方向滞回为进入近零 50 rpm、退出 100 rpm；需求进入 200、退出 100 命令单位。
快保护增益 0.5，低于 P_ref−0.5 W 的两个不同时间戳样本且保持至少 10 ms 后，以 60 W/s 释放；功率计超过 20 ms 陈旧时按 20 W/s 释放。
非单调二次可行区间与增长上限共同求解；最终整数命令再验功率。整数化越界时最多沿八个邻近量化断点退让，再按整数命令区间的轮级二次功率下界剪枝，从最大比例向下搜索。全部区间均不可行时标记不可行；默认 256 个搜索节点耗尽时单独标记 `guard_search_exhausted`，两种情况均驱动置零。节点预算可配置为 1..1024，须结合真实任务耗时验证。制动候选不增加，也不抵扣负功率。
功率计关闭时无容忍、快慢补偿，保留预测约束和反馈不足兜底。反馈超过 5 ms 陈旧时停止外推，Daemon 离线后输出零。

阶段 3 的慢环只在 RUN 连续稳定 100 ms、有驱动需求且需求充足、基础模型实际减少驱动电流、数据新鲜且无制动时学习。高速零电流滑行不学习正补偿，并纳入低需求衰减。

阶段 3 另有持续超限的负向通道：新鲜功率连续高于目标加死区 100 ms、四轮反馈新鲜、周期有效、有驱动且无制动、没有启动容忍时，允许在 RUN/CLAMP 中继续向下纠偏。该通道不要求模型已限幅或预测需求充足，补偿和积分上限为 0 W；正在负向纠偏时暂停低需求衰减。底盘将外环死区通过控制快照传给电机任务，两个模块使用同一确认阈值。

功率样本升高并加深快保护会重新确认，短尖峰不进入慢积分；新负向窗口重置滤波为当前功率，避免之前尖峰的拖尾。阈值升高重新计时，阈值降低保留已有确认。瞬态快照发布确认阈值，底盘目标先改变时拒绝使用更低阈值的旧确认。原有阶段 0/1/2 的慢环行为保持不变。
采用 0.3 W 连续死区，Kp=0.5、Ki=1/s、K_aw=10/s；回算反馈仅含慢环自身限幅和 20 W/s 增长，不含快修正和临时正补偿撤销。
低需求持续超过 500 ms 后，历史补偿与积分以 1 W/s 衰减；预算下降超过 1 W 清除正学习状态，零力/急停清空控制状态。

## 验证

主机记录与解码测试：

```powershell
make -C tests/chassis_power_capture CC=gcc test
./tests/chassis_power_capture/test_decoder.ps1
```

2026-10-09 主机验证结果：19 组主机回归全部通过；DJI 集成覆盖四个阶段 × 功率计开关 × 17/26 列，共 16 个独立配置；两种遥测格式单独回归通过。记录解码验证有效数据、CRC 损坏、缺帧、重复、截断及未知版本。

ARM GNU 13.3.1 / Make / `-Og` 完整构建四个阶段 × 功率计开关 × 普通/诊断，共 16 个配置，目录为 `build/startup-stage*-meter*-capture*`。本环境没有 CMake/Ninja 可执行程序，CMake 配置已同步，未执行 CMake 构建。

同一工具链重新构建原始提交 `0dd6d57`：`.data=928 B`、`.bss=38364 B`，静态合计 39292 B。新增版本最大 `.data=928 B`、`.bss=53488 B`，合计 54416 B；新增 15124 B，小于 16384 B。普通固件最大新增 624 B。上述比较使用 map 的实际 `.data/.bss`，不把链接器预留堆栈重复计入。

`-fstack-usage` 显示诊断版 `ApplyChassisPowerGroup` 单帧 1064 B、整数搜索后备单帧 456 B，原电机任务 256 words 栈不足，已改为 512 words（2048 B，使用现有 FreeRTOS 堆）。静态报告不能替代运行时栈检查。诊断日志固件提供调试器变量 `chassis_power_motor_max_dt_ms` 和 `chassis_power_motor_stack_free_words`；后者每 200 ms 在低优先级任务采样，单位为 4 字节 word。

上板分别记录架空/落地、20/30/40 W、正反直行、横移、自旋，各至少五次；记录负载、电压和超级电容状态。
先用阶段 0 采基线，再依次使用 1、2、3，每次只改变一个控制因素。
验收目标沿用设计：t90 中位增幅不超过 5%，同时报告最差值；窗口结束后超限不持续；充足需求下恢复争取 250 ms 内；稳态驱动 2 s 平均误差争取 ±0.5 W。
同时检查最新 map、任务栈高水位和真实控制耗时，电机任务总耗时须满足 1 ms 并保留余量。

每轮操作：零力等待至少 500 ms，确认四轮和功率计数据新鲜；施加固定可达目标并保持，t90 达到或 3 s 超时后置零力；等 151 帧排空（至少约 3.2 s）后保存二进制并解码。`DJIChassisPowerGetSnapshot()` 查看实际命令、建议命令和状态；诊断版的 `chassis_capture` 查看 t90 状态和丢失触发计数。

架空与落地分别报告：t90 中位值/最大值及相对阶段 0 的变化、原始功率峰值、P_raw>B 的持续时间和积分能量、恢复到 P_ref±max(0.5 W,2% P_ref) 并保持 50 ms 的时间、稳定 2 s 的平均误差、最大任务耗时及最小剩余栈。分析实测功率时按 meter_tick 去重；短记录不覆盖完整 3 s 加速过程，必要时结合 25 Hz 常规遥测。

本次没有执行硬件采集；上面的性能与任务耗时/栈余量均待上板测量。第二阶段分支模型仍等待独立标定数据。

2026-10-11 持续超限修复增加 `tests/chassis_power_overpower`：在可控响应中注入 8 W 模型偏差，50 W 预算、49 W 目标、Kp=1/Ki=1.5 时，5 s 后实测响应约 49.34 W（原算法稳定在 53 W）。这只是主机响应仿真。回归还覆盖模型需求门槛、死区、启动、已有 CLAMP 中的尖峰、陈旧数据、异常周期、停机、阈值升降、目标与快照的先后顺序和 tick 回绕。

运行 `make -C tests/chassis_power_overpower CC=gcc test`。DJI 集成阶段 0/1/2/3 × 功率计开关 × 普通/标定格式的 16 个组合、已有外环/快保护/功率模型/预算/采集/自旋回归均通过。

ARM GNU 13.3.1 已构建最终阶段 3 固件：`make -j8 BUILD_DIR=build/persistent-overpower-stage3 CHASSIS_POWER_CONTROL_STAGE=3 POWER_METER_ENABLE=1`。产物为该目录下 `basic_framework.elf/.hex/.bin`，使用当前工作区参数；没有执行烧录。
