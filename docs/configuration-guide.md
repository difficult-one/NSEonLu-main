# 宏与参数设置总表

整理日期：2026-10-13。数值来自整理时的工作区，包含已有的未提交修改；这是源码配置快照，不代表开发板当前烧录的固件。

2026-10-15：第 5 节已同步正弦模式的独立中心、起步速度和可达波形参数。

本文说明工程的功能开关、调参项、硬件与任务设置，以及参数实际生效的条件。[完整宏与初始化参数索引](configuration-index.md)按文件列出全部项目源码宏、条件分支、已注释的宏选项、配置结构体初值和 HAL 外设设置。

范围：`application/`、`bsp/`、`modules/`、`Inc/`、`Src/`，另含 SEGGER RTT 配置、Make/CMake 和链接脚本。`Drivers/`、其余 `Middlewares/` 中的芯片寄存器和第三方库内部宏不作为机器人调参项逐条解释；`tests/` 是测试配置，`portable_modules/` 是独立移植副本，均不参与主工程配置。

## 1. 先看当前配置

| 项目 | 当前源码设置 | 生效说明 |
| --- | --- | --- |
| 开发板分工 | `ONE_BOARD` | 单板；`CHASSIS_BOARD`、`GIMBAL_BOARD` 已注释 |
| 实际运行的应用 | 命令输入、底盘 | `robot.c` 中云台与发射的 Init/Task 调用已注释 |
| USB 用途 | `USB_TELEMETRY_ENABLE=1` | 默认向网页上报遥测；设为 0 恢复 `VISION_USE_VCP` 的视觉 USB 协议 |
| UART1 | 默认定义存在拼接异常 | 详见第 2 节；显式传 `UART1_USE=0/1/2` 可确定选择 |
| 底盘轮型 | `CHASSIS_DRIVE_TYPE=CHASSIS_DRIVE_MECANUM`，即 `0` | 麦克纳姆；X 型全向轮是 `1` |
| 台架功率回退 | `CHASSIS_POWER_BENCH_TEST=1`，`50 W` | 仅裁判功率快照无效时回退，不覆盖有效裁判功率限制 |
| 电源模式 | `CHASSIS_POWER_SOURCE_MODE=0` | 直供，当前不初始化超级电容模块 |
| 裁判缓冲能量控制 | `CHASSIS_BUFFER_POWER_ENABLE=1` | 启用；增补仍需有效裁判缓冲数据和输入功率反馈 |
| 功率计 | `POWER_METER_ENABLE=1` | CAN1，状态 ID `0x605` |
| 功率控制阶段 | `CHASSIS_POWER_CONTROL_STAGE=3` | 快保护、启动预测保护、完整慢环 |
| 小陀螺 | `CHASSIS_SPIN_ADAPTIVE_ENABLE=1`，未显式指定 MODE | 默认自适应模式 `1`；同向正弦代码已存在但默认未选中 |
| 标定 CSV | `CHASSIS_POWER_CALIBRATION_ENABLE=0` | 标准 17 列；设为 1 时追加 9 列 |
| 启动波形抓取 | `CHASSIS_POWER_CAPTURE_ENABLE=0` | 关闭；UART 排出任务另需 UART 日志模式 |
| 普通分级日志 | Make：关闭；CMake：默认开启 | Make 固定传 `-DDISABLE_LOG_SYSTEM`，等价于 1；头文件默认 0 |

## 2. 编译开关和覆盖方法

### 2.1 UART1：裁判、日志、RTT

位置：[board_config.h](../Inc/board_config.h)。

| `UART1_USE` | 常量名 | UART1 裁判接收 | UART1 日志输出 | RTT 输出 |
| --- | --- | --- | --- | --- |
| `0` | `UART1_USE_REFEREE` | 开 | 关 | 开 |
| `1` | `UART1_USE_LOG` | 关 | 开 | 关 |
| `2` | `UART1_USE_LOG_RTT` | 关 | 开 | 开 |

派生开关 `UART1_REFEREE_ENABLED`、`UART1_LOG_ENABLED`、`UART1_RTT_ENABLED` 自动由选择值计算。`VISION_USE_UART` 与 UART 日志共用 USART1 时会触发配置检查。

**当前第 10 行实际为 `#define UART1_USEUART1_USE_REFEREE`，没有定义 `UART1_USE`。** 未从编译命令传值时，预处理 `#if` 把未定义的 `UART1_USE` 当作 0，因此上述三个条件呈现为“裁判开、UART 日志关、RTT 开”。这不是一个正确的默认宏定义，不能把它记作源码已经定义了 `UART1_USE=0`。预期写法是 `#define UART1_USE UART1_USE_REFEREE`；本次仅整理文档，保留原源码。

### 2.2 主功能开关

| 宏 | 当前默认值 | 取值/作用 | 修改位置 |
| --- | --- | --- | --- |
| `ONE_BOARD` / `CHASSIS_BOARD` / `GIMBAL_BOARD` | 仅定义 `ONE_BOARD` | 分别为单板、底盘板、云台板；按源码约定只选一个 | `application/robot_def.h` |
| `VISION_USE_VCP` / `VISION_USE_UART` | 仅定义 VCP | USB CDC / UART 视觉通信；选择一个 | `application/robot_def.h` |
| `USB_TELEMETRY_ENABLE` | `1` | `1` 启用 50 Hz USB CSV 网页遥测并关闭 USB 视觉收发；`0` 恢复原有视觉 USB 分支 | `modules/telemetry/usb_telemetry_config.h` |
| `CHASSIS_DRIVE_TYPE` | `0` | `0` 麦克纳姆，`1` X 型全向轮 | `application/robot_def.h` |
| `CHASSIS_POWER_BENCH_TEST` | `1` | `0/1`；裁判数据无效时是否允许台架功率回退。源码注明正式固件保持 0 | `application/robot_def.h` |
| `CHASSIS_POWER_BENCH_LIMIT_W` | `50.0f` W | 台架回退预算 | `application/robot_def.h` |
| `POWER_METER_ENABLE` | `1` | `0/1`；关闭时不创建功率计实例和 CAN 过滤器 | `robot_def.h` 与 `chassis_power_config.h`，均用 `#ifndef` |
| `CHASSIS_POWER_CONTROL_STAGE` | `3` | `0` 观察；`1` 快保护与恢复；`2` 增加启动/预测约束；`3` 完整慢环 | `modules/algorithm/chassis_power_config.h` |
| `CHASSIS_POWER_CAPTURE_ENABLE` | `0` | `0/1`；启动二进制采样；UART 排出任务需 `UART1_LOG_ENABLED=1` | 同上 |
| `CHASSIS_BUFFER_POWER_ENABLE` | `1` | `0` 原预算；`1` 动态消耗裁判缓冲能量 | `modules/algorithm/chassis_energy_config.h` |
| `CHASSIS_POWER_SOURCE_MODE` | `0` | `CHASSIS_POWER_SOURCE_DIRECT=0` 直供；`CHASSIS_POWER_SOURCE_SUPERCAP=1` 电容 | 同上 |
| `CHASSIS_SPIN_ADAPTIVE_ENABLE` | `1` | 兼容开关；未指定 MODE 时，1 选自适应、0 选固定 | `modules/algorithm/chassis_spin_config.h` |
| `CHASSIS_SPIN_MODE` | 默认展开为 `1` | `FIXED=0`、`ADAPTIVE=1`、`SINE=2`；显式 MODE 优先于兼容开关 | 同上 |
| `CHASSIS_SPIN_DYNAMIC_ENABLED` | 默认真 | 自动计算：MODE 不等于 FIXED | 同上 |
| `CHASSIS_POWER_CALIBRATION_ENABLE` | `0` | `0/1`；CSV 是否追加标定字段 | `modules/telemetry/rtt_telemetry.h` |
| `REFEREE_LEGACY_POWER_FIELDS_ENABLED` | `0` | `0/1`；只在已确认旧协议含有效实测功率字段时打开；16 字节长度本身不足以判断 | `modules/referee/referee_power.h` |
| `DISABLE_LOG_SYSTEM` | 头文件 `0`，Make 实际 `1` | 1 屏蔽 `LOGINFO/WARNING/ERROR`；不直接屏蔽原始 `BSPLogWrite`、`LOG` 与遥测 | `bsp/log/bsp_log.h`、`Makefile` |

