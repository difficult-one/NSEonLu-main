# 裁判系统—C 板—超级电容链路 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 C 板可靠解析 2026 裁判系统功率数据，并在关键数据有效时沿现有 CAN 协议下发给超级电容、失效或超时时安全关闭 DCDC。

**Architecture:** 将任意 UART 数据块先送入无硬件依赖的字节流解析器，再把有效 `0x0201`/`0x0202` 帧送入独立功率状态机。底盘任务通过原子快照读取裁判功率，并调用纯策略函数同时生成超级电容指令和基础功率预算；HAL/FreeRTOS 文件只负责输入输出接线。

**Tech Stack:** C11、STM32F407 HAL、FreeRTOS/CMSIS-RTOS、经典 CAN、GNU Make、MinGW GCC 主机测试、ARM GNU Toolchain 13.3。

## Global Constraints

- 以 `docs/superpowers/specs/2026-09-10-referee-c-board-supercap-link-design.md` 为唯一功能规格。
- `0x0201` 数据段固定为 13 字节，`0x0202` 数据段固定为 14 字节。
- 裁判功率关键数据超时固定为 300 ms。
- 不修改超级电容 CAN ID：C 板发送 `0x061`，接收 `0x051`，DLC 均为 8。
- 不修改现有超级电容 CAN 字节布局。
- `CHASSIS_POWER_BENCH_TEST` 仓库默认值必须为 `0`；显式设为 `1` 时才允许台架回退。
- 保留且不覆盖用户对 `modules/super_cap/super_cap.c`、`modules/super_cap/super_cap.h`、`modules/imu/BMI088driver.c` 的未提交改动。
- 每次提交只暂存本任务明确列出的文件，禁止使用 `git add .`。

## File Structure

- `modules/referee/referee_stream_parser.h/.c`：保存 UART 跨回调残片，完成 SOF 搜索、长度读取、CRC8/CRC16 校验和坏帧重同步。
- `modules/referee/referee_power.h/.c`：从有效功率帧提取字段，维护两类命令的独立更新时间并生成一致快照。
- `modules/referee/referee_protocol.h`：把功率链路使用的数据布局更新为 2026 版本。
- `modules/referee/rm_referee.h/.c`：将 UART、字节流解析器、旧 `referee_info_t` 与新功率快照接口连接起来。
- `bsp/usart/bsp_usart.h/.c`：保存本次 DMA/IDLE 回调实际收到的字节数。
- `application/chassis/chassis_power_link.h/.c`：纯函数决定正式/台架/失效状态下的 `0x061` 指令和基础预算。
- `application/chassis/chassis.c`：每周期获取一次快照并使用同一结果驱动 CAN 指令和功率预算。
- `application/robot_def.h`：恢复正式配置的失效安全默认值。
- `Makefile`：显式加入新增固件源文件；`CMakeLists.txt` 使用递归 glob，无需修改。
- `tests/referee_stream_parser/*`、`tests/referee_power/*`、`tests/chassis_power_link/*`：三个无硬件依赖的主机测试目标。
- `docs/supercap-can-protocol.md`、`bsp/usart/bsp_usart.md`：记录新鲜度和实际接收长度语义。

---

### Task 1: 增加可恢复的裁判 UART 字节流解析器

**Files:**
- Create: `modules/referee/referee_stream_parser.h`
- Create: `modules/referee/referee_stream_parser.c`
- Create: `tests/referee_stream_parser/test_referee_stream_parser.c`
- Create: `tests/referee_stream_parser/Makefile`

**Interfaces:**
- Consumes: 任意分片的 UART 字节块；现有 `Verify_CRC8_Check_Sum()`、`Verify_CRC16_Check_Sum()`。
- Produces: `RefereeStreamParserInit()`、`RefereeStreamParserReset()`、`RefereeStreamParserFeed()`、`RefereeStreamParserGetStats()`；有效帧回调接收命令码、载荷、载荷长度和序号。

- [ ] **Step 1: 写字节流行为的失败测试**

在测试中声明捕获回调及帧构造器，帧构造器使用现有真实 CRC 实现生成输入，但所有期望命令码、载荷和计数均使用手写字面量：

