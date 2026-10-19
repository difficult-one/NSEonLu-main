# 四轮 X 型全向轮底盘 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为现有 STM32F407 机器人固件增加可通过编译宏选择的四轮 X 型全向轮运动学，同时保持默认麦克纳姆底盘行为不变。

**Architecture:** 新建无硬件依赖的纯 C 运动学模块，分别承载现有麦轮公式和 X 型全向轮 45° 投影公式。`chassis.c` 只负责准备输入、按编译配置选择函数并把结果交给既有电机和功率控制链路。

**Tech Stack:** C11、GNU Make、arm-none-eabi-gcc、主机 GCC 单元测试

## Global Constraints

- 默认 `CHASSIS_DRIVE_TYPE` 必须为 `CHASSIS_DRIVE_MECANUM`。
- 电机位置和 CAN ID 保持左前 1、右前 2、左后 4、右后 3。
- 不改变急停、电机离线、DJI 速度环和底盘功率控制行为。
- 不覆盖或提交用户对 `CHASSIS_POWER_BENCH_LIMIT_W` 的未提交修改。

---

### Task 1: 可测试的底盘运动学模块

**Files:**
- Create: `application/chassis/chassis_kinematics.h`
- Create: `application/chassis/chassis_kinematics.c`
- Create: `tests/chassis_kinematics/test_chassis_kinematics.c`
- Create: `tests/chassis_kinematics/Makefile`

**Interfaces:**
- Consumes: `vx`、`vy`、`wz`，四个旋转力臂和四个轮向系数。
- Produces: `bool ChassisMecanumCalculate(const ChassisKinematicsInput_s *, ChassisWheelOutput_s *)` 和 `bool ChassisOmniXCalculate(const ChassisKinematicsInput_s *, const float wheel_direction[CHASSIS_WHEEL_COUNT], ChassisWheelOutput_s *)`。

- [ ] **Step 1: 写失败测试**

用手工推导的字面量断言零输入、前进、右移、旋转、组合输入和轮向系数，并断言空参数返回 `false`：

```c
const ChassisKinematicsInput_s input = {
    .vx = 2.0f,
    .vy = 4.0f,
    .wz = 3.0f,
    .rotation_lever_sum = {1.0f, 2.0f, 3.0f, 4.0f},
};
const float direction[CHASSIS_WHEEL_COUNT] = {1.0f, -1.0f, 1.0f, -1.0f};
ChassisWheelOutput_s output;
ASSERT_TRUE(ChassisOmniXCalculate(&input, direction, &output));
ASSERT_NEAR(-6.363961f, output.rpm[CHASSIS_WHEEL_LEFT_FRONT], 0.0001f);
ASSERT_NEAR( 2.828427f, output.rpm[CHASSIS_WHEEL_RIGHT_FRONT], 0.0001f);
ASSERT_NEAR(-7.778175f, output.rpm[CHASSIS_WHEEL_LEFT_BACK], 0.0001f);
ASSERT_NEAR( 4.242641f, output.rpm[CHASSIS_WHEEL_RIGHT_BACK], 0.0001f);
```

- [ ] **Step 2: 运行测试并确认因模块缺失而失败**

Run: `make -C tests/chassis_kinematics test`
Expected: FAIL，提示 `chassis_kinematics.h` 或实现不存在。

- [ ] **Step 3: 写最小实现**

```c
#define OMNI_X_PROJECTION 0.70710678118654752440f

output->rpm[CHASSIS_WHEEL_LEFT_FRONT] =
    (-input->vx - input->vy -
     input->wz * input->rotation_lever_sum[CHASSIS_WHEEL_LEFT_FRONT]) *
    OMNI_X_PROJECTION * wheel_direction[CHASSIS_WHEEL_LEFT_FRONT];
```

对其余三轮使用设计文档中的对应公式；麦轮函数原样搬移 `chassis.c` 的既有公式。所有指针必须非空，否则返回 `false`。

