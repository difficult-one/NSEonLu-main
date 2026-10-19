# RTT Output Centralization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Confine direct SEGGER RTT access to the BSP log transport and move every structured RTT header and row format into one telemetry module without changing the existing chassis-power stream.

**Architecture:** `bsp/log/bsp_log.c` becomes the sole project-owned SEGGER RTT caller and exposes formatted, string, raw-byte, and clear operations. `modules/telemetry/rtt_telemetry.c` owns the chassis-power header, CSV conversion, private output buffer, and writes through `bsp_log`; `dji_motor.c` retains only the 10:1 emission schedule and passes current data into that module.

**Tech Stack:** C11, STM32F407, FreeRTOS, SEGGER RTT, GNU Arm Embedded Make/CMake builds, host GCC/Make tests, PowerShell architecture checks.

## Global Constraints

- Preserve the exact 19-column chassis-power CSV schema and `\r\n` line ending.
- Preserve nominal 100 Hz output by retaining the existing 10:1 divider in the 1 kHz DJI motor-control path.
- Do not change power calculation, allocation, limiting, motor commands, or measured-power data flow.
- Keep the existing `LOG`, `LOGINFO`, `LOGWARNING`, `LOGERROR`, `LOG_CLEAR`, `PrintLog`, and `Float2Str` source-level interfaces.
- Only `bsp/log/bsp_log.c` may call project-owned `SEGGER_RTT_*` functions.
- Structured RTT headers, row formats, and private output buffers belong in `modules/telemetry/rtt_telemetry.c`.
- RTT remains non-blocking and observational; failed or short output must not affect control behavior.

## File Structure

- Create `modules/telemetry/rtt_telemetry.h`: public structured telemetry interface and CSV maximum length.
- Create `modules/telemetry/rtt_telemetry.c`: chassis-power header, pure CSV formatter, private row buffer, and BSP-backed write functions.
- Modify `bsp/log/bsp_log.h`: declare raw/string/clear wrappers and route existing macros through BSP functions.
- Modify `bsp/log/bsp_log.c`: contain every project-owned direct SEGGER RTT call.
- Modify `modules/algorithm/power_model.h`: remove CSV-specific size constant and formatter declaration.
- Modify `modules/algorithm/power_model.c`: remove CSV conversion helpers and formatter implementation.
- Modify `modules/motor/DJImotor/dji_motor.c`: replace local RTT details with telemetry API calls while retaining the divider.
- Modify `Makefile`: compile and include the new telemetry module for the GNU Arm build.
- Modify `bsp/log/bsp_log.md`: document the transport/structured-telemetry split and future placement rule.
- Create `tests/bsp_log/*`: host tests and SEGGER stubs for the BSP transport wrappers.
- Create `tests/rtt_telemetry/*`: host tests and BSP stub for exact header/CSV/output behavior.
- Create `tests/rtt_architecture/test_rtt_architecture.ps1`: enforce the ownership boundary with source checks.
- Modify `tests/power_model/test_power_model.c`: remove tests that move with the CSV formatter; retain snapshot-building tests.

---

### Task 1: Encapsulate SEGGER RTT Transport in `bsp_log`

**Files:**
- Create: `tests/bsp_log/stubs/SEGGER_RTT.h`
- Create: `tests/bsp_log/stubs/SEGGER_RTT_Conf.h`
- Create: `tests/bsp_log/test_bsp_log.c`
- Create: `tests/bsp_log/Makefile`
- Modify: `bsp/log/bsp_log.h`
- Modify: `bsp/log/bsp_log.c`

**Interfaces:**
- Consumes: SEGGER `SEGGER_RTT_Init`, `SEGGER_RTT_vprintf`, `SEGGER_RTT_Write`, and `SEGGER_RTT_WriteString`.
- Produces: `unsigned BSPLogWrite(const void *data, unsigned length)`, `unsigned BSPLogWriteString(const char *text)`, and `void BSPLogClear(void)`; existing logging interfaces remain unchanged.

- [ ] **Step 1: Add SEGGER test stubs and a failing BSP transport test**

Create `tests/bsp_log/stubs/SEGGER_RTT_Conf.h` with only an include guard. Create
`tests/bsp_log/stubs/SEGGER_RTT.h` with the color/control constants used by
`bsp_log.h` and these declarations:

