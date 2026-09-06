# STM32 J-Link 开发环境设计

## 目标

为 `NSEonLu-main` 的 STM32F407IG 跃鹿框架建立一套可直接使用的 Windows 开发环境：使用 Arm GNU Toolchain 与 GNU Make 编译，使用 SEGGER J-Link 完成烧录和 GDB 调试，并提供 Ozone 调试入口。

## 已确认环境

- 目标 MCU：STM32F407IG，Cortex-M4F，硬件浮点。
- 构建系统：项目现有 `Makefile`。
- 编译器：Arm GNU Toolchain 13.3.Rel1。
- Make：GNU Make 4.4.1，通过 `mingw32-make.exe` 调用。
- 调试器：J-Link，SWD 接口，4 MHz。
- VS Code 扩展：Cortex-Debug 1.12.1、Makefile Tools 0.12.17。
- SEGGER 软件：J-Link V9.38a、Ozone。
- 固件产物：`build/basic_framework.elf`、`build/basic_framework.hex`、`build/basic_framework.bin`。

## 方案

采用跃鹿框架推荐的 Arm GNU + Make + VS Code/Cortex-Debug + J-Link + Ozone 工作流，不引入 PlatformIO，也不迁移到 CMake/Ninja。

### 编译

VS Code 默认构建任务调用 `mingw32-make` 并行编译。任务使用 GCC problem matcher，使编译错误可直接跳转到源文件。另提供清理任务，但普通构建不依赖清理，保留增量编译能力。

编译成功必须生成 ELF、HEX 和 BIN 三种产物，并显示 STM32 Flash/RAM 使用情况。

### 烧录

烧录使用 SEGGER J-Link Commander，而不是 OpenOCD 或 J-Flash GUI。工程内保存固定的 Commander 脚本，按以下顺序执行：连接 STM32F407IG、停止目标、复位、加载固件、再次复位、启动程序、退出。

VS Code 的“编译并烧录”任务先执行默认构建任务，只有构建成功后才调用 J-Link。烧录输入使用 ELF 文件，以便 J-Link 同时获得地址和映像信息。

### VS Code 调试

Cortex-Debug 使用 SEGGER 原生 J-Link GDB Server，配置如下：

- 目标设备为 `STM32F407IG`。
- 接口为 SWD，速度为 4000 kHz。
- GDB 使用 Arm GNU Toolchain 中的 `arm-none-eabi-gdb.exe`。
- 符号文件为 `build/basic_framework.elf`。
- 启动调试前自动执行默认构建任务。
- launch 模式下载程序并运行到 `main`。
- attach 模式连接现有程序，不重新下载。
- 加载项目自带的 `STM32F407.svd`，启用 FreeRTOS 感知与 Live Watch。

现有 STM32Cube、ST-Link、CMSIS-DAP 调试项不属于选定工作流，将从工作区调试列表中移除，避免误选；已安装的扩展和系统软件不卸载。

### Ozone 调试

工程内新增 Ozone 项目脚本，使用同一个 `build/basic_framework.elf`、STM32F407IG、J-Link、SWD 4 MHz。VS Code 提供一个任务启动该 Ozone 项目；启动前先完成编译。Ozone 用于实时变量、RTT、波形和较复杂的运行时分析，VS Code 用于日常断点和寄存器调试。

### 路径策略

工作区配置显式引用本机已探测到的安装位置：

- Arm GNU：`D:\Arm_GNU_Toolchain\13.3 rel1\bin`
- J-Link：`C:\Program Files\SEGGER\JLink_V938a`
- Ozone：`C:\Program Files\SEGGER\Ozone`

这样可避免 Windows PATH 中存在多个 GCC、GDB 或 J-Link 版本时选错工具。路径只放在工作区配置中，不修改系统环境变量。

## 文件边界

- 修改 `.vscode/tasks.json`：编译、清理、编译并烧录、Ozone 入口。
- 修改 `.vscode/launch.json`：J-Link launch 与 attach。
- 修改 `.vscode/settings.json`：Arm GNU、J-Link 与 Make 的工作区路径。
- 修改 `.vscode/c_cpp_properties.json`：使用 ARM GCC 的 IntelliSense 模式、宏和编译器路径。
- 新增 `scripts/flash.jlink`：J-Link Commander 烧录脚本。
- 新增 `debug_ozone.jdebug`：Ozone 工程配置。
- 仅在实际验证发现必要时，最小修改 `Makefile` 的 J-Link 目标；不修改应用、BSP、模块或 HAL 代码。

## 错误处理

- 编译失败时，中止后续烧录或调试启动。
- 找不到工具时，任务输出明确显示缺失的可执行文件路径。
- 找不到 ELF 时，不启动 J-Link 或 Ozone。
- J-Link 未连接、目标未供电或 SWD 接线异常时，保留 SEGGER 原始错误输出，不通过脚本隐藏失败。

## 验收标准

1. `mingw32-make` 从干净输出目录完整编译成功，生成 ELF、HEX、BIN。
2. VS Code 默认构建任务与命令行构建使用同一套 Arm GNU 工具链。
3. Cortex-Debug 的 launch/attach 配置可被扩展解析，路径和设备参数正确。
4. J-Link Commander 能读取烧录脚本；连接硬件后能写入并校验固件，随后复位运行。
5. Ozone 能打开工程并加载 ELF 符号；连接硬件后可进入 `main`、查看变量和 FreeRTOS 状态。
6. `git diff` 只包含开发环境配置、脚本和文档，不包含业务代码变化或构建产物。

## 测试策略

本次变更以配置文件为主，经用户批准采用命令级验收代替单元测试式 TDD。先执行静态配置检查和完整编译，再检查 ELF/HEX/BIN 产物。硬件连接存在时继续验证真实烧录、VS Code 调试连接和 Ozone 调试连接；硬件不在线时明确标记这些项目为待上板验证，而不将其误报为通过。
