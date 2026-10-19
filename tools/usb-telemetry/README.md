# USB 遥测仪表盘

C 板通过 USB CDC 虚拟串口上报数据，网页实时显示底盘、遥控器、四电机、功率和缓冲能量。页面、图表和数据处理都在本机运行。

## 打开网页

电脑安装 Node.js 后，双击本目录的 `start.cmd`，或在仓库根目录运行：

```powershell
node tools/usb-telemetry/serve.cjs
```

保持命令窗口运行，用电脑端 Edge / Chrome 打开 [本地仪表盘](http://127.0.0.1:4173)。浏览器需要支持 Web Serial；页面会检查支持情况。端口占用时可用 `node tools/usb-telemetry/serve.cjs 4174` 换端口，并打开对应地址。

点击“演示数据”可先查看界面，演示模式有明显标识。使用真实设备前点击连接，选择 C 板虚拟串口并授权；进入真实连接时会清除演示数据。C 板描述符目前仍可能显示为 **Vision Comm port**（VID `0483`、PID `00CA`）。

网页的连接授权由浏览器弹窗完成。请在独立浏览器窗口打开页面；内嵌预览环境可能没有串口能力。

## 在线调参和项目文件同步

烧录支持在线调参的新固件后，重启本目录的 start.cmd，连接 C 板并进入“在线调参”页签。连接时读取当前参数；切换参数组时会读取该组。监测和调参分为两个工作区，共用连接状态与关键读数，切换页签不停止采集。

| 参数组 | 首版支持范围 |
| --- | --- |
| PID | 底盘 LF/RF/LB/RB 四轮速度环，各自 Kp、Ki、Kd、输出限幅、积分限幅、死区；误差为度/秒，电流为 DJI 原始单位 |
| 功控 | 功率慢外环全部 11 项；电容最大增益功率、预算上升速率、需求余量、空/满电容比例；缓冲最大增益功率、最大恢复降额、恢复目标时间 |
| 小陀螺 | 当前编译模式使用的目标/边界、功率反馈和正弦波形参数；模式由固件决定，网页显示实际模式 |

输入框下显示板端最后确认值。编辑只改变草稿，点击“应用并同步文件”后整组发送，板端校验通过，在所属控制任务下个周期应用并回传实际值。网页使用回执值同步文件，不将未确认或被拒绝的草稿写入文件。小陀螺运行中保持目标过渡速率，改变旋转方向需先退出小陀螺模式。修改 PID 会清除四轮积分，功控修改会重置慢外环学习量；保护链路继续生效。

同步文件为仓库中的 modules/telemetry/tuning_overrides.h。每组的 OVERRIDE=1 且 MODE 匹配当前固件模式时，VALUES 覆盖原始源码默认；有效的板端保存值具有更高优先级。字段顺序见 runtime_tuning.h。文件由本地 Node 服务原子替换，并保留其他组设置及对应模式。

每次成功应用会自动排队保存到板端 Flash。请进入零力模式，使全部已注册 DJI 电机停用、反馈在线且速度不超过 50 rpm，持续至少 100 ms；控制快照必须新鲜、零电流帧发送成功，才会保存。网页单独显示“等待板端保存 / 已保存到板端 / 保存失败”，确认已保存后再断电。保存前断电会恢复上一次完整保存值；文件已同步不代表 Flash 已保存。

板端保存失败后停止自动重试，点击“重试板端保存”重新排队，不会再次应用草稿；尚未保存的启动参数可用“保存到板端”。断开 USB 不取消已生效参数的待保存工作。文件同步失败与板端保存状态独立，可“重试同步文件”；重新读取后使用最新确认值。网页不自动烧录固件。

重启优先恢复模式匹配且校验有效的 Flash 参数；不匹配或组约束无效时，回退到模式匹配的电脑参数覆盖文件，再回退原始默认。切换控制模式后不会套用旧模式的 Flash 或源码覆盖参数。电脑文件更新仍需重新编译、烧录才进入固件，且不会覆盖已有有效板端配置。

### 掉电保护与分区

预留 STM32F407 扇区 10、11（0x080C0000 / 0x080E0000，各 128 KiB），应用链接空间为前 768 KiB。每条日志 512 字节，包含格式版本、序号、分组模式、参数、CRC32 与最后写入的提交标记；最新记录提交前，上一份有效记录始终保留。追加写入、相同值去重，写满才轮换擦除。

Flash 擦写会影响本 MCU 从 Flash 执行代码，所以只在上述停机条件满足时写入，写入期间暂停任务调度。先发送参数回执，再尝试保存。正常项目烧录不包含预留区，J-Flash 选区限制为 0～9；使用其他工具时也应保留扇区 10/11。全片擦除会清除板端参数。

串口超时表示结果未确认，请重新读取实际值；旧固件需升级。演示模式停用调参。启动服务仍为旧版本时，关闭旧命令窗口并重新运行 start.cmd。源码目录必须允许当前用户写入参数配置文件。

### 调参协议

与 CSV v1 共用串口，使用独立 @T1 行前缀，CRLF 结束；每次只允许一个在途请求。每个 float32 以八位十六进制 IEEE754 位模式传输，避免嵌入式浮点格式化。

~~~text
@T1,<请求ID>,GET,<pid|power|spin>
@T1,<请求ID>,SET,<组>,<读取的组版本>,<整组float32位模式...>
@T1,<请求ID>,SAVE,<组>,<当前生效版本>
@T1,<请求ID>,OK,<组>,<当前版本>,<模式>,<当前生效值...>,<保存状态>
@T1,<请求ID>,ERR,<组>,<错误码>
~~~

PID 为 24 项、功控 19 项、自旋 16 项。模式对 PID 为 0、功控为阶段 0～3、自旋为固定 0 / 自适应 1 / 正弦 2。版本不匹配返回 conflict；非法值、字段数量和组合约束也会拒绝。成功 SET 才递增版本，并在控制任务应用后回复。

状态为 saved（当前值已保存）、pending（等待安全保存）、error（保存失败）、volatile（启动默认尚未单独保存）、unsupported（无持久化后端）。SAVE 只排队保存确认版本，不改变实时参数；OK,pending 不代表 Flash 提交成功，继续 GET 查询直到 saved。网页兼容不带保存状态的旧回复，但会提示旧固件不支持掉电保存。

本地文件接口 POST /api/tuning 只接受本机同源 JSON 请求，不接受客户端指定文件路径；用于同步确认后的参数，不负责向 C 板发送指令。

## PID 自动整定（架空台架）

进入“自动整定”页签，可先点“演示整定流程”查看完整过程。演示使用合成数据，不能采用或保存，也不会向串口发送试调指令。

真实设备需要重新编译并烧录包含 `@A1` 支持的新固件。原来的在线调参固件只支持 `@T1`，不能执行自动试验。构建示例：

```powershell
make -j8 BUILD_DIR=build_online_tuning_autotune_usb UART1_USE=0 USB_TELEMETRY_ENABLE=1
```

1. 将底盘架空并固定，让四轮离地。打开仪表盘、连接 C 板，进入“自动整定”。
2. 遥控器左拨杆下、右拨杆中，处于无跟随模式；四摇杆、拨轮居中，不按键鼠，四轮静止。板端还检查遥控器和四电机反馈的新鲜度。若机器人之前处于急停，先将拨轮居中、右拨杆上恢复，再回到中档。
3. 选择单轮，或“依次测试全部四轮”。默认测试 500 电机 rpm、保持 1000 ms、临时电流上限 3000 原始单位；确认底盘架空后开始。
4. 每次试验先归零 300 ms，再保持目标转速，最后归零 300 ms。网页先测试原参数，再在限定范围搜索 Kp/Ki；Kd、死区和正式限幅保持原值，临时限幅不会超过原轮限幅。板端计算误差、超调、稳定时间和电流变化，明显受功率限流影响的试验会剔除。
5. 最佳候选经过独立复测，综合评分比基线改善至少 5% 才推荐采用。没有改善时保留原参数。架空结果需要落地带负载验证。
6. 试调结束后原参数已恢复。将拨轮向下超过一半（数值大于 300）进入零力模式；查看对比后点击“采用并保存推荐参数”。网页重新核对基线版本和值，板端确认后同步项目文件，并显示独立的 Flash 保存状态。同步失败可重试，不重复应用参数。确认“已保存到板端”再断电。

“停止并恢复”、摇杆/拨杆变化、零力、反馈过期、异常转速、CAN 失败和心跳超时都会结束试调。网页每 200 ms 查询状态，租期 1000 ms，单会话最长 180 秒。结束后板端锁存停机，必须经过一次明确零力模式才恢复常规控制；断线重连不会自动继续试验。候选参数不会写入项目文件或 Flash，试调期间其他参数组的修改和保存也会暂时锁定，原有功率保护继续生效。

`@A1` 与 CSV v1 / `@T1` 共用串口，支持 `BEGIN`、`TRIAL`、`STATUS`、`END`；详细字段和状态见 [设计说明](../../docs/superpowers/specs/2026-10-16-pid-autotune-design.md)。试验引擎和协议主机测试：

```powershell
make -C tests/pid_autotune test
make -B -C tests/dji_chassis_power_rtt test CC=gcc
node --test tools/usb-telemetry/tests/autotune.test.cjs
```

浏览器脚本 `tests/autotune-check.js` 使用本地端口 4176，覆盖演示、取消、窄屏、分片串口、版本冲突、采用、同步重试、板端保存状态及同步中断连重连。脚本中的串口和文件接口均被替代，不向真实开发板发指令，也不修改仓库参数文件。主机测试、浏览器模拟和固件构建通过不代表完成实物台架验收。

## 固件配置与构建

`modules/telemetry/usb_telemetry_config.h`：

| 配置 | 默认 | 含义 |
| --- | --- | --- |
| `USB_TELEMETRY_ENABLE` | `1` | 1：USB CSV 遥测；0：原有 USB 视觉协议 |
| `USB_TELEMETRY_PERIOD_MS` | `20` | 采样发送间隔，默认约 50 Hz |
| `USB_TELEMETRY_HEADER_PERIOD_MS` | `1000` | 重复表头间隔 |
| `USB_TELEMETRY_BUFFER_SIZE` | `2048` | 静态行缓冲字节数 |

使用项目现有 ARM GCC 和 make 工具链，从仓库根目录构建：

```powershell
make -j8 BUILD_DIR=build_usb_telemetry UART1_USE=0 USB_TELEMETRY_ENABLE=1
```

使用项目现有烧录方式，将 `build_usb_telemetry/basic_framework.hex` 或 `.bin` 写入 C 板。构建命令不会自动烧录。遥测模式与 USB 视觉模式互斥；USB 遥测启用时，原有 `VisionSend()` 不再往 USB 混入视觉二进制帧。

切回视觉模式，使用独立构建目录，避免沿用其他编译选项生成的旧对象文件：

```powershell
make -j8 BUILD_DIR=build_usb_vision UART1_USE=0 USB_TELEMETRY_ENABLE=0
```

`UART1_USE=0` 选择裁判链路。使用 UART1 日志配置时，网页无法获得有效裁判功率上限和原始缓冲能量。功率计、电容、遥控器等也必须已连接并启用对应固件配置，页面才会显示为在线。

## 数据与单位

数据默认每 20 ms 采样。来源分别取得完整快照，并保留来源时间；不同设备的采样时刻可以不同。USB 发送忙时丢弃新样本，不等待、不积压，避免影响控制任务。

| 区域 | 显示内容 |
| --- | --- |
| 底盘 | 模式、坐标变换后的 Vx/Vy 控制指令、最终 Wz 指令 |
| 电机 | LF 左前 / RF 右前 / LB 左后 / RB 右后，目标与实际 rpm、电流命令和反馈、在线/使能状态 |
| 遥控器 | 摇杆、拨轮、左右档位、鼠标和键盘 |
| 功率 | 实测功率、模型预测、预算、目标和限制，单位 W |
| 缓冲 | 裁判原始缓冲能量、估算下限和预测能量，单位 J |
| 电容 | 能量比例 %、输入功率、输出状态、故障码 |

底盘的整车实际速度估计尚未实现，因此 Vx/Vy/Wz 显示控制原始单位；电流显示 DJI 原始值，不标为 A。转速是电机侧 rpm。裁判缓冲能量 J 与电容能量比例 % 是不同量。

网页把无效、离线或过期数据留空为“—”，曲线在相应位置断开。固件在发送时重新检查模型、功率计、电容状态和电容输入功率的 20 ms 新鲜度；底盘快照和遥控器使用 100 ms，裁判缓冲使用 300 ms。浏览器还会拒绝超过 500 ms 的底盘/模型快照；连续 500 ms 没有新行时，整条数据流标记陈旧。时间和序号回退、缺行以及长间隔都会分段。

“暂停显示”冻结已有历史和横轴时刻，仍继续接收数据。页面最多保留最近 10 分钟且不超过 30000 条样本，普通卡片默认显示 60 秒总览。暂停快照独立保留，实时缓存更新不会删掉正在查看的数据。导出 CSV 保存当前接收缓存中的历史（最多 10 分钟），原始整数单位保持不变；可导入项目现有 [波形工具](../waveform-viewer/README.md) 做离线分析。

点击功率、缓冲能量卡片或四轮转速绘图区可弹窗放大，也可使用图表旁的“放大 ↗”按钮。按 Esc、点击关闭按钮或弹窗外区域返回；图表获得键盘焦点后，Enter / 空格也能放大。放大图跟随当前实时或暂停状态，暂停时仍可打开和调整窗口大小。

在放大图内按住 Ctrl 滚动滚轮，可以围绕鼠标位置缩放时间轴，查看 100 ms～60 s 的细节，纵轴自动适配可见数据。Ctrl + 加号/减号以图中心缩放；Ctrl + 0 或“恢复全图”回到当前显示末端前的 60 s，保持暂停状态。弹窗打开时这些操作只作用于波形，关闭后恢复浏览器快捷键；普通滚轮仍可滚动。

按住放大图内的鼠标左键左右拖动，或移动“历史位置”滑条，可查看已保留的历史。开始平移会自动暂停跟随，采集继续；滑条两端对应最早可用历史和冻结时刻的最新窗口。暂停时改选曲线、调整窗口尺寸，或关闭后重新打开同一图，都保留查看位置和缩放。点击“返回实时”或主工具栏的“恢复显示”，回到最新 60 秒并继续跟随。模式和范围提示会区分实时与历史回看，坐标及悬停时间均相对当前显示末端。

每张图旁的“显示数据”列出该图全部曲线，勾选哪些就显示哪些，可用“全选”“清空”批量操作。普通图与放大图共享选择，暂停时也可改选，已有缩放范围保持不变；悬停只显示已选数据。窄屏时选择区移到图下方。

转速图可分别选择四轮实际转速、四轮目标转速（加粗虚线）和底盘旋转目标 Wz。轮速使用左轴 rpm，Wz 使用右轴“指令值”，取自固件上报的最终 Wz 控制指令；它不是电机 rpm 或底盘实测转速。各轴独立适配有效数据，底盘数据无效时 Wz 留空。

鼠标放在普通图或大图内，可查看定位线对应的最近真实采样，包括距窗口末端的秒数、板端 tick（ms）、各条曲线名称和数值。功率为 W，能量为 J，转速为 rpm；读数已经换算，CSV 导出仍保留原始整数单位。无效来源显示“—”，通信断点和距离采样超过 500 ms 的空白不显示读数。无数据或遥控器过期时，键盘和鼠标按键保持熄灭。

## CSV v1 协议

行以 CRLF 结束，表头在串口打开后及每秒重发。串口读取块不等同于一整行，解析器能处理任意分片。

| 后缀/字段 | 原始单位 | 网页换算 |
| --- | --- | --- |
| `*_cw` | 0.01 W | 除以 100 |
| `*_cj` | 0.01 J | 除以 100 |
| `*_milli` | 0.001 控制原始单位 | 除以 1000 |
| `cap_energy_pm` | 0.001 能量比例 | 除以 10 得到 % |
| `buffer_j` | J | 保持整数 J |
| `tick` / `*_tick_ms` / `*_age_ms` | ms | 未取得样本的年龄为 4294967295 |
| `i_*_cmd` / `i_*_fb` | DJI 原始值 | 不换算为 A |

固定 72 列，`version=1`：

```csv
version,seq,tick,tx_drop_count,chassis_valid,chassis_tick_ms,chassis_mode,vx_cmd_milli,vy_cmd_milli,wz_cmd_milli,rc_online,rc_age_ms,rc_lx,rc_ly,rc_rx,rc_ry,rc_dial,rc_sw_left,rc_sw_right,mouse_x,mouse_y,mouse_left,mouse_right,keys,model_tick_ms,motor_online_mask,motor_enabled_mask,rpm_lf,rpm_rf,rpm_lb,rpm_rb,target_lf_rpm,target_rf_rpm,target_lb_rpm,target_rb_rpm,i_lf_cmd,i_rf_cmd,i_lb_cmd,i_rb_cmd,i_lf_fb,i_rf_fb,i_lb_fb,i_rb_fb,power_valid,p_unlimited_cw,p_limited_cw,effective_limit_cw,p_meter_cw,meter_online,meter_age_ms,nominal_budget_cw,target_power_cw,power_source,referee_online,referee_limit_cw,buffer_j,buffer_age_ms,buffer_control_enabled,buffer_estimate_valid,buffer_lower_cj,buffer_predicted_cj,buffer_recovering,cap_online,cap_output_enabled,cap_error,cap_energy_pm,cap_chassis_cw,cap_limit_cw,cap_age_ms,cap_input_online,cap_input_cw,cap_input_age_ms
```

`seq` 按采样周期递增（包括被丢弃的周期）；表头重发可能占用发送机会。网页显示缺行计数，并保留 `tx_drop_count` 便于判断发送拥塞。序号和 tick 使用 uint32，支持自然回绕。

`chassis_mode`：0 零力、1 小陀螺、2 无跟随、3 跟随云台。遥控器开关：1 上、3 中、2 下。`power_source`：0 直供、1 超级电容。在线/有效标志为 0/1，四电机掩码从最低位起为 LF/RF/LB/RB。

串口打开参数为 115200、8N1；此项目 USB CDC 的波特率是主机控制设置，不是 UART 线路速率限制。网页打开端口后设置 DTR，固件据此开始上报。

## 验证

```powershell
powershell -ExecutionPolicy Bypass -File tests/usb_telemetry/run_tests.ps1
make -C tests/tuning_flash test
node --test tools/usb-telemetry/tests/*.test.cjs
node --test tools/waveform-viewer/tests/core.test.cjs
```

跨端测试会使用 GCC 编译 C 格式化测试程序，并将其实际输出分成任意字节块交给网页解析器，验证字段顺序、符号、功率/能量单位和导出兼容性。独立运行网页无需 GCC；只有开发测试和固件构建需要它。

浏览器 QA 脚本位于 `tools/usb-telemetry/tests/browser-check.js`，通过 Playwright CLI 在本地页面执行：

```powershell
playwright-cli -s=usb open http://127.0.0.1:4173 --browser msedge
playwright-cli -s=usb run-code --filename tools/usb-telemetry/tests/browser-check.js
playwright-cli -s=usb run-code --filename tools/usb-telemetry/tests/chart-check.js
playwright-cli -s=usb run-code --filename tools/usb-telemetry/tests/zoom-check.js
playwright-cli -s=usb run-code --filename tools/usb-telemetry/tests/selection-check.js
playwright-cli -s=usb run-code --filename tools/usb-telemetry/tests/history-check.js
playwright-cli -s=usb run-code --filename tools/usb-telemetry/tests/tuning-check.js
playwright-cli -s=usb run-code --filename tools/usb-telemetry/tests/persistence-check.js
playwright-cli -s=usb close
```

浏览器测试使用真实 ReadableStream 模拟串口，不代表已完成 C 板实物枚举、烧录或实时性能验收。

## 连接排查

- 找不到端口：确认使用可传数据的 USB 线、板卡固件已启用 CDC，且系统识别到虚拟串口。
- 端口占用：关闭正在使用该端口的串口助手、其他网页或调试程序。
- 已连接但没有数据：确认固件使用遥测模式，等待周期表头，检查是否烧录了对应构建产物。
- 部分来源离线：检查该设备的供电、CAN/UART 链路和配置；网页连接 USB 成功并不意味着每个设备都在线。
- 拔插后：点击连接重新选择端口；已有窗口数据仍可导出。