```c
#ifndef TEST_SEGGER_RTT_H
#define TEST_SEGGER_RTT_H

#include <stdarg.h>

#define RTT_CTRL_CLEAR "\x1B[2J"
#define RTT_CTRL_RESET "\x1B[0m"
#define RTT_CTRL_TEXT_BRIGHT_GREEN "\x1B[92m"
#define RTT_CTRL_TEXT_BRIGHT_YELLOW "\x1B[93m"
#define RTT_CTRL_TEXT_BRIGHT_RED "\x1B[91m"

void SEGGER_RTT_Init(void);
int SEGGER_RTT_vprintf(unsigned buffer_index, const char *format, va_list *args);
unsigned SEGGER_RTT_Write(unsigned buffer_index, const void *data, unsigned length);
unsigned SEGGER_RTT_WriteString(unsigned buffer_index, const char *text);

#endif
```

In `test_bsp_log.c`, implement the four SEGGER functions as capture stubs and
assert the new wrappers use buffer zero and preserve bytes exactly:

```c
static void TestRawWriteUsesDefaultBuffer(void)
{
    static const char data[] = {'a', '\0', 'b'};
    ResetCapture();
    ASSERT_EQ_UINT(3U, BSPLogWrite(data, 3U));
    ASSERT_EQ_UINT(0U, captured_buffer_index);
    ASSERT_EQ_UINT(3U, captured_length);
    ASSERT_TRUE(memcmp(data, captured_data, 3U) == 0);
}

static void TestStringWriteUsesDefaultBuffer(void)
{
    ResetCapture();
    ASSERT_EQ_UINT(5U, BSPLogWriteString("hello"));
    ASSERT_EQ_UINT(0U, captured_buffer_index);
    ASSERT_STREQ("hello", captured_data);
}

static void TestClearWritesTerminalControlSequence(void)
{
    ResetCapture();
    BSPLogClear();
    ASSERT_STREQ("  " RTT_CTRL_CLEAR, captured_data);
}
```

Use this `tests/bsp_log/Makefile` structure:

```make
CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_bsp_log.exe
SOURCE := test_bsp_log.c ../../bsp/log/bsp_log.c
INCLUDES := -Istubs -I../../bsp/log

.PHONY: all test clean
all: $(TARGET)
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -o $(TARGET)
test: $(TARGET)
	./$(TARGET)
clean:
	-if exist $(TARGET) del /Q $(TARGET)
```

- [ ] **Step 2: Run the BSP test and verify RED**

Run:

```powershell
make -C tests/bsp_log clean test
```

Expected: compilation fails because `BSPLogWrite`, `BSPLogWriteString`, and
`BSPLogClear` are not declared or defined.

- [ ] **Step 3: Add the minimal BSP wrapper implementation**

Add declarations to `bsp_log.h`. Change `LOG_PROTO` to call `PrintLog` and
change `LOG_CLEAR()` to call `BSPLogClear()`:

```c
unsigned BSPLogWrite(const void *data, unsigned length);
unsigned BSPLogWriteString(const char *text);
void BSPLogClear(void);

#define LOG_PROTO(type, color, format, ...)                 \
    PrintLog("  %s%s" format "\r\n%s",                    \
             color, type, ##__VA_ARGS__, RTT_CTRL_RESET)

#define LOG_CLEAR() BSPLogClear()
```

Implement the wrappers in `bsp_log.c`, keeping all direct SEGGER calls there:

```c
unsigned BSPLogWrite(const void *data, unsigned length)
{
    return SEGGER_RTT_Write(BUFFER_INDEX, data, length);
}

unsigned BSPLogWriteString(const char *text)
{
    return SEGGER_RTT_WriteString(BUFFER_INDEX, text);
}

void BSPLogClear(void)
{
    (void)BSPLogWriteString("  " RTT_CTRL_CLEAR);
}
```

Include `<stdlib.h>` explicitly for the existing `abs` calls.

- [ ] **Step 4: Run the BSP test and verify GREEN**

Run:

```powershell
make -C tests/bsp_log clean test
```

Expected: `PASS: bsp log tests` with no compiler warnings.

- [ ] **Step 5: Commit the transport boundary**

```powershell
git add -- bsp/log/bsp_log.h bsp/log/bsp_log.c tests/bsp_log
git commit -m "refactor: encapsulate SEGGER RTT transport"
```

---

### Task 2: Move Chassis-Power Formatting into the RTT Telemetry Module

