# C 板功率计 CAN 单向通信 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 NSEonLu 中新增可由宏开关控制的独立 `power_meter` 模块，让 C 板通过 CAN1 接收 `0x605` 功率计状态帧并提供一致的数据快照接口。

**Architecture:** 将固定 8 字节载荷的字节序和缩放解析放进无硬件依赖的纯 C 协议单元；设备模块拥有跃鹿框架的 `CANInstance` 与 `DaemonInstance`，在 CAN 回调中保存有效测量并维护在线状态；底盘应用只在启用宏为 1 时创建模块，不把测量数据接入控制闭环。

**Tech Stack:** C11、STM32F407 HAL、FreeRTOS/CMSIS-RTOS、跃鹿 `bsp_can`/`daemon`、经典 CAN 1 Mbps、MinGW GCC 主机测试、ARM GNU Make 固件构建。

## Global Constraints

- 以 `docs/superpowers/specs/2026-09-14-power-meter-can-integration-design.md` 为功能规格。
- 功率计连接 C 板 CAN1；标准帧 ID 固定为 `0x605`，DLC 固定为 8，波特率固定为 1 Mbps。
- 四个字段均为大端 `int16_t`：分流电压、总线电压、电流除以 1000，功率除以 100。
- 通信仅允许功率计到 C 板的单向状态上报；不得新增 C 板发送命令。
- `POWER_METER_ENABLE` 默认必须为 `1`，只允许 `0` 或 `1`。
- `POWER_METER_ENABLE=0` 时不得创建功率计实例、daemon 或 CAN 过滤器。
- 离线窗口固定为约 100 ms，即 100 Hz daemon 的 10 个计数；模块保留 daemon 注册/重载，并以最后有效帧的无符号 tick 差值作为权威在线判据。
- 测量数据仅保存并提供读取接口，不参与底盘功率控制和超级电容控制。
- 不修改 `Src/main.c`、CubeMX `.ioc`、生成的 CAN 初始化代码或功率计固件。
- 保留用户当前对 `Src/usart.c`、`application/chassis/chassis.c`、`basic_framework.ioc`、`modules/referee/rm_referee.c` 的未提交改动。
- 每次提交只暂存本任务明确列出的文件；`application/chassis/chassis.c` 必须按 hunk 暂存，禁止把用户已有的 USART1 裁判系统修改带入任务提交；禁止使用 `git add .`。

## File Structure

- `modules/power_meter/power_meter_protocol.h/.c`：定义 CAN ID、DLC、测量结构和纯 C 大端解码函数。
- `modules/power_meter/power_meter.h/.c`：封装 CAN 注册、接收回调、daemon、诊断计数和线程安全 getter。
- `tests/power_meter_protocol/test_power_meter_protocol.c`：验证协议字节序、符号、缩放和拒绝行为。
- `tests/power_meter_protocol/Makefile`：构建并运行协议主机测试。
- `tests/power_meter/test_power_meter.c`：用真实模块代码和最小 BSP/daemon 桩验证模块行为。
- `tests/power_meter/stubs/*.h`：提供模块主机测试所需的 HAL、CAN、daemon 与临界区接口。
- `tests/power_meter/Makefile`：构建并运行模块主机测试。
- `application/robot_def.h`：新增并校验 `POWER_METER_ENABLE`。
- `application/chassis/chassis.c`：在宏开启时于 `ChassisInit()` 创建 CAN1 功率计实例。
- `Makefile`：显式加入新增固件源文件和 include 路径；`CMakeLists.txt` 的递归规则无需修改。

---

### Task 1: 用主机测试驱动功率计协议解码器

**Files:**
- Create: `modules/power_meter/power_meter_protocol.h`
- Create: `modules/power_meter/power_meter_protocol.c`
- Create: `tests/power_meter_protocol/test_power_meter_protocol.c`
- Create: `tests/power_meter_protocol/Makefile`

**Interfaces:**
- Consumes: `const uint8_t *frame`、`uint8_t dlc`。
- Produces: `PowerMeterMeasurement_s` 和 `bool PowerMeterProtocolDecodeStatus(const uint8_t *frame, uint8_t dlc, PowerMeterMeasurement_s *measurement)`。

- [ ] **Step 1: 写协议解码失败测试**

创建 `tests/power_meter_protocol/test_power_meter_protocol.c`：

