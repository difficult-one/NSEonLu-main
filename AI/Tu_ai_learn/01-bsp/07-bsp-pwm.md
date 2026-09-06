# BSP PWM — PWM 输出控制

## 模块职责

对 STM32 HAL 定时器 PWM 接口进行封装，自动计算定时器时钟频率，提供周期、占空比设置接口，并支持 DMA 波形生成。

---

## 设计思路

### 为什么需要自动计算时钟频率？

STM32F407 的定时器挂在不同 APB 总线上：

- APB1 上的定时器：TIM2-TIM7, TIM12-TIM14
- APB2 上的定时器：TIM1, TIM8-TIM11

时钟频率取决于 APB 预分频器的配置。当 APB 预分频不为 1 时，定时器时钟会自动倍频 2 倍。`PWMSelectTclk()` 自动处理这个逻辑，上层无需关心时钟树配置。

### 周期和占空比的计算

- **周期（ARR）** = `period * (tclk / (Prescaler + 1))`，其中 period 单位为秒
- **占空比（CCR）** = `dutyratio * ARR`，其中 dutyratio 范围 0~1

### 注册即启动

`PWMRegister()` 在注册完成后自动调用 `HAL_TIM_PWM_Start()` 启动 PWM 输出，并设置初始周期和占空比。

---

## 核心数据结构

### PWMInstance

```c
// basic_framework/bsp/pwm/bsp_pwm.h:22-31
typedef struct pwm_ins_temp
{
    TIM_HandleTypeDef *htim;                 // 定时器句柄
    uint32_t channel;                        // 通道号
    uint32_t tclk;                           // 定时器时钟频率（Hz）
    float period;                            // 周期（秒）
    float dutyratio;                         // 占空比（0~1）
    void (*callback)(struct pwm_ins_temp *);  // DMA 传输完成回调
    void *id;                                // Parent Pointer
} PWMInstance;
```

逐字段说明：

| 字段 | 作用 | 备注 |
|------|------|------|
| `htim` | 定时器句柄 | 指向 CubeMX 生成的 htimX |
| `channel` | PWM 通道号 | 如 `TIM_CHANNEL_1`，注意是 HAL 定义的宏值 |
| `tclk` | 定时器时钟频率 | 由 `PWMSelectTclk()` 自动计算，单位 Hz |
| `period` | PWM 周期 | 单位秒，如 0.001 = 1kHz |
| `dutyratio` | 占空比 | 范围 0~1，如 0.5 = 50% |
| `callback` | DMA 完成回调 | DMA 波形生成时使用 |
| `id` | Parent Pointer | 指向拥有此实例的 Module 结构体 |

### PWM_Init_Config_s

```c
// basic_framework/bsp/pwm/bsp_pwm.h:33-41
typedef struct
{
    TIM_HandleTypeDef *htim;
    uint32_t channel;
    float period;
    float dutyratio;
    void (*callback)(PWMInstance *);
    void *id;
} PWM_Init_Config_s;
```

---

## 函数详解

### PWMSelectTclk()

```c
// basic_framework/bsp/pwm/bsp_pwm.c:91-109
static uint32_t PWMSelectTclk(TIM_HandleTypeDef *htim)
{
    uintptr_t tclk_temp = ((uintptr_t)((htim)->Instance));

    if ((tclk_temp <= (APB1PERIPH_BASE + 0x2000UL)) &&
        (tclk_temp >= (APB1PERIPH_BASE + 0x0000UL)))
    {
        // APB1 定时器：TIM2-TIM7, TIM12-TIM14
        // 如果 APB1 预分频不为 1，定时器时钟 = APB1 时钟 * 2
        return (HAL_RCC_GetPCLK1Freq() * (APBPrescTable[(RCC->CFGR & RCC_CFGR_PPRE1)
               >> RCC_CFGR_PPRE1_Pos] == 0 ? 1 : 2));
    }
    else if (((tclk_temp <= (APB2PERIPH_BASE + 0x0400UL)) &&
             (tclk_temp >= (APB2PERIPH_BASE + 0x0000UL))) ||
             ((tclk_temp <= (APB2PERIPH_BASE + 0x4800UL)) &&
             (tclk_temp >= (APB2PERIPH_BASE + 0x4000UL))))
    {
        // APB2 定时器：TIM1, TIM8-TIM11
        return (HAL_RCC_GetPCLK2Freq() * (APBPrescTable[(RCC->CFGR & RCC_CFGR_PPRE1)
               >> RCC_CFGR_PPRE1_Pos] == 0 ? 1 : 2));
    }
    return 0;
}
```

