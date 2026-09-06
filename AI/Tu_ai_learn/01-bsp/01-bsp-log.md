# BSP Log — 日志系统（SEGGER RTT）

## 模块职责

基于 SEGGER RTT 提供轻量级、非阻塞的日志输出能力，支持多级别彩色日志和浮点数转字符串打印。

---

## 设计思路

### 为什么用 SEGGER RTT 而不是 UART/printf？

1. **零阻塞** — RTT 通过 RAM 缓冲区与调试器通信，不占用 UART 资源，不阻塞实时任务
2. **高速** — 写 RAM 比串口快几个数量级，适合高频日志输出
3. **免接线** — 只需 J-Link 调试器即可查看，无需额外串口转 USB 模块

### 为什么需要 Float2Str？

SEGGER RTT 的 `SEGGER_RTT_printf()` 是精简实现，**不支持 `%f` 格式化**。直接用 `%f` 会输出空字符串或乱码。因此框架提供了 `Float2Str()` 将 float 转为字符串后再用 `%s` 打印。

---

## 核心数据结构

本模块没有复杂的数据结构，核心是几个宏定义。

### BUFFER_INDEX

```c
// basic_framework/bsp/log/bsp_log.h:8
#define BUFFER_INDEX 0
```

SEGGER RTT 支持多个虚拟终端（buffer），本框架只使用第 0 号通道。所有日志输出都写入同一个通道。

### 日志级别与颜色

| 宏 | 级别 | 颜色 | 格式前缀 |
|----|------|------|----------|
| `LOG(format, ...)` | 通用 | 无色 | 无 |
| `LOGINFO(format, ...)` | 信息 | 绿色 | `I:` |
| `LOGWARNING(format, ...)` | 警告 | 黄色 | `W:` |
| `LOGERROR(format, ...)` | 错误 | 红色 | `E:` |

---

## 函数详解

### BSPLogInit()

```c
// basic_framework/bsp/log/bsp_log.c:8-11
void BSPLogInit()
{
    SEGGER_RTT_Init();
}
```

- **调用时机**：在 `BSPInit()` 中被调用，紧跟 `DWT_Init(168)` 之后
- **功能**：初始化 SEGGER RTT 的内部缓冲区和控制块
- **调用链**：`BSPInit()` -> `BSPLogInit()` -> `SEGGER_RTT_Init()`

### LOG_PROTO 宏

```c
// basic_framework/bsp/log/bsp_log.h:20-25
#define LOG_PROTO(type, color, format, ...)                       \
        SEGGER_RTT_printf(BUFFER_INDEX, "  %s%s" format "\r\n%s", \
                          color,                                  \
                          type,                                   \
                          ##__VA_ARGS__,                          \
                          RTT_CTRL_RESET)
```

这是所有日志宏的底层原型，参数含义：

- `type`：级别标识，如 `"I:"`、`"W:"`、`"E:"`
- `color`：RTT 颜色控制码，如 `RTT_CTRL_TEXT_BRIGHT_GREEN`
- `format`：格式字符串（不支持 `%f`）
- `##__VA_ARGS__`：可变参数，`##` 使得无额外参数时不报错
- `RTT_CTRL_RESET`：输出后重置颜色，避免后续输出带色

输出格式：`  I:内容\r\n`（前面有两个空格缩进）。

### LOG / LOGINFO / LOGWARNING / LOGERROR

```c
// basic_framework/bsp/log/bsp_log.h:33-51
#define LOG(format, ...) LOG_PROTO("", "", format, ##__VA_ARGS__)

#if DISABLE_LOG_SYSTEM
#define LOGINFO(format, ...)
#define LOGWARNING(format, ...)
#define LOGERROR(format, ...)
#else
#define LOGINFO(format, ...) LOG_PROTO("I:", RTT_CTRL_TEXT_BRIGHT_GREEN, format, ##__VA_ARGS__)
#define LOGWARNING(format, ...) LOG_PROTO("W:", RTT_CTRL_TEXT_BRIGHT_YELLOW, format, ##__VA_ARGS__)
#define LOGERROR(format, ...) LOG_PROTO("E:", RTT_CTRL_TEXT_BRIGHT_RED, format, ##__VA_ARGS__)
#endif
```

- **条件编译**：当 `DISABLE_LOG_SYSTEM` 宏为真时，所有带颜色的日志宏展开为空，完全不产生代码。这用于 Release 版本减少体积和运行时开销。
- **注意**：无颜色的 `LOG()` 不受 `DISABLE_LOG_SYSTEM` 控制，始终可用（适合关键调试信息）。