```c
#include "power_meter_protocol.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

static int failures;

#define ASSERT_TRUE(value) \
    do { if (!(value)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #value); ++failures; } } while (0)
#define ASSERT_FALSE(value) ASSERT_TRUE(!(value))
#define ASSERT_NEAR(expected, actual, tolerance) \
    do { \
        const float expected_ = (expected); \
        const float actual_ = (actual); \
        if (fabsf(expected_ - actual_) > (tolerance)) { \
            printf("FAIL %s:%d: expected %.6f, got %.6f\n", \
                   __FILE__, __LINE__, (double)expected_, (double)actual_); \
            ++failures; \
        } \
    } while (0)

static void TestDecodesKnownPositiveFrame(void)
{
    const uint8_t frame[8] = {0x00U, 0x0CU, 0x5FU, 0xB4U,
                              0x0CU, 0xB2U, 0x1FU, 0x1BU};
    PowerMeterMeasurement_s value = {0};

    ASSERT_TRUE(PowerMeterProtocolDecodeStatus(frame, 8U, &value));
    ASSERT_NEAR(0.012f, value.shunt_voltage_v, 0.00001f);
    ASSERT_NEAR(24.5f, value.bus_voltage_v, 0.00001f);
    ASSERT_NEAR(3.25f, value.current_a, 0.00001f);
    ASSERT_NEAR(79.63f, value.power_w, 0.00001f);
}

static void TestPreservesSignedValues(void)
{
    const uint8_t frame[8] = {0xFFU, 0xF4U, 0x12U, 0x34U,
                              0xFBU, 0x1EU, 0xF6U, 0x3CU};
    PowerMeterMeasurement_s value = {0};

    ASSERT_TRUE(PowerMeterProtocolDecodeStatus(frame, 8U, &value));
    ASSERT_NEAR(-0.012f, value.shunt_voltage_v, 0.00001f);
    ASSERT_NEAR(4.66f, value.bus_voltage_v, 0.00001f);
    ASSERT_NEAR(-1.25f, value.current_a, 0.00001f);
    ASSERT_NEAR(-25.0f, value.power_w, 0.00001f);
}

static void TestDecodesSignedBoundaries(void)
{
    const uint8_t frame[8] = {0x7FU, 0xFFU, 0x80U, 0x00U,
                              0x7FU, 0xFFU, 0x80U, 0x00U};
    PowerMeterMeasurement_s value = {0};

    ASSERT_TRUE(PowerMeterProtocolDecodeStatus(frame, 8U, &value));
    ASSERT_NEAR(32.767f, value.shunt_voltage_v, 0.00001f);
    ASSERT_NEAR(-32.768f, value.bus_voltage_v, 0.00001f);
    ASSERT_NEAR(32.767f, value.current_a, 0.00001f);
    ASSERT_NEAR(-327.68f, value.power_w, 0.0001f);
}

static void TestRejectsInvalidInputWithoutOverwritingOutput(void)
{
    const uint8_t frame[8] = {0};
    PowerMeterMeasurement_s value = {
        .shunt_voltage_v = 1.0f,
        .bus_voltage_v = 2.0f,
        .current_a = 3.0f,
        .power_w = 4.0f,
    };

    ASSERT_FALSE(PowerMeterProtocolDecodeStatus(frame, 7U, &value));
    ASSERT_NEAR(1.0f, value.shunt_voltage_v, 0.0f);
    ASSERT_NEAR(4.0f, value.power_w, 0.0f);
    ASSERT_FALSE(PowerMeterProtocolDecodeStatus(NULL, 8U, &value));
    ASSERT_FALSE(PowerMeterProtocolDecodeStatus(frame, 8U, NULL));
}

int main(void)
{
    TestDecodesKnownPositiveFrame();
    TestPreservesSignedValues();
    TestDecodesSignedBoundaries();
    TestRejectsInvalidInputWithoutOverwritingOutput();

    if (failures != 0)
        return 1;
    puts("PASS: power meter protocol tests");
    return 0;
}
```

创建测试 Makefile：

```make
CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_power_meter_protocol.exe
SOURCE := test_power_meter_protocol.c ../../modules/power_meter/power_meter_protocol.c
INCLUDES := -I../../modules/power_meter

.PHONY: all test clean
all: $(TARGET)
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -lm -o $(TARGET)
test: $(TARGET)
	./$(TARGET)
clean:
	-if exist $(TARGET) del /Q $(TARGET)
```

- [ ] **Step 2: 运行测试并确认 RED**

Run: `mingw32-make -C tests/power_meter_protocol clean test`

Expected: 编译失败，提示缺少 `power_meter_protocol.h` 或 `PowerMeterProtocolDecodeStatus`。

- [ ] **Step 3: 声明最小协议接口**

创建 `modules/power_meter/power_meter_protocol.h`：

```c
#ifndef POWER_METER_PROTOCOL_H
#define POWER_METER_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#define POWER_METER_STATUS_CAN_ID 0x605U
#define POWER_METER_CAN_DLC 8U

typedef struct
{
    float shunt_voltage_v;
    float bus_voltage_v;
    float current_a;
    float power_w;
} PowerMeterMeasurement_s;

bool PowerMeterProtocolDecodeStatus(const uint8_t *frame,
                                    uint8_t dlc,
                                    PowerMeterMeasurement_s *measurement);

#endif
```

