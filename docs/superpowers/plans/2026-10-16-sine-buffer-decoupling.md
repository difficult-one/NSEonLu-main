# 正弦自旋与缓冲预算解耦实施计划

**Goal:** 消除需求预算回落引起的正弦目标跳变，保留现有功率和电流保护，验证并提交。

**Architecture:** 正弦相位和转速包络不再按相邻需求预算比例缩放；实际超功率、限流和能量恢复继续通过原有门槛和斜率约束转速。沿用全部输出保护和现有接口。

**Tech Stack:** C11、主机 GCC、GNU Make、STM32F407 ARM GCC。

## 约束

用户已批准方案并明确要求实施，无需再次确认设计。当前八个文件的未提交调参不纳入本次提交。不改协议、在线调参字段或 Flash 布局，不刷写硬件。

## 任务 1：复现与最小修复

文件：`modules/algorithm/chassis_spin.c`、`chassis_spin.h`、`tests/chassis_spin/test_chassis_spin_sine.c`、`test_chassis_spin_buffer.c`、`Makefile`。

- [x] 添加真实预算与正弦组合测试：80 W 裁判额度、60 J 缓冲、20 W 实测，在高低需求间切换，正负方向对比固定预算基准，检查 600..1000 波形及每步变化。
- [x] 将旧的比例骤降预期改为真实超功率下按斜率收缩；分别构建和运行新增预算组合测试与更新的正弦测试，确认原代码出现波形及斜率断言失败。
- [x] 删除正弦分支的相邻预算比例缩放和相关历史依赖，保留自适应分支和全部保护输入。重新运行同一套测试。

## 任务 2：保护回归、文档和提交

文件：`tests/chassis_spin/test_chassis_spin_sine_power.c`、正弦设计文档、`docs/chassis.md`、配置指南、`tools/usb-telemetry/tuning.js` 及本计划。网页只调整预算复位参数的模式可见性，不改变参数字段布局。

- [x] 将动态预算接入实际模型和最终保护组合测试，断言输出功率仍遵守最终保护允许值、受限时不升速，并记录预算变化与轮速周期情况。
- [x] 运行自旋、动态缓冲、旧预算、功率外环、瞬态、持续过功率、DJI 最终保护相关主机测试。
- [x] 独立目录构建正弦直供、正弦电容、关闭缓冲、固定和自适应固件，保存构建结果。
- [x] 更新正弦行为文档和配置说明，独立代码审查无阻塞问题。暂存后仅检查本任务差异，避免混入原有调参中的空白问题。
- [x] 精确暂存本任务文件，暂存差异检查通过；用户原有八个调参文件仍保持未提交。以本计划随修复一并创建本地提交，交付时报告编号与实机待验证边界。

## 验证结果

- 旧代码在新增健康预算测试中最大单步变化为 379.36、与固定预算基准的最大差为 771.98，正负方向均失败；恢复和超功率斜率断言也失败。修复后四种正常预算回落场景差值为 0，最大 5 ms 变化为 1.57，完整波形 600..1000。
- `make -C tests/chassis_spin -B test`：7 个测试可执行程序全部通过，含固定、自适应、显式模式覆盖、正弦状态机、真实预算组合与最终保护仿真。
- 最终保护仿真：固定 50 W 在额度下降并恢复后，连续三个周期实际轮速 3384..5647 rpm、周期平均差小于 0.1 rpm；动态 40 W 基础额度（中间降至 25 W）下为 3384..4914 rpm、周期平均差约 0.1 rpm。后者波峰允许受真实保护压低；检查了动态增补和回落、目标斜率、受限不升速、电流不超过 15000、预测功率不超过允许值及源硬上限；无解或搜索耗尽时驱动电流清零。
- `chassis_buffer_budget`、`chassis_power_budget`、`chassis_power_outer_loop`、`chassis_power_transient`、`chassis_power_overpower` 及 stage 3 的 `dji_chassis_power_rtt` 主机回归均通过。
- `node --test tools/usb-telemetry/tests/*.test.cjs`：24 项通过，参数协议字段仍为 16 项。正弦模式隐藏已不使用的预算跌落复位阈值，功率自适应仍显示。
- ARM 构建公共参数：`UART1_USE=0 USB_TELEMETRY_ENABLE=1 CHASSIS_POWER_CONTROL_STAGE=3`，均 `make -j4`。配置为：

| 构建目录 | 自旋模式 | 电源模式 | 缓冲启用 |
| --- | --- | --- | --- |
| `build/sine-buffer-direct` | 2 | 0 | 1 |
| `build/sine-buffer-supercap` | 2 | 1 | 1 |
| `build/sine-buffer-disabled` | 2 | 0 | 0 |
| `build/sine-buffer-fixed` | 0 | 0 | 1 |
| `build/sine-buffer-adaptive` | 1 | 0 | 1 |

五种配置编译、链接及 BIN/HEX 生成均通过；保留既有传感器/机器人配置提示及 libnosys 系统调用链接警告。各目录的 `build.log` 保存完整输出。直供正弦 BIN 为 `build/sine-buffer-direct/basic_framework.bin`，130192 字节，使用当前工作区硬件和调参配置。不自动刷写，实机仍需同时观察 Wz、四轮实际转速、功率和缓冲能量至少三个周期。

直供正弦 BIN 的 SHA-256：`C81359B3D8329CC8AD25DFCB75737E5923D77E80658DF48AD78CC3028B3AA34D`。
