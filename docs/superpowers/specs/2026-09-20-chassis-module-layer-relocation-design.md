# 底盘辅助代码 Module 层迁移设计

## 目标

清理 `application/chassis` 的职责边界，使 Application 层只保留底盘任务编排入口：

- `application/chassis/chassis.c`
- `application/chassis/chassis.h`

运动学、功率链路转换和功率反馈汇总全部迁移到 `modules` 层。迁移保持现有函数计算结果、CAN 协议、底盘控制周期和消息数据格式不变。

## 文件归属

### 运动学算法

移动：

- `application/chassis/chassis_kinematics.c` → `modules/algorithm/chassis_kinematics.c`
- `application/chassis/chassis_kinematics.h` → `modules/algorithm/chassis_kinematics.h`

该模块只依赖 C 标准类型，继续提供麦克纳姆轮和四轮 X 型全向轮运动学计算，不依赖 Application、HAL 或 FreeRTOS。

### 裁判系统到超级电容的功率链路

移动：

- `application/chassis/chassis_power_link.c` → `modules/super_cap/chassis_power_link.c`
- `application/chassis/chassis_power_link.h` → `modules/super_cap/chassis_power_link.h`

该模块把 `RefereePowerSnapshot_s` 和台架配置转换为 `SuperCapCommand_s` 及基础功率上限，因此归入超级电容模块目录。它只依赖 `referee_power.h`、`super_cap_protocol.h` 和 C 标准库，不依赖 Application。

### 功率反馈汇总

删除独立文件：

- `application/chassis/chassis_power_feedback.c`
- `application/chassis/chassis_power_feedback.h`

将其数据类型和行为合并到：

- `modules/algorithm/power_model.h`
- `modules/algorithm/power_model.c`

`power_model.h` 定义应用消息和功率模型共同使用的反馈数据类型：

```c
typedef struct
{
    float predicted_unlimited_power_w;
    float predicted_limited_power_w;
    float effective_limit_w;
} Chassis_Power_Control_Data_s;
```

`power_model.c` 文件顶部保存模块内单例指针：

```c
static Chassis_Power_Control_Data_s *chassis_power_feedback;
```

公开接口为：

```c
bool ChassisPowerFeedbackInit(void);
void ChassisPowerFeedbackUpdate(bool registered,
                                const ChassisPowerOutput_s *power);
bool ChassisPowerFeedbackGet(Chassis_Power_Control_Data_s *feedback);
```

`ChassisPowerFeedbackInit()` 使用 `malloc()` 创建并清零唯一实例。空指针表示尚未初始化；重复初始化返回 `false`，不覆盖现有实例。

`ChassisPowerFeedbackUpdate()` 不再接收外部存储地址，而是汇总四轮限流前功率、限流后功率和有效功率上限，写入模块内部实例。未初始化时无操作；功控未注册、模型状态非法、输入为空或汇总结果非有限时清零内部反馈。

`ChassisPowerFeedbackGet()` 先清零非空的调用方输出，再把内部反馈复制到调用方；未初始化或输出为空时返回 `false`。这样即使初始化失败，应用发布的数据也保持为零。

更新与读取均由 `ChassisTask()` 顺序调用，不存在中断写入或跨任务写入，因此不把 FreeRTOS 依赖引入纯算法模块。若以后增加其他写入者，再在接口内部加入同步，不改变调用方。

## 应用消息类型依赖

`Chassis_Power_Control_Data_s` 从 `application/robot_def.h` 移除。`robot_def.h` 包含 `power_model.h`，继续在 `Chassis_Upload_Data_s` 中使用该类型。

依赖方向变为：

```text
application/robot_def.h
        ↓
modules/algorithm/power_model.h
```

本次迁入 `modules/algorithm` 和 `modules/super_cap` 的底盘辅助代码不包含或依赖
`robot_def.h`，避免新增底层模块反向依赖 Application。仓库中既有的裁判 UI 和视觉配置
依赖不属于本次底盘迁移范围。

## Application 调用流程

`ChassisInit()` 在 DJI 底盘功控组注册后调用：

