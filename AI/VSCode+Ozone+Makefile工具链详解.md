# 工程工具链详解：VSCode + Ozone + Makefile

## 一、工具链全景

本项目是**湖南大学跃鹿战队 RoboMaster 嵌入式通用框架**，目标平台是 **STM32F407IG**（Cortex-M4 内核），运行 FreeRTOS。整个工具链可以概括为：

```
编辑(VSCode) → 编译(Makefile+arm-gnu-gcc) → 下载/调试(OpenOCD/JLink+Ozone)
```

```
┌────────────────────────────────────────────────────────────────────┐
│                        VSCode (编辑器/IDE)                          │
│  ┌──────────┐  ┌──────────────┐  ┌─────────────┐  ┌─────────────┐ │
│  │ 代码编辑  │  │ IntelliSense │  │  Task 任务   │  │  Debug 调试  │ │
│  │ 高亮/补全 │  │ 跳转/静态检查 │  │ 编译/下载    │  │ 断点/变量查看 │ │
│  └──────────┘  └──────────────┘  └──────┬──────┘  └──────┬──────┘ │
└─────────────────────────────────────────┼────────────────┼─────────┘
                                          │                │
                    ┌─────────────────────▼────────┐       │
                    │   mingw32-make (构建工具)      │       │
                    │   读取 Makefile 规则文件       │       │
                    └─────────────┬───────────────┘       │
                                  │                       │
              ┌───────────────────▼──────────────────────┐│
              │        Arm GNU Toolchain                 ││
              │  ┌─────────────────┐                    ││
              │  │arm-none-eabi-gcc│  编译器+汇编器      ││
              │  │arm-none-eabi-ld │  链接器             ││
              │  │arm-none-eabi-objcopy│ 格式转换(elf→hex/bin)│
              │  │arm-none-eabi-size│  体积统计           ││
              │  │arm-none-eabi-gdb│  GDB调试器          ││
              │  └─────────────────┘                    ││
              └──────────────────┬──────────────────────┘│
                                 │                       │
              ┌──────────────────▼──────────────────────┐│
              │     GDB Server (调试桥梁)               ││
              │  OpenOCD / JLinkGDBServer               ││
              │  连接硬件调试器 (DAP/JLink/STLink)       ││
              └──────────────────┬──────────────────────┘│
                                 │                       │
              ┌──────────────────▼──────────────────────┐│
              │        硬件调试器 + STM32F407           ││
              │    J-Link / DAP-Link / ST-Link          ││
              └─────────────────────────────────────────┘│
┌─────────────────────────────────────────────────────────┴──────────┐
│                    Ozone (高级调试/可视化)                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────────┐  │
│  │ 变量动态监视  │  │ 示波器可视化  │  │ RTT Log / 寄存器 / Trace │  │
│  └──────────────┘  └──────────────┘  └──────────────────────────┘  │
└────────────────────────────────────────────────────────────────────┘
```

## 二、各组件详解

### 2.1 Arm GNU Toolchain —— 核心编译器套件

这是整个工具链的**核心**，取代了传统 KEIL 使用的 arm-cc（ARM 商业编译器）。

| 工具 | 可执行文件 | 作用 |
|------|-----------|------|
| **编译器** | `arm-none-eabi-gcc` | 将 `.c` 编译为 `.o`（实际包含预处理→编译→汇编三步） |
| **链接器** | `arm-none-eabi-ld`（通过 gcc 调用） | 将多个 `.o` 和库文件链接成 `.elf` |
| **格式转换** | `arm-none-eabi-objcopy` | 将 `.elf` 转为 `.hex` / `.bin`（烧录用） |
| **尺寸统计** | `arm-none-eabi-size` | 输出 text/data/bss 段大小 |
| **调试器** | `arm-none-eabi-gdb` | GDB 客户端，通过 GDB Server 连接硬件调试器 |

关键编译参数（在 Makefile 的 `CFLAGS` 和 `LDFLAGS` 中配置）：

