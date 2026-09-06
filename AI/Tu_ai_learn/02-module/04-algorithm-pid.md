# PID 控制器 -- 串级 PID 与优化环节

## 模块职责

PID 控制器模块（controller）提供带多种优化环节的位置式 PID 实现，是电机串级闭环控制的核心算法组件。

## 设计思路

### 为什么 PID 需要这么多优化环节

在 RoboMaster 电机控制场景下，基础 PID 存在几个典型问题：

- **积分饱和**：电机堵转时积分项持续累积，恢复后产生超调
- **微分突变**：设定值跳变时微分项产生尖峰（"微分冲击"）
- **高频噪声**：传感器反馈的高频噪声被微分项放大
- **积分过度**：误差大时积分应该弱，误差小时积分应该强

框架通过位掩码（`PID_Improvement_e`）让用户按需启用优化环节，而不是强制所有功能。这种设计既避免了不必要的计算开销，也降低了调试难度。

### 位置式 vs 增量式

本框架使用**位置式 PID**，即输出 = Pout + Iout + Dout。位置式的优势在于输出绝对值，便于直接限幅和调试观察。

## 核心数据结构

### PID_Improvement_e -- 优化环节位掩码

```c
// controller.h
typedef enum {
    PID_IMPROVE_NONE              = 0b00000000, // 无优化
    PID_Integral_Limit            = 0b00000001, // 积分限幅
    PID_Derivative_On_Measurement = 0b00000010, // 微分先行（对反馈微分而非误差）
    PID_Trapezoid_Intergral       = 0b00000100, // 梯形积分
    PID_Proportional_On_Measurement = 0b00001000, // 比例先行
    PID_OutputFilter              = 0b00010000, // 输出低通滤波
    PID_ChangingIntegrationRate   = 0b00100000, // 变速积分
    PID_DerivativeFilter          = 0b01000000, // 微分滤波
    PID_ErrorHandle               = 0b10000000, // 堵转检测
} PID_Improvement_e;
```

通过位或运算组合启用多个优化，如 `PID_Integral_Limit | PID_Derivative_On_Measurement | PID_ChangingIntegrationRate`。

### PIDInstance -- PID 实例

```c
// controller.h
typedef struct {
    // === 初始化配置区 ===
    float Kp, Ki, Kd;         // PID 增益
    float MaxOut;              // 输出限幅
    float DeadBand;            // 死区（误差在死区内输出为零）

    PID_Improvement_e Improve; // 启用的优化环节位掩码
    float IntegralLimit;       // 积分限幅值
    float CoefA, CoefB;        // 变速积分参数
    float Output_LPF_RC;       // 输出滤波器时间常数 (RC = 1/omega_c)
    float Derivative_LPF_RC;   // 微分滤波器时间常数

    // === 运行时计算区 ===
    float Measure, Last_Measure;  // 当前/上次反馈值
    float Err, Last_Err;          // 当前/上次误差
    float Pout, Iout, Dout;       // P/I/D 各项输出
    float ITerm, Last_ITerm;      // 积分增量

    float Output, Last_Output;    // 当前/上次总输出
    float Last_Dout;              // 上次微分项（用于微分滤波）

    float Ref;                    // 当前设定值
    uint32_t DWT_CNT;             // DWT 计数器（用于计算 dt）
    float dt;                     // 两次计算的时间间隔

    PID_ErrorHandler_t ERRORHandler; // 堵转检测
} PIDInstance;
```

### PID_Init_Config_s -- 初始化配置

```c
// controller.h
typedef struct {
    float Kp, Ki, Kd;
    float MaxOut;
    float DeadBand;
    PID_Improvement_e Improve;
    float IntegralLimit;
    float CoefA, CoefB;
    float Output_LPF_RC;
    float Derivative_LPF_RC;
} PID_Init_Config_s;
```

注意：`PID_Init_Config_s` 的前几个字段与 `PIDInstance` 完全一致，`PIDInit()` 中利用了结构体内存连续的特性，通过 `memcpy` 直接复制配置参数到 PID 实例。

## 函数详解

### PIDInit() -- 初始化 PID 实例

