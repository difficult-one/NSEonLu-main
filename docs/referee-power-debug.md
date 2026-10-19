# 裁判系统实时功率调试

## 当前接收支持

USART1 裁判模式（`UART1_USE=0`）支持两种 `0x0202` 数据段长度：

| 协议 | 数据段长度 | 偏移 4 的 float | 偏移 8 的 uint16 | 42 mm 枪口热量 |
|---|---|---|---|---|
| 2024 等明确提供功率的旧协议 | 16 字节 | 实时底盘功率，单位 W | 缓冲能量，单位 J | 偏移 14 |
| 2025 协议 | 16 字节 | 保留，不作为有效功率 | 缓冲能量，单位 J | 偏移 14 |
| 2026 协议 | 14 字节 | 保留，不作为有效功率 | 缓冲能量，单位 J | 偏移 12 |

**不能仅凭 16 字节帧长判断是否提供实时功率。** [2025 官方协议 V1.9.0](https://bbs-web-static.robomaster.com/771a4231a34b4a0cb491204998e3a9e31764044900872/RoboMaster%20Referee%20System%20Serial%20Port%20Protocol%20V1.9.0%EF%BC%8820250704%EF%BC%89.pdf) 表 2-9 将偏移 0、2、4 都定义为保留字段；屏幕显示实时功率不表示 User 串口输出同一数据。切换在线模式不能改变协议中保留字段的定义。

`REFEREE_LEGACY_POWER_FIELDS_ENABLED` 默认为 `0`，保留字段不发布为实测功率。只有确认固件对应的协议明确提供旧版功率字段时才设为 `1`；Make 和 CMake 均支持这个编译选项。该开关只改变字段解释，不能让发送端输出未发送的功率。

[2025 官方用户手册](https://rm-static.djicdn.com/tem/17348/RM%202025%20%E8%A3%81%E5%88%A4%E7%B3%BB%E7%BB%9F%20%E7%94%A8%E6%88%B7%E6%89%8B%E5%86%8C%EF%BC%88%E4%B8%AD%EF%BC%89V1.4.pdf) 第 21 页说明，启用离线模式可让电管的底盘、云台、发射机构接口正常供电，没有说明会恢复串口的保留功率字段。当前底盘输出为 true、屏幕有功率，不支持将串口功率为 0 归因于未供电。

载荷长度指帧头中的 `data_length`，不含帧头、命令码和 CRC16。两种完整帧都必须通过 CRC8、CRC16 校验；其他长度不更新功率状态。

用户实际设备已通过断点确认发送 16 字节数据段。之前仅接受 14 字节导致整帧丢弃：虽然 `0x0201` 的功率上限为 80 W，`has_power_heat` 仍是 false，缓冲能量和实时功率位置也保持零值。

兼容帧长后，用户上板确认缓冲能量为 60 J、`has_power_heat=true`、底盘输出开启，但电压、电流、功率的原始字段都为 0，主控屏幕功率为约 5 W。照片中主控 APP 为 `11.0.0.8`，电管为 `11.0.0.4`。当前数据与 2025 保留字段的情况一致；未查到这两个具体固件版本的官方协议映射，不按版本号猜测协议。此前 `has_chassis_power=true` 是仅凭长度及有限浮点数产生的误判，本次已修正默认行为。

## Watch 表达式

烧录兼容固件后，调试器中添加：

```c
referee_power_state.chassis_power_w
referee_power_state.has_chassis_power
referee_power_state.buffer_energy_j
referee_power_state.has_power_heat
referee_power_state.power_heat_tick_ms
referee_power_state.chassis_power_limit_w
```

- `chassis_power_w`：已明确启用旧版功率字段时的实时底盘功率，单位 W；字段不可用时为 0。
- `has_chassis_power`：仅在已启用旧版功率字段、帧长为 16 字节且功率为有限浮点数时为 true。默认保留字段模式、14 字节帧或 NaN/无穷大时为 false，功率值清零。false 表示没有有效测量，不表示真实功率为 0 W。
- `buffer_energy_j`：缓冲能量，单位 J；它是能量，不能与实时功率互换。
- `has_power_heat`：是否成功解析过功率热量帧。
- `power_heat_tick_ms`：最近成功解析时的 HAL tick，应随有效帧更新。
- `chassis_power_limit_w`：`0x0201` 下发的功率上限，单位 W。

上述原始状态保存最近数据，时间过期不会自动清除。使用 `RefereeGetPowerSnapshot()` 时，仍要求 `0x0201` 和 `0x0202` 都在 300 ms 内更新，超过 300 ms 快照清零且 `valid`、`chassis_power_valid` 为 false。

```c
RefereePowerSnapshot_s power = {0};
if (RefereeGetPowerSnapshot(&power) && power.chassis_power_valid)
{
    /* power.chassis_power_w is fresh referee-measured chassis power in W. */
}
```

`referee_info.PowerHeatData.reserved_power` 保留偏移 4 的原始字节解释，没有独立的新鲜度或数值有效标志。只有明确提供旧版功率字段的协议才能把它解释为实测功率；在 2025/2026 协议中它是保留字段。

`chassis_power->state.measured_power_w` 来自 CAN1、ID `0x605` 的独立功率计。本次提供裁判系统的功率读取与快照接口，未切换控制外环的数据来源；仅接裁判系统时，该 CAN 功率计变量仍可能为零。

## 编译与上板核对

```powershell
make -W Makefile -j8 UART1_USE=0
```

烧录文件为 `build/basic_framework.hex`。固件也同时生成 ELF 和 BIN。修改编译选项后必须完整重编译，避免复用其他配置的旧目标文件。

现有 VS Code 调试配置使用 `build/basic_framework.elf`。停止当前调试会话，重新启动并下载程序后再观察新变量。若手动烧录其他目录中的 HEX，调试器应加载同目录的 ELF。

1. 确认电管 User 串口连接 USART1，串口设置 115200、8N1。
2. 移除调试接收函数中的条件断点，让程序持续运行。
3. 确认 `has_power_heat` 变为 true，`power_heat_tick_ms` 持续更新。默认配置的 `has_chassis_power` 应为 false。
4. 仅在已核实旧版功率协议并启用对应编译选项后，对照主控模块屏幕上的底盘实时功率，查看 `chassis_power_w` 是否随负载变化。缓冲能量按发送端实际值显示，不保证非零。
5. 长时间暂停调试器后，快照可能超时；恢复运行并等待新帧后再次查看。

## 本次验证结果

裁判功率和接收集成测试分别构建默认保留字段与显式旧版功率两种配置并通过；默认模式使用全零及非零保留字节，验证两者都不发布为实测功率，但缓冲能量、热量、时间戳和功率指令正常更新。两种配置均覆盖 14/16 字节接收、坏 CRC、非法长度和超时，旧版模式另覆盖非有限功率。字节流解析、功率链路测试也通过。

默认调试目录和此前提供的 `build/referee-legacy-power` 隔离目录固件都已重新构建成功，默认使用保留字段模式，RAM 使用 98,392 B，FLASH 使用 107,404 B。保留既有 libnosys 的 `_close`、`_lseek`、`_read`、`_write` 链接警告。需重新下载固件，才能在硬件上观察修正后的有效标志。
