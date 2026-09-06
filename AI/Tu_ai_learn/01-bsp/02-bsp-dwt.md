# BSP DWT — 高精度周期计数定时器

## 模块职责

利用 Cortex-M4 内核的 DWT（Data Watchpoint and Trace）周期计数器，提供微秒级精度的计时、延时和时间轴功能，且不依赖中断。

---

## 设计思路

### 为什么不用 SysTick / HAL_Delay？

1. **HAL_Delay 依赖中断** — 它基于 SysTick 中断递增的 uwTick 计数，在 `__disable_irq()` 临界区内完全失效
2. **精度不够** — SysTick 默认 1ms 中断，无法提供 us 级计时
3. **DWT 是硬件计数器** — 每个 CPU 时钟周期自增 1，168MHz 下精度约 6ns，且不依赖任何中断

### 32 位溢出问题

DWT 的 CYCCNT 是 32 位寄存器，168MHz 下约 25.6 秒溢出一次。框架通过 `DWT_CNT_Update()` 检测溢出并扩展为 64 位计数，保证长时间运行的计时准确。

---

## 核心数据结构

### DWT_Time_t

```c
// basic_framework/bsp/dwt/bsp_dwt.h:21-26
typedef struct
{
    uint32_t s;    // 秒
    uint16_t ms;   // 毫秒
    uint16_t us;   // 微秒
} DWT_Time_t;
```

- 用于存储分解后的绝对时间，由 `DWT_SysTimeUpdate()` 填充
- `s` 用 `uint32_t` 可表示约 136 年，足够
- `ms` 和 `us` 用 `uint16_t`，范围 0-999 和 0-999

### 静态变量

```c
// basic_framework/bsp/dwt/bsp_dwt.c:14-18
static DWT_Time_t SysTime;                          // 当前系统时间
static uint32_t CPU_FREQ_Hz, CPU_FREQ_Hz_ms, CPU_FREQ_Hz_us;  // 频率换算常量
static uint32_t CYCCNT_RountCount;                  // 溢出次数计数
static uint32_t CYCCNT_LAST;                        // 上次读取的 CYCCNT 值
static uint64_t CYCCNT64;                           // 64 位扩展的周期计数值
```

- `CPU_FREQ_Hz`：CPU 频率，单位 Hz（如 168000000）
- `CPU_FREQ_Hz_ms`：CPU 频率，单位 kHz（如 168000）
- `CPU_FREQ_Hz_us`：CPU 频率，单位 MHz（如 168）
- `CYCCNT_RountCount`：CYCCNT 从 0 溢出到 0 的次数，用于扩展为 64 位

### TIME_ELAPSE 宏

```c
// basic_framework/bsp/dwt/bsp_dwt.h:33-40
#define TIME_ELAPSE(dt, code)                    \
    do                                           \
    {                                            \
        float tstart = DWT_GetTimeline_s();      \
        code;                                    \
        dt = DWT_GetTimeline_s() - tstart;       \
        LOGINFO("[DWT] " #dt " = %f s\r\n", dt); \
    } while (0)
```

- **功能**：测量一段代码的执行时间，自动打印结果
- **注意**：内部使用了 `LOGINFO`，而 RTT 不支持 `%f`，此宏中用 `%f` 打印 float 可能输出不正确。实际使用建议改为 `Float2Str` 后用 `%s` 打印。

---

## 函数详解

### DWT_CNT_Update()

```c
// basic_framework/bsp/dwt/bsp_dwt.c:28-41
static void DWT_CNT_Update(void)
{
    static volatile uint8_t bit_locker = 0;
    if (!bit_locker)          // 简易互斥锁，防止重入
    {
        bit_locker = 1;
        volatile uint32_t cnt_now = DWT->CYCCNT;
        if (cnt_now < CYCCNT_LAST)  // 当前值比上次小，说明溢出了
            CYCCNT_RountCount++;     // 溢出次数 +1
        CYCCNT_LAST = DWT->CYCCNT;  // 更新上次值
        bit_locker = 0;
    }
}
```

- **功能**：检测 CYCCNT 是否发生溢出，并更新溢出计数
- **bit_locker 互斥**：用 `volatile uint8_t` 实现简易自旋锁，防止中断中重入导致溢出计数错误
- **溢出检测原理**：CYCCNT 从 0xFFFFFFFF 溢出到 0x00000000 时，当前值会小于上次值
- **假设**：两次调用之间的时间间隔不超过一次溢出（约 25.6 秒 @168MHz）

### DWT_Init()