阶段 0 仍保留原有功率限幅，不等于关闭全部保护。当能量策略提供非零 `power_ceiling_w` 时，阶段 0/1 也会使用预测保护电流，不能将阶段 0 理解为完全不限制输出。

### 2.3 Make / CMake 的传参边界

当前两个构建入口都专门转发以下 11 项：`UART1_USE`、`POWER_METER_ENABLE`、`CHASSIS_POWER_CONTROL_STAGE`、`CHASSIS_POWER_CAPTURE_ENABLE`、`CHASSIS_SPIN_ADAPTIVE_ENABLE`、`CHASSIS_SPIN_MODE`、`CHASSIS_BUFFER_POWER_ENABLE`、`CHASSIS_POWER_SOURCE_MODE`、`REFEREE_LEGACY_POWER_FIELDS_ENABLED`、`CHASSIS_POWER_CALIBRATION_ENABLE`、`USB_TELEMETRY_ENABLE`。

USB 网页遥测的本地启动、完整 72 列协议、单位和连接排查参见 [USB 遥测仪表盘](../tools/usb-telemetry/README.md)。默认周期 `USB_TELEMETRY_PERIOD_MS=20`，重复表头间隔 `USB_TELEMETRY_HEADER_PERIOD_MS=1000`，静态缓冲 `USB_TELEMETRY_BUFFER_SIZE=2048` 字节。编译时切换 USB 用途需使用独立构建目录，避免复用其他编译选项的旧对象文件。

例如，使用独立目录构建直供、裁判 UART、自适应小陀螺版本：

```powershell
make BUILD_DIR=build/referee-direct UART1_USE=0 POWER_METER_ENABLE=1 CHASSIS_POWER_CONTROL_STAGE=3 CHASSIS_POWER_CAPTURE_ENABLE=0 CHASSIS_BUFFER_POWER_ENABLE=1 CHASSIS_POWER_SOURCE_MODE=0 CHASSIS_SPIN_MODE=1
```

同向正弦版本将 `CHASSIS_SPIN_MODE=1` 改为 `2`，并使用新的 `BUILD_DIR`。CMake 对应使用独立构建目录，例如：

```powershell
cmake -S . -B build/cmake-sine -G Ninja -DUART1_USE=0 -DPOWER_METER_ENABLE=1 -DCHASSIS_POWER_CONTROL_STAGE=3 -DCHASSIS_SPIN_MODE=2 -DCHASSIS_POWER_SOURCE_MODE=0
cmake --build build/cmake-sine
```

以上是传参示例，不是已执行的构建或正式上车配置。台架开关当前仍取源码的 1。

`CHASSIS_POWER_BENCH_TEST`、`CHASSIS_POWER_BENCH_LIMIT_W`、`CHASSIS_DRIVE_TYPE`、各个小陀螺数值宏虽然多数有 `#ifndef`，**目前不在这 10 项转发列表中**。仅写 `make CHASSIS_POWER_BENCH_TEST=0` 或 `cmake -DCHASSIS_SPIN_SINE_PERIOD_S=3.0f` 不会自动变成 C 预处理定义，应修改相应头文件，或明确扩展构建入口。能量阈值和机械参数中直接 `#define` 的项也不能假定命令行会优先覆盖。

Make 的同一目录不会仅因 `-D` 参数变化自动重编所有 `.o`。改宏后用新的构建目录或执行干净重编译。CMake 当前直接写 `set(CMAKE_BUILD_TYPE Debug)`；不能假定传 `-DCMAKE_BUILD_TYPE=Release` 就已切换成功。

## 3. 机械参数、方向与电机闭环

### 3.1 机器人定义

位置：[robot_def.h](../application/robot_def.h)。

| 宏 | 当前值 | 单位/意义 | 当前使用情况 |
| --- | --- | --- | --- |
| `WHEEL_BASE` | `350` | mm，纵向轴距 | 半轴距用于旋转力臂 |
| `TRACK_WIDTH` | `300` | mm，横向轮距 | 半轮距用于旋转力臂 |
| `CENTER_GIMBAL_OFFSET_X` | `0` | mm，云台中心前后偏移 | 用于四轮旋转力臂 |
| `CENTER_GIMBAL_OFFSET_Y` | `0` | mm，云台中心左右偏移 | 用于四轮旋转力臂 |
| `RADIUS_WHEEL` | `60` | mm，轮半径 | 仅用于定义 `PERIMETER_WHEEL`；现有轮速输出未引用周长 |
| `REDUCTION_RATIO_WHEEL` | `19.0f` | 轮电机减速比 | 当前控制代码未引用 |
| `OMNI_X_LF/RF/LB/RB_DIRECTION` | 四项均 `1.0f` | 各轮 `+1/-1` 方向校正 | 仅 X 型全向轮分支使用 |
| `YAW_CHASSIS_ALIGN_ECD` | `3734` | 编码器计数，云台/底盘同向零位 | 用于 `YAW_ALIGN_ANGLE` |
| `YAW_ECD_GREATER_THAN_4096` | `0` | 对齐计数是否超过 4096 | 决定偏角换算分支，修改零位时配套修改 |
| `PITCH_HORIZON_ECD` | `3412` | 编码器计数，Pitch 水平位 | 仅定义派生 `PTICH_HORIZON_ANGLE`，未用于限位 |
| `PITCH_MAX_ANGLE` / `PITCH_MIN_ANGLE` | `0 / 0` | Pitch 角度上下限 | 当前命令代码的“软件限位”尚无实现 |
| `GYRO2GIMBAL_DIR_YAW/PITCH/ROLL` | 三项均 `1` | 陀螺仪与云台方向，`1/-1` | 当前控制代码未引用 |
| `ONE_BULLET_DELTA_ANGLE` | `36` | 单发角度增量 | 发射单发/三连发分支使用；应用目前未启用 |
| `REDUCTION_RATIO_LOADER` | `36.0f` | 拨盘减速比 | 连发参考值换算使用 |
| `NUM_PER_CIRCLE` | `10` | 每圈载弹数 | 当前连发公式没有引用，实际分母写死为 8 |

