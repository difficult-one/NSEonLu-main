# NSEonLu

基于湖南大学 RoboMaster 跃鹿战队 `basic_framework` 二次开发的 STM32 电控项目，当前主要用于底盘控制、功率管理、超级电容通信和调试工具开发。

本仓库发布的是本地“超级电容”分支的最新工作版本，包含 USB 遥测、在线调参与单轮 PID 自动整定等扩展。上游框架介绍保留在 [YUELU.md](YUELU.md)。

## 当前功能

- **底盘控制**：支持麦克纳姆轮和四轮 X 型全向轮运动学；提供固定、功率自适应和正弦小陀螺模式。
- **功率管理**：结合功率模型、功率计反馈、裁判系统功率上限与缓冲能量，提供启动保护、恢复及慢外环调节。
- **超级电容**：通过 CAN 交换功率、能量与状态信息，参与底盘功率预算。
- **USB 遥测仪表盘**：通过 USB CDC 在浏览器中查看底盘、遥控器、四轮转速、电流和功率波形，并导出采样数据。
- **在线调参**：读取和修改 PID、功控及小陀螺参数，将板端确认的参数同步到源码覆盖文件，并支持停机条件下保存到板端 Flash。
- **单轮 PID 自动整定**：提供架空台架试验、结果预览和参数采用流程。
- **离线波形工具**：导入 CSV/TXT 日志，支持多曲线、缩放、平移和可选滤波。

源码包含云台、发射、视觉、遥控器、裁判系统等模块；**当前 `application/robot.c` 启用了底盘，云台和发射调用仍被注释**。模块存在并不表示已在当前机器人上启用或完成实机验证。

## 硬件与环境

- RoboMaster C 型开发板，STM32F407IGHx / Cortex-M4F。
- STM32 HAL、FreeRTOS、CMSIS-DSP；相关依赖源码与库随工程提供。
- Arm GNU Toolchain（`arm-none-eabi-gcc`、`objcopy`、`size`）和 GNU Make；Windows 可使用 MinGW Make。
- 烧录可选 OpenOCD + CMSIS-DAP，或 J-Link / J-Flash。
- 网页遥测工具需要 Node.js 和支持 Web Serial 的桌面 Edge / Chrome；离线波形工具可直接用浏览器打开。

本项目与 A 板 STM32F427 适配工程不同，不能直接共用固件。接线、CAN ID 和传感器型号应以当前源码与实际硬件为准。

## 快速开始

```powershell
git clone https://github.com/difficult-one/NSEonLu-main.git
cd NSEonLu-main
arm-none-eabi-gcc --version
make --version
```

如果系统只有 `mingw32-make`，将下文的 `make` 替换为 `mingw32-make`。工具链不在 PATH 时，可通过 Make 的 `GCC_PATH` 指向工具链 `bin` 目录。

### 编译固件

建议显式指定通信与功控配置，并为不同组合使用独立构建目录，避免复用编译选项不同的对象文件：

```powershell
# 裁判系统 + USB 遥测 + 完整功控慢环
New-Item -ItemType Directory -Force build/usb-telemetry | Out-Null
make -j8 BUILD_DIR=build/usb-telemetry UART1_USE=0 USB_TELEMETRY_ENABLE=1 CHASSIS_POWER_CONTROL_STAGE=3

# 使用 USB 视觉通信时关闭 USB 遥测
New-Item -ItemType Directory -Force build/usb-vision | Out-Null
make -j8 BUILD_DIR=build/usb-vision UART1_USE=0 USB_TELEMETRY_ENABLE=0 CHASSIS_POWER_CONTROL_STAGE=3
```

输出位于指定目录，文件名为 `basic_framework.elf`、`basic_framework.hex` 和 `basic_framework.bin`。USB 遥测和 USB 视觉通信互斥。`UART1_USE=0` 选择裁判系统；`1` 为串口日志，`2` 为串口日志与 RTT。

工程也提供 [CMakeLists.txt](CMakeLists.txt)，配置示例见[底盘启动保护与快速恢复](docs/chassis-power-startup.md)。

### 烧录与调试

连接调试器后，可使用现有 Make 目标烧录已编译固件：

```powershell
make download_dap BUILD_DIR=build/usb-telemetry
# 或使用已安装的 J-Flash
make download_jlink BUILD_DIR=build/usb-telemetry
```