- **功能**：根据定时器实例的内存地址判断其挂载在哪条 APB 总线上，返回对应的时钟频率
- **APB 预分频倍频规则**：当 APB 分频系数不为 1 时，定时器时钟自动 x2。`APBPrescTable[]` 存储了预分频值，0 表示不分频（x1），非 0 表示有分频（x2）
- **地址范围判断**：
  - APB1：`0x40000000 - 0x40002000`
  - APB2：`0x40010000 - 0x40010400` 或 `0x40014000 - 0x40014800`
- **注意**：APB2 的判断中用了 `RCC_CFGR_PPRE1`（应该是 `PPRE2`），这是一个潜在的 Bug。不过如果 APB1 和 APB2 预分频配置相同，结果不影响。

### PWMRegister()

```c
// basic_framework/bsp/pwm/bsp_pwm.c:27-48
PWMInstance *PWMRegister(PWM_Init_Config_s *config)
{
    if (idx >= PWM_DEVICE_CNT)
        while (1);

    PWMInstance *pwm = (PWMInstance *)malloc(sizeof(PWMInstance));
    memset(pwm, 0, sizeof(PWMInstance));

    pwm->htim = config->htim;
    pwm->channel = config->channel;
    pwm->period = config->period;
    pwm->dutyratio = config->dutyratio;
    pwm->callback = config->callback;
    pwm->id = config->id;
    pwm->tclk = PWMSelectTclk(pwm->htim);  // 自动计算时钟频率

    // 注册后立即启动 PWM
    HAL_TIM_PWM_Start(pwm->htim, pwm->channel);
    PWMSetPeriod(pwm, pwm->period);         // 设置初始周期
    PWMSetDutyRatio(pwm, pwm->dutyratio);   // 设置初始占空比

    pwm_instance[idx++] = pwm;
    return pwm;
}
```

- **注册即启动** — 注册后自动调用 `HAL_TIM_PWM_Start()`，PWM 立即输出
- **tclk 自动计算** — 不需要用户手动传入时钟频率
- **初始化输出** — 设置注册时的周期和占空比，PWM 通道立即开始输出

### PWMStart() / PWMStop()

```c
// basic_framework/bsp/pwm/bsp_pwm.c:51-60
void PWMStart(PWMInstance *pwm)
{
    HAL_TIM_PWM_Start(pwm->htim, pwm->channel);
}

void PWMStop(PWMInstance *pwm)
{
    HAL_TIM_PWM_Stop(pwm->htim, pwm->channel);
}
```

- **功能**：启动/停止 PWM 输出
- **注意**：`PWMStop()` 会同时关闭定时器通道输出，引脚回到默认状态

### PWMSetPeriod()

```c
// basic_framework/bsp/pwm/bsp_pwm.c:68-71
void PWMSetPeriod(PWMInstance *pwm, float period)
{
    __HAL_TIM_SetAutoreload(pwm->htim,
        period * ((pwm->tclk) / (pwm->htim->Init.Prescaler + 1)));
}
```

- **功能**：设置 PWM 周期，单位秒
- **ARR 计算公式**：`ARR = period * (tclk / (Prescaler + 1))`
  - `tclk`：定时器输入时钟频率（Hz）
  - `Prescaler + 1`：预分频系数（CubeMX 中配置）
  - `period`：期望的周期（秒）
- **注意**：修改 ARR 后，如果当前 CCR 大于新的 ARR，占空比会变为 100%。建议先设周期再设占空比。

**计算示例**（1kHz PWM on TIM1 @168MHz, Prescaler=167）：

```
ARR = 0.001 * (168000000 / (167 + 1)) = 0.001 * 1000000 = 1000
PWM 频率 = tclk / ((PSC+1) * (ARR+1)) = 168000000 / (168 * 1001) ≈ 999Hz
```

### PWMSetDutyRatio()

```c
// basic_framework/bsp/pwm/bsp_pwm.c:78-81
void PWMSetDutyRatio(PWMInstance *pwm, float dutyratio)
{
    __HAL_TIM_SetCompare(pwm->htim, pwm->channel,
        dutyratio * (pwm->htim->Instance->ARR));
}
```

- **功能**：设置 PWM 占空比，范围 0~1
- **CCR 计算公式**：`CCR = dutyratio * ARR`
  - `dutyratio = 0`：一直低电平
  - `dutyratio = 0.5`：50% 占空比
  - `dutyratio = 1`：一直高电平
- **读取当前 ARR** — 通过 `pwm->htim->Instance->ARR` 获取硬件寄存器的当前值，确保与实际周期一致

### PWMStartDMA()

```c
// basic_framework/bsp/pwm/bsp_pwm.c:84-87
void PWMStartDMA(PWMInstance *pwm, uint32_t *pData, uint32_t Size)
{
    HAL_TIM_PWM_Start_DMA(pwm->htim, pwm->channel, pData, Size);
}
```