```c
#include "referee_stream_parser.h"
#include "crc_ref.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    uint16_t command_id;
    uint16_t payload_length;
    uint8_t sequence;
    uint8_t payload[32];
} CapturedFrame_s;

static CapturedFrame_s captured[8];
static size_t captured_count;
static int failures;

#define ASSERT_TRUE(value) do { if (!(value)) { ++failures; } } while (0)
#define ASSERT_EQ_U32(expected, actual) \
    do { if ((uint32_t)(expected) != (uint32_t)(actual)) { ++failures; } } while (0)

static void CaptureFrame(void *context,
                         uint16_t command_id,
                         const uint8_t *payload,
                         uint16_t payload_length,
                         uint8_t sequence)
{
    (void)context;
    ASSERT_TRUE(captured_count < 8U);
    if (captured_count >= 8U)
        return;
    CapturedFrame_s *frame = &captured[captured_count++];
    frame->command_id = command_id;
    frame->payload_length = payload_length;
    frame->sequence = sequence;
    ASSERT_TRUE(payload_length <= sizeof(frame->payload));
    if (payload_length <= sizeof(frame->payload))
        memcpy(frame->payload, payload, payload_length);
}

static size_t BuildFrame(uint16_t command_id,
                         uint8_t sequence,
                         const uint8_t *payload,
                         uint16_t payload_length,
                         uint8_t *frame)
{
    const size_t frame_length = (size_t)payload_length + 9U;
    frame[0] = 0xA5U;
    frame[1] = (uint8_t)payload_length;
    frame[2] = (uint8_t)(payload_length >> 8);
    frame[3] = sequence;
    Append_CRC8_Check_Sum(frame, 5U);
    frame[5] = (uint8_t)command_id;
    frame[6] = (uint8_t)(command_id >> 8);
    memcpy(&frame[7], payload, payload_length);
    Append_CRC16_Check_Sum(frame, (uint32_t)frame_length);
    return frame_length;
}
```

加入以下独立测试并在 `main()` 中逐个调用：

```c
static void TestParsesTwoFramesFromOneChunk(void)
{
    RefereeStreamParser_s parser;
    uint8_t bytes[64];
    const uint8_t first_payload[] = {0x11U, 0x22U};
    const uint8_t second_payload[] = {0x33U};
    size_t used = BuildFrame(0x0201U, 7U, first_payload, 2U, bytes);
    used += BuildFrame(0x0202U, 8U, second_payload, 1U, &bytes[used]);

    captured_count = 0U;
    ASSERT_TRUE(RefereeStreamParserInit(&parser, CaptureFrame, NULL));
    RefereeStreamParserFeed(&parser, bytes, used);

    ASSERT_EQ_U32(2U, captured_count);
    ASSERT_EQ_U32(0x0201U, captured[0].command_id);
    ASSERT_EQ_U32(7U, captured[0].sequence);
    ASSERT_EQ_U32(0x22U, captured[0].payload[1]);
    ASSERT_EQ_U32(0x0202U, captured[1].command_id);
}

static void TestPreservesFrameSplitAcrossChunks(void)
{
    RefereeStreamParser_s parser;
    uint8_t bytes[32];
    const uint8_t payload[] = {1U, 2U, 3U};
    const size_t length = BuildFrame(0x0202U, 9U, payload, 3U, bytes);

    captured_count = 0U;
    ASSERT_TRUE(RefereeStreamParserInit(&parser, CaptureFrame, NULL));
    RefereeStreamParserFeed(&parser, bytes, 4U);
    ASSERT_EQ_U32(0U, captured_count);
    RefereeStreamParserFeed(&parser, &bytes[4], length - 4U);
    ASSERT_EQ_U32(1U, captured_count);
    ASSERT_EQ_U32(3U, captured[0].payload_length);
}

static void TestResynchronizesAfterNoiseAndBadCrc(void)
{
    RefereeStreamParser_s parser;
    uint8_t bytes[80] = {0x10U, 0x20U, 0x30U};
    const uint8_t payload[] = {0x5AU};
    size_t used = 3U;
    const size_t bad_offset = used;
    used += BuildFrame(0x0201U, 1U, payload, 1U, &bytes[used]);
    bytes[bad_offset + 7U] ^= 0x01U;
    used += BuildFrame(0x0202U, 2U, payload, 1U, &bytes[used]);

    captured_count = 0U;
    ASSERT_TRUE(RefereeStreamParserInit(&parser, CaptureFrame, NULL));
    RefereeStreamParserFeed(&parser, bytes, used);
    const RefereeStreamParserStats_s *stats = RefereeStreamParserGetStats(&parser);

    ASSERT_EQ_U32(1U, captured_count);
    ASSERT_EQ_U32(0x0202U, captured[0].command_id);
    ASSERT_EQ_U32(1U, stats->crc16_error_count);
    ASSERT_TRUE(stats->discarded_byte_count >= 3U);
}

static void TestRejectsBadHeaderCrcThenFindsGoodFrame(void)
{
    RefereeStreamParser_s parser;
    uint8_t bytes[48];
    const uint8_t payload[] = {0x66U};
    size_t used = BuildFrame(0x0201U, 1U, payload, 1U, bytes);
    bytes[4] ^= 0x01U;
    used += BuildFrame(0x0202U, 2U, payload, 1U, &bytes[used]);

    captured_count = 0U;
    ASSERT_TRUE(RefereeStreamParserInit(&parser, CaptureFrame, NULL));
    RefereeStreamParserFeed(&parser, bytes, used);
    const RefereeStreamParserStats_s *stats = RefereeStreamParserGetStats(&parser);

    ASSERT_EQ_U32(1U, captured_count);
    ASSERT_EQ_U32(0x0202U, captured[0].command_id);
    ASSERT_EQ_U32(1U, stats->crc8_error_count);
}

static void TestRejectsOversize16BitLengthThenFindsGoodFrame(void)
{
    RefereeStreamParser_s parser;
    uint8_t bytes[32] = {0xA5U, 0x00U, 0x01U, 0x03U, 0U};
    const uint8_t payload[] = {0x44U};
    Append_CRC8_Check_Sum(bytes, 5U);
    size_t used = 5U;
    used += BuildFrame(0x0201U, 4U, payload, 1U, &bytes[used]);

    captured_count = 0U;
    ASSERT_TRUE(RefereeStreamParserInit(&parser, CaptureFrame, NULL));
    RefereeStreamParserFeed(&parser, bytes, used);
    const RefereeStreamParserStats_s *stats = RefereeStreamParserGetStats(&parser);

    ASSERT_EQ_U32(1U, captured_count);
    ASSERT_EQ_U32(1U, stats->oversize_frame_count);
}
```

