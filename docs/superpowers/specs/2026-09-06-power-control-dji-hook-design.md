# DJI 电机统一驱动下的底盘功率控制替换设计

## 1. 目标

将 `PowerManager-main` 中的新功率预测、功率分配和单电机限流算法移入 `NSEonLu-main`，完整删除框架现有的旧功率模型和独立底盘电机驱动副本。

迁移完成后：

- 所有 DJI 电机只通过 `DJIMotorInit()` 注册；
- 所有 DJI 电机只由 `DJIMotorControl()` 计算和发送；
- 四个底盘 M3508 在 PID 输出后统一经过新功率管理；
- 云台、拨弹等非底盘电机不参与底盘功率分配；
- 不保留旧算法、旧算法开关或运行时回退路径；
- Git 历史作为旧实现的唯一回退手段。

## 2. 当前问题

框架当前存在两套 DJI 电机控制路径：

1. `modules/motor/DJImotor/dji_motor.c` 管理普通 DJI 电机；
2. `modules/motor/power_control.c` 复制了电机注册、反馈解析、PID、CAN 分组和发送逻辑，单独管理四个底盘电机。

旧功率控制采用硬编码的机械功率模型和整车同比缩放。两套控制路径会造成重复维护、速度单位不一致以及误注册后重复发送 CAN 控制帧的风险。

此外，`application/chassis/chassis.c` 中左前轮参考值缺少 `DJIMotorSetRef()` 调用；超级电容模块已经解析电压、电流和功率，但没有定义可直接使用的剩余能量百分比。

## 3. 选定架构

采用“一套 DJI 驱动 + 可选功率管理组”的结构。

```text
ChassisTask
  ├─ 设置四个底盘电机速度目标
  ├─ 设置裁判系统功率上限
  └─ 设置超级电容衰减系数
          │
          ▼
DJIMotorControl
  ├─ 阶段一：计算所有 DJI 电机的原始 PID 输出
  ├─ 阶段二：仅对底盘功控组的四个输出分配功率并限流
  └─ 阶段三：统一执行反转、启停、CAN 打包和发送
```

普通 DJI 电机与底盘电机共用注册、反馈解析、守护线程和 CAN 发送机制。功率管理只处理显式加入同一个底盘功控组的四个电机。

## 4. 模块边界

### 4.1 纯 C 功率算法核心

新增：

- `modules/algorithm/power_model.h`
- `modules/algorithm/power_model.c`

该模块只进行数学计算，不包含 HAL、CAN、FreeRTOS、裁判系统或 `DJIMotorInstance` 依赖。使用 C11、固定长度数组和 `float`，不使用动态内存。

职责包括：

- 六项多项式电机功率预测；
- 四电机按速度误差分配功率；
- 单电机目标电流限幅；
- 参数与输入合法性检查；
- 生成每轮预测功率、功率额度和衰减系数。

### 4.2 DJI 驱动挂接层

修改：

- `modules/motor/DJImotor/dji_motor.h`
- `modules/motor/DJImotor/dji_motor.c`

DJI 驱动增加底盘功控组注册接口，但不在通用电机初始化配置中强制启用功率控制。只有底盘应用显式注册的四个电机进入功控组。

`DJIMotorControl()` 改为三阶段处理：

1. 遍历全部 DJI 电机并计算原始 PID 输出，暂存为 `float`；
2. 调用功率管理组处理四个底盘电机的暂存输出；
3. 将处理后的输出限幅、转换为 `int16_t`、写入 CAN 缓冲区并统一发送。

### 4.3 底盘应用

修改：

- `application/chassis/chassis.c`

底盘四电机改用 `DJIMotorInit()` 注册。初始化完成后，把四个电机和各自的 M3508 功率模型参数注册为一个功控组。

底盘任务继续负责：

- 设置四轮速度目标；
- 从裁判系统读取 `chassis_power_limit`；
- 更新功控组总功率上限；
- 后续在协议明确后更新超级电容衰减系数。

底盘任务不直接计算 PID，不直接修改目标电流，也不发送 DJI 电机 CAN 报文。

### 4.4 旧功率控制模块

删除：

- `modules/motor/power_control.c`
- `modules/motor/power_control.h`

同时从 `modules/motor/motor_task.c`、`Makefile` 和所有包含处删除旧接口：

- `PowerControlInit()`；
- `PowerControl()`；
- `SetPowerLimit()`；
- 旧模型系数、机械功率公式和整车同比缩放逻辑。

## 5. 数据结构和接口

### 5.1 电机模型参数