- [ ] **Step 4: 运行测试并确认通过**

Run: `make -C tests/chassis_kinematics clean test`
Expected: `PASS: chassis kinematics tests`

- [ ] **Step 5: 检查改动**

Run: `git diff --check`
Expected: 无输出，退出码 0。

### Task 2: 编译期选择与底盘集成

**Files:**
- Modify: `application/robot_def.h`
- Modify: `application/chassis/chassis.c`
- Modify: `Makefile`

**Interfaces:**
- Consumes: Task 1 的两个运动学函数和 `ChassisWheelOutput_s`。
- Produces: `CHASSIS_DRIVE_TYPE` 编译配置，以及继续写入现有 `vt_lf/vt_rf/vt_lb/vt_rb` 的底盘控制流。

- [ ] **Step 1: 在配置头中加入受检查的底盘类型和轮向宏**

```c
#define CHASSIS_DRIVE_MECANUM 0
#define CHASSIS_DRIVE_OMNI_X 1
#ifndef CHASSIS_DRIVE_TYPE
#define CHASSIS_DRIVE_TYPE CHASSIS_DRIVE_MECANUM
#endif
#if CHASSIS_DRIVE_TYPE != CHASSIS_DRIVE_MECANUM && \
    CHASSIS_DRIVE_TYPE != CHASSIS_DRIVE_OMNI_X
#error CHASSIS_DRIVE_TYPE must be CHASSIS_DRIVE_MECANUM or CHASSIS_DRIVE_OMNI_X.
#endif
```

再定义 `OMNI_X_LF_DIRECTION`、`OMNI_X_RF_DIRECTION`、`OMNI_X_LB_DIRECTION`、`OMNI_X_RB_DIRECTION`，默认均为 `1.0f`。

- [ ] **Step 2: 将 `chassis.c` 接入纯运动学模块**

创建常量输入中的四个既有旋转力臂；按 `CHASSIS_DRIVE_TYPE` 调用对应函数，将数组结果复制到四个现有目标变量。若函数异常返回 `false`，四个目标保持为零。

- [ ] **Step 3: 把新源文件加入 GNU Make 固件源列表**

在 `application/chassis/chassis.c` 相邻位置加入：

```make
application/chassis/chassis_kinematics.c \
```

- [ ] **Step 4: 编译并运行运动学测试**

Run: `make -C tests/chassis_kinematics clean test`
Expected: `PASS: chassis kinematics tests`

- [ ] **Step 5: 分别编译默认麦轮和 X 型全向轮固件**

Run: `make -j4`
Expected: 固件构建成功。

Run: `make clean` 后用编译参数覆盖 `CHASSIS_DRIVE_TYPE=CHASSIS_DRIVE_OMNI_X` 再执行 `make -j4`
Expected: X 型配置构建成功。

### Task 3: 文档与全量回归

**Files:**
- Modify: `application/chassis/chassis.md`

**Interfaces:**
- Consumes: Task 2 的配置宏和轮向宏。
- Produces: 可直接用于现场选择、架空调试和方向校正的说明。

- [ ] **Step 1: 更新底盘文档**

写明默认麦轮、选择 `CHASSIS_DRIVE_OMNI_X` 的位置、四个 CAN ID、右手坐标系、四个方向宏和架空调试顺序。

- [ ] **Step 2: 运行全部主机测试**

Run: 对每个 `tests/*/Makefile` 执行 `make clean test`
Expected: 所有测试输出 PASS 且退出码为 0。

- [ ] **Step 3: 最终静态检查**

Run: `git diff --check`
Expected: 无输出，退出码 0。

- [ ] **Step 4: 审查变更边界**

Run: `git status --short` 和 `git diff --stat`
Expected: 用户的 40 W 配置仍在工作区且未被覆盖；功能变更只涉及设计、计划、运动学、底盘集成、构建和文档文件。