- `-mcpu=cortex-m4 -mthumb`：目标 CPU 架构
- `-mfpu=fpv4-sp-d16 -mfloat-abi=hard`：启用硬件浮点单元
- `-Og` / `-O0`~`-Ofast`：编译优化等级（调试用 `-Og`，发布用 `-O2`/`-Ofast`）
- `-g -gdwarf-2`：生成调试符号（Debug 版本）
- `-specs=nano.specs`：使用 nano 版 C 库，减小体积
- `-T STM32F407IGHx_FLASH.ld`：链接脚本，定义内存布局
- `-flto`：链接时优化（Link Time Optimization）

> **与 KEIL 的核心差异**：KEIL 使用 arm-cc（不支持 `uint8_t x = 0b00001111` 这种二进制字面量）、单线程编译；arm-gnu 则支持多线程编译（`-j24`），速度快一个数量级（全量编译 ~10s，增量 ~3s）。

### 2.2 MinGW / MSYS2 —— 提供 make 工具

Windows 原生不支持 `make`。项目通过 **MSYS2** 的 mingw64 环境提供：

- `mingw32-make.exe`：GNU Make 的 Windows 版本，读取 Makefile 并驱动编译
- 类 Unix shell 环境（bash、find、rm 等）

从终端输出 `mingw32-make: Nothing to be done for 'all'` 可以看出，系统已经在 PATH 中找到了 `mingw32-make`，且项目已经编译过、没有改动需要重新编译。

**推荐安装方式**（文档附录5）：通过 MSYS2 的 `pacman` 一键安装所有依赖：

```shell
pacman -S mingw-w64-x86_64-toolchain \
          mingw-w64-x86_64-arm-none-eabi-toolchain \
          mingw-w64-x86_64-ccache \
          mingw-w64-x86_64-openocd
```

### 2.3 VSCode —— 编辑器 + 集成开发环境

VSCode 通过以下插件/配置文件实现了"类 IDE"的功能：

| 配置文件 | 作用 |
|----------|------|
| `.vscode/tasks.json` | 定义构建任务（build task）、下载任务（download_dap/jlink）、日志任务（RTT client） |
| `.vscode/launch.json` | 定义调试配置（DAPlink/Jlink 的 launch 和 attach 模式） |
| `.vscode/c_cpp_properties.json` | 配置 IntelliSense（代码补全、跳转、高亮的索引信息） |
| `.vscode/settings.json` | 工作区级设置（文件关联、CMake 路径等） |

**核心插件**：

| 插件 | 功能 |
|------|------|
| **C/C++** (Microsoft) | C/C++ 语言支持、IntelliSense |
| **Cortex-Debug** + Device Support Pack | MCU 调试支持，连接 OpenOCD/JLink |
| **Makefile Tools** (Microsoft) | Makefile 语法支持，自动配置 IntelliSense |
| **Better C++ Syntax** | 增强语法高亮 |
| **IntelliCode / GitHub Copilot** | AI 代码补全 |

### 2.4 OpenOCD / JLinkGDBServer —— 调试桥接层

这是 **GDB 和硬件调试器之间的桥梁**：

```
arm-none-eabi-gdb  ←→  OpenOCD/JLinkGDBServer  ←→  J-Link/DAP-Link  ←→  STM32F407 DBG外设
   (GDB客户端)           (GDB Server)                (硬件调试器)           (MCU调试模块)
```

- **OpenOCD**：开源的 GDB Server，支持 DAP-Link、ST-Link、J-Link 等。配置文件在项目根目录：
  - `openocd_dap.cfg`：DAP-Link / 无线调试器配置
  - `openocd_jlink.cfg`：J-Link 配置
- **JLinkGDBServer**：SEGGER 官方 GDB Server（仅支持 J-Link），性能更强

DBG 外设通过**专用总线**（AHB-AP）直接访问内存和外设寄存器，**不占用 CPU 资源**——这就是为什么 Ozone 可以高频采样变量而不影响程序实时性的原因。

### 2.5 Ozone —— 高级调试与可视化

