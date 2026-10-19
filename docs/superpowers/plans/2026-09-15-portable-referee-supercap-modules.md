# Portable Referee and Supercapacitor Modules Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Build two isolated, shareable C11 modules that preserve the current referee-system and supercapacitor wire behavior while allowing an A-board application to provide its own UART, CAN, clock, and critical-section functions.

**Architecture:** Port the current protocol knowledge into a new portable_modules/modules package and replace STM32/RTOS/BSP dependencies with callbacks and caller-owned instances. UART and CAN receive callbacks feed raw data into the modules; the modules perform protocol work and call injected transmit functions. Existing project files and build configuration are never changed.

**Tech Stack:** ISO C11, standard C library, GCC host tests, GNU Make

## Global Constraints

- Do not modify, move, delete, format, stage, or commit any file that existed before this feature.
- Preserve every pre-existing working-tree change byte-for-byte, including Src/usart.c, application/chassis/chassis.c, basic_framework.ioc, modules/referee/rm_referee.c, modules/power_meter/power_meter.c, modules/power_meter/power_meter.h, and tests/power_meter/test_power_meter.c.
- Create only portable_modules/modules/referee and portable_modules/modules/super_cap as share-package module directories; keep each module's tests and README inside its own directory.
- Do not add the package to the existing Makefile, CMake build, CubeMX project, application, BSP, or RTOS tasks.
- Production module files may include only standard C headers and headers from the same module.
- Do not include STM32 HAL, FreeRTOS, CMSIS-RTOS, BSP, daemon, logging, robot_def.h, or dynamic allocation.
- Port protocol constants, frame layouts, CRC behavior, decoded fields, UI functions, and supercapacitor semantics from the current source; do not invent a different wire protocol.
- Support the referee commands currently decoded by modules/referee/rm_referee.c; updating to another competition-year protocol is outside scope.
- Encode and decode wire data byte-by-byte so compiler packing, endianness, and bit-field layout do not affect frames.
- Compile every host test with -std=c11 -Wall -Wextra -Werror -pedantic.

---

## File Map

### Referee module

- Create portable_modules/modules/referee/referee_protocol.h: protocol constants and normal decoded-data types.
- Create portable_modules/modules/referee/referee_crc.h and referee_crc.c: portable CRC8/CRC16 implementation ported from the current module.
- Create portable_modules/modules/referee/referee.h and referee.c: callbacks, caller-owned instance, stream parser, decoder, queries, diagnostics, timeout, and generic interaction transmission.
- Create portable_modules/modules/referee/referee_ui.h and referee_ui.c: UI descriptions, safe builders, explicit 15-byte graph encoding, and UI transmission.
- Create portable_modules/modules/referee/README.md: A-board UART integration instructions.
- Create portable_modules/modules/referee/tests/Makefile and test_referee.c: standalone host tests.

### Supercapacitor module

- Create portable_modules/modules/super_cap/super_cap.h and super_cap.c: protocol, callbacks, caller-owned instance, codec, timeout, and diagnostics.
- Create portable_modules/modules/super_cap/README.md: A-board CAN integration instructions.
- Create portable_modules/modules/super_cap/tests/Makefile and test_super_cap.c: standalone host tests.

---

### Task 1: Portable referee receive and query module

**Files:**
- Create: portable_modules/modules/referee/referee_protocol.h
- Create: portable_modules/modules/referee/referee_crc.h
- Create: portable_modules/modules/referee/referee_crc.c
- Create: portable_modules/modules/referee/referee.h
- Create: portable_modules/modules/referee/referee.c
- Create: portable_modules/modules/referee/tests/Makefile
- Create: portable_modules/modules/referee/tests/test_referee.c

**Interfaces:**
- Consumes: raw UART chunks, UART-send and millisecond callbacks, and optional matching critical-section callbacks.
- Produces:

~~~c
typedef bool (*RefereeUartSendFn)(void *context,
                                  const uint8_t *data,
                                  size_t length);
typedef uint32_t (*RefereeNowMsFn)(void *context);
typedef void (*RefereeCriticalFn)(void *context);