### PrintLog()

```c
// basic_framework/bsp/log/bsp_log.c:13-20
int PrintLog(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = SEGGER_RTT_vprintf(BUFFER_INDEX, fmt, &args);
    va_end(args);
    return n;
}
```

- **功能**：对 `SEGGER_RTT_vprintf()` 的可变参数封装，方便不使用宏的场景直接调用
- **参数**：`fmt` 格式字符串，`...` 可变参数
- **返回值**：打印的字符数
- **注意**：同样不支持 `%f`

### Float2Str()

```c
// basic_framework/bsp/log/bsp_log.c:22-33
void Float2Str(char *str, float va)
{
    int flag = va < 0;       // 判断是否为负数
    int head = (int)va;      // 取整数部分（负数时会保留负号）
    int point = (int)((va - head) * 1000);  // 取小数部分，精确到3位
    head = abs(head);        // 取绝对值，负号由 flag 单独处理
    point = abs(point);      // 取绝对值
    if (flag)
        sprintf(str, "-%d.%d", head, point);
    else
        sprintf(str, "%d.%d", head, point);
}
```

- **功能**：将 float 转换为字符串，保留 3 位小数
- **参数**：`str` 输出字符串缓冲区（调用者保证空间足够），`va` 待转换浮点数
- **关键逻辑**：
  1. 先判断正负，记录 `flag`
  2. 整数部分用 `(int)` 强转提取（对负数如 -3.14，`(int)` 得到 -3）
  3. 小数部分 = 原值 - 整数部分，再乘 1000 取整
  4. `abs()` 取绝对值后，根据 `flag` 决定是否添加负号
- **精度**：固定 3 位小数，不支持自定义精度

**使用示例**：

```c
float val = -3.14159f;
char buf[20];
Float2Str(buf, val);  // buf = "-3.141"
LOGINFO("value = %s", buf);
```

### LOG_CLEAR()

```c
// basic_framework/bsp/log/bsp_log.h:30
#define LOG_CLEAR() SEGGER_RTT_WriteString(0, "  " RTT_CTRL_CLEAR)
```

清空 RTT 终端屏幕，调试时偶尔使用。

---

## 调用链

### 初始化调用链

```
RobotInit()
  +-- BSPInit()
        +-- DWT_Init(168)    // 先初始化计时器
        +-- BSPLogInit()     // 再初始化日志
              +-- SEGGER_RTT_Init()
```

### 日志输出调用链

```
LOGINFO("val=%d", x)
  +-- LOG_PROTO("I:", RTT_CTRL_TEXT_BRIGHT_GREEN, "val=%d", x)
        +-- SEGGER_RTT_printf(0, "  %s%s" "val=%d" "\r\n%s", color, "I:", x, reset)
              +-- RTT 写入 RAM 缓冲区
                    +-- J-Link 读取并显示在 RTT Viewer
```

### 浮点日志调用链

```
LOGINFO("pos=%s", Float2Str(buf, val))
  // 注意：Float2Str 是函数调用，不能直接嵌在 LOGINFO 的参数中
  // 正确用法是先调用 Float2Str，再用 %s 打印
  Float2Str(buf, val);
  LOGINFO("pos=%s", buf);
```

---

## 注意事项

1. **不支持 `%f`** — 这是 SEGGER RTT 的精简实现决定的，不是 bug。使用 `Float2Str()` 转换后再用 `%s` 打印。
2. **DISABLE_LOG_SYSTEM 的作用** — 在 Makefile 中定义此宏可以完全关闭日志输出，减少代码体积。但 `LOG()` 宏不受此控制，始终生效。
3. **Float2Str 精度限制** — 只保留 3 位小数，对于需要更高精度的场景（如 IMU 数据），可能需要自行修改或扩展。
4. **Float2Str 的缓冲区** — 调用者需要保证 `str` 指向的空间足够容纳结果（约 15 字节即可）。
5. **RTT 需要 J-Link 调试器** — 不连接 J-Link 时日志无处输出，但不会阻塞程序运行（RTT 写入 RAM 缓冲区，缓冲区满后丢弃）。
6. **日志的实时性** — RTT 写入速度极快（纳秒级），不会对实时任务造成影响，可以放心在中断中使用。
7. **`PrintLog()` vs 宏** — `PrintLog()` 是函数调用，有函数调用开销；宏是内联展开，效率更高。优先使用宏。