```c
typedef struct
{
    float k0;
    float k1;
    float k2;
    float k3;
    float k4;
    float k5;
    float current_conversion;
} MotorPowerModelConfig_s;
```

首版四个底盘电机使用相同的 M3508 参数：

```text
k0 = 0.65213
k1 = -0.15659
k2 = 0.00041660
k3 = 0.00235415
k4 = 0.20022
k5 = 1.08e-7
current_conversion = 1000
```

参数通过配置对象传入，不写死在算法函数中，以便实车重新拟合后替换。

### 5.2 功控组配置

```c
typedef struct
{
    DJIMotorInstance *motors[4];
    MotorPowerModelConfig_s models[4];
    float safety_factor;
    float small_error_threshold;
    float reserved_power_threshold;
    float per_motor_reserved_power;
    float max_current_command;
} DJIChassisPowerConfig_s;
```

首版配置为：

```text
safety_factor = 0.98
small_error_threshold = 500 rpm
reserved_power_threshold = 54 W
per_motor_reserved_power = 8 W
max_current_command = 15000
```

### 5.3 驱动层接口

```c
bool DJIChassisPowerRegister(const DJIChassisPowerConfig_s *config);
void DJIChassisPowerSetLimit(float referee_power_limit_w);
void DJIChassisPowerSetAttenuation(float attenuation);
const DJIChassisPowerState_s *DJIChassisPowerGetState(void);
```

接口规则：

- 功控组只能成功注册一次；
- 四个电机指针必须非空且互不重复；
- 四个电机必须已经由 `DJIMotorInit()` 注册；
- `attenuation` 被限制在 `[0, 1]`；
- 未设置衰减系数时使用 `1.0`；
- 无有效功率限制时，功控组输出电流为零；
- 非功控组电机不受这些接口影响。

### 5.4 诊断状态

`DJIChassisPowerState_s` 提供只读调试信息：

- 裁判系统功率上限；
- 衰减系数；
- 安全系数后的有效功率预算；
- 四轮速度误差；
- 四轮原始目标电流；
- 四轮原始预测功率；
- 四轮分配功率；
- 四轮限幅系数；
- 四轮最终目标电流；
- 四轮限幅后预测功率；
- 最近一次错误状态。

诊断状态不参与控制决策，可由 Ozone 或后续遥测读取。

## 6. 控制算法

### 6.1 单电机功率预测

电流先除以 `current_conversion`，随后电流和速度均取绝对值：

```text
P = K0
  + K1 × I
  + K2 × speed
  + K3 × I × speed
  + K4 × I²
  + K5 × speed²
```

速度统一使用 `rpm`，电流输入和输出统一使用 DJI 电调原始命令值。首版不将制动回馈功率计为负功率，避免高估可用功率。

### 6.2 总功率预算

```text
effective_limit
= referee_power_limit
× clamp(supercap_attenuation, 0, 1)
× safety_factor
```

首版超级电容衰减固定为 `1.0`。电容板剩余能量定义明确后，只修改底盘应用的衰减映射，不修改功率算法。

### 6.3 四轮功率分配

先计算四个速度误差绝对值之和：

- 总误差不超过 `500 rpm`：四轮均分有效功率；
- 有效功率低于 `54 W`：按误差比例分配；
- 有效功率不低于 `54 W`：每轮预留 `8 W`，剩余功率按误差比例分配。

分配完成后必须满足：

- 每轮额度非负；
- 四轮额度之和不大于有效功率预算；
- 输入异常时不产生 NaN 或无穷大。

### 6.4 单电机限流

先预测原始目标电流对应的功率。未超过额度时保持原输出；超过额度时求解衰减系数 `k`：

```text
a × k² + b × k + c = 0
```

选择 `[0, 1]` 中最大的有效根，并执行：

```text
limited_current = desired_current × k
```

需要覆盖二次项退化为一次方程、重根、无实根和无有效根的情况。源 C++ 实现中一次方程分支只返回 `k`、没有实际缩放目标电流，迁移时必须修正。

无有效解时将该电机电流清零。最终输出统一限制在 `[-15000, 15000]`。

## 7. 执行时序

功率限制必须作用在速度 PID 已经生成目标电流之后、CAN 打包之前。一次 `DJIMotorControl()` 中的顺序固定为：

1. 读取所有电机当前参考值和反馈；
2. 计算角度环、速度环、电流环和前馈；
3. 保存原始输出，不发送；
4. 收集功控组四个电机的目标速度、反馈速度和原始输出；
5. 计算四轮功率额度；
6. 修改功控组四个原始输出；
7. 应用反馈反向设置；
8. 应用电机启停和电流硬限幅；
9. 统一写入并发送 CAN 报文。