- [ ] **Step 4: 实现最小大端有符号解码器**

创建 `modules/power_meter/power_meter_protocol.c`：先把两个字节组合成 `uint16_t`，再在 bit15 置位时从 `int32_t` 中减去 65536，避免依赖超范围无符号到有符号转换。只有全部输入校验成功后才写输出：

```c
#include "power_meter_protocol.h"

#include <stddef.h>

static int32_t ReadI16BE(const uint8_t *bytes)
{
    int32_t value = (int32_t)(((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1]);
    if (value >= 0x8000)
        value -= 0x10000;
    return value;
}

bool PowerMeterProtocolDecodeStatus(const uint8_t *frame,
                                    uint8_t dlc,
                                    PowerMeterMeasurement_s *measurement)
{
    if (frame == NULL || measurement == NULL || dlc != POWER_METER_CAN_DLC)
        return false;

    const PowerMeterMeasurement_s decoded = {
        .shunt_voltage_v = (float)ReadI16BE(&frame[0]) / 1000.0f,
        .bus_voltage_v = (float)ReadI16BE(&frame[2]) / 1000.0f,
        .current_a = (float)ReadI16BE(&frame[4]) / 1000.0f,
        .power_w = (float)ReadI16BE(&frame[6]) / 100.0f,
    };
    *measurement = decoded;
    return true;
}
```

- [ ] **Step 5: 运行测试并确认 GREEN**

Run: `mingw32-make -C tests/power_meter_protocol clean test`

Expected: `PASS: power meter protocol tests`，无编译警告。

- [ ] **Step 6: 提交协议单元**

```powershell
git add -- modules/power_meter/power_meter_protocol.h modules/power_meter/power_meter_protocol.c tests/power_meter_protocol/test_power_meter_protocol.c tests/power_meter_protocol/Makefile
git commit -m "feat: decode power meter CAN status"
```

---

### Task 2: 用桩测试驱动跃鹿功率计设备模块

**Files:**
- Create: `modules/power_meter/power_meter.h`
- Create: `modules/power_meter/power_meter.c`
- Create: `tests/power_meter/test_power_meter.c`
- Create: `tests/power_meter/stubs/bsp_can.h`
- Create: `tests/power_meter/stubs/daemon.h`
- Create: `tests/power_meter/stubs/FreeRTOS.h`
- Create: `tests/power_meter/stubs/task.h`
- Create: `tests/power_meter/Makefile`

**Interfaces:**
- Consumes: `CANRegister()`、`CANSetDLC()`、`DaemonRegister()`、`DaemonReload()`、`DaemonIsOnline()`、`HAL_GetTick()` 和 Task 临界区宏。
- Produces: `PowerMeterInit()` 与 `PowerMeterGetStatus()`；实例诊断字段 `received_frame_count`、`rejected_frame_count`、`last_rx_tick_ms`。

- [ ] **Step 1: 创建最小主机桩头文件**

`tests/power_meter/stubs/bsp_can.h` 定义仅供测试使用的 `CAN_HandleTypeDef`、`CANInstance`、`CAN_Init_Config_s`，字段名与生产 BSP 保持一致，并声明 `CANRegister()`、`CANSetDLC()`、`HAL_GetTick()`。`CANInstance` 至少包含 `can_handle`、`tx_id`、`rx_id`、`tx_buff[8]`、`rx_buff[8]`、`rx_len`、回调和 `id`。

```c
#ifndef TEST_BSP_CAN_H
#define TEST_BSP_CAN_H

#include <stdint.h>

typedef struct
{
    uint32_t marker;
} CAN_HandleTypeDef;

typedef struct CANInstance CANInstance;
struct CANInstance
{
    CAN_HandleTypeDef *can_handle;
    uint32_t tx_id;
    uint32_t rx_id;
    uint8_t tx_buff[8];
    uint8_t rx_buff[8];
    uint8_t rx_len;
    void (*can_module_callback)(CANInstance *);
    void *id;
};

typedef struct
{
    CAN_HandleTypeDef *can_handle;
    uint32_t tx_id;
    uint32_t rx_id;
    void (*can_module_callback)(CANInstance *);
    void *id;
} CAN_Init_Config_s;

CANInstance *CANRegister(CAN_Init_Config_s *config);
void CANSetDLC(CANInstance *instance, uint8_t length);
uint32_t HAL_GetTick(void);

#endif
```

`tests/power_meter/stubs/daemon.h` 定义与生产代码同名的 `offline_callback`、`DaemonInstance`、`Daemon_Init_Config_s`，并声明三个 daemon 函数。