`HALF_WHEEL_BASE`、`HALF_TRACK_WIDTH`、四个 `*_ROTATION_LEVER_SUM` 是自动派生量，修改源机械尺寸即可。当前底盘把 `wz` 按原有命令尺度乘旋转力臂，再把解算结果作为电机 rpm；平移量没有完整使用轮周长/减速比换算，因此小陀螺 `WZ` 参数应按现有命令单位调节，不能直接称为电机 rpm 或已标定的真实底盘角速度。

### 3.2 电机映射

| 电机 | 型号 | 总线 | 电调 ID (`tx_id`) | 方向 | 闭环/反馈 |
| --- | --- | --- | --- | --- | --- |
| 左前 LF | M3508 | CAN1 | `1` | NORMAL | SPEED / 电机反馈 |
| 右前 RF | M3508 | CAN1 | `2` | NORMAL | SPEED / 电机反馈 |
| 左后 LB | M3508 | CAN1 | `4` | NORMAL | SPEED / 电机反馈 |
| 右后 RB | M3508 | CAN1 | `3` | NORMAL | SPEED / 电机反馈 |
| Yaw（未启用） | GM6020 | CAN1 | `1` | NORMAL | ANGLE+SPEED / `YawTotalAngle`、`Gyro[2]` |
| Pitch（未启用） | GM6020 | CAN2 | `2` | NORMAL | ANGLE+SPEED / `Pitch`、`Gyro[0]` |
| 左摩擦轮（未启用） | M3508 | CAN2 | `1` | NORMAL | SPEED+CURRENT / 电机反馈 |
| 右摩擦轮（未启用） | M3508 | CAN2 | `2` | REVERSE | SPEED+CURRENT / 电机反馈 |
| 拨盘（未启用） | M2006 | CAN2 | `3` | NORMAL | 初始 SPEED+CURRENT / 电机反馈 |

这些 ID 是电调编号，不是整条 CAN 帧的发送 ID；DJI 驱动自动分组。M3508/M2006 的反馈 ID 为 `0x200+电调ID`，GM6020 为 `0x204+电调ID`。当前 Pitch 的 CAN2 反馈 ID 为 `0x206`，拨盘为 `0x203`；启用模块或改电调编号时按这一规则核对总线映射。

### 3.3 当前 PID 数值

未显式初始化的字段按 C 的结构体初始化规则为 0。位置：底盘、云台、发射各自的 `.c`，以及 `modules/imu/ins_task.c`。

| 环 | Kp | Ki | Kd | IntegralLimit | MaxOut | DeadBand | Improve |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 四轮底盘速度 | `0.75` | `0` | `0` | `3000` | `15000` | `0` | `0x07` |
| 旧 Buffer PID | `0.1` | `0` | `0` | `1000` | `1000` | `0` | `0x07` |
| Yaw 角度 | `8` | `0` | `0` | `100` | `500` | `0.1` | `0x07` |
| Yaw 速度 | `50` | `200` | `0` | `3000` | `20000` | `0` | `0x07` |
| Pitch 角度 | `10` | `0` | `0` | `100` | `500` | `0` | `0x07` |
| Pitch 速度 | `50` | `350` | `0` | `2500` | `20000` | `0` | `0x07` |
| 摩擦轮速度/电流（各一套） | `0` | `0` | `0` | `10000` | `15000` | `0` | `0x01` |
| 拨盘角度 | `0` | `0` | `0` | `0` | `200` | `0` | `0` |
| 拨盘速度/电流 | `0` | `0` | `0` | `5000` | `5000` | `0` | `0x01` |
| IMU 温控 | `1000` | `20` | `0` | `300` | `2000` | `0` | `0x01` |

底盘速度环使用度/秒，运动学/功率模型使用 rpm，驱动中按 `6` 转换。底盘 `Kp=0.75` 对应旧 rpm 尺度的 `4.5`。旧 `buffer_PID` 仅初始化，未调用计算，不参与当前动态缓冲策略。

底盘另填 `Output_LPF_RC=0.3`，但 `Improve` 没有包含 `PID_OutputFilter`，因此当前输出低通未启用。单改 RC 不会开启滤波。

PID 通用可设置字段：`Kp/Ki/Kd`、`MaxOut`、`DeadBand`、`IntegralLimit`、`CoefA/CoefB`（变速积分）、`Output_LPF_RC`、`Derivative_LPF_RC`、`Improve`。Improve 在 [controller.h](../modules/algorithm/controller.h) 中按位组合：

| 位值 | 标志 | 用途 |
| --- | --- | --- |
| `0x01` | `PID_Integral_Limit` | 积分限幅 |
| `0x02` | `PID_Derivative_On_Measurement` | 对反馈量微分 |
| `0x04` | `PID_Trapezoid_Intergral` | 梯形积分 |
| `0x08` | `PID_Proportional_On_Measurement` | 对反馈量比例处理 |
| `0x10` | `PID_OutputFilter` | 输出低通 |
| `0x20` | `PID_ChangingIntegrationRate` | 变速积分 |
| `0x40` | `PID_DerivativeFilter` | 微分低通 |
| `0x80` | `PID_ErrorHandle` | 异常处理 |

电机通用设置位于 `modules/motor/motor_def.h`：`outer_loop_type` 选最外环，`close_loop_type` 按 `CURRENT_LOOP=1`、`SPEED_LOOP=2`、`ANGLE_LOOP=4` 位或组合；`motor_reverse_flag` 与 `feedback_reverse_flag` 分别控制电机/反馈方向（正常 0、反向 1）。`angle_feedback_source`、`speed_feedback_source` 取 `MOTOR_FEED=0` 或 `OTHER_FEED=1`，后者需配置反馈指针。`feedforward_flag` 按电流前馈 1、速度前馈 2 组合，并绑定对应前馈指针。各配置接口的完整字段声明也收录于索引。

## 4. 底盘功率参数

### 4.1 功率模型和分配

位置：[chassis.c](../application/chassis/chassis.c)，模型为四个轮子共用的 `m3508_power_model`。

当前公式：`P = k0 + k1*I + k2*n + k3*I*n + k4*I² + k5*n²`。`I=abs(current_command/current_conversion)`，`n=abs(speed_rpm)`，输出 W。系数必须与这套电流/转速换算约定一起使用。

| 字段/宏 | 当前值 | 意义 |
| --- | --- | --- |
| `k0` | `1.3123816` | 常数损耗项 |
| `k1` | `-0.25836727` | 电流一次项 |
| `k2` | `0.0007424524` | 转速一次项 |
| `k3` | `0.0024271158` | 电流×转速项 |
| `k4` | `0.081198552` | 电流平方项 |
| `k5` | `3.890596e-08` | 转速平方项 |
| `current_conversion` | `1000.0f` | 模型电流换算分母 |
| `CHASSIS_POWER_SAFETY_FACTOR` / `algorithm.safety_factor` | `0.98f` | 功率安全系数 |
| `small_error_threshold_rpm` | `500.0f` rpm | 小速度误差分配阈值 |
| `reserved_power_threshold_w` | `54.0f` W | 低预算分配阈值 |
| `per_motor_reserved_power_w` | `8.0f` W | 每轮预留功率 |
| `max_current_command` | `15000.0f` | 电流命令限幅，与底盘速度 PID MaxOut 配套 |
| 初始 `attenuation` | `1.0f` | `DJIChassisPowerSetAttenuation()` 设置的额外衰减 |