```c
chassis_power_feedback_ready = ChassisPowerFeedbackInit();
```

`ChassisTask()` 获取 `DJIChassisPowerState_s` 后按顺序调用：

```c
ChassisPowerFeedbackUpdate(
    power_state_valid && power_state.registered,
    power_state_valid ? &power_state.power : NULL);

(void)ChassisPowerFeedbackGet(
    &chassis_feedback_data.power_control);
```

初始化失败时，getter 会把 `chassis_feedback_data.power_control` 清零，不影响电机控制、功率预算或急停路径。反馈模块只负责诊断数据，不参与控制决策。

## 文档与空文件

- `application/chassis/chassis.md` 移动为 `docs/chassis.md`；
- 删除未被引用且内容为空的 `application/chassis/balance.h`；
- 删除未被引用且内容为空的 `application/chassis/mecanum.h`；
- 删除未被引用且内容为空的 `application/chassis/steering.h`。

完成后 `application/chassis` 目录只保留 `chassis.c` 和 `chassis.h`。

## 构建调整

根 `Makefile`：

- 删除 `application/chassis/chassis_kinematics.c`；
- 删除 `application/chassis/chassis_power_link.c`；
- 删除 `application/chassis/chassis_power_feedback.c`；
- 增加 `modules/algorithm/chassis_kinematics.c`；
- 增加 `modules/super_cap/chassis_power_link.c`；
- 继续使用已有的 `-Imodules/algorithm` 和 `-Imodules/super_cap`，不新增 include 目录。

CMake 已递归收集 `modules/*.c` 和 `application/*.c`，文件移动后自动使用新位置，无需修改 `CMakeLists.txt`。

相关主机测试 Makefile 更新源文件和 include 路径：

- `tests/chassis_kinematics`；
- `tests/chassis_power_link`；
- `tests/referee_integration`；
- `tests/power_model`。

原 `tests/chassis_power_feedback` 中的行为测试合并到 `tests/power_model/test_power_model.c`，随后删除独立测试目录，避免同一行为维护两套测试入口。

## TDD 与验证

实施顺序：

1. 先修改测试，使其引用新的 Module 路径和新的功率反馈单例接口；
2. 确认测试因新接口和新文件尚不存在而失败；
3. 移动运动学和功率链路文件并恢复相关测试；
4. 在 `power_model` 中实现反馈实例、更新和读取接口；
5. 删除 Application 层旧文件和空文件；
6. 更新 `chassis.c`、`robot_def.h` 和构建文件；
7. 运行全部相关主机测试；
8. 分别构建 `POWER_METER_ENABLE=1` 和 `POWER_METER_ENABLE=0` 的 ARM 固件。

测试至少覆盖：

- 反馈模块未初始化时 update 安全无操作、get 返回失败；
- 首次初始化成功、重复初始化失败；
- 有效模型结果正确汇总四轮功率；
- 未注册、空输入、非法状态和非有限结果清零反馈；
- getter 空输出安全失败；
- 麦克纳姆轮和全向轮现有测试无回归；
- 功率链路和裁判系统集成测试无回归；
- `application/chassis` 最终只剩两个核心文件；
- 两种功率计编译配置均通过 ARM 全量构建。

## 非目标

- 不修改底盘运动学公式；
- 不修改功率模型、预算或外环算法；
- 不修改超级电容和裁判系统协议；
- 不重命名现有运动学或功率链路公开函数；
- 不新增 FreeRTOS 任务；
- 不实现平衡底盘、舵轮底盘或新的运动模型；
- 不整理本次迁移范围外的代码。

## 完成标准

- `application/chassis` 只保留 `chassis.c/.h`；
- 运动学代码位于 `modules/algorithm`；
- 功率链路代码位于 `modules/super_cap`；
- 功率反馈类型、实例指针和接口位于 `power_model.h/.c`；
- 本次迁入 Module 层的底盘辅助代码不依赖 `robot_def.h`；
- 所有相关测试和两种固件配置构建通过；
- 用户现有未提交修改不被覆盖或误提交。
