# 功率模块实例指针封装设计

## 目标

把功率计、底盘功率控制模型和超级电容的运行数据完整封装在 `modules` 层。保存运行数据的实例结构体类型定义在对应头文件中，实例指针作为文件级 `static` 变量放在对应实现文件开头，由模块初始化函数负责分配和赋值。

应用层不再保存这些模块的实例指针，也不直接访问实例成员，只通过模块公开接口初始化、发送命令和读取一致的数据快照。

## 采用的实例形式

三个模块均使用模块内单例指针，形式如下：

```c
static PowerMeterInstance *power_meter;
static SuperCapInstance *cap;
static DJIChassisPowerInstance_s *chassis_power;
```

结构体对象继续通过 `malloc()` 创建，初始化成功后由对应模块在整个固件生命周期内持有。当前硬件只有一个功率计、一个超级电容和一组四轮底盘功率控制器，因此不增加实例数组、索引或注销接口。

## 模块边界

### 功率计

`modules/power_meter/power_meter.h` 保留并公开：

- `PowerMeterStatus_s`；
- `PowerMeterInstance`；
- `PowerMeter_Init_Config_s`。

`modules/power_meter/power_meter.c` 文件开头新增：

```c
static PowerMeterInstance *power_meter;
```

初始化函数在模块内部完成内存分配、CAN 注册和 daemon 注册，并把成功创建的实例赋给 `power_meter`。公开接口改为单例接口：

```c
bool PowerMeterInit(const PowerMeter_Init_Config_s *config);
bool PowerMeterGetStatus(PowerMeterStatus_s *status);
```

CAN 回调仍通过 `CANInstance.id` 获取所属实例，避免中断路径依赖可变的全局查找；对外状态读取使用模块内部的 `power_meter`。

### 超级电容

`modules/super_cap/super_cap.h` 保留并公开：

- `SuperCapInstance`；
- `SuperCap_Init_Config_s`；
- 协议头文件提供的 `SuperCapCommand_s` 和 `SuperCapStatus_s`。

`modules/super_cap/super_cap.c` 文件开头新增：

```c
static SuperCapInstance *cap;
```

初始化函数在模块内部完成内存分配、CAN 注册和 daemon 注册，并把成功创建的实例赋给 `cap`。公开接口改为单例接口：

```c
bool SuperCapInit(const SuperCap_Init_Config_s *config);
bool SuperCapSendCommand(const SuperCapCommand_s *command);
bool SuperCapGetStatus(SuperCapStatus_s *status);
```

CAN 回调继续使用 `CANInstance.id` 中的实例地址；发送和快照接口使用模块内部的 `cap`。

### 底盘功率控制模型

当前保存功控组运行数据的私有结构体 `DJIChassisPowerGroup_s` 位于 `dji_motor.c`。该类型移动到 `modules/motor/DJImotor/dji_motor.h`，并更名为职责明确的 `DJIChassisPowerInstance_s`。结构体继续保存：

- 注册状态；
- 四个底盘电机在 DJI 实例表中的索引；
- 四组电机功率模型参数；
- 四轮功率分配参数；
- 当前功率模型输入；
- 对外诊断状态；
- 功率计实测底盘功率。

`modules/motor/DJImotor/dji_motor.c` 文件开头声明：

```c
static DJIChassisPowerInstance_s *chassis_power;
```

`DJIChassisPowerRegister()` 首次成功注册时分配并填充该实例。现有的预算、衰减、实测功率、状态快照和电机控制接口继续保持无实例参数的单例形式。纯数学模块 `modules/algorithm/power_model.c` 继续保持无状态，不引入 HAL、FreeRTOS、电机实例或动态内存依赖。

## 应用层调整

`application/chassis/chassis.c` 删除：

```c
static SuperCapInstance *cap;
static PowerMeterInstance *power_meter;
```

初始化改为只检查模块初始化结果，不接收或保存实例地址。周期任务通过无实例参数接口发送超级电容命令、读取超级电容状态和读取功率计状态。

底盘预算、外环状态、反馈数据以及四个电机实例不属于本次三个模块的内部存储，继续保留在其现有所有者中。

## 生命周期和重复初始化

- 静态实例指针启动时为 `NULL`；
- 传入空配置时初始化失败且指针保持 `NULL`；
- 内存分配失败时初始化失败；
- 依赖注册失败时初始化失败，不发布半初始化实例；
- 同一模块重复初始化时返回失败，不覆盖已注册实例，也不重复注册 CAN 或 daemon；
- 固件运行期间不释放成功注册的单例实例，因为 CAN 和 daemon 均保存该实例地址并在整个系统生命周期中使用。

如果 daemon 已成功注册而后续 CAN 注册失败，框架目前没有 daemon 注销接口。模块不得把该半初始化对象发布给公开接口；失败路径保持安全离线。此次不扩展 daemon 框架的注销能力。

## 并发和快照

- 功率计和超级电容的 CAN 回调在中断上下文更新状态；
- 任务上下文读取状态时继续使用 FreeRTOS 临界区复制完整快照；
- 底盘功率控制状态在电机任务中更新，应用层读取时继续通过临界区复制 `DJIChassisPowerState_s`；
- 本次只改变实例所有权和接口传参，不改变现有在线判断、功率算法、控制周期或 CAN 协议。

## 测试策略

先增加或调整主机测试，使其在旧的带实例参数接口下编译失败，从而验证接口确实改为模块内单例。测试至少覆盖：

- 功率计初始化后可通过无实例参数接口取得状态；
- 超级电容初始化后可通过无实例参数接口发送命令和取得状态；
- 未初始化时 getter 和发送接口安全失败；
- 空配置、空输出和重复初始化安全失败；
- 功率控制组注册后，预算、模型输出和快照行为保持不变；
- 现有协议、功率模型、预算和外环主机测试无回归；
- 完整 ARM 固件在 `POWER_METER_ENABLE=1` 和 `POWER_METER_ENABLE=0` 两种配置下均能构建。

## 非目标

- 不改变功率计或超级电容 CAN 报文；
- 不修改 M3508 功率模型系数和四轮分配算法；
- 不增加多个功率计、多个超级电容或多个底盘功控组；
- 不引入新的 FreeRTOS 任务；
- 不修改底盘预算、实测功率外环或电机 PID 参数；
- 不整理与本次实例封装无关的代码和用户未提交修改。

## 完成标准

- 三类运行数据结构体均定义在对应 `.h` 文件中；
- 三个实例指针均作为 `static` 变量位于对应模块 `.c` 文件开头；
- `chassis.c` 不再保存功率计和超级电容实例指针；
- 应用层不能通过公开调用路径直接修改模块实例；
- 初始化失败和未初始化调用安全返回失败；
- 现有控制行为、快照一致性和 CAN 协议保持不变；
- 相关主机测试和 ARM 构建全部通过。
