# BSP GPIO — GPIO 控制与 EXTI 中断分发

## 模块职责

对 HAL GPIO 接口进行实例化封装，提供简洁的读写/翻转 API，并实现 EXTI 外部中断的回调分发机制。

---

## 设计思路

### 为什么 GPIO 也需要封装？

虽然 `HAL_GPIO_WritePin()` 已经很简洁，但封装仍有价值：

1. **EXTI 中断分发** — 多个 GPIO 引脚都可能触发外部中断，需要一个分发机制将中断路由到对应的 Module 回调
2. **实例化管理** — 与其他 BSP 模块保持一致的注册制模式
3. **状态封装** — 将 GPIOx、Pin、状态等打包在一起，上层不用散乱地传递多个参数

### 为什么 EXTI 分发不需要检查 GPIOx？

STM32 的 EXTI 中断线与引脚号是一一对应的：

- PA0、PB0、PC0 共享 EXTI0 线
- 同一时刻只有一个 GPIO 端口的 PinX 可以连接到 EXTIX

因此在回调中只需要通过 `GPIO_Pin` 就能唯一确定是哪个 GPIO 实例。框架假设同一 Pin 号不会在多个端口上同时配置为 EXTI（这是硬件限制，不是软件 Bug）。

---

## 核心数据结构

### GPIO_EXTI_MODE_e 枚举

```c
// basic_framework/bsp/gpio/bsp_gpio.h:10-16
typedef enum
{
    GPIO_EXTI_MODE_RISING,          // 上升沿触发
    GPIO_EXTI_MODE_FALLING,         // 下降沿触发
    GPIO_EXTI_MODE_RISING_FALLING,  // 双边沿触发
    GPIO_EXTI_MODE_NONE,            // 不使用 EXTI（普通 GPIO）
} GPIO_EXTI_MODE_e;
```

- 这个枚举在注册时传入，但当前框架并未在注册时自动配置 EXTI 触发方式
- EXTI 的触发方式仍需在 CubeMX 中配置，此枚举仅作标记用途

### GPIOInstance

```c
// basic_framework/bsp/gpio/bsp_gpio.h:22-33
typedef struct tmpgpio
{
    GPIO_TypeDef *GPIOx;                    // GPIO 端口（GPIOA, GPIOB, ...）
    GPIO_PinState pin_state;                // 引脚当前状态（Set/Reset）
    GPIO_EXTI_MODE_e exti_mode;             // EXTI 触发模式
    uint16_t GPIO_Pin;                      // 引脚号（GPIO_PIN_0, GPIO_PIN_1, ...）
    void (*gpio_model_callback)(struct tmpgpio *); // EXTI 中断回调
    void *id;                               // Parent Pointer
} GPIOInstance;
```

逐字段说明：

| 字段 | 作用 | 备注 |
|------|------|------|
| `GPIOx` | GPIO 端口 | 如 GPIOA、GPIOB，用于 HAL 函数调用 |
| `pin_state` | 引脚状态 | Set 或 Reset，不频繁使用 |
| `exti_mode` | EXTI 模式 | 标记用途，未在代码中实际使用 |
| `GPIO_Pin` | 引脚号 | 如 `GPIO_PIN_5`，EXTI 分发时用于匹配 |
| `gpio_model_callback` | EXTI 回调 | 中断触发时调用 |
| `id` | Parent Pointer | 指向拥有此 GPIO 实例的 Module 结构体 |

### GPIO_Init_Config_s

```c
// basic_framework/bsp/gpio/bsp_gpio.h:39-50
typedef struct
{
    GPIO_TypeDef *GPIOx;
    GPIO_PinState pin_state;
    GPIO_EXTI_MODE_e exti_mode;
    uint16_t GPIO_Pin;
    void (*gpio_model_callback)(GPIOInstance *);
    void *id;
} GPIO_Init_Config_s;
```

与 `GPIOInstance` 字段一一对应。

---

## 函数详解

### GPIORegister()

```c
// basic_framework/bsp/gpio/bsp_gpio.c:30-43
GPIOInstance *GPIORegister(GPIO_Init_Config_s *GPIO_config)
{
    GPIOInstance *ins = (GPIOInstance *)malloc(sizeof(GPIOInstance));
    memset(ins, 0, sizeof(GPIOInstance));

    ins->GPIOx = GPIO_config->GPIOx;
    ins->GPIO_Pin = GPIO_config->GPIO_Pin;
    ins->pin_state = GPIO_config->pin_state;
    ins->exti_mode = GPIO_config->exti_mode;
    ins->id = GPIO_config->id;
    ins->gpio_model_callback = GPIO_config->gpio_model_callback;
    gpio_instance[idx++] = ins;
    return ins;
}
```

- **功能**：注册一个 GPIO 实例，保存到静态数组中
- **没有硬件初始化** — GPIO 的初始化（时钟使能、模式配置等）由 CubeMX 完成，注册只是建立软件层面的管理
- **没有重复检测** — 与 CAN/USART 不同，GPIO 不检查重复注册

