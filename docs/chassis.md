# chassis


## 底盘类型选择

底盘通过 `application/robot_def.h` 中的 `CHASSIS_DRIVE_TYPE` 在编译期选择：

```c
// 默认配置，保持原有麦克纳姆轮行为
#define CHASSIS_DRIVE_TYPE CHASSIS_DRIVE_MECANUM

// 四轮 X 型全向轮
#define CHASSIS_DRIVE_TYPE CHASSIS_DRIVE_OMNI_X
```

四轮位置和 CAN ID 沿用现有接线：左前 1、右前 2、左后 4、右后 3。底盘坐标系以向前为 `+x`、向右为 `+y`；X 型全向轮按 45° 安装，运动学对平移量和轴距/轮距分量之和做 `1/sqrt(2)` 投影。默认 350 mm × 300 mm 底盘的旋转有效力臂约为 229.81 mm。

首次调试必须架空车轮，并依次给出纯前进、纯右移和原地旋转命令。如果只有某个车轮方向相反，将对应的 `OMNI_X_LF_DIRECTION`、`OMNI_X_RF_DIRECTION`、`OMNI_X_LB_DIRECTION` 或 `OMNI_X_RB_DIRECTION` 从 `1.0f` 改为 `-1.0f`，不要修改运动学公式。

## 自转功率余量调节

`modules/algorithm/chassis_spin_config.h` 中的宏用于对比：

```c
#define CHASSIS_SPIN_ADAPTIVE_ENABLE 0 // 默认：固定 wz=4000
// 改为 1：在跟上目标且仍有功率余量时逐步提高自转目标
```

也可以用 Make 参数覆盖；两组必须使用不同构建目录，避免复用不同宏的对象文件：

```powershell
New-Item -ItemType Directory -Force build/spin-fixed, build/spin-adaptive | Out-Null
make -j8 BUILD_DIR=build/spin-fixed CHASSIS_SPIN_ADAPTIVE_ENABLE=0
make -j8 BUILD_DIR=build/spin-adaptive CHASSIS_SPIN_ADAPTIVE_ENABLE=1
```

CMake 支持 `-DCHASSIS_SPIN_ADAPTIVE_ENABLE=0` 或 `1`。功率计关闭或反馈失效时，开启宏也会回到固定目标；有效反馈恢复后重新等待稳定。

目标功率沿用当前预算的 95%，包含已允许的超级电容增补。功率低于目标且超出 1 W 死区时提高目标，功率高于目标时降低目标；保留模型限流与速度 PID。自适应自转未受基础模型限流且功率低于目标时，暂停旧外环的正向学习并保留已学修正；超功率时仍可向下纠偏。正向速度调节要求四轮速度误差均不超过 500 rpm、没有基础功率限流或速度 PID 电流饱和，并且连续稳定 200 ms；启动、恢复、制动时暂停增加。阶段 2/3 的预测保护正在减流、增长兜底、无解或搜索耗尽时也暂停增加。

初始目标为 4000，范围为 `CHASSIS_SPIN_MIN_WZ=1000` 至 `CHASSIS_SPIN_MAX_WZ=6000`，每秒最多增加 400、降低 2000。下限必须大于零，并足够让停稳后的目标经运动学和 PID 产生驱动需求；设置成很小的正数仍可能落入瞬态 IDLE 而无法自行恢复，初次对比保留 1000。增益 `CHASSIS_SPIN_POWER_GAIN=40` 将死区外功率误差转换为目标变化率，功率低通为 100 ms。这些值使用原工程的 `wz` 命令单位，不是电机 rpm；上下限和增益是待上板验证的实验参数。预算下降超过 1 W 时立即移除高于 4000 的目标增量。退出自转会重置，重新进入从 4000 开始。

开启宏后，用相同负载、预算、电容状态、PID 和功率控制阶段比较四轮实测转速、总功率和稳定性。调试器可查看 `chassis_spin_state.target_wz` 和 `filtered_power_w`；现有遥测中的四轮目标/实际转速也可用于比较。目标动态变化时不要只比较固定阶跃的 t90。

平移命令保持原值，平移消耗增加会通过总功率误差降低自转目标；此功能没有单独实现平移优先功率分配。达到电机能力、速度上限或稳定性限制时允许剩余功率未用满。