保留阈值应满足 `reserved_power_threshold_w >= 4*per_motor_reserved_power_w`；安全系数必须在 `(0,1]`。旧 `modules/motor/power_control.c` 的 `TORQUE_COEF`、`POWER_COEF` 是另一套遗留算法；当前底盘注册的是 `DJIChassisPowerRegister`，不要把旧系数当作本模型系数。

### 4.2 超级电容预算参数

位置：`chassis.c` 的 `chassis_budget_config`；直供模式不会使用电容增补。

| 字段 | 当前值 | 意义 |
| --- | --- | --- |
| `empty_energy_ratio` | `0.10f` | 电容低能量端点 |
| `full_energy_ratio` | `0.30f` | 电容全额增补端点 |
| `max_boost_w` | `100.0f` W | 电容负载预算最大增补 |
| `rise_rate_w_per_s` | `200.0f` W/s | 电容负载预算独立上升斜率，动态缓冲分支也使用 |
| `demand_margin_w` | `5.0f` W | 实际驱动需求超过基础目标时，允许的额外物理功率余量 |

这里的 `100 W` 是电容负载增补上限；下节 `60 W` 是裁判缓冲输入预算增补上限，两者不同。

### 4.3 裁判缓冲能量策略

位置：[chassis_energy_config.h](../modules/algorithm/chassis_energy_config.h)。除满值检查外，各能量门槛作用于保守估算/预测值，不能直接与裁判原始 J 读数等同。

| 宏 | 当前值 | 用途 |
| --- | --- | --- |
| `CHASSIS_BUFFER_MAX_ENERGY_J` | `60.0f` J | 原始裁判缓冲读数上限，超过即无效 |
| `CHASSIS_BUFFER_RECOVERY_ENTER_J` | `15.0f` J | 估算下限降至此处进入回充 |
| `CHASSIS_BUFFER_RECOVERY_EXIT_J` | `30.0f` J | 估算下限恢复至此处，确认后退出回充 |
| `CHASSIS_BUFFER_HARD_FLOOR_J` | `5.0f` J | 预测能量硬底座 |
| `CHASSIS_BUFFER_PREDICTION_WINDOW_S` | `0.5f` s | 将富余能量折算为增补功率的时间窗 |
| `CHASSIS_BUFFER_GUARD_WINDOW_S` | `0.3f` s | 输入超限耗能保护时间窗 |
| `CHASSIS_BUFFER_SETTLEMENT_DELAY_S` | `0.1f` s | 裁判结算延迟预留 |
| `CHASSIS_BUFFER_ENERGY_MARGIN_J` | `3.0f` J | 估算下限额外扣除的能量余量 |
| `CHASSIS_BUFFER_POWER_MARGIN_W` | `5.0f` W | 估算/超限判断的功率余量 |
| `CHASSIS_BUFFER_BOOST_MAX_W` | `60.0f` W | 输入预算增补上限 |
| `CHASSIS_BUFFER_RECOVERY_POWER_W` | `20.0f` W | 最大回充减让，不是固定减去 20 W |
| `CHASSIS_BUFFER_RECOVERY_TARGET_TIME_S` | `1.0f` s | 按能量缺口计算减让的目标恢复时间 |
| `CHASSIS_BUFFER_OBSERVER_MAX_STEP_S` | `0.020f` s | 最大连续积分间隔，超过撤销增补 |
| `CHASSIS_BUFFER_RECOVERY_HOLD_S` | `0.1f` s | 退出回充的持续确认时间 |
| `CHASSIS_BUFFER_RISE_RATE_W_PER_S` | `600.0f` W/s | 输入预算上升斜率；直供负载跟随，电容负载另用 200 W/s |
| `CHASSIS_BUFFER_CORRECTION_RISE_J_PER_S` | `20.0f` J/s | 估计已建立时，新裁判帧向上校正的速度；丢帧也受限，向下立即校正 |
| `CHASSIS_POWER_DEMAND_MARGIN_W` | `5.0f` W | 超过基础功率目标的有效需求所允许的增补余量 |

缓冲增补只在新鲜、完整、非制动的驱动需求超过基础功率目标时放开，最多增加需求缺口加余量。低负载时输入目标保持基础预算，避免为回充电容主动耗用裁判缓冲。需求来自统一电机任务快照中的未限流模型功率，并结合负载侧实测功率。

估算器用 `CHASSIS_BUFFER_HISTORY_LENGTH=96` 段固定历史回看帧龄与结算延迟窗口；连续反馈按实测区间上界扣能量，缺失历史按最坏消耗扣减。新帧仍保留 3 J 固定余量；稳定的 80 W 输入、80 W 限额和零帧龄下，完整历史对应的测量下限为 `60 - 3 - 5*0.1 = 56.5 J`，不是固定扣除 9.5 J。负输入功率不计作再生能量。

电容负载预算中的输入贡献取本周期请求和新鲜 `0x052` 实际输入目标的较小值；反馈未确认的输入增量不提前计入负载预算。预算下降立即生效，控制步长超过 20 ms 时禁止上升。电容电量比例策略保持原样，可用焦耳与持续时间策略本次暂缓。

能量门槛须满足 `HARD_FLOOR < RECOVERY_ENTER < RECOVERY_EXIT <= MAX_ENERGY`，预测窗、保护窗、恢复目标时间、积分间隔及上升斜率必须为正。

### 4.4 功率外环

位置：`chassis.c` 的 `chassis_outer_loop_config`。目标功率取基准预算乘 `0.98`，修正量单位 W。

| 字段 | 当前值 | 用途 |
| --- | --- | --- |
| `kp` | `1.0f` | 实测误差比例增益 |
| `ki` | `1.5f` | 实测误差积分增益 |
| `measurement_lpf_tau_s` | `0.030f` s | 功率计低通时间常数 |
| `activation_margin_w` | `5.0f` W | 需求接近目标时的介入余量 |
| `error_deadband_w` | `0.3f` W | 误差连续死区 |
| `max_positive_correction_w` | `8.0f` W | 最大正修正 |
| `positive_slew_rate_w_per_s` | `60.0f` W/s | 正修正增长/回零速率；下调不限速 |
| `budget_drop_reset_w` | `1.0f` W | 预算下跌时清理正向学习的门槛 |
| `antiwindup_gain_per_s` | `10` /s | 积分回算增益 |
| `low_demand_delay_s` | `0.5f` s | 低需求衰减前等待 |
| `low_demand_decay_w_per_s` | `1` W/s | 低需求学习量衰减 |

自适应小陀螺在未触发基础限流时会保持正向慢环学习，由目标调速利用余量。正弦模式不设置这项保持条件。

### 4.5 快保护、启动、预测保护

位置：[chassis_power_transient.c](../modules/algorithm/chassis_power_transient.c) 的 `ChassisPowerTransientDefaultConfig()`。以下是实际应用值；`eta` 和电流上限会在 DJI 控制周期内被 `algorithm` 覆盖。