```c
// basic_framework/bsp/dwt/bsp_dwt.c:43-60
void DWT_Init(uint32_t CPU_Freq_mHz)
{
    /* 使能 DWT 外设 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* DWT CYCCNT 寄存器计数清 0 */
    DWT->CYCCNT = (uint32_t)0u;

    /* 使能 Cortex-M DWT CYCCNT 寄存器 */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    CPU_FREQ_Hz = CPU_Freq_mHz * 1000000;
    CPU_FREQ_Hz_ms = CPU_FREQ_Hz / 1000;
    CPU_FREQ_Hz_us = CPU_FREQ_Hz / 1000000;
    CYCCNT_RountCount = 0;

    DWT_CNT_Update();  // 初始化 CYCCNT_LAST
}
```

- **参数**：`CPU_Freq_mHz` — CPU 频率，单位 MHz。C 板（F407）传 168，A 板（F427）传 180
- **三个步骤**：使能 DWT -> 清零 CYCCNT -> 使能 CYCCNT 计数
- **频率换算**：预计算 Hz/kHz/MHz 三种频率常量，避免后续除法运算中的重复计算
- **调用时机**：在 `BSPInit()` 中被首先调用

### DWT_GetDeltaT()

```c
// basic_framework/bsp/dwt/bsp_dwt.c:62-71
float DWT_GetDeltaT(uint32_t *cnt_last)
{
    volatile uint32_t cnt_now = DWT->CYCCNT;
    float dt = ((uint32_t)(cnt_now - *cnt_last)) / ((float)(CPU_FREQ_Hz));
    *cnt_last = cnt_now;      // 更新上次值
    DWT_CNT_Update();         // 检测溢出
    return dt;                // 返回秒为单位的间隔
}
```

- **功能**：获取两次调用之间的时间间隔（秒），float 精度
- **参数**：`cnt_last` — 上一次的时间戳指针，由函数内部自动更新
- **返回值**：时间间隔，单位秒
- **关键**：`cnt_now - *cnt_last` 即使跨溢出也能正确计算，因为 uint32_t 减法会自动取模
- **典型用法**：在控制循环中计算 dt 用于 PID 积分和微分

```c
// 控制循环中的典型用法
static uint32_t dwt_cnt;
while (1) {
    float dt = DWT_GetDeltaT(&dwt_cnt);
    PIDCalculate(&pid, dt);
    vTaskDelayUntil(&wake, delay);
}
```

### DWT_GetDeltaT64()

```c
// basic_framework/bsp/dwt/bsp_dwt.c:73-82
double DWT_GetDeltaT64(uint32_t *cnt_last)
{
    volatile uint32_t cnt_now = DWT->CYCCNT;
    double dt = ((uint32_t)(cnt_now - *cnt_last)) / ((double)(CPU_FREQ_Hz));
    *cnt_last = cnt_now;
    DWT_CNT_Update();
    return dt;
}
```

- 与 `DWT_GetDeltaT()` 完全相同，仅返回类型为 `double`
- **使用场景**：当 float 的 7 位有效数字不够用时（如长时间累计 dt 的精密计算）

### DWT_SysTimeUpdate()

```c
// basic_framework/bsp/dwt/bsp_dwt.c:84-98
void DWT_SysTimeUpdate(void)
{
    volatile uint32_t cnt_now = DWT->CYCCNT;
    static uint64_t CNT_TEMP1, CNT_TEMP2, CNT_TEMP3;

    DWT_CNT_Update();  // 先更新溢出计数

    CYCCNT64 = (uint64_t)CYCCNT_RountCount * (uint64_t)UINT32_MAX + (uint64_t)cnt_now;
    CNT_TEMP1 = CYCCNT64 / CPU_FREQ_Hz;          // 秒
    CNT_TEMP2 = CYCCNT64 - CNT_TEMP1 * CPU_FREQ_Hz;  // 余数
    SysTime.s = CNT_TEMP1;
    SysTime.ms = CNT_TEMP2 / CPU_FREQ_Hz_ms;     // 毫秒
    CNT_TEMP3 = CNT_TEMP2 - SysTime.ms * CPU_FREQ_Hz_ms;
    SysTime.us = CNT_TEMP3 / CPU_FREQ_Hz_us;     // 微秒
}
```

- **功能**：将 64 位周期计数分解为 s/ms/us 三级时间
- **64 位扩展**：`溢出次数 * UINT32_MAX + 当前值` 得到自 DWT 初始化以来的总周期数
- **注意**：`CYCCNT_RountCount * UINT32_MAX` 而非 `* (UINT32_MAX + 1)`，这里存在一个 off-by-one 误差，但对实际使用影响极小