typedef struct {
    RefereeUartSendFn uart_send;
    RefereeNowMsFn now_ms;
    RefereeCriticalFn enter_critical;
    RefereeCriticalFn exit_critical;
    void *context;
    uint32_t offline_timeout_ms;
} RefereeConfig;

bool RefereeInit(Referee *instance, const RefereeConfig *config);
bool RefereeFeed(Referee *instance, const uint8_t *data, size_t length);
bool RefereeGetData(Referee *instance, RefereeData *data);
bool RefereeGetPower(Referee *instance, RefereePower *power);
bool RefereeGetDiagnostics(Referee *instance,
                           RefereeDiagnostics *diagnostics);
bool RefereeIsOnline(Referee *instance);
uint16_t RefereeClientId(uint16_t robot_id);
~~~

RefereeData stores the latest decoded values for IDs 0x0001, 0x0002, 0x0003, 0x0101, 0x0102, 0x0201 through 0x0207, and 0x0301. Interaction data contains its six-byte header, a length, and up to 113 bytes. RefereePower contains valid, chassis_output_enabled, chassis_power_limit_w, and buffer_energy_j.

- [ ] **Step 1: Record the protected-file baseline**

Run:

~~~powershell
git status --short
git hash-object Src/usart.c application/chassis/chassis.c basic_framework.ioc modules/referee/rm_referee.c modules/power_meter/power_meter.c modules/power_meter/power_meter.h tests/power_meter/test_power_meter.c
~~~

Expected: all listed existing files remain modified exactly as before this feature. Record their hashes for Task 4.

- [ ] **Step 2: Write the referee receive tests before production files exist**

Create this standalone Makefile:

~~~make
CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_referee.exe
SOURCE := test_referee.c ../referee.c ../referee_crc.c
INCLUDES := -I..

.PHONY: all test clean
all: $(TARGET)
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -o $(TARGET)
test: $(TARGET)
	./$(TARGET)
clean:
	$(RM) $(TARGET)
~~~

Start test_referee.c with real fake transport/time functions:

~~~c
static uint32_t fake_now_ms;
static uint8_t sent_frame[REFEREE_MAX_FRAME_SIZE];
static size_t sent_length;
static bool send_result = true;

static uint32_t FakeNow(void *context)
{
    (void)context;
    return fake_now_ms;
}

static bool FakeSend(void *context, const uint8_t *data, size_t length)
{
    (void)context;
    memcpy(sent_frame, data, length);
    sent_length = length;
    return send_result;
}
~~~

Add these focused tests:

~~~c
static void TestInitRejectsMissingCallbacksAndMismatchedCriticalPair(void);
static void TestSplitAndCoalescedFramesDecodeWithoutLosingBytes(void);
static void TestNoiseBadCrcAndOversizeFramesUpdateDiagnostics(void);
static void TestEverySupportedCommandDecodesExactLittleEndianFields(void);
static void TestKnownCommandWithWrongLengthKeepsPreviousValue(void);
static void TestInteractionAcceptsZeroThrough113DataBytes(void);
static void TestPowerRequiresFreshRobotStateAndPowerHeatFrames(void);
static void TestOnlineTimeoutUsesUnsignedWraparoundArithmetic(void);
static void TestQueriesUseMatchingCriticalCallbacks(void);
static void TestNullInputsAreRejectedWithoutChangingState(void);
~~~

Use the existing power payload layout:

~~~c
static const uint8_t robot_state[13] = {
    0x03, 0x02, 0x34, 0x12, 0x78, 0x56, 0x10,
    0x00, 0x20, 0x00, 0x78, 0x00, 0x02
};
static const uint8_t power_heat[14] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0x37, 0x00, 0, 0, 0, 0
};

ASSERT_TRUE(power.valid);
ASSERT_TRUE(power.chassis_output_enabled);
ASSERT_EQ_U16(120U, power.chassis_power_limit_w);
ASSERT_EQ_U16(55U, power.buffer_energy_j);
~~~