```c
#ifndef TEST_DAEMON_H
#define TEST_DAEMON_H

#include <stdint.h>

typedef void (*offline_callback)(void *);

typedef struct
{
    uint16_t reload_count;
    offline_callback callback;
    uint16_t temp_count;
    void *owner_id;
} DaemonInstance;

typedef struct
{
    uint16_t reload_count;
    uint16_t init_count;
    offline_callback callback;
    void *owner_id;
} Daemon_Init_Config_s;

DaemonInstance *DaemonRegister(Daemon_Init_Config_s *config);
void DaemonReload(DaemonInstance *instance);
uint8_t DaemonIsOnline(DaemonInstance *instance);

#endif
```

`tests/power_meter/stubs/FreeRTOS.h` 只使用 include guard：

```c
#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H
#endif
```

`tests/power_meter/stubs/task.h` 定义：

```c
#ifndef TEST_TASK_H
#define TEST_TASK_H
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#endif
```

- [ ] **Step 2: 写模块行为的失败测试**

在 `tests/power_meter/test_power_meter.c` 内实现捕获式桩：`CANRegister()` 保存配置并返回一个静态 CAN 实例；`DaemonRegister()` 保存回调、owner 和计数并返回静态 daemon；`DaemonReload()` 恢复计数并增加测试侧 reload 次数；`DaemonIsOnline()` 返回 `temp_count > 0`；`HAL_GetTick()` 返回测试侧可设置的毫秒值。

测试文件开头和辅助实现使用以下实际内容：

```c
#include "power_meter.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;
static CAN_Init_Config_s captured_can_config;
static Daemon_Init_Config_s captured_daemon_config;
static CANInstance fake_can_instance;
static DaemonInstance fake_daemon_instance;
static uint8_t captured_dlc;
static uint32_t fake_tick_ms;
static uint32_t daemon_reload_calls;

#define ASSERT_TRUE(value) \
    do { if (!(value)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #value); ++failures; } } while (0)
#define ASSERT_FALSE(value) ASSERT_TRUE(!(value))
#define ASSERT_EQ_U32(expected, actual) \
    do { if ((uint32_t)(expected) != (uint32_t)(actual)) { ++failures; } } while (0)
#define ASSERT_NEAR(expected, actual, tolerance) \
    do { if (fabsf((expected) - (actual)) > (tolerance)) { ++failures; } } while (0)

CANInstance *CANRegister(CAN_Init_Config_s *config)
{
    captured_can_config = *config;
    memset(&fake_can_instance, 0, sizeof(fake_can_instance));
    fake_can_instance.can_handle = config->can_handle;
    fake_can_instance.tx_id = config->tx_id;
    fake_can_instance.rx_id = config->rx_id;
    fake_can_instance.can_module_callback = config->can_module_callback;
    fake_can_instance.id = config->id;
    return &fake_can_instance;
}

void CANSetDLC(CANInstance *instance, uint8_t length)
{
    (void)instance;
    captured_dlc = length;
}

DaemonInstance *DaemonRegister(Daemon_Init_Config_s *config)
{
    captured_daemon_config = *config;
    fake_daemon_instance.reload_count = config->reload_count;
    fake_daemon_instance.temp_count = config->init_count;
    fake_daemon_instance.callback = config->callback;
    fake_daemon_instance.owner_id = config->owner_id;
    return &fake_daemon_instance;
}

void DaemonReload(DaemonInstance *instance)
{
    instance->temp_count = instance->reload_count;
    ++daemon_reload_calls;
}

uint8_t DaemonIsOnline(DaemonInstance *instance)
{
    return instance->temp_count > 0U;
}

uint32_t HAL_GetTick(void)
{
    return fake_tick_ms;
}

static void ResetFakes(void)
{
    memset(&captured_can_config, 0, sizeof(captured_can_config));
    memset(&captured_daemon_config, 0, sizeof(captured_daemon_config));
    memset(&fake_can_instance, 0, sizeof(fake_can_instance));
    memset(&fake_daemon_instance, 0, sizeof(fake_daemon_instance));
    captured_dlc = 0U;
    fake_tick_ms = 0U;
    daemon_reload_calls = 0U;
}

static PowerMeterInstance *InitMeter(void)
{
    static CAN_HandleTypeDef can1;
    ResetFakes();
    const PowerMeter_Init_Config_s config = {
        .can_config = {
            .can_handle = &can1,
            .tx_id = POWER_METER_STATUS_CAN_ID,
            .rx_id = POWER_METER_STATUS_CAN_ID,
        },
        .offline_reload_count = 10U,
    };
    return PowerMeterInit(&config);
}

static void DeliverFrame(const uint8_t frame[8], uint8_t dlc)
{
    memcpy(fake_can_instance.rx_buff, frame, sizeof(fake_can_instance.rx_buff));
    fake_can_instance.rx_len = dlc;
    fake_can_instance.can_module_callback(&fake_can_instance);
}
```