```c
void PIDInit(PIDInstance *pid, PID_Init_Config_s *config)
```

**逻辑**（`controller.c:128`）：
1. `memset(pid, 0, sizeof(PIDInstance))` -- 将整个 PID 实例清零
2. `memcpy(pid, config, sizeof(PID_Init_Config_s))` -- 将配置参数复制到 PID 实例的前半段
3. `DWT_GetDeltaT(&pid->DWT_CNT)` -- 初始化 DWT 计时器

**注意**：`memcpy` 的正确性依赖于 `PID_Init_Config_s` 和 `PIDInstance` 的前 N 个字段排列完全一致。代码注释中也标注了 `@todo: 不建议这样做，可扩展性差`。

### PIDCalculate() -- 单次 PID 计算

```c
float PIDCalculate(PIDInstance *pid, float measure, float ref)
```

**逻辑**（`controller.c:147`）：

1. **堵转检测**（若启用 `PID_ErrorHandle`）
   - 当输出接近满量程且误差 > 95% 持续 500 次，标记堵转错误

2. **获取 dt**：`pid->dt = DWT_GetDeltaT(&pid->DWT_CNT)`

3. **计算误差**：`pid->Err = ref - measure`

4. **死区判断**：
   - 若 `|Err| > DeadBand`：执行完整 PID 计算
   - 若在死区内：清零积分增量和输出

5. **基本 PID 计算**（位置式）：
   ```c
   pid->Pout = pid->Kp * pid->Err;
   pid->ITerm = pid->Ki * pid->Err * pid->dt;  // 积分增量
   pid->Dout = pid->Kd * (pid->Err - pid->Last_Err) / pid->dt;
   ```

6. **优化环节**（按顺序执行）：
   - 梯形积分：`ITerm = Ki * (Err + Last_Err) / 2 * dt`
   - 变速积分：误差大时抑制积分增量，误差小时全量积分
   - 微分先行：`Dout = Kd * (Last_Measure - Measure) / dt`，对反馈微分避免设定值跳变冲击
   - 微分滤波：一阶低通滤波 Dout
   - 积分限幅：防止 Iout 超过 IntegralLimit，且输出饱和时停止积分

7. **累加积分**：`Iout += ITerm`

8. **计算输出**：`Output = Pout + Iout + Dout`

9. **输出滤波**（若启用）：一阶低通

10. **输出限幅**：`|Output| <= MaxOut`

11. **保存历史值**：Last_Err, Last_Measure, Last_Output, Last_Dout, Last_ITerm

### 各优化环节详解

#### 梯形积分 `f_Trapezoid_Intergral`

```c
// controller.c:17
pid->ITerm = pid->Ki * ((pid->Err + pid->Last_Err) / 2) * pid->dt;
```

用梯形面积代替矩形面积计算积分增量，精度更高。

#### 变速积分 `f_Changing_Integration_Rate`

```c
// controller.c:24
if (abs(pid->Err) <= pid->CoefB)
    return;                                     // 全量积分
if (abs(pid->Err) <= (pid->CoefA + pid->CoefB))
    pid->ITerm *= (CoefA - |Err| + CoefB) / CoefA;  // 部分积分
else
    pid->ITerm = 0;                             // 不积分
```

误差在 `[0, CoefB]` 内全量积分，在 `[CoefB, CoefA+CoefB]` 内线性衰减，超过 `CoefA+CoefB` 完全不积分。这实际上同时实现了积分分离的效果。

#### 积分限幅 `f_Integral_Limit`

```c
// controller.c:38
// 输出已饱和且积分仍在累积 -> 停止积分
if (abs(temp_Output) > pid->MaxOut && pid->Err * pid->Iout > 0)
    pid->ITerm = 0;
// Iout 本身超限 -> 截断
if (temp_Iout > pid->IntegralLimit)
    pid->Iout = pid->IntegralLimit;
```

双重保护：输出饱和时停止积分增长，积分项本身也有上限。

#### 微分先行 `f_Derivative_On_Measurement`

```c
// controller.c:64
pid->Dout = pid->Kd * (pid->Last_Measure - pid->Measure) / pid->dt;
```