`main()` 在 `failures == 0` 时打印 `PASS: referee stream parser tests` 并返回 0，否则返回 1。

测试 Makefile：

```make
CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_referee_stream_parser.exe
SOURCE := test_referee_stream_parser.c \
          ../../modules/referee/referee_stream_parser.c \
          ../../modules/referee/crc_ref.c
INCLUDES := -I../../modules/referee

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -o $(TARGET)

test: $(TARGET)
	./$(TARGET)

clean:
	-if exist $(TARGET) del /Q $(TARGET)
```

- [ ] **Step 2: 运行测试并确认 RED**

Run: `mingw32-make -C tests/referee_stream_parser clean test`

Expected: 编译失败，提示缺少 `referee_stream_parser.h` 或对应 API 尚未定义。

- [ ] **Step 3: 声明最小解析器接口**

`referee_stream_parser.h` 使用以下公共类型：

```c
#ifndef REFEREE_STREAM_PARSER_H
#define REFEREE_STREAM_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define REFEREE_STREAM_BUFFER_CAPACITY 256U

typedef void (*RefereeFrameHandler)(void *context,
                                    uint16_t command_id,
                                    const uint8_t *payload,
                                    uint16_t payload_length,
                                    uint8_t sequence);

typedef struct
{
    uint32_t valid_frame_count;
    uint32_t crc8_error_count;
    uint32_t crc16_error_count;
    uint32_t oversize_frame_count;
    uint32_t discarded_byte_count;
} RefereeStreamParserStats_s;

typedef struct
{
    uint8_t buffer[REFEREE_STREAM_BUFFER_CAPACITY];
    size_t used;
    RefereeFrameHandler handler;
    void *context;
    RefereeStreamParserStats_s stats;
} RefereeStreamParser_s;

bool RefereeStreamParserInit(RefereeStreamParser_s *parser,
                             RefereeFrameHandler handler,
                             void *context);
void RefereeStreamParserReset(RefereeStreamParser_s *parser);
void RefereeStreamParserFeed(RefereeStreamParser_s *parser,
                             const uint8_t *data,
                             size_t length);
const RefereeStreamParserStats_s *RefereeStreamParserGetStats(
    const RefereeStreamParser_s *parser);

#endif
```

- [ ] **Step 4: 实现最小流解析算法**

`referee_stream_parser.c` 固定使用 5 字节帧头、2 字节命令码和 2 字节 CRC16。实现以下处理循环：

```c
#define REFEREE_SOF 0xA5U
#define REFEREE_HEADER_LENGTH 5U
#define REFEREE_COMMAND_LENGTH 2U
#define REFEREE_TAIL_LENGTH 2U
#define REFEREE_FRAME_OVERHEAD 9U

static uint16_t ReadU16LE(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static void DiscardPrefix(RefereeStreamParser_s *parser, size_t count)
{
    if (count >= parser->used)
    {
        parser->stats.discarded_byte_count += (uint32_t)parser->used;
        parser->used = 0U;
        return;
    }
    memmove(parser->buffer, &parser->buffer[count], parser->used - count);
    parser->used -= count;
    parser->stats.discarded_byte_count += (uint32_t)count;
}
```

处理函数必须先找 SOF；CRC8 失败、16 位小端声明长度加 9 超过 256、CRC16 失败时分别累加计数并丢弃一个字节；数据不足完整帧时保留；完整有效帧调用 handler 后按整帧长度移除。`Feed()` 分批复制超长输入，每次缓存填充后都运行处理函数，避免写越界。`Reset()` 只清除待解析字节，保留累计诊断计数。

- [ ] **Step 5: 运行测试并确认 GREEN**

Run: `mingw32-make -C tests/referee_stream_parser clean test`

Expected: `PASS: referee stream parser tests`。

- [ ] **Step 6: 提交解析器**

```powershell
git add -- modules/referee/referee_stream_parser.h modules/referee/referee_stream_parser.c tests/referee_stream_parser/test_referee_stream_parser.c tests/referee_stream_parser/Makefile
git commit -m "feat: add streaming referee frame parser"
```

---