加入以下独立行为测试：

```c
static void TestInitRegistersFrameworkInstances(void)
{
    ResetFakes();
    CAN_HandleTypeDef can1 = {0};
    const PowerMeter_Init_Config_s config = {
        .can_config = {
            .can_handle = &can1,
            .tx_id = POWER_METER_STATUS_CAN_ID,
            .rx_id = POWER_METER_STATUS_CAN_ID,
        },
        .offline_reload_count = 10U,
    };

    PowerMeterInstance *meter = PowerMeterInit(&config);
    ASSERT_TRUE(meter != NULL);
    ASSERT_TRUE(captured_can_config.can_handle == &can1);
    ASSERT_EQ_U32(POWER_METER_STATUS_CAN_ID, captured_can_config.rx_id);
    ASSERT_TRUE(captured_can_config.can_module_callback != NULL);
    ASSERT_TRUE(captured_can_config.id == meter);
    ASSERT_EQ_U32(10U, captured_daemon_config.reload_count);
    ASSERT_TRUE(captured_daemon_config.owner_id == meter);
    ASSERT_EQ_U32(POWER_METER_CAN_DLC, captured_dlc);
}

static void TestStartsOfflineAndRejectsNullArguments(void)
{
    PowerMeterStatus_s status = {0};
    PowerMeterInstance *meter = InitMeter();

    ASSERT_TRUE(PowerMeterGetStatus(meter, &status));
    ASSERT_FALSE(status.online);
    ASSERT_FALSE(PowerMeterGetStatus(NULL, &status));
    ASSERT_FALSE(PowerMeterGetStatus(meter, NULL));
    ASSERT_TRUE(PowerMeterInit(NULL) == NULL);
}

static void TestValidFrameUpdatesSnapshotAndDiagnostics(void)
{
    static const uint8_t frame[8] = {0x00U, 0x0CU, 0x5FU, 0xB4U,
                                     0x0CU, 0xB2U, 0x1FU, 0x1BU};
    PowerMeterStatus_s status = {0};
    PowerMeterInstance *meter = InitMeter();
    fake_tick_ms = 1234U;

    DeliverFrame(frame, 8U);

    ASSERT_TRUE(PowerMeterGetStatus(meter, &status));
    ASSERT_TRUE(status.online);
    ASSERT_NEAR(24.5f, status.bus_voltage_v, 0.00001f);
    ASSERT_NEAR(3.25f, status.current_a, 0.00001f);
    ASSERT_NEAR(79.63f, status.power_w, 0.00001f);
    ASSERT_EQ_U32(1U, meter->received_frame_count);
    ASSERT_EQ_U32(0U, meter->rejected_frame_count);
    ASSERT_EQ_U32(1234U, meter->last_rx_tick_ms);
    ASSERT_EQ_U32(1U, daemon_reload_calls);
}

static void TestBadLengthDoesNotOverwriteOrFeedDaemon(void)
{
    static const uint8_t valid[8] = {0, 1, 0, 2, 0, 3, 0, 4};
    static const uint8_t invalid[8] = {0, 9, 0, 9, 0, 9, 0, 9};
    PowerMeterStatus_s status = {0};
    PowerMeterInstance *meter = InitMeter();
    DeliverFrame(valid, 8U);
    const uint32_t reloads_before = daemon_reload_calls;

    DeliverFrame(invalid, 7U);

    ASSERT_TRUE(PowerMeterGetStatus(meter, &status));
    ASSERT_NEAR(0.002f, status.bus_voltage_v, 0.00001f);
    ASSERT_EQ_U32(1U, meter->received_frame_count);
    ASSERT_EQ_U32(1U, meter->rejected_frame_count);
    ASSERT_EQ_U32(reloads_before, daemon_reload_calls);
}

static void TestOfflineCallbackClearsOnlyOnlineState(void)
{
    static const uint8_t frame[8] = {0, 1, 0, 2, 0, 3, 0, 4};
    PowerMeterStatus_s status = {0};
    PowerMeterInstance *meter = InitMeter();
    DeliverFrame(frame, 8U);

    captured_daemon_config.callback(captured_daemon_config.owner_id);

    ASSERT_TRUE(PowerMeterGetStatus(meter, &status));
    ASSERT_FALSE(status.online);
    ASSERT_NEAR(0.04f, status.power_w, 0.00001f);
}
```

测试辅助函数 `InitMeter()` 必须调用 `ResetFakes()` 后使用 `offline_reload_count = 10U` 初始化；`DeliverFrame()` 把给定字节和 DLC 写入静态 CAN 实例，再调用捕获到的模块回调。测试还要覆盖 `offline_reload_count = 0U` 时采用默认值 10。`main()` 全部通过时打印 `PASS: power meter module tests`。