For all-command coverage, build one CRC-valid frame per supported ID and assert an endian-sensitive field from every decoded structure, including float values with a tolerance.

- [ ] **Step 3: Run the new test and verify the red state**

Run:

~~~powershell
make -C portable_modules/modules/referee/tests test
~~~

Expected: compilation fails because the portable referee headers and sources do not exist.

- [ ] **Step 4: Port protocol types and CRC code**

Create normal decoded structures, not packed wire overlays. Decode game_type, game_progress, output flags, armor ID, and hurt type as separate scalar fields. Define:

~~~c
#define REFEREE_SOF 0xA5U
#define REFEREE_HEADER_SIZE 5U
#define REFEREE_FRAME_OVERHEAD 9U
#define REFEREE_STREAM_CAPACITY 256U
#define REFEREE_MAX_FRAME_SIZE 256U
#define REFEREE_MAX_INTERACTION_DATA 113U

#define REFEREE_CMD_GAME_STATE 0x0001U
#define REFEREE_CMD_GAME_RESULT 0x0002U
#define REFEREE_CMD_GAME_ROBOT_HP 0x0003U
#define REFEREE_CMD_EVENT_DATA 0x0101U
#define REFEREE_CMD_SUPPLY_ACTION 0x0102U
#define REFEREE_CMD_ROBOT_STATE 0x0201U
#define REFEREE_CMD_POWER_HEAT 0x0202U
#define REFEREE_CMD_ROBOT_POSITION 0x0203U
#define REFEREE_CMD_BUFF 0x0204U
#define REFEREE_CMD_AERIAL_ENERGY 0x0205U
#define REFEREE_CMD_ROBOT_HURT 0x0206U
#define REFEREE_CMD_SHOOT_DATA 0x0207U
#define REFEREE_CMD_STUDENT_INTERACTION 0x0301U
~~~

Port the CRC tables and algorithms from modules/referee/crc_ref.c and expose:

~~~c
uint8_t RefereeCrc8(const uint8_t *data, size_t length, uint8_t initial);
uint16_t RefereeCrc16(const uint8_t *data, size_t length, uint16_t initial);
bool RefereeCrc8Verify(const uint8_t *frame, size_t length);
bool RefereeCrc16Verify(const uint8_t *frame, size_t length);
void RefereeCrc8Append(uint8_t *frame, size_t length);
void RefereeCrc16Append(uint8_t *frame, size_t length);
~~~

- [ ] **Step 5: Implement the caller-owned parser and decoder**

Referee contains configuration, a 256-byte stream buffer, decoded state, diagnostics, timestamps, and an eight-bit transmit sequence. It has no global mutable state.

Port the proven stream logic from modules/referee/referee_stream_parser.c: discard pre-SOF noise, validate CRC8 before using length, reject oversized frames, retain incomplete frames, resynchronize to a later complete valid frame, validate CRC16, and consume every complete frame.

Decode values with explicit little-endian helpers:

~~~c
static uint16_t ReadU16LE(const uint8_t *bytes);
static uint32_t ReadU32LE(const uint8_t *bytes);
static uint64_t ReadU64LE(const uint8_t *bytes);
static float ReadF32LE(const uint8_t *bytes);
~~~

Only exact payload lengths update fixed-size commands. For 0x0301 require at least six bytes and at most 119 bytes. Count CRC-valid frames, CRC errors, oversized frames, discarded bytes, payload-length errors, and unsupported commands independently. Only correctly decoded 0x0201/0x0202 frames refresh their power timestamps.

Use wraparound-safe freshness:

~~~c
fresh = has_value &&
        (uint32_t)(now_ms - value_tick_ms) <= timeout_ms;
~~~

Default a zero timeout to 300 ms. Require both critical callbacks or neither. Never call uart_send while a critical section is held.

- [ ] **Step 6: Run the referee receive suite until green**

~~~powershell
make -C portable_modules/modules/referee/tests clean
make -C portable_modules/modules/referee/tests test
~~~

Expected: PASS: portable referee receive tests, with no warnings.

- [ ] **Step 7: Commit only Task 1 files**