| 字段 | 默认 → 实际 | 单位/用途 |
| --- | --- | --- |
| `eta` | `0.95 → 0.98` | 预测安全系数 |
| `fast_gain` | `0.5` | 快速功率压制增益 |
| `release_w_per_s` | `60` | W/s，新鲜反馈下释放速率 |
| `stale_release_w_per_s` | `20` | W/s，反馈不足时释放速率 |
| `meter_fresh_ms` | `20` | ms，功率计反馈新鲜度 |
| `feedback_fresh_ms` | `5` | ms，电机反馈新鲜度 |
| `release_confirm_ms` | `10` | ms，释放确认 |
| `stable_run_ms` | `100` | ms，稳定运行确认 |
| `max_current_command` | `15000 → 15000` | 电流命令上限 |
| `acceleration_tau_s` | `0.010` | s，加速度低通 |
| `prediction_s` | `0.008` | s，预测提前量 |
| `growth_time_s` | `0.005` | s，增长兜底时间尺度 |
| `peak_ratio` | `1.30` | 启动峰值预算比率；非零源功率硬上限仍约束最终输出 |
| `low_rpm` | `100` | rpm，低速条件 |
| `low_current` | `500` | 低电流命令条件 |
| `target_step_rpm` | `300` | rpm，启动目标阶跃门槛 |
| `zero_enter_rpm` / `zero_exit_rpm` | `50 / 100` | rpm，近零速度滞回 |
| `demand_enter_current` / `demand_exit_current` | `200 / 100` | 电流需求滞回 |
| `low_hold_ms` | `100` | ms，低速低电流保持 |
| `startup_window_ms` | `30` | ms，启动额度窗口 |
| `startup_cooldown_ms` | `500` | ms，连续启动不应期 |
| `guard_search_nodes` | `256` | 预测可行电流搜索节点预算，允许 1～1024 |

此外，瞬态控制要求 `0 < dt <= 5 ms`；底盘应用中也写死了功率反馈 `20 ms`、控制快照/电机反馈 `5 ms` 的新鲜度检查。若修改默认配置中的时间门槛，需核对这些调用端条件。

## 5. 小陀螺全部调参项

位置：[chassis_spin_config.h](../modules/algorithm/chassis_spin_config.h)。`WZ` 使用现有底盘命令单位，`*_RATE` 使用该命令单位/s。

| 宏 | 当前值 | 适用/意义 |
| --- | --- | --- |
| `CHASSIS_SPIN_FIXED_WZ` | `4000.0f` | 固定速度；自适应初始值 |
| `CHASSIS_SPIN_MIN_WZ` | `1000.0f` | 自适应速度下限 |
| `CHASSIS_SPIN_MAX_WZ` | `6000.0f` | 动态模式速度上限 |
| `CHASSIS_SPIN_POWER_GAIN` | `40.0f` | 功率误差到调速速率的增益，命令单位/(W·s) |
| `CHASSIS_SPIN_RISE_RATE` | `400.0f` | 自适应最大加速率 |
| `CHASSIS_SPIN_FALL_RATE` | `2000.0f` | 自适应最大减速率 |
| `CHASSIS_SPIN_POWER_LPF_TAU_S` | `0.1f` s | 动态模式功率低通 |
| `CHASSIS_SPIN_POWER_DEADBAND_W` | `1.0f` W | 动态调速功率死区 |
| `CHASSIS_SPIN_SETTLE_TIME_S` | `0.2f` s | 允许加速条件持续确认 |
| `CHASSIS_SPIN_TRACKING_ERROR_RPM` | `500.0f` rpm | 各轮目标/实际速度误差门槛 |
| `CHASSIS_SPIN_BUDGET_DROP_RESET_W` | `1.0f` W | 仅功率自适应模式：功率目标下跌后的复位门槛；正弦模式不使用 |
| `CHASSIS_SPIN_SINE_CENTER_WZ` | `800.0f` | 正弦中心，独立于固定/自适应参数 |
| `CHASSIS_SPIN_SINE_MIN_WZ` | `200.0f` | 正弦起步和反馈失效时的同方向低速目标 |
| `CHASSIS_SPIN_SINE_AMPLITUDE_WZ` | `200.0f` | 正弦稳态幅度 |
| `CHASSIS_SPIN_SINE_PERIOD_S` | `4.0f` s | 正弦周期 |
| `CHASSIS_SPIN_SINE_RAMP_S` | `1.0f` s | 正弦幅度建立/收缩时间尺度 |
| `CHASSIS_SPIN_SINE_RISE_RATE` | `400.0f` | 正弦模式速度上升斜率 |
| `CHASSIS_SPIN_SINE_FALL_RATE` | `2000.0f` | 正弦模式速度下降斜率 |
| `CHASSIS_SPIN_SINE_DIRECTION` | `1` | 正弦旋转方向，只允许 `+1/-1`；一次旋转期间锁定方向 |

默认正弦目标在中心/幅度完全建立时为 `800 + 200*sin(2πt/4)`，即命令范围 `600～1000`。默认麦轮几何、没有平移时，目标轮速幅值约为 `3403～5672 rpm`，再经过功率、跟踪和斜率限制。进入时从下限 200 建立中心和幅度，不保证立即输出完整正弦波。原中心 4000 对应约 22689 rpm，在当前 50 W 台架预算下无法建立中心和振幅，因此单独下调正弦参数。

正弦模式不按相邻功率预算的比例缩放波形。缓冲增补因需求下降而撤销时，只要实测功率仍有余量且保护允许，目标保持原正弦节奏；实际超功率时平滑降低中心和振幅，限流或能量恢复时收缩振幅并暂停升速。目标变化受 `SINE_RISE_RATE` / `SINE_FALL_RATE` 约束，最终功率、电流保护仍即时生效。在线调参和 Flash 字段布局保持兼容，预算跌落复位阈值仅在自适应模式显示。

自适应约束：`0 < MIN <= FIXED <= MAX`。正弦使用独立的 `SINE_MIN` 和 `SINE_CENTER`，需满足 `0 < SINE_MIN <= SINE_CENTER`、`SINE_CENTER-AMPLITUDE >= SINE_MIN`、`SINE_CENTER+AMPLITUDE <= MAX`，周期、建立时间、上升/下降斜率为正。动态更新 `dt` 最大为 `20 ms`。

## 6. 通信、日志、缓冲区与实例容量

### 6.1 通信协议和超时