- **功能**：启动 DMA 波形生成，用于需要复杂 PWM 波形（如 WS2812 LED 驱动）
- **原理**：DMA 自动将数据数组依次写入 CCR 寄存器，每个 PWM 周期更新一次占空比
- **注意**：数据的位数必须和 CubeMX 中配置的 DMA 传输位数一致（如 16 位数据需配置 DMA 为 Half Word）

### HAL_TIM_PWM_PulseFinishedCallback()

```c
// basic_framework/bsp/pwm/bsp_pwm.c:14-25
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    for (uint8_t i = 0; i < idx; i++)
    {
        if (pwm_instance[i]->htim == htim
            && htim->Channel == (1 << (pwm_instance[i]->channel / 4)))
        {
            if (pwm_instance[i]->callback)
                pwm_instance[i]->callback(pwm_instance[i]);
            return;
        }
    }
}
```

- **功能**：PWM 脉冲完成回调（DMA 模式下最后一个脉冲发送完成时触发）
- **通道匹配公式**：`1 << (channel / 4)`
  - HAL 定义的通道号：`TIM_CHANNEL_1 = 0`, `TIM_CHANNEL_2 = 4`, `TIM_CHANNEL_3 = 8`, `TIM_CHANNEL_4 = 12`
  - `channel / 4` 得到 0/1/2/3，`1 << (channel/4)` 得到 1/2/4/8
  - `htim->Channel` 是 HAL 设置的位掩码，两者按此方式匹配
- **return 退出** — 一个通道只对应一个实例

---

## 调用链

### 设置占空比路径

```
APP 层
  +-- Module 层调用 PWMSetDutyRatio(pwm, 0.75)
        +-- __HAL_TIM_SetCompare(htim, channel, 0.75 * ARR)
              +-- 写入 CCR 寄存器
                    +-- 下一周期 PWM 输出变为 75% 占空比
```

### DMA 波形生成路径

```
Module 层
  +-- PWMStartDMA(pwm, wave_data, 100)
        +-- HAL_TIM_PWM_Start_DMA(htim, channel, pData, Size)
              +-- DMA 控制器每个 PWM 周期从数组读取一个值写入 CCR
              +-- 全部发送完成后触发中断
                    +-- HAL_TIM_PWM_PulseFinishedCallback(htim)
                          +-- 遍历匹配 htim + channel
                          +-- callback(pwm_instance)  // 通知 Module 层 DMA 完成
```

### 注册路径

```
Module 层
  +-- PWMRegister(&config)
        +-- malloc + memset
        +-- tclk = PWMSelectTclk(htim)   // 自动计算时钟
        +-- HAL_TIM_PWM_Start()           // 启动 PWM
        +-- PWMSetPeriod()                // 设置初始周期
        +-- PWMSetDutyRatio()             // 设置初始占空比
        +-- pwm_instance[idx++] = pwm
        +-- return pwm
```

---

## 注意事项

1. **APB2 时钟计算中的 Bug** — `PWMSelectTclk()` 中 APB2 分支使用了 `RCC_CFGR_PPRE1`（APB1 的预分频位），应为 `RCC_CFGR_PPRE2`。如果 APB1 和 APB2 预分频配置相同则无影响，否则 APB2 定时器的时钟频率会计算错误。
2. **注册即启动** — `PWMRegister()` 会自动启动 PWM 输出。如果不希望注册后立即输出，需要先设占空比为 0，注册后再手动 `PWMStop()`。
3. **DMA 数据位数** — 使用 `PWMStartDMA()` 时，数据数组的元素大小必须与 CubeMX 中配置的 DMA 传输位数一致。16 位数据配 Half Word，32 位数据配 Word。配置错误会导致指针越界或数据错误。
4. **先设周期再设占空比** — `PWMSetDutyRatio()` 使用当前 ARR 值计算 CCR。如果先减小 ARR 再设占空比，中间可能出现短暂的 100% 占空比。
5. **Prescaler 由 CubeMX 决定** — `PWMSetPeriod()` 的计算依赖 `htim->Init.Prescaler`，这个值在 CubeMX 中配置。修改预分频值会影响所有使用同一定时器的 PWM 通道。
6. **同一定时器多通道共享 ARR** — STM32 的定时器只有一个 ARR 寄存器，同一定时器的所有 PWM 通道共享周期。不能对同一定时器的不同通道设置不同的频率。
7. **tclk 为 0 的情况** — 如果 `PWMSelectTclk()` 无法识别定时器地址（返回 0），后续的周期和占空比计算将全部失效。确保使用的定时器在已知的地址范围内。