**Ozone 是本工具链的"大杀器"**，由 SEGGER（做 J-Link 的公司）开发。它在基础调试之上提供了：

| 功能 | 说明 |
|------|------|
| **变量动态监视** | 不暂停 CPU 的情况下实时刷新变量值（可设刷新率） |
| **多通道示波器** | 将变量以波形图显示，支持缩放、游标测量 |
| **数据导出 CSV** | 所有采样数据可导出，用于系统辨识、前馈设计 |
| **RTT 日志** | 通过终端窗口接收 `bsp_log` 输出的日志，替代串口调试 |
| **外设寄存器查看** | 通过 `.svd` 文件可视化查看所有外设寄存器 |
| **FreeRTOS 支持** | 加载插件后可查看所有任务状态、栈使用 |
| **指令 Trace** | 追踪 CPU 执行的每一条指令（通过 ITM/ETM） |

**Ozone 与 VSCode 调试的关系**：

```
VSCode 调试 (Cortex-Debug)  →  日常开发、断点调试、简单变量查看
Ozone                        →  复杂调试、变量可视化、数据导出、PID整定
```

两者不冲突——你修改代码后 VSCode 重新编译，Ozone 会自动检测 `.elf` 变化并提示重新加载。

---

## 三、Makefile 在工具链中的角色与工作方式

### 3.1 Makefile 的本质

Makefile 是一个**构建规则文件**，它将"调用哪个编译器、传递什么参数、编译哪些文件、如何链接"这些繁琐的命令行操作**自动化**。

打个比方：如果不用 Makefile，编译一个多文件项目你需要手动输入：

```shell
arm-none-eabi-gcc -c -mcpu=cortex-m4 -mthumb ... Src/main.c -o build/main.o
arm-none-eabi-gcc -c -mcpu=cortex-m4 -mthumb ... Src/gpio.c -o build/gpio.o
# ... (几百个文件)
arm-none-eabi-gcc build/main.o build/gpio.o ... -o build/basic_framework.elf
```

Makefile 让你只需要输入 **`mingw32-make -j24`**，make 就会自动：

1. 检查哪些文件需要重新编译（比对新旧 `.o` 和 `.c` 的时间戳）
2. 只编译改动的文件（增量编译）
3. 并行编译（`-j24` = 24个线程）
4. 链接生成最终产物

### 3.2 本工程 Makefile 的结构

项目根目录的 `Makefile`（356行）由 CubeMX 自动生成 + 手动添加，分为以下几个部分：

```
Makefile 结构：
├── ① 目标名         TARGET = basic_framework
├── ② 编译变量       DEBUG=1, OPT=-Og
├── ③ 源文件列表     C_SOURCES = ...（所有参与编译的.c文件，需手动维护）
│                    C_INCLUDES = -I...（所有头文件目录）
├── ④ 工具链定义     PREFIX = arm-none-eabi-
│                    CC = $(PREFIX)gcc
│                    CP = $(PREFIX)objcopy
├── ⑤ 编译参数       CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) ...
│                    LDFLAGS = $(MCU) -T$(LDSCRIPT) $(LIBDIR) $(LIBS) ...
├── ⑥ 构建规则       $(BUILD_DIR)/%.o: %.c  →  .c编译为.o
│                    $(TARGET).elf: $(OBJECTS)  →  链接所有.o为.elf
│                    %.hex / %.bin  →  格式转换
├── ⑦ clean规则     删除 build/ 目录
├── ⑧ 依赖包含       -include $(BUILD_DIR)/*.d  →  自动头文件依赖
└── ⑨ 下载伪目标     download_dap / download_jlink  →  一键烧录
```

### 3.3 编译流程详解

当你执行 `mingw32-make -j24` 时，发生了什么：

#### 第一步：源文件 → 目标文件（Compile）

```makefile
$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR) 
    @$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=... $< -o $@
```

实际展开为：

```shell
arm-none-eabi-gcc -c \
  -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
  -DUSE_HAL_DRIVER -DSTM32F407xx -DARM_MATH_CM4 \
  -IInc -IDrivers/... -Ibsp/... -Imodules/... -Iapplication/... \
  -Og -g -gdwarf-2 -Wall -fdata-sections -ffunction-sections \
  -MMD -MP -MF"build/main.d" \
  Src/main.c -o build/main.o
```