~~~powershell
git add -- portable_modules/modules/referee/referee_protocol.h portable_modules/modules/referee/referee_crc.h portable_modules/modules/referee/referee_crc.c portable_modules/modules/referee/referee.h portable_modules/modules/referee/referee.c portable_modules/modules/referee/tests/Makefile portable_modules/modules/referee/tests/test_referee.c
git commit -m "feat: add portable referee receive module"
~~~

---

### Task 2: Portable referee UI and interaction transmission

**Files:**
- Create: portable_modules/modules/referee/referee_ui.h
- Create: portable_modules/modules/referee/referee_ui.c
- Modify: portable_modules/modules/referee/referee.h
- Modify: portable_modules/modules/referee/referee.c
- Modify: portable_modules/modules/referee/tests/Makefile
- Modify: portable_modules/modules/referee/tests/test_referee.c

**Interfaces:**
- Consumes: Referee, its UART callback, CRC functions, sender/receiver IDs, normal UI descriptions, and interaction bytes.
- Produces:

~~~c
typedef struct {
    uint8_t name[3];
    uint8_t operation;
    uint8_t type;
    uint8_t layer;
    uint8_t color;
    uint16_t start_angle;
    uint16_t end_angle;
    uint16_t width;
    uint16_t start_x;
    uint16_t start_y;
    uint16_t radius;
    uint16_t end_x;
    uint16_t end_y;
} RefereeGraphic;

typedef struct {
    RefereeGraphic graphic;
    uint8_t text[30];
    size_t length;
} RefereeText;

bool RefereeUiMakeLine(RefereeGraphic *graphic, const char name[3],
                       uint8_t operation, uint8_t layer, uint8_t color,
                       uint16_t width, uint16_t start_x, uint16_t start_y,
                       uint16_t end_x, uint16_t end_y);
bool RefereeUiMakeRectangle(RefereeGraphic *graphic, const char name[3],
                            uint8_t operation, uint8_t layer, uint8_t color,
                            uint16_t width, uint16_t start_x, uint16_t start_y,
                            uint16_t end_x, uint16_t end_y);
bool RefereeUiMakeCircle(RefereeGraphic *graphic, const char name[3],
                         uint8_t operation, uint8_t layer, uint8_t color,
                         uint16_t width, uint16_t center_x,
                         uint16_t center_y, uint16_t radius);
bool RefereeUiMakeEllipse(RefereeGraphic *graphic, const char name[3],
                          uint8_t operation, uint8_t layer, uint8_t color,
                          uint16_t width, uint16_t center_x,
                          uint16_t center_y, uint16_t x_radius,
                          uint16_t y_radius);
bool RefereeUiMakeArc(RefereeGraphic *graphic, const char name[3],
                      uint8_t operation, uint8_t layer, uint8_t color,
                      uint16_t start_angle, uint16_t end_angle,
                      uint16_t width, uint16_t center_x, uint16_t center_y,
                      uint16_t x_radius, uint16_t y_radius);
bool RefereeUiMakeFloat(RefereeGraphic *graphic, const char name[3],
                        uint8_t operation, uint8_t layer, uint8_t color,
                        uint16_t font_size, uint16_t decimal_digits,
                        uint16_t width, uint16_t start_x, uint16_t start_y,
                        int32_t scaled_value);
bool RefereeUiMakeInteger(RefereeGraphic *graphic, const char name[3],
                          uint8_t operation, uint8_t layer, uint8_t color,
                          uint16_t font_size, uint16_t width,
                          uint16_t start_x, uint16_t start_y, int32_t value);
bool RefereeUiMakeText(RefereeText *text, const char name[3],
                       uint8_t operation, uint8_t layer, uint8_t color,
                       uint16_t font_size, uint16_t width,
                       uint16_t start_x, uint16_t start_y,
                       const char *content);
bool RefereeUiDelete(Referee *instance, uint16_t sender_id,
                     uint16_t client_id, uint8_t operation, uint8_t layer);
bool RefereeUiSendGraphics(Referee *instance, uint16_t sender_id,
                           uint16_t client_id,
                           const RefereeGraphic *graphics, size_t count);