| 项目/宏或字段 | 当前值 | 位置/说明 |
| --- | --- | --- |
| `POWER_METER_STATUS_CAN_ID` / `POWER_METER_CAN_DLC` | `0x605 / 8` | `modules/power_meter/power_meter_protocol.h` |
| 功率计 `.offline_reload_count` | `10` | 底盘初始化；daemon 每 10 ms，约 100 ms 离线门槛；控制反馈另要求 20 ms 新鲜 |
| `SUPERCAP_COMMAND_CAN_ID` | `0x061` | C 板发电容命令 |
| `SUPERCAP_STATUS_CAN_ID` | `0x051` | 电容负载/状态反馈 |
| `SUPERCAP_INPUT_POWER_CAN_ID` | `0x052` | 电容输入功率反馈，动态缓冲电容模式需要 |
| `SUPERCAP_INPUT_COMMAND_TAG` / `SUPERCAP_INPUT_FEEDBACK_VERSION` / `SUPERCAP_CAN_DLC` | `0xA5 / 1 / 8` | `modules/super_cap/super_cap_protocol.h`；必须与电容固件协议匹配 |
| 电容 `.offline_reload_count` | `20` | 约 200 ms daemon 门槛；动态缓冲时状态另要求 20 ms 新鲜 |
| `REFEREE_ROBOT_STATE_COMMAND_ID` / `REFEREE_POWER_HEAT_COMMAND_ID` | `0x0201 / 0x0202` | 裁判功率快照输入 |
| `REFEREE_ROBOT_STATE_PAYLOAD_LENGTH` / `EXTENDED_PAYLOAD_LENGTH` | `13 / 17` 字节 | 裁判机器人状态支持长度 |
| `REFEREE_POWER_HEAT_PAYLOAD_LENGTH` / `LEGACY_PAYLOAD_LENGTH` | `14 / 16` 字节 | 裁判功率热量支持长度 |
| `REFEREE_POWER_DATA_TIMEOUT_MS` | `300` ms | 裁判快照有效时限 |
| `REFEREE_STREAM_BUFFER_CAPACITY` | `256` 字节 | 裁判流解析缓冲 |
| 双板底盘发送/接收 ID | `0x311 / 0x312` | 底盘用 CAN2；云台命令端用 CAN1、方向相反；需按实际板间连线配套 |
| `CAN_COMM_MAX_BUFFSIZE` / `MX_CAN_COMM_COUNT` | `60 / 4` | 双板通信字节上限/实例数 |
| `CAN_COMM_HEADER/TAIL/OFFSET_BYTES` | `'s' / 'e' / 4` | 双板帧格式，修改需两端一致 |
| `REMOTE_CONTROL_FRAME_SIZE` | `18` 字节 | 遥控器帧 |
| `RC_CH_VALUE_MIN/OFFSET/MAX` | `364 / 1024 / 1684` | 遥控器通道范围及中心 |
| 遥控器 `.reload_count` | `10` | 约 100 ms 离线门槛 |
| `VISION_RECV_SIZE / VISION_SEND_SIZE` | `18 / 36` 字节 | `master_process.h`；接收宏旁的“36 字节”注释与实际值不符，以 18 为准 |
| UART 视觉 `.reload_count` | `10` | 仅 UART 分支，约 100 ms |
| `Communicate_Data_LEN` | `5` | 裁判自定义交互数据长度 |
| `HC05_DATASIZE / HC05_BUFFERSIZE` | `4 / 6` 字节 | 蓝牙负载/含头尾缓冲；帧头尾 `0xAA/0x55` |

CAN 协议帧标记、命令 ID、错误位、长度和 CRC 是对端约定，不作为单侧调参开关使用；全部定义可在索引中查到。

### 6.2 日志与采样

| 宏/参数 | 当前值 | 用途 |
| --- | --- | --- |
| `BUFFER_INDEX` | `0` | BSP 日志 RTT 通道 |
| `BSP_LOG_FORMAT_BUFFER_SIZE` | `384` 字节 | 日志格式化缓冲 |
| `BSP_UART_LOG_RING_SIZE` | `4096` 字节 | UART 日志环形缓冲 |
| `RTT_TELEMETRY_CHASSIS_POWER_CSV_MAX_LENGTH` | `320` 字节 | 单行遥测格式化缓冲 |
| `CHASSIS_POWER_RTT_DIVIDER` | `40` | 每 40 个电机控制周期输出一次，1 ms 任务延时下约 25 Hz |
| `SEGGER_RTT_MAX_NUM_UP_BUFFERS / DOWN_BUFFERS` | `3 / 3` | RTT 通道容量 |
| `BUFFER_SIZE_UP / BUFFER_SIZE_DOWN` | `1024 / 16` 字节 | RTT 终端上行/下行缓冲 |
| `SEGGER_RTT_PRINTF_BUFFER_SIZE` | `64u` 字节 | RTT printf 临时缓冲 |
| `SEGGER_RTT_MODE_DEFAULT` | `SEGGER_RTT_MODE_NO_BLOCK_SKIP` | 默认非阻塞，空间不足跳过 |
| `SEGGER_RTT_MAX_INTERRUPT_PRIORITY` | `0x20` | RTT 临界区中断屏蔽阈值 |
| `CHASSIS_POWER_CAPTURE_RECORDS` | `150` | 启动波形记录数 |
| `CHASSIS_POWER_CAPTURE_RECORD_BYTES / FRAME_BYTES` | `96 / 102` | 记录/传输帧字节数，解码脚本需同步 |
| `CHASSIS_POWER_CAPTURE_QUANTIZED` | `0x80` | 量化/截断标志位 |

采集内部另写死：采样间隔 `2 ms`，触发前最多 `25` 条，触发后含触发样本 `125` 条；阶跃阈值 `300 rpm`；低速 `<100 rpm`、低电流命令 `<500` 持续 `100 ms`；触发冷却 `500 ms`。T90 使用 `90%` 目标、持续 `20 ms`、超时 `3000 ms`。停止控制后才排出波形，帧间隔至少 `21 ms`。修改位置：`modules/telemetry/chassis_power_capture.c`。

### 6.3 实例与资源上限

| 宏 | 当前值 | 位置/意义 |
| --- | --- | --- |
| `CAN_MX_REGISTER_CNT / MX_CAN_FILTER_CNT / DEVICE_CAN_CNT` | `16 / 28 / 2` | `bsp/can/bsp_can.h`，注册数/硬件过滤器数/总线数 |
| `DEVICE_USART_CNT / USART_RXBUFF_LIMIT` | `3 / 256` | `bsp/usart/bsp_usart.h` |
| `SPI_DEVICE_CNT / MX_SPI_BUS_SLAVE_CNT` | `2 / 4` | `bsp/spi/bsp_spi.h` |
| `IIC_DEVICE_CNT / MX_IIC_SLAVE_CNT` | `2 / 8` | `bsp/iic/bsp_iic.h` |
| `GPIO_MX_DEVICE_NUM` | `10` | `bsp/gpio/bsp_gpio.h` |
| `PWM_DEVICE_CNT` | `16` | `bsp/pwm/bsp_pwm.h` |
| `MX_SIG_LIST_SIZE` | `32` | `bsp/bsp_tools.c`，源码注明不可修改的信号量表上限 |
| `MAX_TOPIC_NAME_LEN / MAX_TOPIC_COUNT / QUEUE_SIZE` | `32 / 12 / 1` | 消息中心话题名/话题数/队列深度 |
| `DAEMON_MX_CNT` | `64` | 守护实例数 |
| `DJI_MOTOR_CNT` | `12` | DJI 电机实例数 |
| `DM_MOTOR_CNT / HT_MOTOR_CNT / LK_MOTOR_MX_CNT` | `4 / 4 / 4` | 对应电机驱动上限 |
| `SERVO_MOTOR_CNT / Servo_MAX_BUFF` | `7 / 10` | 舵机数量/缓冲 |
| `BUZZER_DEVICE_CNT` | `5` | 蜂鸣器实例数 |

`DEVICE_CAN_CNT`、`MX_CAN_FILTER_CNT` 等受芯片限制，增大数组值不代表增加硬件。其他上限增大后需重新核算 RAM、堆和总线负载。

### 6.4 电机驱动、显示和传感器常量