- `-c`：只编译不链接
- `-D...`：定义预处理器宏（相当于在代码里写 `#define`）
- `-I...`：头文件搜索路径
- `-MMD -MP -MF`：生成 `.d` 依赖文件（记录该 `.c` 包含了哪些 `.h`），下次编译时如果头文件变了也会触发重新编译
- `-Wa,-a,-ad,-alms=...`：同时生成 `.lst` 汇编列表文件

#### 第二步：链接（Link）

```makefile
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
    @$(CC) $(OBJECTS) $(LDFLAGS) -o $@
```

将所有 `.o` + CMSIS-DSP 库（`libCMSISDSP.a`）+ nano C 库（`-lc -lm -lnosys`）链接成 `basic_framework.elf`。

链接脚本 `STM32F407IGHx_FLASH.ld` 规定了：

- FLASH 起始地址 `0x08000000`，大小 1MB
- RAM 起始地址 `0x20000000`，大小 128KB
- CCM RAM 起始地址 `0x10000000`，大小 64KB
- 各段（.text / .data / .bss / heap / stack）的布局

关键链接参数：

- `-Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref`：生成详细的内存映射文件
- `-Wl,--gc-sections`：垃圾回收未使用的代码段
- `-flto`：链接时优化
- `-Wl,--print-memory-usage`：打印内存使用情况

#### 第三步：格式转换（Objcopy）

```makefile
$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
    $(HEX) $< $@       # → arm-none-eabi-objcopy -O ihex ... .elf ... .hex
$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
    $(BIN) $< $@       # → arm-none-eabi-objcopy -O binary -S ... .elf ... .bin
```

三种输出格式：

| 格式 | 用途 | 特点 |
|------|------|------|
| `.elf` | 调试 | 含符号表、调试信息，文件最大 |
| `.hex` | J-Flash 烧录 | Intel Hex 格式，含地址信息 |
| `.bin` | OpenOCD 烧录 | 纯二进制，最小 |

#### 最终输出示例

```
  text    data     bss     dec     hex filename
  31100     484   35916   67500   107ac build/basic_framework.elf
```

- **text**：代码段 + 常量（Flash 占用）
- **data**：已初始化全局变量（Flash 存储初始值，启动时拷贝到 RAM）
- **bss**：未初始化全局变量（仅占用 RAM，启动时清零）
- **dec**：text + data + bss（总字节数）
- **hex**：dec 的十六进制表示

### 3.4 Makefile 在日常开发中的具体使用场景

| 操作 | 命令 | 触发方式 |
|------|------|---------|
| **编译** | `mingw32-make -j24` | `Ctrl+Shift+B`（VSCode 默认构建任务） |
| **清理** | `mingw32-make clean` | 删除 `build/` 目录，下次全量重编 |
| **DAP 下载** | `mingw32-make download_dap` | `Terminal → Run Task → download dap` |
| **JLink 下载** | `mingw32-make download_jlink` | `Terminal → Run Task → download jlink` |
| **编译+下载** | 由 tasks.json 中的复合命令串联 | `mingw32-make -j24 ; mingw32-make download_dap` |

**VSCode tasks.json 中的集成**：

```json
{
    "label": "build task",
    "type": "shell",
    "command": "mingw32-make -j24",
    "group": { "kind": "build", "isDefault": true }
}
```

这就是你执行 `Ctrl+Shift+B` 时实际运行的命令。

**launch.json 中的 preLaunchTask 机制**：

```json
"preLaunchTask": "build task"  // 调试前自动编译
```

当按 `F5` 启动调试时，VSCode 会先执行 build task（编译），编译成功后再启动调试——实现"一键编译+调试"。

#### 下载功能的 Makefile 伪目标

