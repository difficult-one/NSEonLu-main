# 同向正弦自转实施计划

**目标：** 新增正弦自转模式，保持方向、现有两种模式及功率保护。

**架构：** 统一模式宏控制纯算法和应用接入；正弦使用独立相位、中心和振幅状态，运动目标受到已有预算与保护允许条件约束。

**技术：** C11、主机 GCC、GNU Make/CMake、STM32F407 ARM GCC。

## 任务 1：纯算法和模式选择

文件：`modules/algorithm/chassis_spin.[ch]`、`chassis_spin_config.h`、`tests/chassis_spin/test_chassis_spin_sine.c`、`tests/chassis_spin/Makefile`。

- [x] 写完整周期、方向与保护失败测试。完整渐入后每周期正向目标最小约 3000、最大约 5000，负向目标全程为负；单次正常变化不超过 `2000 * dt`。
- [x] 增加模式宏/API 字段，运行正弦测试，确认旧固定/自适应行为不能通过波形断言。
- [x] 加入正弦状态更新：`phase=fmod(phase+dt,period)`；目标为 `center + amplitude*(center/fixed)*sin(2*pi*phase/period)`，正常升降限速，预算下降立即收紧。
- [x] 反馈/保护允许时先恢复中心到基准，再渐入振幅；受限时收缩振幅，过功率时降低中心，方向从进入到退出保持锁定。
- [x] 保留并运行固定、自适应测试和新增正弦测试。

## 任务 2：应用接入、构建与提交

文件：`application/chassis/chassis.c`、`Makefile`、`CMakeLists.txt`、本设计和计划。

- [x] 动态模式共有数据路径改为 `CHASSIS_SPIN_DYNAMIC_ENABLED`；追加正弦配置和 `power_limited`、`energy_recovering` 输入。旧自适应的功率学习隔离仍仅用于模式 1。
- [x] Make/CMake 透传 `CHASSIS_SPIN_MODE`，工作区默认仍兼容用户现有开关。
- [x] 运行主机自转及关联功率回归测试，构建配置：

```powershell
make -C tests/chassis_spin -B test
make -C tests/chassis_buffer_budget -B test
make -C tests/chassis_power_outer_loop -B test
make -C tests/chassis_power_transient -B test
make -C tests/chassis_power_overpower -B test
make -C tests/dji_chassis_power_rtt -B test
make -j4 BUILD_DIR=build/spin-fixed CHASSIS_SPIN_MODE=0
make -j4 BUILD_DIR=build/spin-adaptive CHASSIS_SPIN_MODE=1
make -j4 BUILD_DIR=build/spin-sine CHASSIS_SPIN_MODE=2
make -j4 BUILD_DIR=build/spin-sine-supercap CHASSIS_SPIN_MODE=2 CHASSIS_POWER_SOURCE_MODE=1
```

- [x] 独立代码审查并修正发现；记录构建/测试及实机待验收边界。
- [x] 补充真实瞬态与正弦联动测试，先复现低需求启动/波谷恢复停滞，再为健康无驱动需求 RUN/IDLE 增加稳定资格，保留既有反馈、制动、电流和功率保护。
- [x] 用任务前持久快照生成底盘和自转配置的本任务补丁，原有 PID、安全系数和自适应开启改动留在工作区；提交本次涉及文件。

建议提交信息：`feat: add same-direction sine chassis spin`。