### Task 2: 增加 2026 裁判功率状态与新鲜度快照

**Files:**
- Create: `modules/referee/referee_power.h`
- Create: `modules/referee/referee_power.c`
- Modify: `modules/referee/referee_protocol.h:89-104, 176-186`
- Create: `tests/referee_power/test_referee_power.c`
- Create: `tests/referee_power/Makefile`

**Interfaces:**
- Consumes: 通过 CRC 的 `0x0201`/`0x0202` 载荷、接收毫秒时刻。
- Produces: `RefereePowerStateReset()`、`RefereePowerStateAcceptFrame()`、`RefereePowerBuildSnapshot()`；`RefereePowerSnapshot_s` 供底盘策略使用。

- [ ] **Step 1: 写字段解析、长度拒绝和新鲜度的失败测试**

测试使用手写载荷，避免通过生产结构体生成期望值：

```c
#include "referee_power.h"

#include <stdint.h>
#include <stdio.h>

static int failures;
#define ASSERT_TRUE(value) do { if (!(value)) { ++failures; } } while (0)
#define ASSERT_FALSE(value) ASSERT_TRUE(!(value))
#define ASSERT_EQ_U32(expected, actual) \
    do { if ((uint32_t)(expected) != (uint32_t)(actual)) { ++failures; } } while (0)

static const uint8_t robot_state_enabled[13] = {
    3U, 1U, 100U, 0U, 200U, 0U, 50U, 0U, 100U, 0U,
    120U, 0U, 0x02U
};

static const uint8_t power_heat[14] = {
    0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
    55U, 0U, 10U, 0U, 20U, 0U
};

static void TestRequiresBothFreshPowerFrames(void)
{
    RefereePowerState_s state;
    RefereePowerSnapshot_s snapshot;
    RefereePowerStateReset(&state);

    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, robot_state_enabled, 13U, 1000U));
    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 1000U, &snapshot));
    ASSERT_FALSE(snapshot.valid);

    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, power_heat, 14U, 1050U));
    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 1050U, &snapshot));
    ASSERT_TRUE(snapshot.valid);
    ASSERT_TRUE(snapshot.chassis_output_enabled);
    ASSERT_EQ_U32(120U, snapshot.chassis_power_limit_w);
    ASSERT_EQ_U32(55U, snapshot.buffer_energy_j);
}

static void TestRejectsWrongPayloadLengthsWithoutOverwritingState(void)
{
    RefereePowerState_s state;
    RefereePowerSnapshot_s snapshot;
    RefereePowerStateReset(&state);
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, robot_state_enabled, 13U, 10U));
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, power_heat, 14U, 10U));

    ASSERT_FALSE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, power_heat, 13U, 20U));
    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 20U, &snapshot));
    ASSERT_TRUE(snapshot.valid);
    ASSERT_EQ_U32(55U, snapshot.buffer_energy_j);
}

static void TestExpiresWhenEitherFrameIsOlderThan300Ms(void)
{
    RefereePowerState_s state;
    RefereePowerSnapshot_s snapshot;
    RefereePowerStateReset(&state);
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, robot_state_enabled, 13U, 100U));
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, power_heat, 14U, 200U));

    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 400U, &snapshot));
    ASSERT_TRUE(snapshot.valid);
    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 401U, &snapshot));
    ASSERT_FALSE(snapshot.valid);
    ASSERT_EQ_U32(0U, snapshot.chassis_power_limit_w);
    ASSERT_EQ_U32(0U, snapshot.buffer_energy_j);
}

static void TestFreshnessHandlesTickWrap(void)
{
    RefereePowerState_s state;
    RefereePowerSnapshot_s snapshot;
    RefereePowerStateReset(&state);
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0201U, robot_state_enabled, 13U, UINT32_MAX - 10U));
    ASSERT_TRUE(RefereePowerStateAcceptFrame(
        &state, 0x0202U, power_heat, 14U, UINT32_MAX - 10U));

    ASSERT_TRUE(RefereePowerBuildSnapshot(&state, 5U, &snapshot));
    ASSERT_TRUE(snapshot.valid);
}
```

另加测试确认 `0x0201` flags 的 bit1 清零时 `chassis_output_enabled == false`，未知命令、空指针和错误长度返回 false。`main()` 在全部通过时打印 `PASS: referee power tests`。

测试 Makefile：

```make
CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_referee_power.exe
SOURCE := test_referee_power.c ../../modules/referee/referee_power.c
INCLUDES := -I../../modules/referee

.PHONY: all test clean
all: $(TARGET)
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -o $(TARGET)
test: $(TARGET)
	./$(TARGET)
clean:
	-if exist $(TARGET) del /Q $(TARGET)
```

- [ ] **Step 2: 运行测试并确认 RED**

Run: `mingw32-make -C tests/referee_power clean test`

Expected: 编译失败，提示 `referee_power.h` 或新 API 尚不存在。

- [ ] **Step 3: 声明功率状态接口**

`referee_power.h`：