烧录目标不会替你重新编译，先确认所选目录的固件与配置一致。在线调参使用 Flash 扇区 10、11，应用程序链接到前 768 KiB；需要保留板端参数时，不要全片擦除。详见[掉电保护与分区说明](tools/usb-telemetry/README.md#掉电保护与分区)。

### 打开 USB 仪表盘

```powershell
node tools/usb-telemetry/serve.cjs
```

打开 [http://127.0.0.1:4173](http://127.0.0.1:4173)，也可双击 `tools/usb-telemetry/start.cmd`。先用“演示数据”查看界面，再连接支持遥测的 C 板虚拟串口。演示数据不代表硬件测试结果。

在线调参、Flash 保存、自动整定及协议的完整操作说明见 [USB 遥测仪表盘文档](tools/usb-telemetry/README.md)。

离线分析无需启动服务，直接打开 [波形工具](tools/waveform-viewer/index.html)，使用方法见[波形工具文档](tools/waveform-viewer/README.md)。

## 参数配置

| 入口 | 主要用途 |
| --- | --- |
| `application/robot_def.h` | 单板/双板角色、底盘类型、几何尺寸、编码器与机器人参数 |
| `application/robot.c` | 控制模块初始化与任务启用 |
| `Inc/board_config.h` | USART1 通道用途 |
| `modules/algorithm/chassis_power_config.h` | 功率控制阶段、阈值与保护参数 |
| `modules/algorithm/chassis_spin_config.h` | 固定、自适应、正弦小陀螺参数 |
| `modules/telemetry/tuning_overrides.h` | 网页确认后同步的参数覆盖文件 |
| `basic_framework.ioc` | CubeMX 外设、引脚与时钟配置 |

当前工作版本使用 `ONE_BOARD`、麦克纳姆轮、功控阶段 3，默认小陀螺模式由源码选择为正弦模式。文档中的历史默认值可能与这一工作版本不同，实际以源码、Make 命令行覆盖及有效板端保存参数为准。

完整参数说明见[宏与参数设置总表](docs/configuration-guide.md)和[配置索引](docs/configuration-index.md)。首次运行应核对轮序、电机方向、CAN ID、遥控器映射、功率上限及传感器反馈。自动整定需按工具文档在架空台架上执行。

## 工程结构

```text
application/       底盘、云台、发射及控制命令等应用
bsp/               CAN、串口、SPI、GPIO 等板级支持
modules/           电机、算法、遥测、遥控器、裁判、电容等模块
Inc/ 与 Src/       STM32 外设配置、中断与 FreeRTOS 入口
Drivers/           STM32 HAL 与 CMSIS
Middlewares/       FreeRTOS、USB 与 DSP 库
portable_modules/  可移植模块
tools/            USB 仪表盘与离线波形工具
scripts/           功率采集解码与烧录辅助脚本
tests/             主机侧协议与控制逻辑测试
docs/              当前功能与参数文档
.Doc/              上游开发指南与调试资料
AI/                项目学习笔记与模块说明
```

## 文档导航

- [底盘控制与小陀螺](docs/chassis.md)
- [功率计标定](docs/chassis-power-calibration.md)
- [启动保护与恢复](docs/chassis-power-startup.md)
- [裁判功率调试](docs/referee-power-debug.md)
- [超级电容 CAN 协议](docs/supercap-can-protocol.md)
- [USB 遥测、在线调参与自动整定](tools/usb-telemetry/README.md)
- [离线波形分析](tools/waveform-viewer/README.md)
- [上游架构与开发指南](.Doc/架构介绍与开发指南.md)
- [模块学习导航](AI/Tu_ai_learn/README.md)

## 验证

网页工具的主机侧测试可在仓库根目录运行，无需安装 npm 依赖：

```powershell
node --test tools/usb-telemetry/tests/*.test.cjs tools/waveform-viewer/tests/core.test.cjs
```

其他控制与协议测试按 `tests/` 各子目录中的构建文件运行。主机测试与固件编译不等同于实机验证；实车功率峰值、轮速跟随和整定效果需结合实际负载确认。

## 来源与许可

本项目由湖南大学 RoboMaster 跃鹿战队 [basic_framework](https://github.com/HNUYueLuRM/basic_framework) 二次开发而来，本地开发仓库来源为 [blue-has-qing/NSEonLu](https://gitee.com/blue-has-qing/NSEonLu)。感谢原作者与贡献者。

项目许可证见 [LICENSE](LICENSE)，保留原有版权声明；STM32 HAL、FreeRTOS、CMSIS 等第三方组件以各自目录中的许可文件为准。