默认离线计数测试和 `main()` 使用：

```c
static void TestZeroReloadUsesDefault(void)
{
    ResetFakes();
    CAN_HandleTypeDef can1 = {0};
    const PowerMeter_Init_Config_s config = {
        .can_config = {
            .can_handle = &can1,
            .tx_id = POWER_METER_STATUS_CAN_ID,
            .rx_id = POWER_METER_STATUS_CAN_ID,
        },
        .offline_reload_count = 0U,
    };

    ASSERT_TRUE(PowerMeterInit(&config) != NULL);
    ASSERT_EQ_U32(10U, captured_daemon_config.reload_count);
    ASSERT_EQ_U32(10U, captured_daemon_config.init_count);
}

int main(void)
{
    TestInitRegistersFrameworkInstances();
    TestStartsOfflineAndRejectsNullArguments();
    TestValidFrameUpdatesSnapshotAndDiagnostics();
    TestBadLengthDoesNotOverwriteOrFeedDaemon();
    TestOfflineCallbackClearsOnlyOnlineState();
    TestZeroReloadUsesDefault();

    if (failures != 0)
        return 1;
    puts("PASS: power meter module tests");
    return 0;
}
```

测试 Makefile：

```make
CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_power_meter.exe
SOURCE := test_power_meter.c \
          ../../modules/power_meter/power_meter.c \
          ../../modules/power_meter/power_meter_protocol.c
INCLUDES := -Istubs -I../../modules/power_meter

.PHONY: all test clean
all: $(TARGET)
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -lm -o $(TARGET)
test: $(TARGET)
	./$(TARGET)
clean:
	-if exist $(TARGET) del /Q $(TARGET)
```

- [ ] **Step 3: 运行模块测试并确认 RED**

Run: `mingw32-make -C tests/power_meter clean test`

Expected: 编译失败，提示缺少 `power_meter.h` 或 `PowerMeterInit`/`PowerMeterGetStatus`。

- [ ] **Step 4: 声明模块公共类型和接口**

`modules/power_meter/power_meter.h`：

```c
#ifndef POWER_METER_H
#define POWER_METER_H

#include "bsp_can.h"
#include "daemon.h"
#include "power_meter_protocol.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    bool online;
    float shunt_voltage_v;
    float bus_voltage_v;
    float current_a;
    float power_w;
} PowerMeterStatus_s;

typedef struct
{
    CANInstance *can_ins;
    DaemonInstance *daemon;
    PowerMeterStatus_s status;
    volatile uint32_t received_frame_count;
    volatile uint32_t rejected_frame_count;
    volatile uint32_t last_rx_tick_ms;
} PowerMeterInstance;

typedef struct
{
    CAN_Init_Config_s can_config;
    uint16_t offline_reload_count;
} PowerMeter_Init_Config_s;

PowerMeterInstance *PowerMeterInit(const PowerMeter_Init_Config_s *config);
bool PowerMeterGetStatus(PowerMeterInstance *instance,
                         PowerMeterStatus_s *status);

#endif
```

- [ ] **Step 5: 实现最小跃鹿实例封装**

`modules/power_meter/power_meter.c` 按现有 `super_cap.c` 风格实现：

```c
#include "power_meter.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdlib.h>
#include <string.h>

#define POWER_METER_DEFAULT_OFFLINE_RELOAD_COUNT 10U

static void PowerMeterOffline(void *owner)
{
    PowerMeterInstance *instance = (PowerMeterInstance *)owner;
    if (instance == NULL)
        return;

    taskENTER_CRITICAL();
    instance->status.online = false;
    taskEXIT_CRITICAL();
}

static void PowerMeterRxCallback(CANInstance *can_instance)
{
    if (can_instance == NULL)
        return;

    PowerMeterInstance *instance = (PowerMeterInstance *)can_instance->id;
    if (instance == NULL)
        return;

    PowerMeterMeasurement_s decoded;
    if (!PowerMeterProtocolDecodeStatus(can_instance->rx_buff,
                                        can_instance->rx_len,
                                        &decoded))
    {
        instance->rejected_frame_count++;
        return;
    }

    instance->status.online = true;
    instance->status.shunt_voltage_v = decoded.shunt_voltage_v;
    instance->status.bus_voltage_v = decoded.bus_voltage_v;
    instance->status.current_a = decoded.current_a;
    instance->status.power_w = decoded.power_w;
    instance->received_frame_count++;
    instance->last_rx_tick_ms = HAL_GetTick();
    DaemonReload(instance->daemon);
}

PowerMeterInstance *PowerMeterInit(const PowerMeter_Init_Config_s *config)
{
    if (config == NULL)
        return NULL;

    PowerMeterInstance *instance =
        (PowerMeterInstance *)malloc(sizeof(PowerMeterInstance));
    if (instance == NULL)
        return NULL;
    memset(instance, 0, sizeof(*instance));

    const uint16_t reload_count =
        config->offline_reload_count == 0U
            ? POWER_METER_DEFAULT_OFFLINE_RELOAD_COUNT
            : config->offline_reload_count;
    Daemon_Init_Config_s daemon_config = {
        .reload_count = reload_count,
        .init_count = reload_count,
        .callback = PowerMeterOffline,
        .owner_id = instance,
    };
    instance->daemon = DaemonRegister(&daemon_config);
    if (instance->daemon == NULL)
    {
        free(instance);
        return NULL;
    }

    CAN_Init_Config_s can_config = config->can_config;
    can_config.can_module_callback = PowerMeterRxCallback;
    can_config.id = instance;
    instance->can_ins = CANRegister(&can_config);
    if (instance->can_ins == NULL)
        return NULL;

    CANSetDLC(instance->can_ins, POWER_METER_CAN_DLC);
    return instance;
}

bool PowerMeterGetStatus(PowerMeterInstance *instance,
                         PowerMeterStatus_s *status)
{
    if (instance == NULL || status == NULL || instance->daemon == NULL)
        return false;

    taskENTER_CRITICAL();
    *status = instance->status;
    status->online =
        status->online && DaemonIsOnline(instance->daemon) != 0U;
    taskEXIT_CRITICAL();
    return true;
}
```