```c
#ifndef REFEREE_POWER_H
#define REFEREE_POWER_H

#include <stdbool.h>
#include <stdint.h>

#define REFEREE_ROBOT_STATE_COMMAND_ID 0x0201U
#define REFEREE_POWER_HEAT_COMMAND_ID 0x0202U
#define REFEREE_ROBOT_STATE_PAYLOAD_LENGTH 13U
#define REFEREE_POWER_HEAT_PAYLOAD_LENGTH 14U
#define REFEREE_POWER_DATA_TIMEOUT_MS 300U

typedef struct
{
    uint16_t chassis_power_limit_w;
    uint16_t buffer_energy_j;
    uint32_t robot_state_tick_ms;
    uint32_t power_heat_tick_ms;
    bool chassis_output_enabled;
    bool has_robot_state;
    bool has_power_heat;
} RefereePowerState_s;

typedef struct
{
    bool valid;
    bool chassis_output_enabled;
    uint16_t chassis_power_limit_w;
    uint16_t buffer_energy_j;
} RefereePowerSnapshot_s;

void RefereePowerStateReset(RefereePowerState_s *state);
bool RefereePowerStateAcceptFrame(RefereePowerState_s *state,
                                  uint16_t command_id,
                                  const uint8_t *payload,
                                  uint16_t payload_length,
                                  uint32_t now_ms);
bool RefereePowerBuildSnapshot(const RefereePowerState_s *state,
                               uint32_t now_ms,
                               RefereePowerSnapshot_s *snapshot);

#endif
```

- [ ] **Step 4: 实现最小功率字段解析和超时逻辑**

`referee_power.c` 只按字节偏移读取：`0x0201` 的功率上限为 10-11，flags 为 12；`0x0202` 的缓冲能量为 8-9。先验证命令和精确长度，再写状态。快照先清零；只有两个 `has_*` 均为真且 `(uint32_t)(now_ms - tick_ms) <= 300U` 时才复制有效值。

- [ ] **Step 5: 修正 2026 协议结构**

在 `referee_protocol.h` 中令 `LEN_power_heat_data = 14`，并把结构改为：

```c
typedef struct
{
    uint16_t reserved_voltage;
    uint16_t reserved_current;
    float reserved_power;
    uint16_t buffer_energy;
    uint16_t shooter_17mm_barrel_heat;
    uint16_t shooter_42mm_barrel_heat;
} ext_power_heat_data_t;
```

增加编译期约束，防止布局回退：

```c
_Static_assert(sizeof(ext_game_robot_state_t) == LEN_game_robot_state,
               "0x0201 payload layout mismatch");
_Static_assert(sizeof(ext_power_heat_data_t) == LEN_power_heat_data,
               "0x0202 payload layout mismatch");
```

- [ ] **Step 6: 运行测试并确认 GREEN**

Run: `mingw32-make -C tests/referee_power clean test`

Expected: `PASS: referee power tests`。

- [ ] **Step 7: 提交功率状态和协议布局**

```powershell
git add -- modules/referee/referee_power.h modules/referee/referee_power.c modules/referee/referee_protocol.h tests/referee_power/test_referee_power.c tests/referee_power/Makefile
git commit -m "feat: track fresh 2026 referee power data"
```

---

### Task 3: 将 DMA 实际长度和流解析器接入裁判模块

**Files:**
- Modify: `bsp/usart/bsp_usart.h:23-30`
- Modify: `bsp/usart/bsp_usart.c:102-116`
- Modify: `bsp/usart/bsp_usart.md`
- Modify: `modules/referee/rm_referee.h`
- Modify: `modules/referee/rm_referee.c`
- Modify: `Makefile:145-150`

**Interfaces:**
- Consumes: Task 1 的 `RefereeStreamParser*`，Task 2 的 `RefereePowerState*`，HAL 的 `Size` 和 `HAL_GetTick()`。
- Produces: `USARTInstance.recv_data_len`；`RefereeGetPowerSnapshot(uint32_t, RefereePowerSnapshot_s *)`。

- [ ] **Step 1: 记录 UART 实际接收长度**

在 `USARTInstance` 中加入：

```c
uint16_t recv_data_len; // 最近一次 DMA/IDLE 事件实际收到的字节数
```

在 `HAL_UARTEx_RxEventCallback()` 调用模块回调前赋值，并在清理缓冲后清零：

```c
usart_instance[i]->recv_data_len = Size;
usart_instance[i]->module_callback();
memset(usart_instance[i]->recv_buff, 0, Size);
usart_instance[i]->recv_data_len = 0U;
```

同步更新 `bsp_usart.md`，明确 IDLE 只代表当前数据块结束，不代表协议帧必然完整。

- [ ] **Step 2: 用流解析器替换递归的 `JudgeReadData()`**

在 `rm_referee.c` 中增加静态 `RefereeStreamParser_s` 和 `RefereePowerState_s`。有效帧回调完成以下动作：