bool RefereeUiSendText(Referee *instance, uint16_t sender_id,
                       uint16_t client_id, const RefereeText *text);
bool RefereeSendInteraction(Referee *instance, uint16_t sender_id,
                            uint16_t receiver_id, uint16_t data_command_id,
                            const uint8_t *data, size_t data_length);
~~~

- [ ] **Step 1: Add failing golden-frame tests**

Add tests for exact graphic bytes, delete, graph counts 1/2/5/7, every builder, 30-byte text limit, interaction lengths 0 and 113, rejection at 114, sequence wraparound, and transmit failure diagnostics.

Representative assertion:

~~~c
RefereeGraphic line;
ASSERT_TRUE(RefereeUiMakeLine(&line, "abc", REFEREE_UI_ADD, 2U,
                              REFEREE_UI_GREEN, 3U,
                              100U, 200U, 300U, 400U));
ASSERT_TRUE(RefereeUiSendGraphics(&referee, 3U, 0x0103U, &line, 1U));
ASSERT_EQ_SIZE(30U, sent_length);
ASSERT_TRUE(RefereeCrc8Verify(sent_frame, 5U));
ASSERT_TRUE(RefereeCrc16Verify(sent_frame, sent_length));
ASSERT_EQ_U16(0x0101U, TestReadU16LE(&sent_frame[7]));
~~~

For the line above, preserve the current module's reversed three-byte name and
little-endian bit-field wire layout with this literal golden graphic payload:

~~~c
static const uint8_t expected_line_graphic[15] = {
    0x63, 0x62, 0x61, 0x81, 0x08, 0x00, 0x00, 0x03,
    0x90, 0x01, 0x19, 0x00, 0xB0, 0x04, 0x32
};
ASSERT_BYTES(expected_line_graphic, &sent_frame[13], 15U);
~~~

Generate representative arc, float, integer, text, delete, and interaction frames from the current module once, then place literal byte arrays in the new tests. New tests must never link existing project code.

- [ ] **Step 2: Run tests and verify red**

~~~powershell
make -C portable_modules/modules/referee/tests test
~~~

Expected: compilation fails because referee_ui.h and UI functions do not exist.

- [ ] **Step 3: Implement safe UI builders and explicit packing**

Validate all wire widths before modifying the output object: operation/type <= 7, layer/color <= 15, angles <= 511, width/radius <= 1023, and coordinates <= 2047. Clear unused fields. Copy exactly three name bytes. Limit text to 30 bytes and zero-fill unused bytes.

Encode each graph into exactly 15 bytes with explicit shifts and masks; never memcpy a bit-field structure.

- [ ] **Step 4: Implement UI and interaction frames**

Build frames in a local 256-byte buffer. Map graphic counts exactly:

~~~text
1 -> 0x0101
2 -> 0x0102
5 -> 0x0103
7 -> 0x0104
delete -> 0x0100
text -> 0x0110
outer command -> 0x0301
~~~

Write lengths and IDs in little endian and append both CRCs. Accept interaction data lengths 0 through 113. Advance the sequence once per send attempt, including failed callbacks, matching current attempt semantics while correcting the current graph-send sequence omission. Never call uart_send in a critical section.

- [ ] **Step 5: Run all referee tests until green**

~~~powershell
make -C portable_modules/modules/referee/tests clean
make -C portable_modules/modules/referee/tests test
~~~

Expected: PASS: portable referee tests, with no warnings.

- [ ] **Step 6: Commit only Task 2 files**

~~~powershell
git add -- portable_modules/modules/referee
git commit -m "feat: add portable referee UI and interaction"
~~~

---

### Task 3: Portable supercapacitor module

**Files:**
- Create: portable_modules/modules/super_cap/super_cap.h
- Create: portable_modules/modules/super_cap/super_cap.c
- Create: portable_modules/modules/super_cap/tests/Makefile
- Create: portable_modules/modules/super_cap/tests/test_super_cap.c