| 项目 | 当前值 | 位置/意义 |
| --- | --- | --- |
| DJI `SPEED_SMOOTH_COEF / CURRENT_SMOOTH_COEF` | `0.85 / 0.9` | `dji_motor.h`，反馈滤波 |
| `ECD_ANGLE_COEF_DJI` | `0.043945` | 编码器计数到角度，约 360/8192 |
| DJI daemon `.reload_count` | `2` | 约 20 ms 离线门槛；功率保护另要求 5 ms 反馈新鲜 |
| HT `SPEED_BUFFER_SIZE / HT_SPEED_BIAS / TORQUE_COEF_HT` | `5 / -0.0109901428 / 3.5` | 速度缓冲/偏差 rad/s/扭矩 Nm/A |
| HT `P_MIN/MAX, V_MIN/MAX, T_MIN/MAX` | `±95.5 / ±45 / ±18` | rad / rad/s / Nm，驱动协议编码范围 |
| HT `KP_MIN/MAX, KD_MIN/MAX` | `0～500 / 0～5` | 协议增益范围 |
| DM `DM_P_MIN/MAX, DM_V_MIN/MAX, DM_T_MIN/MAX` | `±12.5 / ±45 / ±18` | 达妙协议编码范围 |
| LK `I_MIN/MAX / REDUCTION_RATIO_DRIVEN` | `±2000 / 1` | 电流协议范围/减速比 |
| LK `ECD_ANGLE_COEF_LK / CURRENT_TORQUE_COEF_LK` | `360/65536 / 0.00512` | 角度/扭矩换算 |
| OLED `OLED_I2C_ADDRESS / MAX_COLUMN / MAX_ROW` | `0x78 / 128 / 64` | I2C 地址/尺寸；字体 `6×12` |
| IST8310 `IST8310_IIC_ADDRESS / MAG_SEN` | `0x0E / 0.3` | 磁力计地址/原始量转 µT |
| `DoFreq/ReFreq/MiFreq/FaFreq/SoFreq/LaFreq/SiFreq` | `523/587/659/698/784/880/988` | 蜂鸣器音符 Hz |

## 7. 命令输入与发射参数

位置：[robot_cmd.c](../application/cmd/robot_cmd.c)、[shoot.c](../application/shoot/shoot.c)。以下均为直接写在函数中的参数，不能通过同名 Make 变量调节。

| 项目 | 当前数值/行为 |
| --- | --- |
| 遥控器平移 | 两轴摇杆分别乘 `5.0f` |
| 遥控器云台增量 | Yaw 每次乘 `0.005f`，Pitch 每次乘 `0.001f`；受命令任务频率影响 |
| 遥控器模式 | 右开关下：小陀螺；中：不跟随；左开关下：遥控器；上：键鼠 |
| 摩擦轮开启门槛 | 拨轮 `< -100` |
| 连发门槛 | 拨轮 `< -500` |
| 遥控器射频 | `shoot_rate=8` 发/s，附近“一发每秒”注释与数值不符 |
| 急停门槛 | 拨轮 `>300`；右开关上恢复 READY |
| 键鼠平移 | `vx=(W-S)*300`，源码 `vy=(S-D)*300`，没有使用 A 键 |
| 鼠标角度增量 | X/Y 分别 `/660*10` |
| Z 键弹速档 | `15 / 18 / 30` |
| E 键发射档 | 停止 / 单发 / 三连发 / 连发 |
| C 键速度档字段 | `40 / 60 / 80 / 100`；底盘当前没有使用 `chassis_speed_buff` |
| 底盘跟随系数 | `wz=-1.5f*offset_angle*abs(offset_angle)` |
| 单发/三连发时间字段 | `150 / 300 ms`；对应休眠判断已注释，当前不产生不应期 |
| 连发拨盘参考 | `shoot_rate*360*REDUCTION_RATIO_LOADER/8`；分母为 8，非 `NUM_PER_CIRCLE=10` |
| 摩擦轮弹速表 | 15/18/30 档参考均为 `0`；默认分支参考为 `30000` 度/秒 |

当前 `GimbalInit/GimbalTask/ShootInit/ShootTask` 未在 `robot.c` 运行，相关 PID 与动作参数是待启用配置；摩擦轮、拨盘 PID 的实际增益均为 0，不能使用其注释里的旧数值作为当前设置。

## 8. IMU 参数

目前 `StartINSTASK` 使用旧 `modules/imu/BMI088driver.*` 路径；新 `modules/BMI088/` 模块的示例注册在命令初始化中已注释。

| 参数 | 当前值 | 修改位置/意义 |
| --- | --- | --- |
| IMU 温度 `RefTemp` | `40` °C | `ins_task.c` |
| `BMI088Init(&hspi1, 1)` | 初始化时在线标定 | `ins_task.c`；标定失败会使用离线参数 |
| 初始化加速度均值 | `100` 次，每次延时 `0.001 s` | `ins_task.c` |
| `IMU_Param.scale[X/Y/Z]` | `1/1/1` | 陀螺仪比例补偿 |
| `IMU_Param.Yaw/Pitch/Roll` | `0/0/0` | 安装角补偿 |
| EKF `process_noise1 / process_noise2 / measure_noise` | `10 / 0.001 / 1000000` | 四元数过程噪声/零偏过程噪声/加速度量测噪声 |
| EKF `lambda / lpf` | `1 / 0` | 渐消因子/加速度滤波系数 |
| `INS.AccelLPF` | `0.0085` s | 导航运动加速度低通 |
| 重力向量模长 | `9.81` m/s² | `ins_task.c` |
| 加速度量程/系数 | `6 g / 0.00179443359375` | `BMI088driver.c/h`；量程与转换系数需配套 |
| 陀螺仪量程/系数 | `2000 / 0.001065264436031695...` | 原始数据转换为 rad/s |
| 温度换算 | `raw*0.125 + 23.0` | `BMI088_TEMP_FACTOR / OFFSET` |
| 在线标定 `CaliTimes` / 超时 | `6000 / 12 s` | `BMI088driver.c` |
| 零偏扣除 `caliOffset` | `1` | 扣除标定得到的零偏 |
| 离线参数选择 `INFANTRY_ID` | 主工程未显式定义；`#if` 按 0 分支处理 | `BMI088driver.h`，0～4 分支详见索引 |
| 0 分支 `GxOFFSET/GyOFFSET/GzOFFSET/gNORM` | `0.00247530174 / 0.000393082853 / 0.000393082853 / 9.69293118` | 离线/标定超时回退参数 |
| 新 BMI088 预标定加速度偏移 | X/Y/Z 均 `0.0f` | `modules/BMI088/bmi088.c`，当前未注册使用 |
| 新 BMI088 `BMI088_PRE_CALI_G_NORM` | `9.805f` | 新模块预标定重力模长 |

`BMI088reg.h`、`bmi088_regNdef.h` 中的量程、ODR、带宽、寄存器地址/位标志全部收录于索引；改变选择值时同步转换系数。

## 9. 硬件初始化、RTOS 和构建

### 9.1 外设

位置：`Src/main.c`、`Src/usart.c`、`Src/can.c`、`Src/spi.c`、`Src/i2c.c`、`Src/tim.c`；完整 HAL 赋值见索引。