```c
static void HandleValidFrame(void *context,
                             uint16_t command_id,
                             const uint8_t *payload,
                             uint16_t payload_length,
                             uint8_t sequence)
{
    (void)context;
    referee_info.FrameHeader.Seq = sequence;
    referee_info.CmdID = command_id;

    if (RefereePowerStateAcceptFrame(&referee_power_state,
                                     command_id,
                                     payload,
                                     payload_length,
                                     HAL_GetTick()))
    {
        if (command_id == ID_game_robot_state)
            memcpy(&referee_info.GameRobotState, payload, LEN_game_robot_state);
        else
            memcpy(&referee_info.PowerHeatData, payload, LEN_power_heat_data);
    }

    /* 其余旧命令继续按现有 switch 分发，但每个 memcpy 前必须验证
       payload_length 与对应 LEN_* 完全相等。 */
    if (referee_daemon != NULL)
        DaemonReload(referee_daemon);
}
```

删除 `JudgeReadData()` 的递归扫描。新接收回调只提交本次真实字节数：

```c
static void RefereeRxCallback(void)
{
    if (referee_usart_instance == NULL ||
        referee_usart_instance->recv_data_len == 0U)
        return;
    RefereeStreamParserFeed(&referee_stream_parser,
                            referee_usart_instance->recv_buff,
                            referee_usart_instance->recv_data_len);
}
```

在 `RefereeInit()` 注册 USART 前重置功率状态并初始化解析器。丢失回调先 `RefereeStreamParserReset()` 再重启 USART，避免断线前半帧与恢复后的数据拼接。

- [ ] **Step 3: 暴露原子功率快照**

在 `rm_referee.h` 包含 `referee_power.h` 并声明：

```c
bool RefereeGetPowerSnapshot(uint32_t now_ms,
                             RefereePowerSnapshot_s *snapshot);
```

实现时先验证输出指针，在 `taskENTER_CRITICAL()`/`taskEXIT_CRITICAL()` 间复制整个 `RefereePowerState_s` 到局部变量，临界区外调用 `RefereePowerBuildSnapshot()`。不要在临界区内做日志、CAN 发送或 CRC 计算。

- [ ] **Step 4: 把新增纯逻辑源文件加入固件 Makefile**

在 `modules/referee/crc_ref.c` 后加入：

```make
modules/referee/referee_stream_parser.c \
modules/referee/referee_power.c \
```

`CMakeLists.txt` 已递归收集 `modules/*.c`，不增加重复条目。

- [ ] **Step 5: 运行裁判侧测试和 ARM 编译**

Run:

```powershell
mingw32-make -C tests/referee_stream_parser clean test
mingw32-make -C tests/referee_power clean test
mingw32-make -j24
```

Expected: 两个主机测试打印 PASS；ARM 固件完成链接，无新增 warning/error。

- [ ] **Step 6: 提交 UART 与裁判集成**

```powershell
git add -- bsp/usart/bsp_usart.h bsp/usart/bsp_usart.c bsp/usart/bsp_usart.md modules/referee/rm_referee.h modules/referee/rm_referee.c Makefile
git commit -m "feat: feed referee UART chunks into validated parser"
```

---

### Task 4: 用纯策略生成超级电容指令和基础功率预算

**Files:**
- Create: `application/chassis/chassis_power_link.h`
- Create: `application/chassis/chassis_power_link.c`
- Create: `tests/chassis_power_link/test_chassis_power_link.c`
- Create: `tests/chassis_power_link/Makefile`
- Modify: `application/chassis/chassis.c:260-317`
- Modify: `application/robot_def.h:27-35`
- Modify: `Makefile:157-160`

**Interfaces:**
- Consumes: `RefereePowerSnapshot_s`、台架配置、底盘零力状态。
- Produces: `ChassisPowerLinkBuild()` 返回唯一的 `SuperCapCommand_s` 和 `base_power_limit_w`。

- [ ] **Step 1: 写正式、失效、权限关闭和台架路径的失败测试**