**Interfaces:**
- Consumes: classic-CAN frames, CAN-send and millisecond callbacks, and optional matching critical-section callbacks.
- Produces:

~~~c
typedef bool (*SuperCapCanSendFn)(void *context,
                                  uint32_t standard_id,
                                  const uint8_t *data,
                                  uint8_t dlc);
typedef uint32_t (*SuperCapNowMsFn)(void *context);
typedef void (*SuperCapCriticalFn)(void *context);

typedef struct {
    SuperCapCanSendFn can_send;
    SuperCapNowMsFn now_ms;
    SuperCapCriticalFn enter_critical;
    SuperCapCriticalFn exit_critical;
    void *context;
    uint32_t command_id;
    uint32_t status_id;
    uint32_t offline_timeout_ms;
} SuperCapConfig;

typedef struct {
    bool enable_dcdc;
    uint16_t referee_power_limit_w;
    uint16_t referee_buffer_energy_j;
} SuperCapCommand;

typedef struct {
    bool online;
    bool output_enabled;
    uint8_t error_code;
    float chassis_power_w;
    uint16_t available_power_limit_w;
    float energy_ratio;
} SuperCapStatus;

bool SuperCapInit(SuperCap *instance, const SuperCapConfig *config);
bool SuperCapReceive(SuperCap *instance, uint32_t standard_id,
                     const uint8_t *data, uint8_t dlc);
bool SuperCapSendCommand(SuperCap *instance,
                         const SuperCapCommand *command);
bool SuperCapGetStatus(SuperCap *instance, SuperCapStatus *status);
bool SuperCapGetDiagnostics(SuperCap *instance,
                            SuperCapDiagnostics *diagnostics);
~~~

- [ ] **Step 1: Write failing supercapacitor tests**

Create a Makefile using the same warning flags and -lm. Test default/custom IDs, callback validation, exact command bytes, valid status decode, wrong ID/DLC/null/NaN rejection, offline timeout boundary, tick wraparound, failed send diagnostics, and critical callback pairing.

Use current frames:

~~~c
static const uint8_t expected_command[8] = {
    0x01, 0x64, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00
};
static const uint8_t status_frame[8] = {
    0x00, 0x00, 0x00, 0xF1, 0x42, 0xB4, 0x00, 0x80
};
~~~

Assert power 120.5f, available power 180, energy 128/255, enabled output, and zero errors.

- [ ] **Step 2: Run tests and verify red**

~~~powershell
make -C portable_modules/modules/super_cap/tests test
~~~

Expected: compilation fails because super_cap.h and super_cap.c do not exist.

- [ ] **Step 3: Implement the caller-owned module**

Define:

~~~c
#define SUPER_CAP_DEFAULT_COMMAND_ID 0x061U
#define SUPER_CAP_DEFAULT_STATUS_ID 0x051U
#define SUPER_CAP_FRAME_DLC 8U
#define SUPER_CAP_DEFAULT_OFFLINE_TIMEOUT_MS 200U
~~~

Treat zero IDs as defaults and reject nonzero IDs above 0x7FF. Require can_send and now_ms plus either both critical callbacks or neither.

Port the command frame exactly: enable at byte 0, little-endian power limit at bytes 1-2, little-endian buffer at bytes 3-4, and zero bytes 5-7.

Decode error bits 0x7F, disabled bit 0x80, little-endian IEEE-754 power at bytes 1-4, available power at bytes 5-6, and energy byte/255. Decode locally and reject non-finite power before updating state. Track received, rejected, transmit-failure, and last-receive diagnostics. Compute online using unsigned subtraction. Never hold a critical section during can_send.

- [ ] **Step 4: Run the suite until green**

~~~powershell
make -C portable_modules/modules/super_cap/tests clean
make -C portable_modules/modules/super_cap/tests test
~~~

Expected: PASS: portable supercapacitor tests, with no warnings.

- [ ] **Step 5: Commit only Task 3 files**

~~~powershell
git add -- portable_modules/modules/super_cap
git commit -m "feat: add portable supercapacitor module"
~~~

---

### Task 4: In-module A-board documentation and isolation verification