```makefile
# 起始地址（F407 Flash 基地址）
download_dap:
    openocd -f openocd_dap.cfg -c init -c halt \
      -c "flash write_image erase $(BUILD_DIR)/$(TARGET).bin 0x08000000" \
      -c reset -c shutdown

download_jlink:
    JFlash -openprj'stm32.jflash' \
      -open'$(BUILD_DIR)/$(TARGET).hex',0x8000000 \
      -auto -startapp -exit
```

- `download_dap`：通过 OpenOCD 连接 DAP-Link，擦除 Flash → 写入 bin → 复位运行
- `download_jlink`：通过 J-Flash 命令行，直接烧录 hex → 启动程序

### 3.5 Makefile 的两个版本

| 文件 | 适用场景 |
|------|---------|
| `Makefile` | **日常使用**。需要手动在 `C_SOURCES` 和 `C_INCLUDES` 中添加新文件路径 |
| `Makefile.upgrade` | **高级用户**。使用 shell `find` 命令递归自动发现所有 `.c` 文件和头文件目录，新增文件无需手动修改 Makefile。但需要在 MSYS2/MinGW 环境下使用 |

**Makefile.upgrade 的关键改进**——自动源文件发现：

```makefile
PROJ_DIR = Src Inc application modules bsp Drivers Middlewares

# Unix/MinGW bash 环境：
ALL_DIRS := $(foreach dire, $(PROJ_DIR), $(shell find $(dire) -maxdepth 10 -type d))
C_SOURCES := $(foreach dire, $(ALL_DIRS), $(wildcard $(dire)/*.c))
C_INCLUDES := $(addprefix -I,$(ALL_DIRS))
```

这样新增 `.c` 文件后无需手动修改 Makefile，直接 `make` 即可。

### 3.6 CMake 的备选方案

项目根目录还有 `CMakeLists.txt`，这是为进阶用户准备的 **CMake + Ninja** 构建方案。CMake 的优势：

- 递归自动发现源文件（`file(GLOB_RECURSE ...)`）
- 更现代的构建配置语法
- 配合 Ninja 构建工具编译速度更快
- 可自动生成 `compile_commands.json` 供 clangd 等语言服务器使用

CMakeLists.txt 关键配置：

```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_C_STANDARD 11)

# 递归包含所有头文件目录
function(include_sub_directories_recursively root_dir)
    # ...递归遍历所有子目录并 include_directories()
endfunction()

# glob 所有源文件
file(GLOB_RECURSE SOURCES
    "Drivers/*.c" "Src/*.c" "Middlewares/*.c"
    "bsp/*.c" "modules/*.c" "application/*.c"
)
```

### 3.7 为什么会出现 "Nothing to be done for 'all'"

```
mingw32-make: Nothing to be done for 'all'.
```

这是 `make` 的**正常工作方式**——它检查所有 `.o` 文件的时间戳，发现它们都比对应的 `.c` 源文件更新，说明**自上次编译以来没有任何源文件被修改过**，因此跳过了所有编译步骤。这是增量编译的核心机制，避免了不必要的重复编译。

如果需要强制重编：

```shell
mingw32-make clean   # 删除所有中间文件
mingw32-make -j24    # 全量重新编译
```

---

## 四、完整工作流

### 4.1 日常开发循环

```
┌─────────────────────────────────────────────────────┐
│  1. CubeMX 配置外设 → 生成初始化代码                   │
│     (工具链选 Makefile)                              │
├─────────────────────────────────────────────────────┤
│  2. VSCode 编写代码                                   │
│     - app 层：gimbal/chassis/shoot/cmd               │
│     - module 层：motor/imu/referee/...               │
│     - bsp 层：can/usart/spi/...                      │
│     - 新文件需在 Makefile 的 C_SOURCES 中添加          │
├─────────────────────────────────────────────────────┤
│  3. Ctrl+Shift+B 编译 (mingw32-make -j24)             │
│     输出: build/basic_framework.elf/.hex/.bin        │
├─────────────────────────────────────────────────────┤
│  4. 下载/调试                                         │
│     ├─ 快速下载: Terminal → download dap/jlink        │
│     ├─ VSCode调试(F5): 断点/变量/寄存器/外设           │
│     └─ Ozone调试: 变量可视化/示波器/RTT/指令Trace      │
└─────────────────────────────────────────────────────┘
```