```c
#include "chassis_power_link.h"

#include <math.h>
#include <stdio.h>

static int failures;
#define ASSERT_TRUE(value) do { if (!(value)) { ++failures; } } while (0)
#define ASSERT_FALSE(value) ASSERT_TRUE(!(value))
#define ASSERT_EQ_U32(expected, actual) \
    do { if ((uint32_t)(expected) != (uint32_t)(actual)) { ++failures; } } while (0)
#define ASSERT_NEAR(expected, actual) \
    do { if (fabsf((expected) - (actual)) > 0.001f) { ++failures; } } while (0)

static void TestValidRefereeSnapshotDrivesCommandAndBudget(void)
{
    const ChassisPowerLinkInput_s input = {
        .referee = {
            .valid = true,
            .chassis_output_enabled = true,
            .chassis_power_limit_w = 120U,
            .buffer_energy_j = 55U,
        },
        .bench_enabled = false,
        .bench_limit_w = 0.0f,
        .chassis_zero_force = false,
    };
    ChassisPowerLinkOutput_s output;
    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_TRUE(output.command.enable_dcdc);
    ASSERT_EQ_U32(120U, output.command.referee_power_limit_w);
    ASSERT_EQ_U32(55U, output.command.referee_buffer_energy_j);
    ASSERT_NEAR(120.0f, output.base_power_limit_w);
}

static void TestInvalidSnapshotFailsSafeInProduction(void)
{
    const ChassisPowerLinkInput_s input = {0};
    ChassisPowerLinkOutput_s output;
    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_FALSE(output.command.enable_dcdc);
    ASSERT_EQ_U32(0U, output.command.referee_power_limit_w);
    ASSERT_EQ_U32(0U, output.command.referee_buffer_energy_j);
    ASSERT_NEAR(0.0f, output.base_power_limit_w);
}

static void TestRefereeOutputPermissionAndZeroForceDisableDcdc(void)
{
    ChassisPowerLinkInput_s input = {
        .referee = {true, false, 100U, 40U},
    };
    ChassisPowerLinkOutput_s output;
    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_FALSE(output.command.enable_dcdc);
    ASSERT_EQ_U32(100U, output.command.referee_power_limit_w);

    input.referee.chassis_output_enabled = true;
    input.chassis_zero_force = true;
    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_FALSE(output.command.enable_dcdc);
}

static void TestExplicitBenchModeUsesLimitButZeroBuffer(void)
{
    const ChassisPowerLinkInput_s input = {
        .bench_enabled = true,
        .bench_limit_w = 60.0f,
        .chassis_zero_force = false,
    };
    ChassisPowerLinkOutput_s output;
    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_TRUE(output.command.enable_dcdc);
    ASSERT_EQ_U32(60U, output.command.referee_power_limit_w);
    ASSERT_EQ_U32(0U, output.command.referee_buffer_energy_j);
    ASSERT_NEAR(60.0f, output.base_power_limit_w);
}

static void TestValidSnapshotEncodesExistingCanLayout(void)
{
    const ChassisPowerLinkInput_s input = {
        .referee = {true, true, 100U, 50U},
    };
    const uint8_t expected[8] = {0x01U, 0x64U, 0x00U, 0x32U,
                                 0x00U, 0x00U, 0x00U, 0x00U};
    uint8_t actual[8] = {0};
    ChassisPowerLinkOutput_s output;

    ASSERT_TRUE(ChassisPowerLinkBuild(&input, &output));
    ASSERT_TRUE(SuperCapProtocolEncodeCommand(&output.command, actual));
    for (size_t i = 0U; i < sizeof(expected); ++i)
        ASSERT_EQ_U32(expected[i], actual[i]);
}
```

另加测试验证有效裁判快照优先于台架值；`bench_limit_w` 为 0、负数或非有限值时不启用；空指针返回 false。`main()` 成功时打印 `PASS: chassis power link tests`。

测试 Makefile：

```make
CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_chassis_power_link.exe
SOURCE := test_chassis_power_link.c \
          ../../application/chassis/chassis_power_link.c \
          ../../modules/super_cap/super_cap_protocol.c
INCLUDES := -I../../application/chassis -I../../modules/referee \
            -I../../modules/super_cap

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

Run: `mingw32-make -C tests/chassis_power_link clean test`

Expected: 编译失败，提示 `chassis_power_link.h` 或新 API 尚不存在。

- [ ] **Step 3: 声明并实现最小策略接口**

`chassis_power_link.h`：

```c
#ifndef CHASSIS_POWER_LINK_H
#define CHASSIS_POWER_LINK_H

#include "referee_power.h"
#include "super_cap_protocol.h"

#include <stdbool.h>

typedef struct
{
    RefereePowerSnapshot_s referee;
    bool bench_enabled;
    float bench_limit_w;
    bool chassis_zero_force;
} ChassisPowerLinkInput_s;

typedef struct
{
    SuperCapCommand_s command;
    float base_power_limit_w;
} ChassisPowerLinkOutput_s;

bool ChassisPowerLinkBuild(const ChassisPowerLinkInput_s *input,
                           ChassisPowerLinkOutput_s *output);

#endif
```

实现优先级固定为：有效裁判快照 > 显式有效台架值 > 全零失效安全。有效裁判快照始终透传功率和缓冲值，但仅在输出权限为真、功率大于 0 且非零力时使能 DCDC。台架功率四舍五入并饱和到 `UINT16_MAX`，缓冲能量固定为 0。

- [ ] **Step 4: 运行策略测试并确认 GREEN**

Run: `mingw32-make -C tests/chassis_power_link clean test`

Expected: `PASS: chassis power link tests`。

- [ ] **Step 5: 将底盘任务改为每周期使用一个快照**

在 `UpdateChassisPowerBudget()` 开头调用一次：

```c
RefereePowerSnapshot_s referee_snapshot = {0};
(void)RefereeGetPowerSnapshot(HAL_GetTick(), &referee_snapshot);