**Files:**
- Create: portable_modules/modules/referee/README.md
- Create: portable_modules/modules/super_cap/README.md

**Interfaces:**
- Consumes: public APIs from Tasks 1-3.
- Produces: copy-ready integration instructions contained inside the two modules.

- [ ] **Step 1: Write the referee README**

Document 115200 8N1, arbitrary DMA/idle chunks, callback/object lifetime, concurrency, UI scheduling/rate responsibility, and the fixed protocol-version boundary. Include:

~~~c
static bool RefereeWrite(void *context, const uint8_t *data, size_t length)
{
    UART_HandleTypeDef *uart = context;
    return HAL_UART_Transmit_DMA(uart, (uint8_t *)data, length) == HAL_OK;
}

static uint32_t BoardClock(void *context)
{
    (void)context;
    return HAL_GetTick();
}

static Referee referee;
static const RefereeConfig config = {
    .uart_send = RefereeWrite,
    .now_ms = BoardClock,
    .context = &huart6,
    .offline_timeout_ms = 300U,
};

RefereeInit(&referee, &config);
RefereeFeed(&referee, dma_buffer, received_length);
~~~

State that the actual UART handle and pins come from the recipient's A-board schematic/CubeMX project.

- [ ] **Step 2: Write the supercapacitor README**

Document classic CAN, standard IDs, DLC 8, RX filter, callbacks, frame tables, and fault masks. Include:

~~~c
static bool CapCanWrite(void *context, uint32_t id,
                        const uint8_t *data, uint8_t dlc)
{
    CAN_HandleTypeDef *can = context;
    CAN_TxHeaderTypeDef header = {
        .StdId = id,
        .IDE = CAN_ID_STD,
        .RTR = CAN_RTR_DATA,
        .DLC = dlc,
    };
    uint32_t mailbox;
    return HAL_CAN_AddTxMessage(can, &header,
                                (uint8_t *)data, &mailbox) == HAL_OK;
}

static SuperCap cap;
static const SuperCapConfig config = {
    .can_send = CapCanWrite,
    .now_ms = BoardClock,
    .context = &hcan1,
};

SuperCapInit(&cap, &config);
SuperCapReceive(&cap, rx_header.StdId, rx_data, rx_header.DLC);
~~~

State that CAN startup, RX notifications, and the status-ID filter remain in the A-board application.

- [ ] **Step 3: Run both standalone suites**

~~~powershell
make -C portable_modules/modules/referee/tests clean
make -C portable_modules/modules/referee/tests test
make -C portable_modules/modules/super_cap/tests clean
make -C portable_modules/modules/super_cap/tests test
~~~

Expected: both suites print PASS without warnings.

- [ ] **Step 4: Audit dependencies and package shape**

~~~powershell
rg -n -S "stm32|FreeRTOS|cmsis_os|bsp_|daemon|robot_def|HAL_|malloc|calloc|realloc|free" portable_modules/modules -g "*.c" -g "*.h"
Get-ChildItem portable_modules/modules -Directory | Select-Object -ExpandProperty Name
~~~

Expected: no production dependency matches; directory output is exactly referee and super_cap.

- [ ] **Step 5: Prove existing files stayed untouched**

~~~powershell
git hash-object Src/usart.c application/chassis/chassis.c basic_framework.ioc modules/referee/rm_referee.c modules/power_meter/power_meter.c modules/power_meter/power_meter.h tests/power_meter/test_power_meter.c
git status --short
git diff --check
~~~

Expected: hashes exactly match Task 1; every original modification remains; feature changes are limited to new package and approved design/plan files.

- [ ] **Step 6: Commit only the two READMEs**

~~~powershell
git add -- portable_modules/modules/referee/README.md portable_modules/modules/super_cap/README.md
git commit -m "docs: explain A board module integration"
~~~

- [ ] **Step 7: Request final code review**

Give the reviewer the design specification, this plan, commit range beginning at d47feba, protected-file hashes, and both test commands. Fix every critical or important finding only inside portable_modules, then repeat Steps 3-5.
