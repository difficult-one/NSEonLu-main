# 连续缓冲估算与渐变恢复实施计划

> 在当前会话逐项执行，先验证失败测试，再完成实现和独立审查。

**目标：** 减少低负载期间的保守额度损失，使缓冲恢复力度随能量缺口连续变化。

**架构：** 观测器保留在纯算法 `chassis_power_budget` 内。适配层只提供裁判帧时间戳和现有输入功率，不改双端协议。配置保留原安全预留和最大减让能力。

**技术：** C11、GNU Make、主机 GCC、STM32F407 ARM GCC。

## 约束

- 默认直供，电容模式仍使用 F334 输入功率而非总负载功率。
- 保留输出许可、最终功率上限、15/30 J 恢复滞回、5 J 硬底座和 100 ms 恢复确认。
- 保留 0.1 s 结算相位预留、3 J 能量余量、5 W 功率余量；负输入不额外计入再生收益。
- 仅暂存本任务变更，底盘文件原有调参继续留在工作区。

## 任务 1：纯算法观测器与恢复策略

文件：`modules/algorithm/chassis_power_budget.[ch]`、`tests/chassis_buffer_budget/test_chassis_buffer_budget.c`。

接口：`ChassisPowerBudgetUpdate` 签名保持不变；输入增加 `uint32_t buffer_tick_ms`；缓冲配置增加 `float observer_max_step_s` 和 `float recovery_target_time_s`；输出增加 `float recovery_reduction_w`。

- [x] 添加下限积分失败用例。新帧 60 J、输入 40 W、帧龄 0 时初始下限 50.5 J；连续 5 ms 有效区间的下限增加 0.175 J：

```c
input.measured_input_power_w = 40;
input.buffer_age_s = 0.005f;
CHECK(ChassisPowerBudgetUpdate(&config, &input, 0.005f, &state, &out));
NEAR(50.675f, out.buffer_lower_j);
```

- [x] 添加时间戳、间隔/反馈降级、帧校正和渐变恢复用例，运行 `make -C tests/chassis_buffer_budget -B test`，确认失败集中在新行为。
- [x] 新帧沿用原保守锚点；相同帧按帧龄增量结算，区间功率取两端较大值，限额取较小值；失效区间扣减最坏耗能并撤销增补。跨帧连续性和恢复确认计时也按时间戳与帧龄计算真实间隔。
- [x] 根据以下公式实现恢复，并在硬底座/预测越界时使用最大减让：

```c
recovery_reduction_w = ClampFloat(
    (buffer->recovery_exit_j - lower) / buffer->recovery_target_time_s,
    0, buffer->recovery_power_w);
target_input_budget = fmaxf(0, input->referee_limit_w - recovery_reduction_w);
```

- [x] 运行动态缓冲和旧电容预算测试，确认原硬上限与模式兼容测试仍通过。

## 任务 2：接入、闭环验证与提交

文件：`modules/algorithm/chassis_energy_config.h`、`application/chassis/chassis.c`、本设计和计划文档。

- [x] 配置与时间戳接入：

```c
.observer_max_step_s = CHASSIS_BUFFER_OBSERVER_MAX_STEP_S,
.recovery_target_time_s = CHASSIS_BUFFER_RECOVERY_TARGET_TIME_S,
.buffer_tick_ms = referee_snapshot.buffer_tick_ms,
```

默认宏分别为 `0.020f` 和 `1.0f`。

- [x] 用独立直供/电容能量积分仿真验证延迟响应、能量下界、恢复和板端总能力限制；两种仿真逐周期检查估算下限不超过物理能量。
- [x] 运行相关主机测试和固件配置构建：

```powershell
make -C tests/chassis_buffer_budget -B test
make -C tests/chassis_power_budget -B test
make -C tests/referee_power -B test
make -C tests/referee_integration -B test
make -C tests/referee_stream_parser -B test
make -C tests/chassis_power_outer_loop -B test
make -C tests/chassis_power_transient -B test
make -C tests/chassis_power_overpower -B test
make -j4
make -j4 BUILD_DIR=build/buffer-disabled CHASSIS_BUFFER_POWER_ENABLE=0
make -j4 BUILD_DIR=build/buffer-supercap CHASSIS_POWER_SOURCE_MODE=1
```

- [x] 独立审查、修正发现、记录验证结果；构造底盘文件仅包含本次三个接入字段的补丁，核对原有参数修改仍留在工作区，暂存该补丁及其他本任务文件，创建本地提交。

建议提交信息：`feat: integrate buffer energy and scale recovery`。

## 验证记录

- 首次失败验证：旧实现出现 30 个行为断言失败，集中在连续估算、反馈降级、新帧识别和渐变恢复。
- 独立审查后的失败验证：新增跨帧连续性及重复观测恢复确认测试产生 6 个断言失败；修正后动态缓冲测试通过。
- 相关 10 套主机测试通过，含裁判普通/legacy 分支；最终时间边界修正后再次验证受影响的动态缓冲和旧预算套件。
- 默认直供、关闭缓冲策略、电容模式固件编译和链接通过；最终时间边界修正后重新构建三种配置。ARM 工具链在沙箱外执行，仅有既有 newlib/libnosys 系统调用警告和配置 pragma 提示。
- 实机刷写、采样误差和结算相位辨识尚未执行；1 s 目标恢复时间与现有安全预留需要结合实测调整。
- 用户进一步明确：本功能是状态估算和提前预算调整，具有前馈作用；真实功率仍由实测闭环控制。实机先测裁判更新间隔与结算延迟，再判断收益和所需余量。