**Files:**
- Create: `modules/telemetry/rtt_telemetry.h`
- Create: `modules/telemetry/rtt_telemetry.c`
- Create: `tests/rtt_telemetry/stubs/bsp_log.h`
- Create: `tests/rtt_telemetry/test_rtt_telemetry.c`
- Create: `tests/rtt_telemetry/Makefile`
- Modify: `modules/algorithm/power_model.h`
- Modify: `modules/algorithm/power_model.c`
- Modify: `tests/power_model/test_power_model.c`

**Interfaces:**
- Consumes: `ChassisPowerTelemetry_s` from `power_model.h` and `BSPLogWrite`/`BSPLogWriteString` from `bsp_log.h`.
- Produces: `RTT_TELEMETRY_CHASSIS_POWER_CSV_MAX_LENGTH`, `RTTTelemetryFormatChassisPowerCsv`, `RTTTelemetryWriteChassisPowerHeader`, and `RTTTelemetryWriteChassisPowerRow`.

- [ ] **Step 1: Write failing telemetry tests against the desired API**

Create `tests/rtt_telemetry/stubs/bsp_log.h`:

```c
#ifndef TEST_BSP_LOG_H
#define TEST_BSP_LOG_H
unsigned BSPLogWrite(const void *data, unsigned length);
unsigned BSPLogWriteString(const char *text);
#endif
```

Create `test_rtt_telemetry.c` with capture implementations for those two
functions. Use `RTT_TELEMETRY_CHASSIS_POWER_CSV_MAX_LENGTH` for the row buffer and
add these exact behavior checks:

```c
static void TestHeaderIsWrittenExactly(void)
{
    ResetCapture();
    RTTTelemetryWriteChassisPowerHeader();
    ASSERT_STREQ(
        "tick,i_lf,i_rf,i_lb,i_rb,rpm_lf,rpm_rf,rpm_lb,rpm_rb,"
        "target_lf,target_rf,target_lb,target_rb,p_lf_cw,p_rf_cw,"
        "p_lb_cw,p_rb_cw,p_total_cw,p_cap_cw\r\n",
        captured_data);
}

static void TestRepresentativeRowPreservesAllFields(void)
{
    ChassisPowerTelemetry_s telemetry = {
        .current_command = {2000.0f, 2100.0f, 2200.0f, 2300.0f},
        .actual_speed_rpm = {1000.0f, 1100.0f, 1200.0f, 1300.0f},
        .target_speed_rpm = {1010.0f, 1111.0f, 1212.0f, 1313.0f},
        .predicted_power_w = {10.0f, 11.0f, 12.0f, 13.0f},
        .predicted_total_power_w = 46.0f,
    };
    char row[RTT_TELEMETRY_CHASSIS_POWER_CSV_MAX_LENGTH];

    ASSERT_TRUE(RTTTelemetryFormatChassisPowerCsv(
                    &telemetry, 123U, 12.34f, row, sizeof(row)) > 0U);
    ASSERT_STREQ(
        "123,2000,2100,2200,2300,1000,1100,1200,1300,"
        "1010,1111,1212,1313,1000,1100,1200,1300,4600,1234\r\n",
        row);
}
```

Add separate tests with these exact expected rows:

```text
measured -1.25 W -> 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-125\r\n
measured NAN    -> 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\r\n
measured FLT_MAX-> 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2147483647\r\n
power[0] FLT_MAX->1,0,0,0,0,0,0,0,0,0,0,0,0,2147483647,0,0,0,0,0\r\n
```

Also assert that null telemetry, null buffer, zero capacity, and an 8-byte
buffer each return zero; buffers with storage must have `buffer[0] == '\0'`.
Finally call `RTTTelemetryWriteChassisPowerRow` with a zero snapshot and assert
the capture contains one complete 19-field row.

Use this test Makefile:

```make
CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g
TARGET := test_rtt_telemetry.exe
SOURCE := test_rtt_telemetry.c ../../modules/telemetry/rtt_telemetry.c
INCLUDES := -Istubs -I../../modules/telemetry -I../../modules/algorithm

.PHONY: all test clean
all: $(TARGET)
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCE) -lm -o $(TARGET)
test: $(TARGET)
	./$(TARGET)
clean:
	-if exist $(TARGET) del /Q $(TARGET)
```

- [ ] **Step 2: Run the telemetry test and verify RED**

Run:

```powershell
make -C tests/rtt_telemetry clean test
```

Expected: compilation fails because `rtt_telemetry.h/.c` and its API do not yet
exist.

- [ ] **Step 3: Implement the focused telemetry module**

Create `rtt_telemetry.h` with:

```c
#ifndef RTT_TELEMETRY_H
#define RTT_TELEMETRY_H

#include "power_model.h"
#include <stddef.h>
#include <stdint.h>

#define RTT_TELEMETRY_CHASSIS_POWER_CSV_MAX_LENGTH 256U

size_t RTTTelemetryFormatChassisPowerCsv(
    const ChassisPowerTelemetry_s *telemetry,
    uint32_t tick_ms,
    float measured_chassis_power_w,
    char *buffer,
    size_t buffer_size);
void RTTTelemetryWriteChassisPowerHeader(void);
void RTTTelemetryWriteChassisPowerRow(
    const ChassisPowerTelemetry_s *telemetry,
    uint32_t tick_ms,
    float measured_chassis_power_w);

#endif
```

In `rtt_telemetry.c`, move `RoundTelemetryValue`, `PowerToCentiwatts`, and the
existing `snprintf` format unchanged from `power_model.c`. Rename the public
formatter to `RTTTelemetryFormatChassisPowerCsv`. Add a private row buffer and
the two write functions:

```c
static char chassis_power_row[RTT_TELEMETRY_CHASSIS_POWER_CSV_MAX_LENGTH];

void RTTTelemetryWriteChassisPowerHeader(void)
{
    (void)BSPLogWriteString(
        "tick,i_lf,i_rf,i_lb,i_rb,rpm_lf,rpm_rf,rpm_lb,rpm_rb,"
        "target_lf,target_rf,target_lb,target_rb,p_lf_cw,p_rf_cw,"
        "p_lb_cw,p_rb_cw,p_total_cw,p_cap_cw\r\n");
}

void RTTTelemetryWriteChassisPowerRow(
    const ChassisPowerTelemetry_s *telemetry,
    uint32_t tick_ms,
    float measured_chassis_power_w)
{
    const size_t length = RTTTelemetryFormatChassisPowerCsv(
        telemetry,
        tick_ms,
        measured_chassis_power_w,
        chassis_power_row,
        sizeof(chassis_power_row));
    if (length > 0U)
        (void)BSPLogWrite(chassis_power_row, (unsigned)length);
}
```

- [ ] **Step 4: Run telemetry tests and verify GREEN**

Run:

```powershell
make -C tests/rtt_telemetry clean test
```

Expected: `PASS: RTT telemetry tests` with no warnings.

- [ ] **Step 5: Remove CSV ownership from the power model**

Delete `CHASSIS_POWER_TELEMETRY_CSV_MAX_LENGTH` and
`ChassisPowerTelemetryFormatCsv` from `power_model.h`. Delete
`RoundTelemetryValue`, `PowerToCentiwatts`, and
`ChassisPowerTelemetryFormatCsv` from `power_model.c`. Remove the six CSV tests
and their calls from `tests/power_model/test_power_model.c`; keep the three
snapshot-building tests intact.

- [ ] **Step 6: Run both affected host test suites**

Run:

```powershell
make -C tests/power_model clean test
make -C tests/rtt_telemetry clean test
```

Expected: both suites print `PASS` and compile without warnings.

- [ ] **Step 7: Commit structured telemetry ownership**

```powershell
git add -- modules/telemetry modules/algorithm/power_model.h modules/algorithm/power_model.c tests/rtt_telemetry tests/power_model/test_power_model.c
git commit -m "refactor: centralize RTT telemetry formatting"
```

---

### Task 3: Integrate DJI Emission and Enforce the Boundary

**Files:**
- Create: `tests/rtt_architecture/test_rtt_architecture.ps1`
- Modify: `modules/motor/DJImotor/dji_motor.c`
- Modify: `Makefile`
- Modify: `bsp/log/bsp_log.md`

**Interfaces:**
- Consumes: `RTTTelemetryWriteChassisPowerHeader(void)` and `RTTTelemetryWriteChassisPowerRow(const ChassisPowerTelemetry_s *, uint32_t, float)` from Task 2.
- Produces: unchanged one-time header emission and nominal 100 Hz chassis-power rows, with the RTT ownership rule executable as a source check.

- [ ] **Step 1: Write a failing architecture test**

Create `tests/rtt_architecture/test_rtt_architecture.ps1` that runs from the
repository root and exits nonzero for direct calls outside `bsp_log.c` or for
CSV ownership left in the power/motor modules:

```powershell
$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
Push-Location $repoRoot
try {
    $matches = @(rg -n 'SEGGER_RTT_[A-Za-z0-9_]+\s*\(' `
        -g '*.c' -g '*.h' -g '!Middlewares/**' -g '!tests/**' `
        -g '!docs/**' -g '!AI/**' .)
    $violations = @($matches | Where-Object {
        (($_ -replace '\\', '/') -notmatch '^\./bsp/log/bsp_log\.c:')
    })
    if ($violations.Count -ne 0) {
        $violations | ForEach-Object { Write-Error "Direct RTT call: $_" }
        exit 1
    }

    if (Select-String -Quiet -Path 'modules/algorithm/power_model.c', `
            'modules/motor/DJImotor/dji_motor.c' `
            -Pattern 'p_cap_cw|ChassisPowerTelemetryFormatCsv') {
        Write-Error 'RTT CSV ownership remains outside rtt_telemetry.c'
        exit 1
    }

    Write-Output 'PASS: RTT architecture checks'
}
finally {
    Pop-Location
}
```

- [ ] **Step 2: Run the architecture test and verify RED**

Run:

```powershell
pwsh -NoProfile -File tests/rtt_architecture/test_rtt_architecture.ps1
```

Expected: FAIL showing the two remaining direct RTT calls and CSV header in
`dji_motor.c`.

- [ ] **Step 3: Replace DJI-owned RTT details with telemetry calls**

In `dji_motor.c`:

- include `rtt_telemetry.h`;
- keep `CHASSIS_POWER_RTT_DIVIDER`, `chassis_power_rtt_count`, and its reset;
- delete `chassis_power_rtt_row` and `PrintChassisPowerTelemetry`;
- replace the registration-time `SEGGER_RTT_WriteString` block with
  `RTTTelemetryWriteChassisPowerHeader();`;
- replace `PrintChassisPowerTelemetry(&telemetry);` with:

```c
RTTTelemetryWriteChassisPowerRow(
    &telemetry,
    HAL_GetTick(),
    chassis_power->measured_chassis_power_w);
```

- [ ] **Step 4: Add the telemetry module to the GNU Arm Makefile**

Add `modules/telemetry/rtt_telemetry.c` to `C_SOURCES` beside the algorithm and
motor modules. Add `-Imodules/telemetry` to the include list. Do not change
`CMakeLists.txt`; its recursive `modules/*.c` source glob and recursive modules
include setup already discover the new directory.

- [ ] **Step 5: Document where future RTT output belongs**

Update `bsp/log/bsp_log.md` so its usage section states:

```text
- 普通分级日志继续使用 bsp_log.h 中的 LOG/LOGINFO/LOGWARNING/LOGERROR。
- 结构化 RTT 数据（CSV 表头、字段格式、输出缓冲区）统一放在
  modules/telemetry/rtt_telemetry.c/.h。
- 业务模块只能调用上述接口，不直接调用 SEGGER_RTT_*。
```

Also correct the stale examples from `printf_log` to the existing `PrintLog`
name while editing this document.

- [ ] **Step 6: Run the architecture test and verify GREEN**

Run:

```powershell
pwsh -NoProfile -File tests/rtt_architecture/test_rtt_architecture.ps1
```

Expected: `PASS: RTT architecture checks`.

- [ ] **Step 7: Run all directly affected host tests**

Run:

```powershell
make -C tests/bsp_log clean test
make -C tests/power_model clean test
make -C tests/rtt_telemetry clean test
```

Expected: all three suites print `PASS` without warnings.

- [ ] **Step 8: Build the complete firmware with both supported build paths**

Run the GNU Arm Make build:

```powershell
make -j4
```

Then configure and build CMake in its existing build directory if present; if
none exists, use a new `build-cmake-rtt` directory:

```powershell
cmake -S . -B build-cmake-rtt -G Ninja
cmake --build build-cmake-rtt
```

Expected: both builds complete without warnings or link errors and include
`rtt_telemetry.c`.

- [ ] **Step 9: Inspect the final diff for scope and format preservation**

Run:

```powershell
git diff --check
git diff --stat
rg -n 'tick,i_lf|p_cap_cw' modules
rg -n 'SEGGER_RTT_[A-Za-z0-9_]+\s*\(' bsp modules application Src
```

Expected: the CSV header appears only in `modules/telemetry/rtt_telemetry.c`;
project-owned direct calls appear only in `bsp/log/bsp_log.c`; no unrelated
files are changed.

- [ ] **Step 10: Commit the integration and guardrail**

```powershell
git add -- modules/motor/DJImotor/dji_motor.c Makefile bsp/log/bsp_log.md tests/rtt_architecture/test_rtt_architecture.ps1
git commit -m "refactor: route chassis telemetry through RTT module"
```