若 `DaemonRegister()` 失败，释放刚分配的实例并返回 NULL；若 `CANRegister()` 失败，返回 NULL，行为与仓库现有模块保持一致，不引入本任务之外的 daemon 注销机制。

- [ ] **Step 6: 运行协议与模块测试并确认 GREEN**

Run:

```powershell
mingw32-make -C tests/power_meter_protocol clean test
mingw32-make -C tests/power_meter clean test
```

Expected: 两个目标分别打印 `PASS: power meter protocol tests` 和 `PASS: power meter module tests`。

- [ ] **Step 7: 提交功率计模块**

```powershell
git add -- modules/power_meter/power_meter.h modules/power_meter/power_meter.c tests/power_meter/test_power_meter.c tests/power_meter/stubs/bsp_can.h tests/power_meter/stubs/daemon.h tests/power_meter/stubs/FreeRTOS.h tests/power_meter/stubs/task.h tests/power_meter/Makefile
git commit -m "feat: add YueLu power meter module"
```

---

### Task 3: 增加启用宏并接入底盘初始化

**Files:**
- Modify: `application/robot_def.h:24-40`
- Modify: `application/chassis/chassis.c:19-53, 159-170`
- Modify: `Makefile:152-155, 228-240, 271-274`

**Interfaces:**
- Consumes: `POWER_METER_ENABLE`、`PowerMeterInit()`、`POWER_METER_STATUS_CAN_ID`、`hcan1`。
- Produces: 默认开启、可编译关闭的 C 板 CAN1 功率计实例。

- [ ] **Step 1: 在 `robot_def.h` 增加严格布尔宏**

在底盘台架功率宏之后加入：

```c
/* 功率计 CAN1 状态接收；设为 0 时不创建实例或注册 CAN 过滤器。 */
#ifndef POWER_METER_ENABLE
#define POWER_METER_ENABLE 1
#endif

#if POWER_METER_ENABLE != 0 && POWER_METER_ENABLE != 1
#error POWER_METER_ENABLE must be 0 or 1.
#endif
```

- [ ] **Step 2: 在 `chassis.c` 按宏接入模块**

在 `#include "robot_def.h"` 后加入受宏保护的 `#include "power_meter.h"`；在 `static SuperCapInstance *cap` 后加入：

```c
#if POWER_METER_ENABLE
static PowerMeterInstance *power_meter;
#endif
```

在 `cap = SuperCapInit(&cap_conf);` 后加入：

```c
#if POWER_METER_ENABLE
    const PowerMeter_Init_Config_s power_meter_config = {
        .can_config = {
            .can_handle = &hcan1,
            .tx_id = POWER_METER_STATUS_CAN_ID,
            .rx_id = POWER_METER_STATUS_CAN_ID,
        },
        .offline_reload_count = 10U,
    };
    power_meter = PowerMeterInit(&power_meter_config);
#endif
```

`tx_id` 仅用于满足当前 `CAN_Init_Config_s` 和 BSP 的 FIFO 选择；模块不暴露发送接口，也不调用 `CANTransmit()`。

- [ ] **Step 3: 更新 Makefile 显式列表**

在超级电容源文件之后加入：

```make
modules/power_meter/power_meter_protocol.c \
modules/power_meter/power_meter.c \
```

在 `-Imodules/super_cap` 后加入：

```make
-Imodules/power_meter \
```