## 正弦自转

显式选择 `CHASSIS_SPIN_MODE=2` 可启用同方向正弦自转；`0` 为固定，`1` 为功率自适应。模式宏优先于旧开关。正弦参数集中于 `modules/algorithm/chassis_spin_config.h`：

| 参数 | 默认值 | 含义 |
| --- | --- | --- |
| `CHASSIS_SPIN_SINE_CENTER_WZ` | 800 | 正弦中心，独立于固定/自适应模式的 4000 |
| `CHASSIS_SPIN_SINE_MIN_WZ` | 200 | 起步及反馈失效时的同方向低速目标 |
| `CHASSIS_SPIN_SINE_AMPLITUDE_WZ` | 200 | 稳态振幅 |
| `CHASSIS_SPIN_SINE_PERIOD_S` | 4 | 周期，秒 |
| `CHASSIS_SPIN_SINE_RISE_RATE` | 400 | 正向增长速率上限，命令单位/秒 |

正常稳态指令为 `800 + 200 * sin(2*pi*t/4)`，方向由 `CHASSIS_SPIN_SINE_DIRECTION` 决定并在一次自转中锁定。以上 `wz` 均是现有工程的命令单位。默认麦轮几何且没有平移时，轮速目标幅值约为 3403..5672 rpm。进入自转后先等待保护和跟随条件稳定 200 ms，从 200 起步升到中心，再用 1 s 渐入振幅。

原先沿用 4000 的中心，对应约 22689 rpm；现有功率模型在该转速、零电流时就预测四轮总功率约 153 W。在 50 W 预算下，跟随/电流保护会阻止中心到位，振幅一直为零。正弦现在单独使用可达的速度参数，保留原来的跟随、制动、功率、反馈和电流保护。受限时波形仍可能缩小或暂停，实车负载需要上板确认。

缓冲增补按电机需求变化，其正常撤销不会再按新旧预算比例缩放正弦中心、振幅或当前转速，也不单独重置稳定确认。实测功率超出当前预算时仍按已有速率降低中心并收缩振幅；限流或能量恢复时收缩振幅并暂停升速。目标始终受正常升降斜率约束，最终功率、电流硬保护继续直接限制电机输出。`CHASSIS_SPIN_BUDGET_DROP_RESET_W` 仅保留给功率自适应模式。

使用独立目录构建，避免复用其他模式的对象文件：

```powershell
New-Item -ItemType Directory -Force build/sine-fix-usb | Out-Null
make -j4 BUILD_DIR=build/sine-fix-usb CHASSIS_SPIN_MODE=2 USB_TELEMETRY_ENABLE=1 UART1_USE=0 CHASSIS_POWER_CONTROL_STAGE=3
```

上板后在 USB 仪表盘同时查看四轮**目标和实际 rpm**，等待起步完成，观察至少三个 4 s 周期。先检查目标是否产生正弦，再比较实际转速的跟随程度；如目标有波动而实际没有，结合功率计、输出电流及反馈在线状态判断限流或跟随能力。主机测试包含基础功率模型、最终预测保护和独立惯性负载；它不等同于实机验证。

## 工作流程

首先进行初始化，`ChasissInit()`会被`RobotInit()`调用，进行裁判系统、底盘电机的初始化。如果为双板模式，则还会初始化IMU，并且将消息订阅者和发布者的初始化改为`CANComm`的初始化。

操作系统启动后，工作顺序为：

1. 从cmd模块获取数据（如果双板则从CANComm获取）
2. 判断当前控制数据的模式，如果为停止则停止所有电机
3. 根据控制数据，计算底盘的旋转速度
4. 根据控制数据中yaw电机的编码器值`angle_offset`，将控制数据映射到底盘坐标系下
5. 根据编译期选择进行麦克纳姆轮或四轮 X 型全向轮运动学解算，得到每个电机的设定值
6. 获取裁判系统的数据，并根据底盘功率限制对输出进行限幅
7. 由电机的反馈数据和IMU（如果有），计算底盘当前的真实运动速度
8. 设置底盘反馈数据，包括运动速度和裁判系统数据
9. 将反馈数据推送到消息中心（如果双板则通过CANComm发送）


### 后续支持平衡底盘

新增一个app balance_chassis