### 4.2 "编译+下载+调试"一键完成

在 `.vscode/launch.json` 中取消 `preLaunchTask` 的注释后：

1. 按 `F5` → 自动执行 `mingw32-make -j24` 编译
2. 编译成功 → 自动启动 OpenOCD/JLinkGDBServer
3. 自动下载 `.elf` 到 STM32
4. 在 `main()` 入口处停住，等待调试

### 4.3 VSCode 调试功能一览

启动调试后（`F5`），可用功能：

| 区域 | 功能 |
|------|------|
| **变量窗口** | 局部变量、全局变量、静态变量实时查看 |
| **Watch 窗口** | 右键变量 → Add to Watch，可临时修改变量值 |
| **调用栈** | 查看函数调用层级，确认程序运行路径 |
| **外设寄存器** | 通过 `.svd` 文件可视化查看所有外设控制/状态寄存器 |
| **断点** | 条件断点、行内断点（表达式级）、汇编断点 |
| **Live Watch** | 4Hz 频率动态刷新变量值 |
| **调试控制** | 复位/继续/暂停/单步跳过/单步进入/单步跳出/重启/终止 |

### 4.4 Ozone 调试功能一览

| 功能 | 操作 |
|------|------|
| **变量动态监视** | 选中变量 `Ctrl+W` → Watch → 右键设置 Refresh Rate |
| **示波器可视化** | Watch 中右键变量 → Graph → 设置采样频率和步长 |
| **数据导出** | 所有采样值自动保存为 CSV |
| **RTT 日志** | View → Terminal 窗口查看 `bsp_log` 输出 |
| **FreeRTOS** | 加载插件 `Project.SetOSPlugin("FreeRTOSPlugin_CM4")` |
| **指令 Trace** | 通过 ITM 追踪 CPU 执行的每条指令 |

---

## 五、编译全流程详解（C 源码到烧录）

以单个 `.c` 文件到最终在 MCU 上运行为例：

```
main.c
  │
  ▼ ① 预处理 (Preprocessor)
main.i          ← 宏展开、注释删除、头文件展开
  │
  ▼ ② 编译 (Compiler)
main.s          ← C → 汇编语言（平台相关）
  │
  ▼ ③ 汇编 (Assembler)
main.o          ← 汇编 → 二进制目标代码（含符号表）
  │
  │  (所有 .o 文件 + 库文件 .a)
  ▼ ④ 链接 (Linker)
basic_framework.elf  ← 地址重映射、符号解析、段合并
  │
  ▼ ⑤ 格式转换 (objcopy)
basic_framework.hex / .bin  ← 烧录用格式
  │
  ▼ ⑥ 烧录 (OpenOCD / J-Flash)
STM32F407 Flash (0x08000000)
  │
  ▼ ⑦ 启动 (Bootloader → Reset_Handler → main)
程序开始运行
```

### Makefile 中各阶段的对应关系

| 阶段 | Makefile 规则 | 关键工具 |
|------|--------------|---------|
| ①~③ 编译+汇编 | `$(BUILD_DIR)/%.o: %.c` | `arm-none-eabi-gcc -c` |
| ④ 链接 | `$(TARGET).elf: $(OBJECTS)` | `arm-none-eabi-gcc $(LDFLAGS)` |
| ⑤ 格式转换 | `%.hex: %.elf` / `%.bin: %.elf` | `arm-none-eabi-objcopy` |
| ⑥ 烧录 | `download_dap` / `download_jlink` | `openocd` / `JFlash` |

---

## 六、工具链选型对比总结