const ChassisPowerLinkInput_s link_input = {
    .referee = referee_snapshot,
#if CHASSIS_POWER_BENCH_TEST
    .bench_enabled = true,
#else
    .bench_enabled = false,
#endif
    .bench_limit_w = CHASSIS_POWER_BENCH_LIMIT_W,
    .chassis_zero_force =
        chassis_cmd_recv.chassis_mode == CHASSIS_ZERO_FORCE,
};
ChassisPowerLinkOutput_s link_output = {0};
(void)ChassisPowerLinkBuild(&link_input, &link_output);
```

发送 `link_output.command`；把预算输入的 `referee_limit_w` 替换为 `link_output.base_power_limit_w`。删除 `chassis.c` 中不再需要的 `PowerValueToU16()`、静态 `referee_data` 指针和直接访问 `GameRobotState/PowerHeatData` 的功率逻辑；初始化处改为 `(void)UITaskInit(&huart6, &ui_data)`。`referee_task.c` 内部继续持有 UI 所需的裁判数据指针，不重构无关功能。

把 `application/robot_def.h` 的默认值改为：

```c
#define CHASSIS_POWER_BENCH_TEST 0
```

在根 `Makefile` 的底盘源文件旁加入：

```make
application/chassis/chassis_power_link.c \
```

- [ ] **Step 6: 运行策略、超级电容和功率预算回归测试**

Run:

```powershell
mingw32-make -C tests/chassis_power_link clean test
mingw32-make -C tests/super_cap_protocol clean test
mingw32-make -C tests/chassis_power_budget clean test
mingw32-make -C tests/power_model clean test
```

Expected: 四个目标均打印 PASS。

- [ ] **Step 7: 编译固件并提交底盘接线**

Run: `mingw32-make -j24`

Expected: ARM 固件完成链接，无新增 warning/error。

```powershell
git add -- application/chassis/chassis_power_link.h application/chassis/chassis_power_link.c tests/chassis_power_link/test_chassis_power_link.c tests/chassis_power_link/Makefile application/chassis/chassis.c application/robot_def.h Makefile
git commit -m "feat: fail safe referee power link to supercap"
```

---

### Task 5: 更新协议说明并执行完整验证

**Files:**
- Modify: `docs/supercap-can-protocol.md`

**Interfaces:**
- Consumes: Tasks 1-4 的最终行为。
- Produces: 可供电控联调使用的链路、断连和恢复说明。

- [ ] **Step 1: 更新协议文档**

在 `docs/supercap-can-protocol.md` 的指令帧说明后加入：

```markdown
### 裁判数据有效性

C 板只在最近 300 ms 内分别收到通过 CRC 和精确长度检查的 `0x0201`
与 `0x0202` 时，才认为裁判功率快照有效。正式配置下，任一关键命令
超时都会令下一周期 `0x061` 变为关闭 DCDC、功率上限 0 W、缓冲能量
0 J；两个命令恢复后链路自动恢复。

`CHASSIS_POWER_BENCH_TEST` 默认关闭。显式台架模式只提供配置的台架功率
上限，缓冲能量仍为 0 J，不能用于正式比赛固件。
```

在文档中注明上游数据遵循 2026 协议：`0x0201` 为 13 字节，`0x0202` 为 14 字节，缓冲能量位于 `0x0202` 数据段偏移 8。

- [ ] **Step 2: 运行所有相关主机测试**

Run:

```powershell
mingw32-make -C tests/referee_stream_parser clean test
mingw32-make -C tests/referee_power clean test
mingw32-make -C tests/chassis_power_link clean test
mingw32-make -C tests/super_cap_protocol clean test
mingw32-make -C tests/chassis_power_budget clean test
mingw32-make -C tests/power_model clean test
mingw32-make -C tests/chassis_power_feedback clean test
```

Expected: 七个测试目标均打印 PASS，且无 warning。

- [ ] **Step 3: 执行干净 ARM 固件构建**

在确认没有调试器占用 `build` 产物后运行：

```powershell
mingw32-make clean
mingw32-make -j24
```

Expected: 生成 `build/basic_framework.elf`、`.hex` 和 `.bin`，无新增 warning/error。

- [ ] **Step 4: 检查改动边界**

Run:

```powershell
git status --short
git diff --check
git diff --stat HEAD~3..HEAD
```

Expected: 没有空白错误；用户原有三个未提交文件仍保持未提交；任务提交中不包含超级电容诊断计数或 BMI088 改动。

- [ ] **Step 5: 提交文档**

```powershell
git add -- docs/supercap-can-protocol.md
git commit -m "docs: describe referee power freshness behavior"
```

- [ ] **Step 6: 上板联调**

依次记录以下可观察结果：

1. 连上裁判系统后，`0x0201` 与 `0x0202` 有效帧持续更新；
2. `0x061` 的功率上限和缓冲能量与裁判系统一致；
3. 拔掉裁判串口后最多 300 ms，`0x061` 变为 `00 00 00 00 00 00 00 00`；
4. 恢复串口并收到两类关键帧后，`0x061` 自动恢复；
5. `0x051` 状态帧接收计数继续增加，四个底盘电机 CAN 通信无回归。