| 项目 | 当前配置 |
| --- | --- |
| 时钟 | HSE `12 MHz`；PLL `M=6,N=168,P=2,Q=7`；SYSCLK/AHB `168 MHz`，APB1 `42 MHz`，APB2 `84 MHz` |
| DWT | `DWT_Init(168)`，MHz；改系统时钟时配套修改 |
| USART1 / USART6 | `115200`，8 数据位，无校验，1 停止位 |
| USART3 遥控器 | `100000`，HAL 9 位字长含偶校验，实际 8 数据位，1 停止位 |
| CAN1 / CAN2 | 正常模式；预分频 `3`，SJW `1TQ`，BS1 `10TQ`，BS2 `3TQ`，当前时钟下 `1 Mbit/s`；自动重发开，AutoBusOff 关 |
| SPI1 | 主机，8 位，CPOL 高，第二边沿，分频 8，当前约 `10.5 MHz` |
| SPI2 | 主机，8 位，CPOL 高，第二边沿，分频 256，当前约 `164.0625 kHz` |
| I2C2 / I2C3 | `400000 Hz`，7 位地址 |
| ADC1 | 12 位，PCLK/6 |
| TIM1 / TIM8 | 预分频 `167`，周期 `19999`，当前 50 Hz |
| TIM4 | 预分频 `167`，周期 `65535` |
| TIM5 | 预分频 `0`，周期 `65535` |
| TIM10 加热 | 预分频 `167`，周期 `4999`，当前 200 Hz |
| USB CDC 缓冲 | `APP_RX_DATA_SIZE=2048`、`APP_TX_DATA_SIZE=2048` |
| USB 描述符 | VID `1155`、PID `202`，产品名 `Vision Comm port`，厂商 `YueLuEmbedded` |
| USB 配置 | 接口/配置数 `1/1`，描述符缓冲 `512`，DEBUG/LPM `0/0`，SELF_POWERED `1` |
| 外设中断优先级 | CAN/SPI/USART 常用 `5`；FLASH `6`；PendSV、HAL tick `15` |

引脚宏逐项见索引的 `Inc/main.h`；外设 AF、DMA 通道/流、GPIO、电源和 RTC 设置见对应 `Src/` 小节。修改 CubeMX 生成设置时需同步核对 `basic_framework.ioc`。

### 9.2 任务频率和栈

位置：[robot_task.h](../application/robot_task.h)。这些任务用循环末尾 `osDelay()`，周期包含执行时间，表中频率为近似值。

| 任务 | 延时/近似频率 | 优先级 | 栈（word / byte） | 条件 |
| --- | --- | --- | --- | --- |
| INS | `1 ms / 1 kHz` | AboveNormal | `1024 / 4096` | 创建 |
| 电机 | `1 ms / 1 kHz` | Normal | `512 / 2048` | 创建 |
| 守护/蜂鸣器 | `10 ms / 100 Hz` | Normal | `128 / 512` | 创建 |
| 机器人应用 | `5 ms / 200 Hz` | Normal | `1024 / 4096` | 创建 |
| UI | 循环末尾 `1 ms`，另受发送延时影响 | Normal | `512 / 2048` | UART1 裁判模式 |
| 功率采集排出 | `5 ms` | BelowNormal | `256 / 1024` | CAPTURE=1 且 UART 日志开 |
| 默认 USB 初始化任务 | 执行后自删 | Normal | `128 / 512` | `Src/freertos.c` |

本工程 CMSIS 适配直接将 `stacksize` 传给 FreeRTOS，单位是 word，F407 上一个 word 为 4 字节。

### 9.3 FreeRTOS、HAL、存储和工具链

| 设置 | 当前值 | 位置 |
| --- | --- | --- |
| `configTICK_RATE_HZ` | `1000` | `Inc/FreeRTOSConfig.h` |
| `configTOTAL_HEAP_SIZE` | `20000` 字节 | 同上 |
| `configMINIMAL_STACK_SIZE` | `128` word | 同上 |
| `configMAX_PRIORITIES / configMAX_TASK_NAME_LEN` | `7 / 32` | 同上 |
| 抢占/静态分配/动态分配/FPU | 均 `1` | 同上；MPU 为 0 |
| 空闲钩子/Tick 钩子/协程 | 均 `0` | 同上 |
| 互斥量/计数信号量 | 均 `1` | 同上；队列注册表 8 |
| 最低中断优先级 / 可调用 RTOS API 的最高优先级 | `15 / 5` | 同上；数值越小优先级越高 |
| HAL `HSE/HSI/LSI/LSE_VALUE` | `12000000/16000000/32000/32768` Hz | `Inc/stm32f4xx_hal_conf.h` |
| HAL `HSE/LSE_STARTUP_TIMEOUT` | `100/5000` ms | 同上 |
| HAL `VDD_VALUE / TICK_INT_PRIORITY / USE_RTOS` | `3300 mV / 15 / 0` | USE_RTOS 是 HAL 配置，不代表主工程未运行 FreeRTOS |
| HAL Prefetch / 指令缓存 / 数据缓存 | `1/1/1` | 同上；HAL 回调注册开关为 0 |
| HAL 启用模块 | ADC/CAN/CRC/DAC/I2C/RNG/RTC/SPI/TIM/UART/PCD，以及 GPIO/EXTI/DMA/RCC/FLASH/PWR/CORTEX | 同上，其余注释选项见索引 |
| 链接最小 Heap / Stack | `0x5000 / 0x5000`，各 20480 字节 | `STM32F407IGHx_FLASH.ld`；与 FreeRTOS 堆不是同一配置 |
| RAM / CCMRAM / Flash | `128K / 64K / 1024K` | Flash 前 768K 用于固件，末尾扇区 10/11 各 128K 用于在线参数掉电日志；普通烧录保留这两个扇区 |
| Make `DEBUG / OPT / BUILD_DIR / TARGET` | `1 / -Og / build / basic_framework` | `Makefile` |
| 架构 | Cortex-M4，Thumb，硬浮点，`fpv4-sp-d16` | Make/CMake |
| 公共编译宏 | `USE_HAL_DRIVER / STM32F407xx / ARM_MATH_CM4` | Make/CMake |
| CMake | C11，默认 Debug；Debug 另定义 `ESC_DEBUG` | `CMakeLists.txt` |

## 10. 使用这份清单时的核对点

优先按功能查本表，再从完整索引跳转到源码。所有“默认值”均要结合构建命令、`#ifndef`、运行路径判断；有初始化不代表已参与控制。

当前容易误读的项已在对应表中注明：UART1 默认宏拼接异常；Make 与 CMake 的分级日志默认不一致；小陀螺默认仍是自适应；瞬态 `eta` 的 0.95 会被覆盖为 0.98；底盘输出低通没有启用位；轮半径/轮减速比尚未进入当前平移换算；云台限位没有应用；云台/发射任务调用已注释；载弹数 10 与连发分母 8 不一致；采集记录/帧长度与内部固定编码布局需配套修改。

功率标定步骤参见 [底盘功率标定](chassis-power-calibration.md)，启动保护参见 [功率启动与恢复](chassis-power-startup.md)，电容接线和帧格式参见 [超级电容 CAN 协议](supercap-can-protocol.md)。