只对反馈值微分，不涉及设定值。设定值突变时不会产生微分冲击。

#### 微分滤波 `f_Derivative_Filter`

```c
// controller.c:70
pid->Dout = pid->Dout * pid->dt / (pid->Derivative_LPF_RC + pid->dt) +
            pid->Last_Dout * pid->Derivative_LPF_RC / (pid->Derivative_LPF_RC + pid->dt);
```

一阶低通滤波器，抑制高频噪声。`Derivative_LPF_RC` 越大，滤波越强。

#### 输出滤波 `f_Output_Filter`

```c
// controller.c:77
pid->Output = pid->Output * pid->dt / (pid->Output_LPF_RC + pid->dt) +
              pid->Last_Output * pid->Output_LPF_RC / (pid->Output_LPF_RC + pid->dt);
```

对最终输出做低通滤波，平滑控制信号。

#### 堵转检测 `f_PID_ErrorHandle`

```c
// controller.c:97
if (|Ref - Measure| / |Ref| > 0.95f && |Output| > MaxOut * 0.001f)
    ERRORCount++;
if (ERRORCount > 500)
    ERRORType = PID_MOTOR_BLOCKED_ERROR;
```

当误差持续接近 100% 且输出不为零，累计 500 次后判定堵转。

## 调用链

### 串级 PID 在电机控制中的使用

```
APP: DJIMotorSetRef(motor, ref)
  -> motor->motor_controller.pid_ref = ref

MotorControlTask() [500Hz-1kHz]
  -> DJIMotorControl()                     // dji_motor.c:233
     -> 位置环: PIDCalculate(&angle_PID, measure, pid_ref)   // controller.c:147
        -> pid_ref = angle_PID.Output      // 位置环输出作为速度环输入
     -> 速度环: PIDCalculate(&speed_PID, measure, pid_ref)
        -> pid_ref = speed_PID.Output      // 速度环输出作为电流环输入
     -> 电流环: PIDCalculate(&current_PID, measure, pid_ref)
        -> pid_ref = current_PID.Output    // 最终输出写入 CAN 发送缓冲
```

`pid_ref` 在串级闭环中充当"数据载体"：每个环的输出覆盖 `pid_ref`，作为下一个环的输入。这种设计使得闭环类型的组合和切换非常灵活。

### PID 初始化路径

```
APP: DJIMotorInit(&config)
  -> PIDInit(&instance->motor_controller.current_PID, &config->controller_param_init_config.current_PID)
  -> PIDInit(&instance->motor_controller.speed_PID,   &config->controller_param_init_config.speed_PID)
  -> PIDInit(&instance->motor_controller.angle_PID,    &config->controller_param_init_config.angle_PID)
```

## 注意事项

1. **dt 的获取依赖 DWT** -- `PIDCalculate()` 每次调用时通过 `DWT_GetDeltaT()` 获取距上次调用的时间间隔。如果调用频率不稳定，PID 参数需要相应调整。

2. **memcpy 初始化方式的隐患** -- `PIDInit()` 用 `memcpy` 将配置结构体复制到实例前半段，这要求两个结构体的字段顺序和类型完全一致。若 `PID_Init_Config_s` 增加字段而 `PIDInstance` 没有，会导致内存越界。

3. **死区内的积分处理** -- 误差进入死区后，`ITerm` 和 `Output` 被清零，但 `Iout`（累积积分）没有被清零。退出死区后积分项会从之前的值继续累加。如果需要完全重置，需要手动处理。

4. **变速积分的参数选择** -- `CoefA` 控制变速积分的衰减范围，`CoefB` 控制全量积分的阈值。一般设置 `CoefA` 为额定误差的 1-2 倍，`CoefB` 为额定误差的 0.1-0.3 倍。

5. **PID 在 IMU 温控中的应用** -- `ins_task.c` 中也使用了 PID 进行 IMU 恒温控制（Kp=1000, Ki=20, Kd=0, 启用积分限幅），说明 PID 模块不限于电机控制。

6. **堵转检测的局限性** -- 当前仅检测误差 > 95% 的情况，对于部分堵转（误差 50%-80%）不会触发。且触发后仅设置标志位，没有自动恢复机制。
