# 裁判系统 0x0201 十七字节帧修正实施计划

## 当前状态

已完成输出状态偏移与标准结构体映射修正，以及主机测试和固件构建验证。实机刷写验收仍待执行。下述问题描述为修正前的状态。

实机收到的 CRC 有效载荷为：

```text
偏移  00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16
数据  03 01 C8 00 C8 00 28 00 F0 00 50 00 00 00 F0 41 07
```

已确认的布局为：

- 字节 0–11：与 13 字节机器人状态帧相同。
- 字节 10–11：底盘功率上限，值为 80 W。
- 字节 12–15：小端浮点数 `30.0f`，当前不依赖其业务含义。
- 字节 16：电源管理输出状态 `0x07`，云台、底盘、发射机构三个输出位均为 1。

现有兼容代码虽然接受 17 字节帧，却仍从 `payload[12]` 读取输出状态，并把前 13 字节直接复制到旧结构体。这会把扩展浮点数的第一个字节 `0x00` 误当成输出状态，导致 `chassis_output_enabled=false`，随后底盘控制电流被清零。

## 实施目标

- 保持标准 13 字节 `0x0201` 行为不变。
- 17 字节 `0x0201` 从字节 16 读取输出状态。
- 公共的机器人状态字段和 80 W 功率上限继续正确解析。
- 将字节 16 映射到现有 13 字节 `GameRobotState` 结构体的最后一个状态字节。
- 不使用字节 12–15 的扩展值参与功率控制。
- 不绕过裁判系统输出许可、急停、零力模式、电机在线检测等安全条件。
- 不改变 `0x0202` 的 14/16 字节兼容逻辑。

## 涉及文件

```text
modules/referee/referee_power.c
modules/referee/rm_referee.c
tests/referee_power/test_referee_power.c
tests/referee_integration/test_referee_integration.c
docs/superpowers/specs/2026-10-12-referee-0201-extended-payload-design.md
```

## 实施步骤

### 1. 先增加失败测试

在 `tests/referee_power/test_referee_power.c` 中使用完整实机载荷：

```c
static const uint8_t extended_robot_state[17] = {
    0x03U, 0x01U, 0xC8U, 0x00U, 0xC8U, 0x00U, 0x28U, 0x00U,
    0xF0U, 0x00U, 0x50U, 0x00U, 0x00U, 0x00U, 0xF0U, 0x41U,
    0x07U,
};
```

断言：

```text
has_robot_state == true
chassis_power_limit_w == 80
chassis_output_enabled == true
robot_state_tick_ms 正确更新
```

保留现有 13 字节用例，确认其输出状态仍从字节 12 读取。

替换原有偏移错误的 17 字节测试载荷和输出关闭断言。增加 13/17 字节切换用例：先接收开启帧，再接收字节 12 为 `0x02`、字节 16 为 `0x05` 的扩展帧，确认底盘输出关闭；随后标准帧仍从字节 12 恢复输出许可。

在 `tests/referee_integration/test_referee_integration.c` 中构造带 CRC 的完整帧，断言：

```text
GameRobotState.robot_id == 3
GameRobotState.chassis_power_limit == 80
三个 power_management_*_output 位均为 1
payload_length_error_count 不增加
snapshot.chassis_output_valid == true
snapshot.chassis_output_enabled == true
相邻结构体字段未被覆盖
```

集成测试同时断言扩展关闭帧将三个输出位更新为 `1/0/1`，随后 13 字节帧可更新为 `0/1/0`。用相邻 `PowerHeatData` 全结构的哨兵字节验证没有越界写入。

在功率状态测试和集成测试中增加 16/18 字节拒绝用例，确认原字段和时间戳保持不变，集成层长度错误计数增加。集成测试在原时间戳后 300 ms/301 ms 检查输出状态有效性，确认拒绝帧不会续期。

先运行两个测试套件，确认新断言在旧实现上失败，且失败点集中在输出状态解析和结构映射。

### 2. 修正功率状态解析

在 `RefereePowerStateAcceptFrame()` 已确认长度为 13 或 17 后，根据长度选择状态字节：

```c
const uint16_t output_flags_offset =
    payload_length == REFEREE_ROBOT_STATE_EXTENDED_PAYLOAD_LENGTH
        ? 16U
        : 12U;

state->chassis_power_limit_w = ReadU16LE(&payload[10]);
state->chassis_output_enabled =
    (payload[output_flags_offset] & 0x02U) != 0U;
```

这里不解析字节 12–15 的 `30.0f`，以免在字段定义未经确认时把它引入控制闭环。

### 3. 修正旧协议结构映射

`ext_game_robot_state_t` 仍保持 13 字节，不能直接接收整个 17 字节载荷。处理方法为：

1. 复制公共的前 12 字节。
2. 13 字节帧把 `payload[12]` 写入结构体最后一个字节。
3. 17 字节帧把 `payload[16]` 写入结构体最后一个字节。

直接在 `rm_referee.c` 的既有机器人状态分支内完成映射：

```c
const uint16_t flags_offset = extended_robot_state ? 16U : 12U;
uint8_t *const destination = (uint8_t *)&referee_info.GameRobotState;
/* Extended frames insert four bytes before the output flags. */
memcpy(destination, payload, LEN_game_robot_state - 1U);
destination[LEN_game_robot_state - 1U] = payload[flags_offset];
```

进入该分支前仍使用现有长度白名单，因此不会访问未知长度载荷。

### 4. 验证

依次运行：

```powershell
make -C tests/referee_power clean test
make -C tests/referee_integration clean test
make -C tests/referee_stream_parser -B test
make -j4
```

验收条件：

- 两个主机测试套件的普通版和 legacy 版全部通过。
- 13 字节和 17 字节帧都能正确解析。
- 非 13/17 字节 `0x0201` 仍计入长度错误且不更新状态。
- `0x0202` 现有测试行为不变。
- 固件构建没有新增编译或链接错误；若本机 ARM 工具链自身仍无法启动，记录原始错误，不把它误判为本次代码问题。

### 本次验证结果

- 修正前：新功率状态测试在输出开启和关闭断言上失败；新集成测试在输出位映射和快照输出许可断言上失败，公共字段和长度拒绝检查通过。
- 修正后：功率状态与集成测试的普通版和 legacy 版全部通过；独立流解析测试通过。
- 固件构建：沙箱内 `make -j4` 与 ARM 编译器 `--version` 均在进程启动阶段报 Windows 错误 623（无效的系统 DLL 重定位）。沙箱外版本检查正常，随后 `make -j4` 编译、链接成功并生成 ELF、HEX 和 BIN。
- 链接器报告 newlib/libnosys 的 `_close`、`_lseek`、`_read`、`_write` 未实现警告；无编译或链接错误。

### 5. 实机验收

刷写后在 `HandleValidFrame()` 返回后观察：

```text
referee_power_state.has_robot_state = true
referee_power_state.chassis_power_limit_w = 80
referee_power_state.chassis_output_enabled = true
referee_info.GameRobotState.power_management_chassis_output = 1
payload_length_error_count 对该帧不增加
```

遥控器进入非零力底盘模式后再观察：

```text
pending_power_control.control_enabled = true
motor_output[0..3] 随遥控输入产生非零值
```

若第一组状态正确但第二组仍不成立，再沿遥控器模式、急停状态和电机在线状态继续排查；不再修改裁判帧解析。

## 提交范围

实施完成后只提交本计划列出的代码、测试和裁判协议文档，不包含当前工作区中的底盘参数、串口配置或遥测频率改动。建议提交信息：

```text
fix: map extended referee output flags
```