在 `C_DEFS` 定义结束后加入可选的命令行覆盖桥接；默认不传变量时仍使用 `robot_def.h` 中的值：

```make
ifneq ($(strip $(POWER_METER_ENABLE)),)
C_DEFS += -DPOWER_METER_ENABLE=$(POWER_METER_ENABLE)
endif
```

- [ ] **Step 4: 验证默认开启的完整固件构建**

Run: `mingw32-make -j24 BUILD_DIR=build/power_meter_on`

Expected: 生成 `build/power_meter_on/basic_framework.elf`、`.hex` 和 `.bin`，没有功率计相关 warning/error。

- [ ] **Step 5: 验证关闭宏的编译路径**

使用 Makefile 的独立构建子目录和命令行覆盖，不修改 `robot_def.h`，也不复用默认开启版本的目标文件：

```powershell
mingw32-make -j24 BUILD_DIR=build/power_meter_off POWER_METER_ENABLE=0
```

Expected: `POWER_METER_ENABLE=0` 的固件完成链接；`chassis.c` 不引用静态 `power_meter` 实例，也不执行 `PowerMeterInit()`。两个验证目录都位于已忽略的 `build/` 内，不暂存。

- [ ] **Step 6: 只暂存本任务的底盘 hunk 并核对边界**

```powershell
git add -- application/robot_def.h Makefile
git add -p -- application/chassis/chassis.c
git diff --cached --check
git diff --cached
git diff -- application/chassis/chassis.c
```

在交互暂存中只选择功率计 include、静态指针和初始化三个 hunk，拒绝用户已有的 `UITaskInit(&huart1, ...)` hunk。缓存区 diff 必须只包含功率计接入；工作区 diff 必须继续保留用户原有 USART1 修改。

- [ ] **Step 7: 提交宏和应用接线**

```powershell
git commit -m "feat: register optional CAN1 power meter"
```

---

### Task 4: 完整回归、自审与代码审查

**Files:**
- Verify only; no planned production changes.

**Interfaces:**
- Consumes: Tasks 1-3 的最终提交和设计规格。
- Produces: 可复现的主机测试、双宏配置构建结果和上板检查清单。

- [ ] **Step 1: 运行全部相关主机测试**

Run:

```powershell
mingw32-make -C tests/power_meter_protocol clean test
mingw32-make -C tests/power_meter clean test
mingw32-make -C tests/super_cap_protocol clean test
mingw32-make -C tests/chassis_power_budget clean test
mingw32-make -C tests/power_model clean test
mingw32-make -C tests/chassis_power_feedback clean test
```

Expected: 六个测试目标均打印 PASS，且无 warning。

- [ ] **Step 2: 再次构建默认固件**

Run: `mingw32-make -j24 BUILD_DIR=build/power_meter_on`

Expected: ARM 固件成功链接，无新增 warning/error。

- [ ] **Step 3: 检查提交与用户改动隔离**

Run:

```powershell
git status --short
git diff --check
git log -5 --oneline
git show --stat --oneline HEAD
```

Expected: 任务提交只包含计划列出的功率计文件、宏、Makefile 和功率计相关 `chassis.c` hunk；用户原有 `Src/usart.c`、`basic_framework.ioc`、`modules/referee/rm_referee.c` 和 `chassis.c` 的 USART1 修改仍保持未提交。

- [ ] **Step 4: 按设计规格进行代码审查**

审查以下硬性问题：

- 协议解码失败是否保持原快照不变；
- 大端有符号边界是否不依赖实现定义行为；
- 错误 DLC 是否不会喂 daemon；
- 初始化前是否保持 offline；
- getter 是否在临界区内复制一致快照；
- getter 与离线回调是否按最后有效帧时间戳判断 100 ms 超时，并正确处理 tick 回绕和 daemon 重载/递减竞态；
- 宏关闭是否完全跳过实例和 CAN 注册；
- 是否没有发送路径、控制闭环改动或高频日志；
- 是否没有覆盖用户已有未提交修改。

发现 Critical 或 Important 问题时，先新增能复现问题的失败测试，再进行最小修复并重跑 Task 4 Step 1-3。

- [ ] **Step 5: 上板联调**

1. 功率计与 C 板 CAN1 共地，并确认总线两端终端电阻配置正确；
2. 观察 `power_meter->received_frame_count` 持续增加，`rejected_frame_count` 保持为 0；
3. 通过 `PowerMeterGetStatus()` 或调试器检查 V、A、W 与功率计 VOFA 输出一致；
4. 断开功率计，约 100 ms 后确认 `status.online == false`，测量值保留最后一次有效值；
5. 恢复连接，确认第一帧有效 `0x605` 后 `status.online == true`；
6. 编译 `POWER_METER_ENABLE=0` 的版本，确认不再注册 `0x605` 且底盘、超级电容行为不变。
