# 按需求调度功率与缓冲能量实现计划

> **For agentic workers:** Execute this plan inline with test-driven-development; request a read-only reviewer before the final commit.

**Goal:** 实现用户确认的第 1、3、4 项，保留暂缓的第 2 项。

**Architecture:** 扩展现有纯算法预算接口，应用层提供新鲜需求与输入目标反馈。固定历史只用于缓冲结算不确定度，输入与负载使用各自斜率。

**Tech Stack:** STM32F407、C11、FreeRTOS、GCC ARM、主机 GCC/Make。

## Global Constraints

- 不修改 F334 固件或电容容量相关代码。
- 保留所有用户原有未提交改动，只提交本任务改动。
- 下降立即生效，失效时取消增补，安全系数只转换一次。
- 无烧录操作；验证后主动创建本地提交。

## Task 1: 需求门槛与两侧限速

Files: `modules/algorithm/chassis_power_budget.h/.c`、`modules/algorithm/chassis_energy_config.h`、`tests/chassis_buffer_budget/test_chassis_buffer_budget.c`。

- [x] 添加行为测试，确保低负载预算不超过基础、高模型需求能获得预算、输入目标未确认时不提前增加输入贡献。
- [x] 用 `make -B -C tests/chassis_buffer_budget test` 观察预期失败。
- [x] 增加 `demand_valid/requested_load_power_w/confirmed_input_target_w`，按需求缺口封顶缓冲增补；输入以缓冲斜率上升，电容负载以独立斜率上升。
- [x] 运行动态缓冲与既有功率预算测试并修复回归。

关键断言：

```c
input.demand_valid = true;
input.requested_load_power_w = 30;
CHECK(out.input_target_w <= 78.4f + 0.01f);
input.requested_load_power_w = 180;
CHECK(out.input_target_w > 78.4f);
```

## Task 2: 功率历史与缓冲校正

Files: 同一预算模块与动态缓冲测试。

- [x] 添加完整低功率历史、新帧尖峰、缺失历史、向上限速和重复帧测试。
- [x] 运行测试，确认固定最坏扣减不能满足历史校正要求。
- [x] 实现 96 段固定历史，按结算窗口计算保守耗能，缺失部分保留最坏扣减，连续预测与新读数融合。
- [x] 运行回绕、失效与恢复测试，确保估算下限不越过独立仿真的真实能量。

关键断言：

```c
CHECK(out.buffer_lower_j <= actual_buffer_j + 0.01f);
CHECK(corrected_with_history > corrected_without_history);
```

## Task 3: 应用接入、文档与验收

Files: `application/chassis/chassis.c`、`docs/supercap-can-protocol.md`、`docs/configuration-guide.md`、本设计和计划。

- [x] 从统一电机快照验证 5 ms 电机/控制数据与 20 ms 功率计数据的新鲜性，只在驱动且非制动时提供需求。
- [x] 接入 0x052 的实际输入目标；保持第 2 项与用户调参不变。
- [x] 更新联合仿真，覆盖低负载回充与高负载恢复。
- [x] 运行相关主机测试，以及直供/电容/关闭缓冲的固件构建。
- [x] 请求只读代码审查，处理重要问题，再仅提交本次变更并报告编号。