| 维度 | 传统 KEIL MDK | 本项目 (VSCode+Makefile+Ozone) |
|------|--------------|-------------------------------|
| **编译器** | arm-cc（商业闭源） | arm-none-eabi-gcc（开源） |
| **编译速度** | 单线程，慢 | 多线程 `-j24`，快 5-10 倍 |
| **代码编辑** | 20世纪风格UI，无补全 | VSCode + Copilot + 智能提示 |
| **调试** | 基础断点 + Watch | VSCode调试 + **Ozone示波器/变量可视化/RTT/Trace** |
| **构建系统** | 魔术棒图形配置 | Makefile（可版本控制、可脚本化） |
| **二进制字面量** | 不支持 `0b00001111` | 完全支持 C11 标准 |
| **字节对齐** | `__packed` 关键字 | `#pragma pack(1)` |
| **环境配置** | 一键安装 | 需手动配置（但有详细文档和 MSYS2 简化方案） |
| **代码跳转** | 基本无 | 定义跳转、引用查找、调用层级 |
| **版本管理** | 无 | Git + GitLens/GitGraph |
| **静态检查** | 弱 | SonarLint + GCC -Wall -Werror |
| **增量编译** | 支持 | 支持（基于时间戳和 .d 依赖文件，更精准） |

---

## 七、开发环境配置清单

### 必装软件

| 软件 | 用途 | 安装方式 |
|------|------|---------|
| **MSYS2** | 提供 make、bash 等工具 | `pacman -S mingw-w64-x86_64-toolchain` |
| **Arm GNU Toolchain** | 交叉编译器 | `pacman -S mingw-w64-x86_64-arm-none-eabi-toolchain` 或官网下载 |
| **OpenOCD** | GDB Server（调试桥梁） | `pacman -S mingw-w64-x86_64-openocd` |
| **STM32CubeMX** | 外设初始化代码生成 | ST 官网 |
| **VSCode** | 编辑器 | 官网 |
| **Ozone** | 高级调试可视化 | SEGGER 官网 |
| **J-Link 软件包** | J-Link 驱动和工具 | SEGGER 官网 |

### 必装 VSCode 插件

- **C/C++** (Microsoft)
- **Cortex-Debug** + **Device Support Pack - STM32F4**
- **Makefile Tools** (Microsoft)
- **Better C++ Syntax**
- **IntelliCode**
- **GitLens**
- **Doxygen Documentation Generator**
- **SonarLint**（静态检查）

### 环境变量

需要将以下路径加入系统 PATH：

- MSYS2 的 `mingw64/bin`（或 MinGW 的 `bin`）
- Arm GNU Toolchain 的 `bin`
- J-Link 安装目录
- Ozone 安装目录

### 验证安装

```shell
gcc -v                  # MinGW/make 可用
arm-none-eabi-gcc -v    # ARM 交叉编译器可用
openocd --version       # OpenOCD 可用
```

---

## 八、核心设计理念

这套工具链的核心设计哲学是：**用现代软件开发的工作流来做嵌入式开发**。

传统嵌入式开发的问题：

- KEIL 编辑器功能孱弱（无代码补全、无智能跳转）
- 串口调试侵入 CPU，影响实时性
- 编译速度慢，单线程
- 构建配置不可版本化管理

本工具链的解决方案：

- **VSCode**：现代编辑器体验（补全、跳转、重构、Copilot）
- **arm-gnu-gcc + Makefile**：多线程编译、可脚本化构建、纳入版本管理
- **DBG 外设 + Ozone**：非侵入式高频调试、变量可视化、数据导出
- **Git**：全流程版本控制，热更新框架

> *"用软件开发的思想设计嵌入式系统是一种降维打击"*
> <p align=right>—— 沃兹基·烁德</p>

---

## 参考文档

- [VSCode+Ozone使用方法.md](.Doc/VSCode+Ozone使用方法.md)
- [架构介绍与开发指南.md](.Doc/架构介绍与开发指南.md)
- [让VSCode成为更称手的IDE.md](.Doc/让VSCode成为更称手的IDE.md)
- [Makefile Tutorial By Example](https://makefiletutorial.com/)
- [Arm GNU Toolchain Downloads](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
- [Cortex-Debug 调试器使用介绍](https://blog.csdn.net/qq_40833810/article/details/106713462)