### DWT_GetTimeline_s / ms / us

```c
// basic_framework/bsp/dwt/bsp_dwt.c:100-125
float DWT_GetTimeline_s(void)
{
    DWT_SysTimeUpdate();
    float DWT_Timelinef32 = SysTime.s + SysTime.ms * 0.001f + SysTime.us * 0.000001f;
    return DWT_Timelinef32;
}

float DWT_GetTimeline_ms(void)
{
    DWT_SysTimeUpdate();
    float DWT_Timelinef32 = SysTime.s * 1000 + SysTime.ms + SysTime.us * 0.001f;
    return DWT_Timelinef32;
}

uint64_t DWT_GetTimeline_us(void)
{
    DWT_SysTimeUpdate();
    uint64_t DWT_Timelinef32 = SysTime.s * 1000000 + SysTime.ms * 1000 + SysTime.us;
    return DWT_Timelinef32;
}
```

- **功能**：获取自 DWT 初始化以来的绝对时间
- 三个函数分别返回秒、毫秒、微秒精度
- `us` 版本使用 `uint64_t` 保证精度

### DWT_Delay()

```c
// basic_framework/bsp/dwt/bsp_dwt.c:127-134
void DWT_Delay(float Delay)
{
    uint32_t tickstart = DWT->CYCCNT;
    float wait = Delay;
    while ((DWT->CYCCNT - tickstart) < wait * (float)CPU_FREQ_Hz)
        ;
}
```

- **功能**：忙等延时，单位秒
- **最大优势**：不受中断开关状态影响，可以在 `__disable_irq()` 临界区内使用
- **参数**：`Delay` — 延时时间，单位秒（如 0.001f = 1ms）
- **原理**：不断读取 CYCCNT，当经过的周期数 >= 目标周期数时退出
- **注意**：这是忙等（CPU 一直在转），不适合长延时，长延时应使用 FreeRTOS 的 `vTaskDelay()`

---

## 调用链

### 初始化调用链

```
BSPInit()
  +-- DWT_Init(168)
        +-- CoreDebug->DEMCR |= TRCENA_Msk     // 使能 DWT
        +-- DWT->CYCCNT = 0                      // 清零
        +-- DWT->CTRL |= CYCCNTENA_Msk           // 开始计数
        +-- 频率常量计算
        +-- DWT_CNT_Update()                      // 初始化 CYCCNT_LAST
```

### 延时调用链

```
DWT_Delay(0.001f)              // 延时 1ms
  +-- tickstart = DWT->CYCCNT   // 记录起始值
  +-- while ((CYCCNT - tickstart) < 168000)  // 等待 168000 个周期
        // 空转，不释放 CPU
```

### 时间轴调用链

```
DWT_GetTimeline_s()
  +-- DWT_SysTimeUpdate()
  |     +-- DWT_CNT_Update()               // 检测溢出
  |     +-- CYCCNT64 = 溢出次数 * UINT32_MAX + 当前值
  |     +-- 分解为 s / ms / us
  +-- return s + ms*0.001 + us*0.000001
```

---

## 注意事项

1. **不同主频的参数** — C 板（STM32F407）为 168MHz，A 板（STM32F427）为 180MHz。传错参数会导致所有计时结果按比例偏差。
2. **DWT_Delay 是忙等** — 不会释放 CPU 给其他任务，仅在短延时或临界区中使用。普通任务延时用 `vTaskDelay()` 或 `vTaskDelayUntil()`。
3. **溢出检测的假设** — `DWT_CNT_Update()` 假设两次调用间隔不超过 25.6 秒（168MHz），如果长时间不调用 timeline 函数，需要手动调用 `DWT_SysTimeUpdate()` 更新。
4. **bit_locker 不是真正的互斥** — 仅在单核 Cortex-M 上大致有效，如果中断在 `bit_locker = 1` 和 `bit_locker = 0` 之间又触发了 `DWT_CNT_Update()`，会被跳过，可能导致溢出检测遗漏一次。但实际影响极小。
5. **off-by-one** — `CYCCNT64 = CYCCNT_RountCount * UINT32_MAX + cnt_now`，严格来说应该是 `* (UINT32_MAX + 1)`，但差异极小。
6. **DWT 与 HAL_Delay 的选择** — 在临界区（`__disable_irq()` 和 `__enable_irq()` 之间）禁止使用 `HAL_Delay()`，必须使用 `DWT_Delay()`。