### GPIOToggel()

```c
// basic_framework/bsp/gpio/bsp_gpio.c:48-51
void GPIOToggel(GPIOInstance *_instance)
{
    HAL_GPIO_TogglePin(_instance->GPIOx, _instance->GPIO_Pin);
}
```

- **注意函数名的拼写**：`Toggel` 应为 `Toggle`，这是框架中的拼写错误，但为保持兼容未修改
- **功能**：翻转 GPIO 电平（高变低，低变高）

### GPIOSet()

```c
// basic_framework/bsp/gpio/bsp_gpio.c:53-56
void GPIOSet(GPIOInstance *_instance)
{
    HAL_GPIO_WritePin(_instance->GPIOx, _instance->GPIO_Pin, GPIO_PIN_SET);
}
```

- **功能**：将 GPIO 设为高电平

### GPIOReset()

```c
// basic_framework/bsp/gpio/bsp_gpio.c:58-61
void GPIOReset(GPIOInstance *_instance)
{
    HAL_GPIO_WritePin(_instance->GPIOx, _instance->GPIO_Pin, GPIO_PIN_RESET);
}
```

- **功能**：将 GPIO 设为低电平

### GPIORead()

```c
// basic_framework/bsp/gpio/bsp_gpio.c:63-66
GPIO_PinState GPIORead(GPIOInstance *_instance)
{
    return HAL_GPIO_ReadPin(_instance->GPIOx, _instance->GPIO_Pin);
}
```

- **功能**：读取 GPIO 当前电平
- **返回值**：`GPIO_PIN_SET`（高）或 `GPIO_PIN_RESET`（低）

### HAL_GPIO_EXTI_Callback()

```c
// basic_framework/bsp/gpio/bsp_gpio.c:15-28
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    GPIOInstance *gpio;
    for (size_t i = 0; i < idx; i++)
    {
        gpio = gpio_instance[i];
        if (gpio->GPIO_Pin == GPIO_Pin && gpio->gpio_model_callback != NULL)
        {
            gpio->gpio_model_callback(gpio);  // 调用 Module 回调
            return;                           // 找到就退出
        }
    }
}
```

- **重载 HAL 弱回调** — `HAL_GPIO_EXTI_Callback()` 在 HAL 中声明为 `__weak`
- **匹配方式** — 只按 `GPIO_Pin` 匹配，不检查 `GPIOx`
- **为什么不需要检查 GPIOx？** — STM32 的 EXTI 线与 Pin 号一一对应，同一 PinX 在同一时刻只能有一个端口连接到 EXTIX。硬件保证了唯一性。
- **return 退出** — 一个 Pin 号只对应一个实例，找到后直接退出
- **回调参数** — 传入 `GPIOInstance*`，Module 层可通过 `instance->id` 获取自己的结构体

---

## 调用链

### 输出控制路径

```
APP 层
  +-- Module 层调用 GPIOSet(instance)
        +-- HAL_GPIO_WritePin(instance->GPIOx, instance->GPIO_Pin, GPIO_PIN_SET)
              +-- 写 GPIO ODR 寄存器
                    +-- 引脚电平改变
```

### EXTI 中断路径

```
外部信号（如按键按下）
  +-- EXTI 中断触发
  +-- HAL 中断处理函数
  +-- HAL_GPIO_EXTI_Callback(GPIO_Pin)
        +-- 遍历 gpio_instance[] 匹配 GPIO_Pin
        +-- gpio_model_callback(gpio)  // 回调到 Module 层
              +-- 如激光开关、限位开关等处理
```

### 注册路径

```
Module 层
  +-- GPIORegister(&config)
        +-- malloc + memset
        +-- 复制配置到实例
        +-- gpio_instance[idx++] = ins
        +-- return ins
```

---

## 注意事项

1. **GPIO 初始化由 CubeMX 完成** — BSP 层的 GPIO 封装不负责引脚的硬件初始化（时钟使能、模式、上下拉等），这些在 CubeMX 中配置。
2. **EXTI 触发方式也由 CubeMX 配置** — `GPIO_EXTI_MODE_e` 枚举在注册时传入，但框架并未用它配置 EXTI 的触发方式。EXTI 触发方式在 CubeMX 中设置。
3. **函数名拼写错误** — `GPIOToggel()` 应为 `GPIOToggle()`，框架中保留了此拼写。
4. **EXTI 线的唯一性约束** — 由于 EXTI 分发只匹配 `GPIO_Pin`，不能在同一 Pin 号的不同端口上同时使用 EXTI。例如不能同时使用 PA0 和 PB0 作为 EXTI。
5. **没有重复注册检测** — 与 CAN/USART 不同，GPIO 的 `GPIORegister()` 不检查是否重复注册同一个引脚。
6. **回调在中断上下文** — `gpio_model_callback()` 在 EXTI 中断中调用，回调函数必须快速返回。
7. **pin_state 不自动更新** — `GPIOInstance` 的 `pin_state` 字段不会在 Set/Reset/Toggle 后自动更新，仅作为初始化时的记录。