功控计算与 CAN 发送处于同一个电机控制任务中，不新增 FreeRTOS 任务，也不引入跨任务共享的电流缓冲区。

## 8. 异常处理

以下情况只清零功控组的四个电机输出，普通 DJI 电机继续正常运行：

- 功控组未注册完整；
- 四个电机存在空指针或重复指针；
- 功率模型参数非法；
- 裁判功率上限小于等于零或不是有限数；
- 算法结果为 NaN 或无穷大；
- 单电机限流方程没有有效解。

电机自身处于 `MOTOR_STOP` 时，现有启停逻辑优先。发送前还要通过 `DaemonIsOnline()` 检查反馈状态；功控组内的离线电机最终发送电流为零。

## 9. 超级电容边界

首版不根据 `vol/current/power` 猜测电容剩余能量，也不把电容瞬时功率直接当作能量百分比。

首版行为：

- `DJIChassisPowerSetAttenuation(1.0f)`；
- 裁判系统功率限制作为基础预算；
- 保留衰减接口；
- 等电容板协议给出能量百分比或明确的电压—能量映射后，再在 `chassis.c` 中生成 `[0,1]` 衰减系数。

## 10. 构建和文件变更

新增：

- `modules/algorithm/power_model.h`
- `modules/algorithm/power_model.c`
- `tests/power_model/test_power_model.c`
- `tests/power_model/Makefile`

修改：

- `modules/motor/DJImotor/dji_motor.h`
- `modules/motor/DJImotor/dji_motor.c`
- `modules/motor/motor_task.c`
- `application/chassis/chassis.c`
- `modules/super_cap/super_cap.h`
- `Makefile`
- 功率控制说明文档

删除：

- `modules/motor/power_control.h`
- `modules/motor/power_control.c`

CMake 使用递归收集 `.c` 文件，新算法会自动进入 CMake 构建；手写 `Makefile` 需要显式增加 `modules/algorithm/power_model.c` 并删除旧功控源文件。

## 11. 测试策略

### 11.1 主机单元测试

使用 MinGW GCC 编译不依赖硬件的 `power_model.c`，测试：

- M3508 样例功率预测；
- 未超额度时限流系数为 `1`；
- 超额时电流正确缩放；
- 二次方程、一次方程和重根分支；
- 判别式小于零；
- 无有效根；
- 小误差均分；
- 低功率按误差分配；
- 高功率保底分配；
- 零、负数、NaN 和无穷大输入；
- 四轮分配额度之和不超过有效预算。

### 11.2 固件构建验证

- ARM GNU Toolchain 全量编译通过；
- 保持 `-Wall -Werror`；
- 不出现重复符号和重复 CAN 注册；
- 检查 Flash、RAM 和栈使用增量；
- 检查功率算法在目标控制频率下的执行时间。

### 11.3 实车验证

按照风险递增顺序进行：

1. 架空轮子，仅观察原始电流和预测功率；
2. 低功率上限下低速直行；
3. 急加速和急刹车；
4. 横移；
5. 平移叠加小陀螺；
6. 切换不同裁判功率上限；
7. 裁判数据为零或中断；
8. 电机掉线和整车急停。

实车首先使用 `0.90` 至 `0.95` 的安全系数验证模型偏差。预测功率稳定低于裁判系统功率后，再逐步提高到设计值 `0.98`。

## 12. 验收标准

- 仓库中不存在旧 `power_control.c/.h`；
- `MotorControlTask()` 只调用一次 `DJIMotorControl()`；
- 所有 DJI 电机共享同一注册表和 CAN 发送路径；
- 四个底盘电机始终经过新功率管理；
- 非底盘 DJI 电机的输出与改造前保持一致；
- 四轮目标电流始终位于 `[-15000, 15000]`；
- 四轮分配功率之和不超过有效功率预算；
- 异常输入不会产生 NaN、无穷大或失控电流；
- 主机单元测试和 ARM 固件全量构建均通过；
- 实车采样确认模型误差可接受后才使用 `0.98` 安全系数。

## 13. 明确不在本次范围内的内容

- 舵轮八电机功率分配；
- 小陀螺方向前馈补偿；
- 在线参数辨识；
- 根据超级电容电压自行估算剩余能量；
- 为非底盘电机分配裁判系统底盘功率；
- 保留旧功率算法作为运行时备用路径。
