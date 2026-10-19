# 完整宏与初始化参数索引

整理日期：2026-10-13。配套说明：[宏与参数设置总表](configuration-guide.md)。

2026-10-15：仅同步正弦配置的独立中心/起步宏、振幅/上升速率及应用初始化分支；其余内容仍为原工作区快照。

扫描项目源码目录 application、bsp、modules、Inc、Src，以及 SEGGER RTT 配置文件，共 192 个 C/H 文件。收录 1078 条未被注释的宏定义（含本次新增的 2 条正弦参数）、56 条注释中的宏选项、32 个配置结构体初值、27 个配置结构体字段声明、481 条 HAL 设置赋值/中断优先级。

这里的“宏定义”是源码中的定义条目，包含头文件保护宏、函数式宏、协议常量、传感器寄存器常量和条件分支，**不代表每条都在当前编译中生效**。同名宏的不同条件分支分别列出，不合并默认值。条件路径的“→”表示同一 #if 链中经过 #elif/#else 到达该分支；#elif/#else 还隐含前面分支未命中。原行注释保留供核对，可能与代码不一致；实际替换文本以源码为准。

初始化列表包括配置类型变量和返回的配置复合字面量；字段声明收录类型名含 Config/Setting 或以 _Init_s 结尾的配置接口。不包括每次运行产生的 Input/Output/State 数据、全部函数形参或第三方库内部设置。函数内硬编码的输入增益、频率、超时等已在总表单列。Drivers 与其余 Middlewares 的内部宏、tests 和 portable_modules 不在此主工程索引范围内。

该文件是工作区快照，源码变更后行号和数值可能变化；点击文件后按宏名查找。

## 文件目录

| 文件 | 宏定义 | 注释宏选项 | 配置初值 | 字段声明 | HAL 设置 |
| --- | --- | --- | --- | --- | --- |
| [application/chassis/chassis.c](../application/chassis/chassis.c) | 8 | 0 | 10 | 0 | 0 |
| [application/chassis/chassis.h](../application/chassis/chassis.h) | 1 | 0 | 0 | 0 | 0 |
| [application/cmd/robot_cmd.c](../application/cmd/robot_cmd.c) | 2 | 0 | 1 | 0 | 0 |
| [application/cmd/robot_cmd.h](../application/cmd/robot_cmd.h) | 1 | 0 | 0 | 0 | 0 |
| [application/gimbal/gimbal.c](../application/gimbal/gimbal.c) | 0 | 0 | 2 | 0 | 0 |
| [application/gimbal/gimbal.h](../application/gimbal/gimbal.h) | 1 | 0 | 0 | 0 | 0 |
| [application/robot_def.h](../application/robot_def.h) | 34 | 3 | 0 | 0 | 0 |
| [application/robot.c](../application/robot.c) | 1 | 0 | 0 | 0 | 0 |
| [application/robot.h](../application/robot.h) | 1 | 0 | 0 | 0 | 0 |
| [application/shoot/shoot.c](../application/shoot/shoot.c) | 0 | 0 | 2 | 0 | 0 |
| [application/shoot/shoot.h](../application/shoot/shoot.h) | 1 | 0 | 0 | 0 | 0 |
| [bsp/bsp_init.h](../bsp/bsp_init.h) | 1 | 0 | 0 | 0 | 0 |
| [bsp/bsp_tools.c](../bsp/bsp_tools.c) | 1 | 0 | 0 | 0 | 0 |
| [bsp/can/bsp_can.h](../bsp/can/bsp_can.h) | 4 | 0 | 0 | 1 | 0 |
| [bsp/dwt/bsp_dwt.h](../bsp/dwt/bsp_dwt.h) | 2 | 0 | 0 | 0 | 0 |
| [bsp/flash/bsp_flash.h](../bsp/flash/bsp_flash.h) | 26 | 0 | 0 | 0 | 0 |
| [bsp/gpio/bsp_gpio.h](../bsp/gpio/bsp_gpio.h) | 1 | 0 | 0 | 1 | 0 |
| [bsp/iic/bsp_iic.h](../bsp/iic/bsp_iic.h) | 2 | 0 | 0 | 1 | 0 |
| [bsp/log/bsp_log.c](../bsp/log/bsp_log.c) | 1 | 0 | 0 | 0 | 0 |
| [bsp/log/bsp_log.h](../bsp/log/bsp_log.h) | 12 | 0 | 0 | 0 | 0 |
| [bsp/log/bsp_uart_log.c](../bsp/log/bsp_uart_log.c) | 1 | 0 | 0 | 0 | 0 |
| [bsp/log/bsp_uart_log.h](../bsp/log/bsp_uart_log.h) | 1 | 0 | 0 | 0 | 0 |
| [bsp/pwm/bsp_pwm.h](../bsp/pwm/bsp_pwm.h) | 2 | 0 | 0 | 1 | 0 |
| [bsp/spi/bsp_spi.h](../bsp/spi/bsp_spi.h) | 2 | 0 | 0 | 1 | 0 |
| [bsp/usart/bsp_usart.h](../bsp/usart/bsp_usart.h) | 3 | 0 | 0 | 1 | 0 |
| [bsp/usb/bsp_usb.h](../bsp/usb/bsp_usb.h) | 0 | 0 | 0 | 1 | 0 |
| [Inc/adc.h](../Inc/adc.h) | 1 | 0 | 0 | 0 | 0 |
| [Inc/board_config.h](../Inc/board_config.h) | 8 | 0 | 0 | 0 | 0 |
| [Inc/can.h](../Inc/can.h) | 1 | 0 | 0 | 0 | 0 |
| [Inc/crc.h](../Inc/crc.h) | 1 | 0 | 0 | 0 | 0 |
| [Inc/dac.h](../Inc/dac.h) | 1 | 0 | 0 | 0 | 0 |
| [Inc/dma.h](../Inc/dma.h) | 1 | 0 | 0 | 0 | 0 |
| [Inc/FreeRTOSConfig.h](../Inc/FreeRTOSConfig.h) | 41 | 0 | 0 | 0 | 0 |
| [Inc/gpio.h](../Inc/gpio.h) | 1 | 0 | 0 | 0 | 0 |
| [Inc/i2c.h](../Inc/i2c.h) | 1 | 0 | 0 | 0 | 0 |
| [Inc/main.h](../Inc/main.h) | 30 | 0 | 0 | 0 | 0 |
| [Inc/rng.h](../Inc/rng.h) | 1 | 0 | 0 | 0 | 0 |
| [Inc/rtc.h](../Inc/rtc.h) | 1 | 0 | 0 | 0 | 0 |
| [Inc/spi.h](../Inc/spi.h) | 1 | 0 | 0 | 0 | 0 |
| [Inc/stm32f4xx_hal_conf.h](../Inc/stm32f4xx_hal_conf.h) | 108 | 33 | 0 | 0 | 0 |
| [Inc/stm32f4xx_it.h](../Inc/stm32f4xx_it.h) | 1 | 0 | 0 | 0 | 0 |
| [Inc/tim.h](../Inc/tim.h) | 1 | 0 | 0 | 0 | 0 |
| [Inc/usart.h](../Inc/usart.h) | 1 | 0 | 0 | 0 | 0 |
| [Inc/usb_device.h](../Inc/usb_device.h) | 1 | 0 | 0 | 0 | 0 |
| [Inc/usbd_cdc_if.h](../Inc/usbd_cdc_if.h) | 3 | 0 | 0 | 0 | 0 |
| [Inc/usbd_conf.h](../Inc/usbd_conf.h) | 20 | 1 | 0 | 0 | 0 |
| [Inc/usbd_desc.h](../Inc/usbd_desc.h) | 5 | 0 | 0 | 0 | 0 |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h) | 47 | 3 | 0 | 0 | 0 |
| [modules/alarm/buzzer.c](../modules/alarm/buzzer.c) | 0 | 0 | 1 | 0 | 0 |
| [modules/alarm/buzzer.h](../modules/alarm/buzzer.h) | 9 | 0 | 0 | 1 | 0 |
| [modules/algorithm/chassis_energy_config.h](../modules/algorithm/chassis_energy_config.h) | 22 | 0 | 0 | 0 | 0 |
| [modules/algorithm/chassis_kinematics.c](../modules/algorithm/chassis_kinematics.c) | 1 | 0 | 0 | 0 | 0 |
| [modules/algorithm/chassis_kinematics.h](../modules/algorithm/chassis_kinematics.h) | 1 | 0 | 0 | 0 | 0 |
| [modules/algorithm/chassis_power_budget.h](../modules/algorithm/chassis_power_budget.h) | 2 | 0 | 0 | 2 | 0 |
| [modules/algorithm/chassis_power_config.h](../modules/algorithm/chassis_power_config.h) | 4 | 0 | 0 | 0 | 0 |
| [modules/algorithm/chassis_power_outer_loop.h](../modules/algorithm/chassis_power_outer_loop.h) | 1 | 0 | 0 | 1 | 0 |
| [modules/algorithm/chassis_power_transient.c](../modules/algorithm/chassis_power_transient.c) | 0 | 0 | 1 | 0 | 0 |
| [modules/algorithm/chassis_power_transient.h](../modules/algorithm/chassis_power_transient.h) | 1 | 0 | 0 | 1 | 0 |
| [modules/algorithm/chassis_spin_config.h](../modules/algorithm/chassis_spin_config.h) | 27 | 0 | 0 | 0 | 0 |
| [modules/algorithm/chassis_spin.h](../modules/algorithm/chassis_spin.h) | 1 | 0 | 0 | 1 | 0 |
| [modules/algorithm/controller.h](../modules/algorithm/controller.h) | 2 | 0 | 0 | 1 | 0 |
| [modules/algorithm/crc16.h](../modules/algorithm/crc16.h) | 4 | 0 | 0 | 0 | 0 |
| [modules/algorithm/crc8.h](../modules/algorithm/crc8.h) | 2 | 0 | 0 | 0 | 0 |
| [modules/algorithm/kalman_filter.h](../modules/algorithm/kalman_filter.h) | 10 | 5 | 0 | 0 | 0 |
| [modules/algorithm/power_model.h](../modules/algorithm/power_model.h) | 2 | 0 | 0 | 2 | 0 |
| [modules/algorithm/QuaternionEKF.h](../modules/algorithm/QuaternionEKF.h) | 3 | 0 | 0 | 0 | 0 |
| [modules/algorithm/user_lib.c](../modules/algorithm/user_lib.c) | 2 | 0 | 0 | 0 | 0 |
| [modules/algorithm/user_lib.h](../modules/algorithm/user_lib.h) | 19 | 0 | 0 | 0 | 0 |
| [modules/bluetooth/HC05.c](../modules/bluetooth/HC05.c) | 3 | 0 | 0 | 0 | 0 |
| [modules/bluetooth/HC05.h](../modules/bluetooth/HC05.h) | 2 | 0 | 0 | 0 | 0 |
| [modules/BMI088/bmi088_regNdef.h](../modules/BMI088/bmi088_regNdef.h) | 165 | 0 | 0 | 0 | 0 |
| [modules/BMI088/bmi088.c](../modules/BMI088/bmi088.c) | 7 | 0 | 0 | 0 | 0 |
| [modules/BMI088/bmi088.h](../modules/BMI088/bmi088.h) | 1 | 0 | 0 | 1 | 0 |
| [modules/can_comm/can_comm.c](../modules/can_comm/can_comm.c) | 0 | 0 | 1 | 0 | 0 |
| [modules/can_comm/can_comm.h](../modules/can_comm/can_comm.h) | 6 | 0 | 0 | 1 | 0 |
| [modules/daemon/daemon.h](../modules/daemon/daemon.h) | 2 | 0 | 0 | 1 | 0 |
| [modules/general_def.h](../modules/general_def.h) | 7 | 0 | 0 | 0 | 0 |
| [modules/imu/BMI088driver.c](../modules/imu/BMI088driver.c) | 6 | 0 | 0 | 0 | 0 |
| [modules/imu/BMI088driver.h](../modules/imu/BMI088driver.h) | 41 | 0 | 0 | 0 | 0 |
| [modules/imu/BMI088Middleware.h](../modules/imu/BMI088Middleware.h) | 2 | 5 | 0 | 0 | 0 |
| [modules/imu/BMI088reg.h](../modules/imu/BMI088reg.h) | 145 | 0 | 0 | 0 | 0 |
| [modules/imu/ins_task.c](../modules/imu/ins_task.c) | 0 | 0 | 1 | 0 | 0 |
| [modules/imu/ins_task.h](../modules/imu/ins_task.h) | 5 | 0 | 0 | 0 | 0 |
| [modules/ist8310/ist8310.c](../modules/ist8310/ist8310.c) | 4 | 0 | 0 | 0 | 0 |
| [modules/ist8310/ist8310.h](../modules/ist8310/ist8310.h) | 4 | 0 | 0 | 1 | 0 |
| [modules/master_machine/master_process.c](../modules/master_machine/master_process.c) | 0 | 0 | 3 | 0 | 0 |
| [modules/master_machine/master_process.h](../modules/master_machine/master_process.h) | 3 | 0 | 0 | 0 | 0 |
| [modules/master_machine/seasky_protocol.h](../modules/master_machine/seasky_protocol.h) | 3 | 0 | 0 | 0 | 0 |
| [modules/message_center/message_center.h](../modules/message_center/message_center.h) | 4 | 0 | 0 | 0 | 0 |
| [modules/motor/DJImotor/dji_motor.c](../modules/motor/DJImotor/dji_motor.c) | 1 | 0 | 1 | 0 | 0 |
| [modules/motor/DJImotor/dji_motor.h](../modules/motor/DJImotor/dji_motor.h) | 5 | 0 | 0 | 1 | 0 |
| [modules/motor/DMmotor/dmmotor.c](../modules/motor/DMmotor/dmmotor.c) | 0 | 0 | 1 | 0 | 0 |
| [modules/motor/DMmotor/dmmotor.h](../modules/motor/DMmotor/dmmotor.h) | 8 | 0 | 0 | 0 | 0 |
| [modules/motor/HTmotor/HT04.c](../modules/motor/HTmotor/HT04.c) | 0 | 0 | 1 | 0 | 0 |
| [modules/motor/HTmotor/HT04.h](../modules/motor/HTmotor/HT04.h) | 16 | 1 | 0 | 0 | 0 |
| [modules/motor/LKmotor/LK9025.c](../modules/motor/LKmotor/LK9025.c) | 0 | 0 | 1 | 0 | 0 |
| [modules/motor/LKmotor/LK9025.h](../modules/motor/LKmotor/LK9025.h) | 9 | 0 | 0 | 0 | 0 |
| [modules/motor/motor_def.h](../modules/motor/motor_def.h) | 2 | 0 | 0 | 3 | 0 |
| [modules/motor/motor_task.h](../modules/motor/motor_task.h) | 1 | 0 | 0 | 0 | 0 |
| [modules/motor/power_control.c](../modules/motor/power_control.c) | 2 | 0 | 1 | 0 | 0 |
| [modules/motor/power_control.h](../modules/motor/power_control.h) | 1 | 0 | 0 | 0 | 0 |
| [modules/motor/servo_motor/servo_motor.h](../modules/motor/servo_motor/servo_motor.h) | 8 | 0 | 0 | 1 | 0 |
| [modules/oled/oled.h](../modules/oled/oled.h) | 10 | 0 | 0 | 0 | 0 |
| [modules/oled/oledfont.h](../modules/oled/oledfont.h) | 1 | 0 | 0 | 0 | 0 |
| [modules/power_meter/power_meter_protocol.h](../modules/power_meter/power_meter_protocol.h) | 3 | 0 | 0 | 0 | 0 |
| [modules/power_meter/power_meter.c](../modules/power_meter/power_meter.c) | 2 | 0 | 1 | 0 | 0 |
| [modules/power_meter/power_meter.h](../modules/power_meter/power_meter.h) | 1 | 0 | 0 | 1 | 0 |
| [modules/referee/crc_ref.h](../modules/referee/crc_ref.h) | 3 | 0 | 0 | 0 | 0 |
| [modules/referee/referee_power.h](../modules/referee/referee_power.h) | 9 | 0 | 0 | 0 | 0 |
| [modules/referee/referee_protocol.h](../modules/referee/referee_protocol.h) | 5 | 0 | 0 | 0 | 0 |
| [modules/referee/referee_stream_parser.c](../modules/referee/referee_stream_parser.c) | 5 | 0 | 0 | 0 | 0 |
| [modules/referee/referee_stream_parser.h](../modules/referee/referee_stream_parser.h) | 2 | 0 | 0 | 0 | 0 |
| [modules/referee/referee_task.c](../modules/referee/referee_task.c) | 0 | 1 | 0 | 0 | 0 |
| [modules/referee/referee_task.h](../modules/referee/referee_task.h) | 1 | 0 | 0 | 0 | 0 |
| [modules/referee/referee_UI.h](../modules/referee/referee_UI.h) | 1 | 0 | 0 | 0 | 0 |
| [modules/referee/rm_referee.c](../modules/referee/rm_referee.c) | 1 | 0 | 1 | 0 | 0 |
| [modules/referee/rm_referee.h](../modules/referee/rm_referee.h) | 1 | 0 | 0 | 0 | 0 |
| [modules/remote/remote_control.c](../modules/remote/remote_control.c) | 1 | 0 | 1 | 0 | 0 |
| [modules/remote/remote_control.h](../modules/remote/remote_control.h) | 32 | 0 | 0 | 0 | 0 |
| [modules/super_cap/chassis_power_link.h](../modules/super_cap/chassis_power_link.h) | 1 | 0 | 0 | 0 | 0 |
| [modules/super_cap/super_cap_protocol.h](../modules/super_cap/super_cap_protocol.h) | 16 | 0 | 0 | 0 | 0 |
| [modules/super_cap/super_cap.c](../modules/super_cap/super_cap.c) | 1 | 0 | 1 | 0 | 0 |
| [modules/super_cap/super_cap.h](../modules/super_cap/super_cap.h) | 1 | 0 | 0 | 1 | 0 |
| [modules/telemetry/chassis_power_capture.h](../modules/telemetry/chassis_power_capture.h) | 5 | 0 | 0 | 0 | 0 |
| [modules/telemetry/rtt_telemetry.h](../modules/telemetry/rtt_telemetry.h) | 3 | 0 | 0 | 0 | 0 |
| [modules/TFminiPlus/tfminiplus.c](../modules/TFminiPlus/tfminiplus.c) | 0 | 0 | 1 | 0 | 0 |
| [modules/TFminiPlus/tfminiplus.h](../modules/TFminiPlus/tfminiplus.h) | 1 | 0 | 0 | 0 | 0 |
| [Src/adc.c](../Src/adc.c) | 0 | 0 | 0 | 0 | 15 |
| [Src/can.c](../Src/can.c) | 0 | 0 | 0 | 0 | 38 |
| [Src/crc.c](../Src/crc.c) | 0 | 0 | 0 | 0 | 1 |
| [Src/dac.c](../Src/dac.c) | 0 | 0 | 0 | 0 | 6 |
| [Src/dma.c](../Src/dma.c) | 0 | 0 | 0 | 0 | 9 |
| [Src/gpio.c](../Src/gpio.c) | 0 | 0 | 0 | 0 | 24 |
| [Src/i2c.c](../Src/i2c.c) | 0 | 0 | 0 | 0 | 57 |
| [Src/main.c](../Src/main.c) | 0 | 0 | 0 | 0 | 13 |
| [Src/rng.c](../Src/rng.c) | 0 | 0 | 0 | 0 | 1 |
| [Src/rtc.c](../Src/rtc.c) | 0 | 0 | 0 | 0 | 9 |
| [Src/spi.c](../Src/spi.c) | 0 | 0 | 0 | 0 | 64 |
| [Src/stm32f4xx_hal_msp.c](../Src/stm32f4xx_hal_msp.c) | 0 | 0 | 0 | 0 | 2 |
| [Src/stm32f4xx_hal_timebase_tim.c](../Src/stm32f4xx_hal_timebase_tim.c) | 0 | 0 | 0 | 0 | 7 |
| [Src/system_stm32f4xx.c](../Src/system_stm32f4xx.c) | 6 | 4 | 0 | 0 | 0 |
| [Src/tim.c](../Src/tim.c) | 0 | 0 | 0 | 0 | 115 |
| [Src/usart.c](../Src/usart.c) | 0 | 0 | 0 | 0 | 103 |
| [Src/usbd_conf.c](../Src/usbd_conf.c) | 0 | 0 | 0 | 0 | 17 |
| [Src/usbd_desc.c](../Src/usbd_desc.c) | 8 | 0 | 0 | 0 | 0 |

## application/chassis/chassis.c

源文件：[打开 application/chassis/chassis.c](../application/chassis/chassis.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [application/chassis/chassis.c:41](../application/chassis/chassis.c#L41) | `HALF_WHEEL_BASE` | `(WHEEL_BASE / 2.0f)` | 无外层条件 | 半轴距 |
| [application/chassis/chassis.c:42](../application/chassis/chassis.c#L42) | `HALF_TRACK_WIDTH` | `(TRACK_WIDTH / 2.0f)` | 无外层条件 | 半轮距 |
| [application/chassis/chassis.c:43](../application/chassis/chassis.c#L43) | `PERIMETER_WHEEL` | `(RADIUS_WHEEL * 2 * PI)` | 无外层条件 | 轮子周长 |
| [application/chassis/chassis.c:44](../application/chassis/chassis.c#L44) | `CHASSIS_POWER_SAFETY_FACTOR` | `0.98f` | 无外层条件 | 功率预算安全系数,用于降低功率预算,避免功率计和底盘功率模型的误差导致底盘超功率 |
| [application/chassis/chassis.c:283](../application/chassis/chassis.c#L283) | `LF_ROTATION_LEVER_SUM` | `((HALF_TRACK_WIDTH + CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE - CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)` | 无外层条件 |  |
| [application/chassis/chassis.c:284](../application/chassis/chassis.c#L284) | `RF_ROTATION_LEVER_SUM` | `((HALF_TRACK_WIDTH - CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE - CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)` | 无外层条件 |  |
| [application/chassis/chassis.c:285](../application/chassis/chassis.c#L285) | `LB_ROTATION_LEVER_SUM` | `((HALF_TRACK_WIDTH + CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE + CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)` | 无外层条件 |  |
| [application/chassis/chassis.c:286](../application/chassis/chassis.c#L286) | `RB_ROTATION_LEVER_SUM` | `((HALF_TRACK_WIDTH - CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE + CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)` | 无外层条件 |  |

### 配置结构体初值

以下记录原始定义，不自动推断运行路径或后续覆盖。未填字段为零；指针/回调表示绑定关系。

[application/chassis/chassis.c:90](../application/chassis/chassis.c#L90)

```c
ChassisSpinConfig_s chassis_spin_config = {
#if CHASSIS_SPIN_MODE == CHASSIS_SPIN_MODE_SINE
    .fixed_wz = CHASSIS_SPIN_SINE_CENTER_WZ,
    .min_wz = CHASSIS_SPIN_SINE_MIN_WZ,
#else
    .fixed_wz = CHASSIS_SPIN_FIXED_WZ,
    .min_wz = CHASSIS_SPIN_MIN_WZ,
#endif
    .max_wz = CHASSIS_SPIN_MAX_WZ,
    .gain_wz_per_w_s = CHASSIS_SPIN_POWER_GAIN,
    .rise_wz_per_s = CHASSIS_SPIN_RISE_RATE,
    .fall_wz_per_s = CHASSIS_SPIN_FALL_RATE,
    .power_lpf_tau_s = CHASSIS_SPIN_POWER_LPF_TAU_S,
    .power_deadband_w = CHASSIS_SPIN_POWER_DEADBAND_W,
    .settle_time_s = CHASSIS_SPIN_SETTLE_TIME_S,
    .budget_drop_reset_w = CHASSIS_SPIN_BUDGET_DROP_RESET_W,
    .sine_amplitude_wz = CHASSIS_SPIN_SINE_AMPLITUDE_WZ,
    .sine_period_s = CHASSIS_SPIN_SINE_PERIOD_S,
    .sine_ramp_s = CHASSIS_SPIN_SINE_RAMP_S,
    .sine_rise_wz_per_s = CHASSIS_SPIN_SINE_RISE_RATE,
    .sine_fall_wz_per_s = CHASSIS_SPIN_SINE_FALL_RATE,
    .sine_direction = CHASSIS_SPIN_SINE_DIRECTION,
};
```

[application/chassis/chassis.c:98](../application/chassis/chassis.c#L98)

```c
ChassisPowerBudgetConfig_s chassis_budget_config = {
    .empty_energy_ratio = 0.10f,
    .full_energy_ratio = 0.30f,
    .max_boost_w = 100.0f,
    .rise_rate_w_per_s = 200.0f,
    .demand_margin_w = CHASSIS_POWER_DEMAND_MARGIN_W,
    .buffer = {
        .max_energy_j = CHASSIS_BUFFER_MAX_ENERGY_J,
        .recovery_enter_j = CHASSIS_BUFFER_RECOVERY_ENTER_J,
        .recovery_exit_j = CHASSIS_BUFFER_RECOVERY_EXIT_J,
        .hard_floor_j = CHASSIS_BUFFER_HARD_FLOOR_J,
        .prediction_window_s = CHASSIS_BUFFER_PREDICTION_WINDOW_S,
        .guard_window_s = CHASSIS_BUFFER_GUARD_WINDOW_S,
        .settlement_delay_s = CHASSIS_BUFFER_SETTLEMENT_DELAY_S,
        .energy_margin_j = CHASSIS_BUFFER_ENERGY_MARGIN_J,
        .power_margin_w = CHASSIS_BUFFER_POWER_MARGIN_W,
        .max_boost_w = CHASSIS_BUFFER_BOOST_MAX_W,
        .recovery_power_w = CHASSIS_BUFFER_RECOVERY_POWER_W,
        .recovery_target_time_s = CHASSIS_BUFFER_RECOVERY_TARGET_TIME_S,
        .observer_max_step_s = CHASSIS_BUFFER_OBSERVER_MAX_STEP_S,
        .recovery_hold_s = CHASSIS_BUFFER_RECOVERY_HOLD_S,
        .rise_rate_w_per_s = CHASSIS_BUFFER_RISE_RATE_W_PER_S,
        .safety_factor = CHASSIS_POWER_SAFETY_FACTOR,
        .correction_rise_j_per_s = CHASSIS_BUFFER_CORRECTION_RISE_J_PER_S,
    },
};
```

[application/chassis/chassis.c:124](../application/chassis/chassis.c#L124)

```c
ChassisPowerOuterLoopConfig_s chassis_outer_loop_config = {
    .kp = 1.0f, // 比例增益,误差 = 目标功率 - 实测功率(W),输出为预算修正量(W)
    .ki = 1.5f, // 积分增益,积分限幅为[-基准预算, max_positive_correction_w]
    .measurement_lpf_tau_s = 0.030f, // 功率计读数一阶低通时间常数(s),抑制采样噪声
    .activation_margin_w = 5.0f, // 介入阈值(W):需求或实测功率距目标不到此值时才进入ACTIVE
    .error_deadband_w = 0.3f, // 误差死区(W):|误差|不超过此值时按0处理,停止积分
    .max_positive_correction_w = 8.0f, // 修正量上限(W):最多只上调8W;下限为-基准预算,即最狠可压到0
    .positive_slew_rate_w_per_s = 60.0f, // 修正量上升/回零速率(W/s);下调不限速,立即生效
    .budget_drop_reset_w = 1.0f, // 基准预算单周期跌落超过此值(W)时,清零正向积分与修正量
    .antiwindup_gain_per_s = 10,
    .low_demand_delay_s = .5f,
    .low_demand_decay_w_per_s = 1,
};
```

[application/chassis/chassis.c:138](../application/chassis/chassis.c#L138)

```c
MotorPowerModelConfig_s m3508_power_model = {
    .k0 = 1.3123816,
    .k1 = -0.25836727,
    .k2 = 0.0007424524,
    .k3 = 0.0024271158,
    .k4 = 0.081198552,
    .k5 = 3.890596e-08,
    .current_conversion = 1000.0f,// 电流单位转换,功率模型使用A,电机模块使用mA
};
```

[application/chassis/chassis.c:158](../application/chassis/chassis.c#L158)

```c
Motor_Init_Config_s chassis_motor_config = {
        .can_init_config.can_handle = &hcan1,
        .controller_param_init_config = {
            .speed_PID = {
                .Kp = 0.75f, // 目标和反馈由 rpm 换为度/秒，保持原 4.5 的等效增益，因为kp乘的是误差，误差单位变为原来的6倍，kp要乘六分之一
                .Ki = 0,   // 0
                .Kd = 0,   // 0
                .IntegralLimit = 3000,
                .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit | PID_Derivative_On_Measurement,
                .MaxOut = 15000,
                .Output_LPF_RC = 0.3,
            },
        },
        .controller_setting_init_config = {
            .angle_feedback_source = MOTOR_FEED,
            .speed_feedback_source = MOTOR_FEED,
            .outer_loop_type = SPEED_LOOP,
            .close_loop_type = SPEED_LOOP,
        },
        .motor_type = M3508,
    };
```

[application/chassis/chassis.c:196](../application/chassis/chassis.c#L196)

```c
DJIChassisPowerConfig_s power_config = {
        .motors = {motor_lf, motor_rf, motor_lb, motor_rb},
        .algorithm = {
            .safety_factor = CHASSIS_POWER_SAFETY_FACTOR,
            .small_error_threshold_rpm = 500.0f,
            .reserved_power_threshold_w = 54.0f,
            .per_motor_reserved_power_w = 8.0f,
            .max_current_command = 15000.0f,
        },
    };
```

[application/chassis/chassis.c:225](../application/chassis/chassis.c#L225)

```c
PID_Init_Config_s Buffer_pid_conf = {
        .Kp = 0.1,
        .Ki = 0,
        .Kd = 0,
        .IntegralLimit = 1000,
        .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit | PID_Derivative_On_Measurement,
        .MaxOut = 1000,
    };
```

[application/chassis/chassis.c:235](../application/chassis/chassis.c#L235)

```c
SuperCap_Init_Config_s cap_conf = {
        .can_config = {
            .can_handle = &hcan1,
            .tx_id = SUPERCAP_COMMAND_CAN_ID,
            .rx_id = SUPERCAP_STATUS_CAN_ID,
        },
        .offline_reload_count = 20U,
        .input_power_feedback_enabled = CHASSIS_BUFFER_POWER_ENABLE != 0,
    };
```

[application/chassis/chassis.c:247](../application/chassis/chassis.c#L247)

```c
PowerMeter_Init_Config_s power_meter_config = {
        .can_config = {
            .can_handle = &hcan1,
            .tx_id = POWER_METER_STATUS_CAN_ID,
            .rx_id = POWER_METER_STATUS_CAN_ID,
        },
        .offline_reload_count = 10U,
    };
```

[application/chassis/chassis.c:265](../application/chassis/chassis.c#L265)

```c
CANComm_Init_Config_s comm_conf = {
        .can_config = {
            .can_handle = &hcan2,
            .tx_id = 0x311,
            .rx_id = 0x312,
        },
        .recv_data_len = sizeof(Chassis_Ctrl_Cmd_s),
        .send_data_len = sizeof(Chassis_Upload_Data_s),
    };
```

## application/chassis/chassis.h

源文件：[打开 application/chassis/chassis.h](../application/chassis/chassis.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [application/chassis/chassis.h:2](../application/chassis/chassis.h#L2) | `CHASSIS_H` | 空定义 | `#ifndef CHASSIS_H` |  |

## application/cmd/robot_cmd.c

源文件：[打开 application/cmd/robot_cmd.c](../application/cmd/robot_cmd.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [application/cmd/robot_cmd.c:17](../application/cmd/robot_cmd.c#L17) | `YAW_ALIGN_ANGLE` | `(YAW_CHASSIS_ALIGN_ECD * ECD_ANGLE_COEF_DJI)` | 无外层条件 | 对齐时的角度,0-360 |
| [application/cmd/robot_cmd.c:18](../application/cmd/robot_cmd.c#L18) | `PTICH_HORIZON_ANGLE` | `(PITCH_HORIZON_ECD * ECD_ANGLE_COEF_DJI)` | 无外层条件 | pitch水平时电机的角度,0-360 |

### 配置结构体初值

以下记录原始定义，不自动推断运行路径或后续覆盖。未填字段为零；指针/回调表示绑定关系。

[application/cmd/robot_cmd.c:107](../application/cmd/robot_cmd.c#L107)

```c
CANComm_Init_Config_s comm_conf = {
        .can_config = {
            .can_handle = &hcan1,
            .tx_id = 0x312,
            .rx_id = 0x311,
        },
        .recv_data_len = sizeof(Chassis_Upload_Data_s),
        .send_data_len = sizeof(Chassis_Ctrl_Cmd_s),
    };
```

## application/cmd/robot_cmd.h

源文件：[打开 application/cmd/robot_cmd.h](../application/cmd/robot_cmd.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [application/cmd/robot_cmd.h:2](../application/cmd/robot_cmd.h#L2) | `ROBOT_CMD_H` | 空定义 | `#ifndef ROBOT_CMD_H` |  |

## application/gimbal/gimbal.c

源文件：[打开 application/gimbal/gimbal.c](../application/gimbal/gimbal.c)。

### 配置结构体初值

以下记录原始定义，不自动推断运行路径或后续覆盖。未填字段为零；指针/回调表示绑定关系。

[application/gimbal/gimbal.c:22](../application/gimbal/gimbal.c#L22)

```c
Motor_Init_Config_s yaw_config = {
        .can_init_config = {
            .can_handle = &hcan1,
            .tx_id = 1,
        },
        .controller_param_init_config = {
            .angle_PID = {
                .Kp = 8, // 8
                .Ki = 0,
                .Kd = 0,
                .DeadBand = 0.1,
                .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit | PID_Derivative_On_Measurement,
                .IntegralLimit = 100,

                .MaxOut = 500,
            },
            .speed_PID = {
                .Kp = 50,  // 50
                .Ki = 200, // 200
                .Kd = 0,
                .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit | PID_Derivative_On_Measurement,
                .IntegralLimit = 3000,
                .MaxOut = 20000,
            },
            .other_angle_feedback_ptr = &gimba_IMU_data->YawTotalAngle,
            // 还需要增加角速度额外反馈指针,注意方向,ins_task.md中有c板的bodyframe坐标系说明
            .other_speed_feedback_ptr = &gimba_IMU_data->Gyro[2],
        },
        .controller_setting_init_config = {
            .angle_feedback_source = OTHER_FEED,
            .speed_feedback_source = OTHER_FEED,
            .outer_loop_type = ANGLE_LOOP,
            .close_loop_type = ANGLE_LOOP | SPEED_LOOP,
            .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
        },
        .motor_type = GM6020};
```

[application/gimbal/gimbal.c:59](../application/gimbal/gimbal.c#L59)

```c
Motor_Init_Config_s pitch_config = {
        .can_init_config = {
            .can_handle = &hcan2,
            .tx_id = 2,
        },
        .controller_param_init_config = {
            .angle_PID = {
                .Kp = 10, // 10
                .Ki = 0,
                .Kd = 0,
                .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit | PID_Derivative_On_Measurement,
                .IntegralLimit = 100,
                .MaxOut = 500,
            },
            .speed_PID = {
                .Kp = 50,  // 50
                .Ki = 350, // 350
                .Kd = 0,   // 0
                .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit | PID_Derivative_On_Measurement,
                .IntegralLimit = 2500,
                .MaxOut = 20000,
            },
            .other_angle_feedback_ptr = &gimba_IMU_data->Pitch,
            // 还需要增加角速度额外反馈指针,注意方向,ins_task.md中有c板的bodyframe坐标系说明
            .other_speed_feedback_ptr = (&gimba_IMU_data->Gyro[0]),
        },
        .controller_setting_init_config = {
            .angle_feedback_source = OTHER_FEED,
            .speed_feedback_source = OTHER_FEED,
            .outer_loop_type = ANGLE_LOOP,
            .close_loop_type = SPEED_LOOP | ANGLE_LOOP,
            .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
        },
        .motor_type = GM6020,
    };
```

## application/gimbal/gimbal.h

源文件：[打开 application/gimbal/gimbal.h](../application/gimbal/gimbal.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [application/gimbal/gimbal.h:2](../application/gimbal/gimbal.h#L2) | `GIMBAL_H` | 空定义 | `#ifndef GIMBAL_H` |  |

## application/robot_def.h

源文件：[打开 application/robot_def.h](../application/robot_def.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [application/robot_def.h:13](../application/robot_def.h#L13) | `ROBOT_DEF_H` | 空定义 | `#ifndef ROBOT_DEF_H` |  |
| [application/robot_def.h:21](../application/robot_def.h#L21) | `ONE_BOARD` | 空定义 | `#ifndef ROBOT_DEF_H` | 单板控制整车 |
| [application/robot_def.h:25](../application/robot_def.h#L25) | `VISION_USE_VCP` | 空定义 | `#ifndef ROBOT_DEF_H` | 使用虚拟串口发送视觉数据 |
| [application/robot_def.h:33](../application/robot_def.h#L33) | `CHASSIS_POWER_BENCH_TEST` | `1` | `#ifndef ROBOT_DEF_H；#ifndef CHASSIS_POWER_BENCH_TEST` |  |
| [application/robot_def.h:36](../application/robot_def.h#L36) | `CHASSIS_POWER_BENCH_LIMIT_W` | `50.0f` | `#ifndef ROBOT_DEF_H；#ifndef CHASSIS_POWER_BENCH_LIMIT_W` | 底盘功率回退限制，单位 W |
| [application/robot_def.h:45](../application/robot_def.h#L45) | `POWER_METER_ENABLE` | `1` | `#ifndef ROBOT_DEF_H；#ifndef POWER_METER_ENABLE` |  |
| [application/robot_def.h:50](../application/robot_def.h#L50) | `POWER_METER_ENABLE_IS_VALID_0` | `1` | `#ifndef ROBOT_DEF_H` | 0 表示不启用功率计 |
| [application/robot_def.h:51](../application/robot_def.h#L51) | `POWER_METER_ENABLE_IS_VALID_1` | `1` | `#ifndef ROBOT_DEF_H` | 1 表示启用功率计 |
| [application/robot_def.h:52](../application/robot_def.h#L52) | `POWER_METER_ENABLE_IS_VALID_IMPL(value)` | `POWER_METER_ENABLE_IS_VALID_##value` | `#ifndef ROBOT_DEF_H` |  |
| [application/robot_def.h:53](../application/robot_def.h#L53) | `POWER_METER_ENABLE_IS_VALID(value)` | `POWER_METER_ENABLE_IS_VALID_IMPL(value)` | `#ifndef ROBOT_DEF_H` |  |
| [application/robot_def.h:66](../application/robot_def.h#L66) | `CHASSIS_DRIVE_MECANUM` | `0` | `#ifndef ROBOT_DEF_H` |  |
| [application/robot_def.h:67](../application/robot_def.h#L67) | `CHASSIS_DRIVE_OMNI_X` | `1` | `#ifndef ROBOT_DEF_H` |  |
| [application/robot_def.h:69](../application/robot_def.h#L69) | `CHASSIS_DRIVE_TYPE` | `CHASSIS_DRIVE_MECANUM` | `#ifndef ROBOT_DEF_H；#ifndef CHASSIS_DRIVE_TYPE` |  |
| [application/robot_def.h:78](../application/robot_def.h#L78) | `YAW_CHASSIS_ALIGN_ECD` | `3734` | `#ifndef ROBOT_DEF_H` | 云台和底盘对齐指向相同方向时的电机编码器值,若对云台有机械改动需要修改 |
| [application/robot_def.h:79](../application/robot_def.h#L79) | `YAW_ECD_GREATER_THAN_4096` | `0` | `#ifndef ROBOT_DEF_H` | ALIGN_ECD值是否大于4096,是为1,否为0;用于计算云台偏转角度 |
| [application/robot_def.h:80](../application/robot_def.h#L80) | `PITCH_HORIZON_ECD` | `3412` | `#ifndef ROBOT_DEF_H` | 云台处于水平位置时编码器值,若对云台有机械改动需要修改 |
| [application/robot_def.h:81](../application/robot_def.h#L81) | `PITCH_MAX_ANGLE` | `0` | `#ifndef ROBOT_DEF_H` | 云台竖直方向最大角度 (注意反馈如果是陀螺仪，则填写陀螺仪的角度) |
| [application/robot_def.h:82](../application/robot_def.h#L82) | `PITCH_MIN_ANGLE` | `0` | `#ifndef ROBOT_DEF_H` | 云台竖直方向最小角度 (注意反馈如果是陀螺仪，则填写陀螺仪的角度) |
| [application/robot_def.h:84](../application/robot_def.h#L84) | `ONE_BULLET_DELTA_ANGLE` | `36` | `#ifndef ROBOT_DEF_H` | 发射一发弹丸拨盘转动的距离,由机械设计图纸给出 |
| [application/robot_def.h:85](../application/robot_def.h#L85) | `REDUCTION_RATIO_LOADER` | `36.0f` | `#ifndef ROBOT_DEF_H` | 2006拨盘电机的减速比,英雄需要修改为3508的19.0f |
| [application/robot_def.h:86](../application/robot_def.h#L86) | `NUM_PER_CIRCLE` | `10` | `#ifndef ROBOT_DEF_H` | 拨盘一圈的装载量 |
| [application/robot_def.h:88](../application/robot_def.h#L88) | `WHEEL_BASE` | `350` | `#ifndef ROBOT_DEF_H` | 纵向轴距(前进后退方向) |
| [application/robot_def.h:89](../application/robot_def.h#L89) | `TRACK_WIDTH` | `300` | `#ifndef ROBOT_DEF_H` | 横向轮距(左右平移方向) |
| [application/robot_def.h:90](../application/robot_def.h#L90) | `CENTER_GIMBAL_OFFSET_X` | `0` | `#ifndef ROBOT_DEF_H` | 云台旋转中心距底盘几何中心的距离,前后方向,云台位于正中心时默认设为0 |
| [application/robot_def.h:91](../application/robot_def.h#L91) | `CENTER_GIMBAL_OFFSET_Y` | `0` | `#ifndef ROBOT_DEF_H` | 云台旋转中心距底盘几何中心的距离,左右方向,云台位于正中心时默认设为0 |
| [application/robot_def.h:92](../application/robot_def.h#L92) | `RADIUS_WHEEL` | `60` | `#ifndef ROBOT_DEF_H` | 轮子半径 |
| [application/robot_def.h:93](../application/robot_def.h#L93) | `REDUCTION_RATIO_WHEEL` | `19.0f` | `#ifndef ROBOT_DEF_H` | 电机减速比,因为编码器量测的是转子的速度而不是输出轴的速度故需进行转换 |
| [application/robot_def.h:97](../application/robot_def.h#L97) | `OMNI_X_LF_DIRECTION` | `1.0f` | `#ifndef ROBOT_DEF_H；#ifndef OMNI_X_LF_DIRECTION` |  |
| [application/robot_def.h:100](../application/robot_def.h#L100) | `OMNI_X_RF_DIRECTION` | `1.0f` | `#ifndef ROBOT_DEF_H；#ifndef OMNI_X_RF_DIRECTION` |  |
| [application/robot_def.h:103](../application/robot_def.h#L103) | `OMNI_X_LB_DIRECTION` | `1.0f` | `#ifndef ROBOT_DEF_H；#ifndef OMNI_X_LB_DIRECTION` |  |
| [application/robot_def.h:106](../application/robot_def.h#L106) | `OMNI_X_RB_DIRECTION` | `1.0f` | `#ifndef ROBOT_DEF_H；#ifndef OMNI_X_RB_DIRECTION` |  |
| [application/robot_def.h:109](../application/robot_def.h#L109) | `GYRO2GIMBAL_DIR_YAW` | `1` | `#ifndef ROBOT_DEF_H` | 陀螺仪数据相较于云台的yaw的方向,1为相同,-1为相反 |
| [application/robot_def.h:110](../application/robot_def.h#L110) | `GYRO2GIMBAL_DIR_PITCH` | `1` | `#ifndef ROBOT_DEF_H` | 陀螺仪数据相较于云台的pitch的方向,1为相同,-1为相反 |
| [application/robot_def.h:111](../application/robot_def.h#L111) | `GYRO2GIMBAL_DIR_ROLL` | `1` | `#ifndef ROBOT_DEF_H` | 陀螺仪数据相较于云台的roll的方向,1为相同,-1为相反 |

### 已注释的宏选项（当前不定义）

| 行 | 原选项 |
| --- | --- |
| [application/robot_def.h:22](../application/robot_def.h#L22) | `#define CHASSIS_BOARD //底盘板` |
| [application/robot_def.h:23](../application/robot_def.h#L23) | `#define GIMBAL_BOARD //云台板` |
| [application/robot_def.h:26](../application/robot_def.h#L26) | `#define VISION_USE_UART // 使用串口发送视觉数据` |

## application/robot.c

源文件：[打开 application/robot.c](../application/robot.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [application/robot.c:8](../application/robot.c#L8) | `ROBOT_DEF_PARAM_WARNING` | 空定义 | `#ifndef ROBOT_DEF_PARAM_WARNING` |  |

## application/robot.h

源文件：[打开 application/robot.h](../application/robot.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [application/robot.h:2](../application/robot.h#L2) | `ROBOT_H` | 空定义 | `#ifndef ROBOT_H` |  |

## application/shoot/shoot.c

源文件：[打开 application/shoot/shoot.c](../application/shoot/shoot.c)。

### 配置结构体初值

以下记录原始定义，不自动推断运行路径或后续覆盖。未填字段为零；指针/回调表示绑定关系。

[application/shoot/shoot.c:24](../application/shoot/shoot.c#L24)

```c
Motor_Init_Config_s friction_config = {
        .can_init_config = {
            .can_handle = &hcan2,
        },
        .controller_param_init_config = {
            .speed_PID = {
                .Kp = 0, // 20
                .Ki = 0, // 1
                .Kd = 0,
                .Improve = PID_Integral_Limit,
                .IntegralLimit = 10000,
                .MaxOut = 15000,
            },
            .current_PID = {
                .Kp = 0, // 0.7
                .Ki = 0, // 0.1
                .Kd = 0,
                .Improve = PID_Integral_Limit,
                .IntegralLimit = 10000,
                .MaxOut = 15000,
            },
        },
        .controller_setting_init_config = {
            .angle_feedback_source = MOTOR_FEED,
            .speed_feedback_source = MOTOR_FEED,

            .outer_loop_type = SPEED_LOOP,
            .close_loop_type = SPEED_LOOP | CURRENT_LOOP,
            .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
        },
        .motor_type = M3508};
```

[application/shoot/shoot.c:63](../application/shoot/shoot.c#L63)

```c
Motor_Init_Config_s loader_config = {
        .can_init_config = {
            .can_handle = &hcan2,
            .tx_id = 3,
        },
        .controller_param_init_config = {
            .angle_PID = {
                // 如果启用位置环来控制发弹,需要较大的I值保证输出力矩的线性度否则出现接近拨出的力矩大幅下降
                .Kp = 0, // 10
                .Ki = 0,
                .Kd = 0,
                .MaxOut = 200,
            },
            .speed_PID = {
                .Kp = 0, // 10
                .Ki = 0, // 1
                .Kd = 0,
                .Improve = PID_Integral_Limit,
                .IntegralLimit = 5000,
                .MaxOut = 5000,
            },
            .current_PID = {
                .Kp = 0, // 0.7
                .Ki = 0, // 0.1
                .Kd = 0,
                .Improve = PID_Integral_Limit,
                .IntegralLimit = 5000,
                .MaxOut = 5000,
            },
        },
        .controller_setting_init_config = {
            .angle_feedback_source = MOTOR_FEED, .speed_feedback_source = MOTOR_FEED,
            .outer_loop_type = SPEED_LOOP, // 初始化成SPEED_LOOP,让拨盘停在原地,防止拨盘上电时乱转
            .close_loop_type = CURRENT_LOOP | SPEED_LOOP,
            .motor_reverse_flag = MOTOR_DIRECTION_NORMAL, // 注意方向设置为拨盘的拨出的击发方向
        },
        .motor_type = M2006 // 英雄使用m3508
    };
```

## application/shoot/shoot.h

源文件：[打开 application/shoot/shoot.h](../application/shoot/shoot.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [application/shoot/shoot.h:2](../application/shoot/shoot.h#L2) | `SHOOT_H` | 空定义 | `#ifndef SHOOT_H` |  |

## bsp/bsp_init.h

源文件：[打开 bsp/bsp_init.h](../bsp/bsp_init.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [bsp/bsp_init.h:2](../bsp/bsp_init.h#L2) | `BSP_INIT_h` | 空定义 | `#ifndef BSP_INIT_h` |  |

## bsp/bsp_tools.c

源文件：[打开 bsp/bsp_tools.c](../bsp/bsp_tools.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [bsp/bsp_tools.c:7](../bsp/bsp_tools.c#L7) | `MX_SIG_LIST_SIZE` | `32` | 无外层条件 | 不可修改,最大信号量数量 |

## bsp/can/bsp_can.h

源文件：[打开 bsp/can/bsp_can.h](../bsp/can/bsp_can.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [bsp/can/bsp_can.h:2](../bsp/can/bsp_can.h#L2) | `BSP_CAN_H` | 空定义 | `#ifndef BSP_CAN_H` |  |
| [bsp/can/bsp_can.h:8](../bsp/can/bsp_can.h#L8) | `CAN_MX_REGISTER_CNT` | `16` | `#ifndef BSP_CAN_H` | 这个数量取决于CAN总线的负载 |
| [bsp/can/bsp_can.h:9](../bsp/can/bsp_can.h#L9) | `MX_CAN_FILTER_CNT` | `(2 * 14)` | `#ifndef BSP_CAN_H` | 最多可以使用的CAN过滤器数量,目前远不会用到这么多 |
| [bsp/can/bsp_can.h:10](../bsp/can/bsp_can.h#L10) | `DEVICE_CAN_CNT` | `2` | `#ifndef BSP_CAN_H` | 根据板子设定,F407IG有CAN1,CAN2,因此为2;F334只有一个,则设为1 |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[bsp/can/bsp_can.h:32](../bsp/can/bsp_can.h#L32)

```c
typedef struct
{
    CAN_HandleTypeDef *can_handle;              // can句柄
    uint32_t tx_id;                             // 发送id
    uint32_t rx_id;                             // 接收id
    void (*can_module_callback)(CANInstance *); // 处理接收数据的回调函数
    void *id;                                   // 拥有can实例的模块地址,用于区分不同的模块(如果有需要的话),如果不需要可以不传入
} CAN_Init_Config_s;
```

## bsp/dwt/bsp_dwt.h

源文件：[打开 bsp/dwt/bsp_dwt.h](../bsp/dwt/bsp_dwt.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [bsp/dwt/bsp_dwt.h:15](../bsp/dwt/bsp_dwt.h#L15) | `_BSP_DWT_H` | 空定义 | `#ifndef _BSP_DWT_H` |  |
| [bsp/dwt/bsp_dwt.h:33](../bsp/dwt/bsp_dwt.h#L33) | `TIME_ELAPSE(dt, code)` | `do { float tstart = DWT_GetTimeline_s(); code; dt = DWT_GetTimeline_s() - tstart; LOGINFO("[DWT] " #dt " = %f s\r\n", dt); } while (0)` | `#ifndef _BSP_DWT_H` |  |

## bsp/flash/bsp_flash.h

源文件：[打开 bsp/flash/bsp_flash.h](../bsp/flash/bsp_flash.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [bsp/flash/bsp_flash.h:2](../bsp/flash/bsp_flash.h#L2) | `_BSP_FLASH_H` | 空定义 | `#ifndef _BSP_FLASH_H` |  |
| [bsp/flash/bsp_flash.h:6](../bsp/flash/bsp_flash.h#L6) | `ADDR_FLASH_SECTOR_0` | `((uint32_t)0x08000000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 0, 16 Kbytes |
| [bsp/flash/bsp_flash.h:7](../bsp/flash/bsp_flash.h#L7) | `ADDR_FLASH_SECTOR_1` | `((uint32_t)0x08004000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 1, 16 Kbytes |
| [bsp/flash/bsp_flash.h:8](../bsp/flash/bsp_flash.h#L8) | `ADDR_FLASH_SECTOR_2` | `((uint32_t)0x08008000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 2, 16 Kbytes |
| [bsp/flash/bsp_flash.h:9](../bsp/flash/bsp_flash.h#L9) | `ADDR_FLASH_SECTOR_3` | `((uint32_t)0x0800C000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 3, 16 Kbytes |
| [bsp/flash/bsp_flash.h:10](../bsp/flash/bsp_flash.h#L10) | `ADDR_FLASH_SECTOR_4` | `((uint32_t)0x08010000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 4, 64 Kbytes |
| [bsp/flash/bsp_flash.h:11](../bsp/flash/bsp_flash.h#L11) | `ADDR_FLASH_SECTOR_5` | `((uint32_t)0x08020000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 5, 128 Kbytes |
| [bsp/flash/bsp_flash.h:12](../bsp/flash/bsp_flash.h#L12) | `ADDR_FLASH_SECTOR_6` | `((uint32_t)0x08040000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 6, 128 Kbytes |
| [bsp/flash/bsp_flash.h:13](../bsp/flash/bsp_flash.h#L13) | `ADDR_FLASH_SECTOR_7` | `((uint32_t)0x08060000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 7, 128 Kbytes |
| [bsp/flash/bsp_flash.h:14](../bsp/flash/bsp_flash.h#L14) | `ADDR_FLASH_SECTOR_8` | `((uint32_t)0x08080000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 8, 128 Kbytes |
| [bsp/flash/bsp_flash.h:15](../bsp/flash/bsp_flash.h#L15) | `ADDR_FLASH_SECTOR_9` | `((uint32_t)0x080A0000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 9, 128 Kbytes |
| [bsp/flash/bsp_flash.h:16](../bsp/flash/bsp_flash.h#L16) | `ADDR_FLASH_SECTOR_10` | `((uint32_t)0x080C0000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 10, 128 Kbytes |
| [bsp/flash/bsp_flash.h:17](../bsp/flash/bsp_flash.h#L17) | `ADDR_FLASH_SECTOR_11` | `((uint32_t)0x080E0000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 11, 128 Kbytes |
| [bsp/flash/bsp_flash.h:19](../bsp/flash/bsp_flash.h#L19) | `FLASH_END_ADDR` | `((uint32_t)0x08100000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 23, 128 Kbytes |
| [bsp/flash/bsp_flash.h:22](../bsp/flash/bsp_flash.h#L22) | `ADDR_FLASH_SECTOR_12` | `((uint32_t)0x08100000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 12, 16 Kbytes |
| [bsp/flash/bsp_flash.h:23](../bsp/flash/bsp_flash.h#L23) | `ADDR_FLASH_SECTOR_13` | `((uint32_t)0x08104000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 13, 16 Kbytes |
| [bsp/flash/bsp_flash.h:24](../bsp/flash/bsp_flash.h#L24) | `ADDR_FLASH_SECTOR_14` | `((uint32_t)0x08108000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 14, 16 Kbytes |
| [bsp/flash/bsp_flash.h:25](../bsp/flash/bsp_flash.h#L25) | `ADDR_FLASH_SECTOR_15` | `((uint32_t)0x0810C000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 15, 16 Kbytes |
| [bsp/flash/bsp_flash.h:26](../bsp/flash/bsp_flash.h#L26) | `ADDR_FLASH_SECTOR_16` | `((uint32_t)0x08110000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 16, 64 Kbytes |
| [bsp/flash/bsp_flash.h:27](../bsp/flash/bsp_flash.h#L27) | `ADDR_FLASH_SECTOR_17` | `((uint32_t)0x08120000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 17, 128 Kbytes |
| [bsp/flash/bsp_flash.h:28](../bsp/flash/bsp_flash.h#L28) | `ADDR_FLASH_SECTOR_18` | `((uint32_t)0x08140000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 18, 128 Kbytes |
| [bsp/flash/bsp_flash.h:29](../bsp/flash/bsp_flash.h#L29) | `ADDR_FLASH_SECTOR_19` | `((uint32_t)0x08160000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 19, 128 Kbytes |
| [bsp/flash/bsp_flash.h:30](../bsp/flash/bsp_flash.h#L30) | `ADDR_FLASH_SECTOR_20` | `((uint32_t)0x08180000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 20, 128 Kbytes |
| [bsp/flash/bsp_flash.h:31](../bsp/flash/bsp_flash.h#L31) | `ADDR_FLASH_SECTOR_21` | `((uint32_t)0x081A0000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 21, 128 Kbytes |
| [bsp/flash/bsp_flash.h:32](../bsp/flash/bsp_flash.h#L32) | `ADDR_FLASH_SECTOR_22` | `((uint32_t)0x081C0000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 22, 128 Kbytes |
| [bsp/flash/bsp_flash.h:33](../bsp/flash/bsp_flash.h#L33) | `ADDR_FLASH_SECTOR_23` | `((uint32_t)0x081E0000)` | `#ifndef _BSP_FLASH_H` | Base address of Sector 23, 128 Kbytes |

## bsp/gpio/bsp_gpio.h

源文件：[打开 bsp/gpio/bsp_gpio.h](../bsp/gpio/bsp_gpio.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [bsp/gpio/bsp_gpio.h:4](../bsp/gpio/bsp_gpio.h#L4) | `GPIO_MX_DEVICE_NUM` | `10` | 无外层条件 |  |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[bsp/gpio/bsp_gpio.h:39](../bsp/gpio/bsp_gpio.h#L39)

```c
typedef struct
{
    GPIO_TypeDef *GPIOx;        // GPIOA,GPIOB,GPIOC...
    GPIO_PinState pin_state;    // 引脚状态,Set,Reset not frequently used
    GPIO_EXTI_MODE_e exti_mode; // 外部中断模式 not frequently used
    uint16_t GPIO_Pin;          // 引脚号,@note 这里的引脚号是GPIO_PIN_0,GPIO_PIN_1...
    // 这些引脚是stm32f4xx_hal_gpio.h中定义的宏!!! 一定要注意

    void (*gpio_model_callback)(GPIOInstance *); // exti中断回调函数
    void *id;                                    // 区分不同的GPIO实例

} GPIO_Init_Config_s;
```

## bsp/iic/bsp_iic.h

源文件：[打开 bsp/iic/bsp_iic.h](../bsp/iic/bsp_iic.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [bsp/iic/bsp_iic.h:4](../bsp/iic/bsp_iic.h#L4) | `IIC_DEVICE_CNT` | `2` | 无外层条件 | C板引出了I2C2和I2C3 |
| [bsp/iic/bsp_iic.h:5](../bsp/iic/bsp_iic.h#L5) | `MX_IIC_SLAVE_CNT` | `8` | 无外层条件 | 最大从机数目,根据需要修改 |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[bsp/iic/bsp_iic.h:47](../bsp/iic/bsp_iic.h#L47)

```c
typedef struct
{
    I2C_HandleTypeDef *handle;       // i2c handle
    uint8_t dev_address;             // 暂时只支持7位地址(还有一位是读写位),注意不需要左移
    IIC_Work_Mode_e work_mode;       // 工作模式
    void (*callback)(IICInstance *); // 接收完成后的回调函数
    void *id;                        // 用于标识i2c instance
} IIC_Init_Config_s;
```

## bsp/log/bsp_log.c

源文件：[打开 bsp/log/bsp_log.c](../bsp/log/bsp_log.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [bsp/log/bsp_log.c:11](../bsp/log/bsp_log.c#L11) | `BSP_LOG_FORMAT_BUFFER_SIZE` | `384U` | 无外层条件 | 384字节的格式化缓冲区,足够打印一行日志,超过则截断 |

## bsp/log/bsp_log.h

源文件：[打开 bsp/log/bsp_log.h](../bsp/log/bsp_log.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [bsp/log/bsp_log.h:2](../bsp/log/bsp_log.h#L2) | `_BSP_LOG_H` | 空定义 | `#ifndef _BSP_LOG_H` |  |
| [bsp/log/bsp_log.h:9](../bsp/log/bsp_log.h#L9) | `DISABLE_LOG_SYSTEM` | `0` | `#ifndef _BSP_LOG_H；#ifndef DISABLE_LOG_SYSTEM` |  |
| [bsp/log/bsp_log.h:12](../bsp/log/bsp_log.h#L12) | `BUFFER_INDEX` | `0` | `#ifndef _BSP_LOG_H` |  |
| [bsp/log/bsp_log.h:33](../bsp/log/bsp_log.h#L33) | `LOG_PROTO(type, color, format, ...)` | `PrintLog(" %s%s" format "\r\n%s", color, type, ##__VA_ARGS__, RTT_CTRL_RESET)` | `#ifndef _BSP_LOG_H` |  |
| [bsp/log/bsp_log.h:40](../bsp/log/bsp_log.h#L40) | `LOG_CLEAR()` | `BSPLogClear()` | `#ifndef _BSP_LOG_H` |  |
| [bsp/log/bsp_log.h:43](../bsp/log/bsp_log.h#L43) | `LOG(format, ...)` | `LOG_PROTO("", "", format, ##__VA_ARGS__)` | `#ifndef _BSP_LOG_H` |  |
| [bsp/log/bsp_log.h:51](../bsp/log/bsp_log.h#L51) | `LOGINFO(format, ...)` | 空定义 | `#ifndef _BSP_LOG_H；#if DISABLE_LOG_SYSTEM` |  |
| [bsp/log/bsp_log.h:52](../bsp/log/bsp_log.h#L52) | `LOGWARNING(format, ...)` | 空定义 | `#ifndef _BSP_LOG_H；#if DISABLE_LOG_SYSTEM` |  |
| [bsp/log/bsp_log.h:53](../bsp/log/bsp_log.h#L53) | `LOGERROR(format, ...)` | 空定义 | `#ifndef _BSP_LOG_H；#if DISABLE_LOG_SYSTEM` |  |
| [bsp/log/bsp_log.h:56](../bsp/log/bsp_log.h#L56) | `LOGINFO(format, ...)` | `LOG_PROTO("I:", RTT_CTRL_TEXT_BRIGHT_GREEN, format, ##__VA_ARGS__)` | `#ifndef _BSP_LOG_H；#if DISABLE_LOG_SYSTEM → #else` |  |
| [bsp/log/bsp_log.h:58](../bsp/log/bsp_log.h#L58) | `LOGWARNING(format, ...)` | `LOG_PROTO("W:", RTT_CTRL_TEXT_BRIGHT_YELLOW, format, ##__VA_ARGS__)` | `#ifndef _BSP_LOG_H；#if DISABLE_LOG_SYSTEM → #else` |  |
| [bsp/log/bsp_log.h:60](../bsp/log/bsp_log.h#L60) | `LOGERROR(format, ...)` | `LOG_PROTO("E:", RTT_CTRL_TEXT_BRIGHT_RED, format, ##__VA_ARGS__)` | `#ifndef _BSP_LOG_H；#if DISABLE_LOG_SYSTEM → #else` |  |

## bsp/log/bsp_uart_log.c

源文件：[打开 bsp/log/bsp_uart_log.c](../bsp/log/bsp_uart_log.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [bsp/log/bsp_uart_log.c:14](../bsp/log/bsp_uart_log.c#L14) | `BSP_UART_LOG_RING_SIZE` | `4096U` | 无外层条件 |  |

## bsp/log/bsp_uart_log.h

源文件：[打开 bsp/log/bsp_uart_log.h](../bsp/log/bsp_uart_log.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [bsp/log/bsp_uart_log.h:2](../bsp/log/bsp_uart_log.h#L2) | `BSP_UART_LOG_H` | 空定义 | `#ifndef BSP_UART_LOG_H` |  |

## bsp/pwm/bsp_pwm.h

源文件：[打开 bsp/pwm/bsp_pwm.h](../bsp/pwm/bsp_pwm.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [bsp/pwm/bsp_pwm.h:13](../bsp/pwm/bsp_pwm.h#L13) | `BSP_PWM_H` | 空定义 | `#ifndef BSP_PWM_H` |  |
| [bsp/pwm/bsp_pwm.h:19](../bsp/pwm/bsp_pwm.h#L19) | `PWM_DEVICE_CNT` | `16` | `#ifndef BSP_PWM_H` | 最大支持的PWM实例数量 |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[bsp/pwm/bsp_pwm.h:33](../bsp/pwm/bsp_pwm.h#L33)

```c
typedef struct
{
    TIM_HandleTypeDef *htim;                 // TIM句柄
    uint32_t channel;                        // 通道
    float period;                         // 周期
    float dutyratio;                      // 占空比
    void (*callback)(PWMInstance*); // DMA传输完成回调函数
    void *id;                                // 实例ID
} PWM_Init_Config_s;
```

## bsp/spi/bsp_spi.h

源文件：[打开 bsp/spi/bsp_spi.h](../bsp/spi/bsp_spi.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [bsp/spi/bsp_spi.h:6](../bsp/spi/bsp_spi.h#L6) | `SPI_DEVICE_CNT` | `2` | 无外层条件 | C型开发板引出两路spi,分别连接BMI088/作为扩展IO在8pin牛角座引出 |
| [bsp/spi/bsp_spi.h:7](../bsp/spi/bsp_spi.h#L7) | `MX_SPI_BUS_SLAVE_CNT` | `4` | 无外层条件 | 单个spi总线上挂载的从机数目 |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[bsp/spi/bsp_spi.h:38](../bsp/spi/bsp_spi.h#L38)

```c
typedef struct
{
    SPI_HandleTypeDef *spi_handle; // SPI外设handle
    GPIO_TypeDef *GPIOx;           // 片选信号对应的GPIO,如GPIOA,GPIOB等等
    uint16_t cs_pin;               // 片选信号对应的引脚号,GPIO_PIN_1,GPIO_PIN_2等等

    SPI_TXRX_MODE_e spi_work_mode; // 传输工作模式

    spi_rx_callback callback; // 接收回调函数
    void *id;                 // 模块指针
} SPI_Init_Config_s;
```

## bsp/usart/bsp_usart.h

源文件：[打开 bsp/usart/bsp_usart.h](../bsp/usart/bsp_usart.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [bsp/usart/bsp_usart.h:2](../bsp/usart/bsp_usart.h#L2) | `BSP_RC_H` | 空定义 | `#ifndef BSP_RC_H` |  |
| [bsp/usart/bsp_usart.h:7](../bsp/usart/bsp_usart.h#L7) | `DEVICE_USART_CNT` | `3` | `#ifndef BSP_RC_H` | C板至多分配3个串口 |
| [bsp/usart/bsp_usart.h:8](../bsp/usart/bsp_usart.h#L8) | `USART_RXBUFF_LIMIT` | `256` | `#ifndef BSP_RC_H` | 如果协议需要更大的buff,请修改这里 |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[bsp/usart/bsp_usart.h:34](../bsp/usart/bsp_usart.h#L34)

```c
typedef struct
{
    uint8_t recv_buff_size;                // 模块接收一包数据的大小
    UART_HandleTypeDef *usart_handle;      // 实例对应的usart_handle
    usart_module_callback module_callback; // 解析收到的数据的回调函数
} USART_Init_Config_s;
```

## bsp/usb/bsp_usb.h

源文件：[打开 bsp/usb/bsp_usb.h](../bsp/usb/bsp_usb.h)。

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[bsp/usb/bsp_usb.h:19](../bsp/usb/bsp_usb.h#L19)

```c
typedef struct
{
    USBCallback tx_cbk;
    USBCallback rx_cbk;
} USB_Init_Config_s;
```

## Inc/adc.h

源文件：[打开 Inc/adc.h](../Inc/adc.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/adc.h:22](../Inc/adc.h#L22) | `__ADC_H__` | 空定义 | `#ifndef __ADC_H__` |  |

## Inc/board_config.h

源文件：[打开 Inc/board_config.h](../Inc/board_config.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/board_config.h:2](../Inc/board_config.h#L2) | `BOARD_CONFIG_H` | 空定义 | `#ifndef BOARD_CONFIG_H` |  |
| [Inc/board_config.h:5](../Inc/board_config.h#L5) | `UART1_USE_REFEREE` | `0` | `#ifndef BOARD_CONFIG_H` |  |
| [Inc/board_config.h:6](../Inc/board_config.h#L6) | `UART1_USE_LOG` | `1` | `#ifndef BOARD_CONFIG_H` |  |
| [Inc/board_config.h:7](../Inc/board_config.h#L7) | `UART1_USE_LOG_RTT` | `2` | `#ifndef BOARD_CONFIG_H` |  |
| [Inc/board_config.h:10](../Inc/board_config.h#L10) | `UART1_USEUART1_USE_REFEREE` | 空定义 | `#ifndef BOARD_CONFIG_H；#ifndef UART1_USE` |  |
| [Inc/board_config.h:20](../Inc/board_config.h#L20) | `UART1_REFEREE_ENABLED` | `(UART1_USE == UART1_USE_REFEREE)` | 无外层条件 |  |
| [Inc/board_config.h:21](../Inc/board_config.h#L21) | `UART1_LOG_ENABLED` | `(UART1_USE != UART1_USE_REFEREE)` | 无外层条件 |  |
| [Inc/board_config.h:22](../Inc/board_config.h#L22) | `UART1_RTT_ENABLED` | `(UART1_USE != UART1_USE_LOG)` | 无外层条件 |  |

## Inc/can.h

源文件：[打开 Inc/can.h](../Inc/can.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/can.h:22](../Inc/can.h#L22) | `__CAN_H__` | 空定义 | `#ifndef __CAN_H__` |  |

## Inc/crc.h

源文件：[打开 Inc/crc.h](../Inc/crc.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/crc.h:22](../Inc/crc.h#L22) | `__CRC_H__` | 空定义 | `#ifndef __CRC_H__` |  |

## Inc/dac.h

源文件：[打开 Inc/dac.h](../Inc/dac.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/dac.h:22](../Inc/dac.h#L22) | `__DAC_H__` | 空定义 | `#ifndef __DAC_H__` |  |

## Inc/dma.h

源文件：[打开 Inc/dma.h](../Inc/dma.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/dma.h:22](../Inc/dma.h#L22) | `__DMA_H__` | 空定义 | `#ifndef __DMA_H__` |  |

## Inc/FreeRTOSConfig.h

源文件：[打开 Inc/FreeRTOSConfig.h](../Inc/FreeRTOSConfig.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/FreeRTOSConfig.h:32](../Inc/FreeRTOSConfig.h#L32) | `FREERTOS_CONFIG_H` | 空定义 | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:55](../Inc/FreeRTOSConfig.h#L55) | `configENABLE_FPU` | `1` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:56](../Inc/FreeRTOSConfig.h#L56) | `configENABLE_MPU` | `0` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:58](../Inc/FreeRTOSConfig.h#L58) | `configUSE_PREEMPTION` | `1` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:59](../Inc/FreeRTOSConfig.h#L59) | `configSUPPORT_STATIC_ALLOCATION` | `1` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:60](../Inc/FreeRTOSConfig.h#L60) | `configSUPPORT_DYNAMIC_ALLOCATION` | `1` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:61](../Inc/FreeRTOSConfig.h#L61) | `configUSE_IDLE_HOOK` | `0` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:62](../Inc/FreeRTOSConfig.h#L62) | `configUSE_TICK_HOOK` | `0` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:63](../Inc/FreeRTOSConfig.h#L63) | `configCPU_CLOCK_HZ` | `( SystemCoreClock )` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:64](../Inc/FreeRTOSConfig.h#L64) | `configTICK_RATE_HZ` | `((TickType_t)1000)` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:65](../Inc/FreeRTOSConfig.h#L65) | `configMAX_PRIORITIES` | `( 7 )` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:66](../Inc/FreeRTOSConfig.h#L66) | `configMINIMAL_STACK_SIZE` | `((uint16_t)128)` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:67](../Inc/FreeRTOSConfig.h#L67) | `configTOTAL_HEAP_SIZE` | `((size_t)20000)` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:68](../Inc/FreeRTOSConfig.h#L68) | `configMAX_TASK_NAME_LEN` | `( 32 )` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:69](../Inc/FreeRTOSConfig.h#L69) | `configUSE_16_BIT_TICKS` | `0` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:70](../Inc/FreeRTOSConfig.h#L70) | `configUSE_MUTEXES` | `1` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:71](../Inc/FreeRTOSConfig.h#L71) | `configQUEUE_REGISTRY_SIZE` | `8` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:72](../Inc/FreeRTOSConfig.h#L72) | `configUSE_COUNTING_SEMAPHORES` | `1` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:73](../Inc/FreeRTOSConfig.h#L73) | `configUSE_PORT_OPTIMISED_TASK_SELECTION` | `1` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:77](../Inc/FreeRTOSConfig.h#L77) | `configMESSAGE_BUFFER_LENGTH_TYPE` | `size_t` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:81](../Inc/FreeRTOSConfig.h#L81) | `configUSE_CO_ROUTINES` | `0` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:82](../Inc/FreeRTOSConfig.h#L82) | `configMAX_CO_ROUTINE_PRIORITIES` | `( 2 )` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:86](../Inc/FreeRTOSConfig.h#L86) | `INCLUDE_vTaskPrioritySet` | `1` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:87](../Inc/FreeRTOSConfig.h#L87) | `INCLUDE_uxTaskPriorityGet` | `1` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:88](../Inc/FreeRTOSConfig.h#L88) | `INCLUDE_vTaskDelete` | `1` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:89](../Inc/FreeRTOSConfig.h#L89) | `INCLUDE_vTaskCleanUpResources` | `0` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:90](../Inc/FreeRTOSConfig.h#L90) | `INCLUDE_vTaskSuspend` | `1` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:91](../Inc/FreeRTOSConfig.h#L91) | `INCLUDE_vTaskDelayUntil` | `1` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:92](../Inc/FreeRTOSConfig.h#L92) | `INCLUDE_vTaskDelay` | `1` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:93](../Inc/FreeRTOSConfig.h#L93) | `INCLUDE_xTaskGetSchedulerState` | `1` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:94](../Inc/FreeRTOSConfig.h#L94) | `INCLUDE_uxTaskGetStackHighWaterMark` | `1` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:99](../Inc/FreeRTOSConfig.h#L99) | `configPRIO_BITS` | `__NVIC_PRIO_BITS` | `#ifndef FREERTOS_CONFIG_H；#ifdef __NVIC_PRIO_BITS` |  |
| [Inc/FreeRTOSConfig.h:101](../Inc/FreeRTOSConfig.h#L101) | `configPRIO_BITS` | `4` | `#ifndef FREERTOS_CONFIG_H；#ifdef __NVIC_PRIO_BITS → #else` |  |
| [Inc/FreeRTOSConfig.h:106](../Inc/FreeRTOSConfig.h#L106) | `configLIBRARY_LOWEST_INTERRUPT_PRIORITY` | `15` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:112](../Inc/FreeRTOSConfig.h#L112) | `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` | `5` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:116](../Inc/FreeRTOSConfig.h#L116) | `configKERNEL_INTERRUPT_PRIORITY` | `( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:119](../Inc/FreeRTOSConfig.h#L119) | `configMAX_SYSCALL_INTERRUPT_PRIORITY` | `( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:124](../Inc/FreeRTOSConfig.h#L124) | `configASSERT( x )` | `if ((x) == 0) {taskDISABLE_INTERRUPTS(); for( ;; );}` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:129](../Inc/FreeRTOSConfig.h#L129) | `vPortSVCHandler` | `SVC_Handler` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:130](../Inc/FreeRTOSConfig.h#L130) | `xPortPendSVHandler` | `PendSV_Handler` | `#ifndef FREERTOS_CONFIG_H` |  |
| [Inc/FreeRTOSConfig.h:135](../Inc/FreeRTOSConfig.h#L135) | `xPortSysTickHandler` | `SysTick_Handler` | `#ifndef FREERTOS_CONFIG_H` |  |

## Inc/gpio.h

源文件：[打开 Inc/gpio.h](../Inc/gpio.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/gpio.h:22](../Inc/gpio.h#L22) | `__GPIO_H__` | 空定义 | `#ifndef __GPIO_H__` |  |

## Inc/i2c.h

源文件：[打开 Inc/i2c.h](../Inc/i2c.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/i2c.h:22](../Inc/i2c.h#L22) | `__I2C_H__` | 空定义 | `#ifndef __I2C_H__` |  |

## Inc/main.h

源文件：[打开 Inc/main.h](../Inc/main.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/main.h:23](../Inc/main.h#L23) | `__MAIN_H` | 空定义 | `#ifndef __MAIN_H` |  |
| [Inc/main.h:60](../Inc/main.h#L60) | `MAG_RST_Pin` | `GPIO_PIN_6` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:61](../Inc/main.h#L61) | `MAG_RST_GPIO_Port` | `GPIOG` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:62](../Inc/main.h#L62) | `IMU_TEMP_Pin` | `GPIO_PIN_6` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:63](../Inc/main.h#L63) | `IMU_TEMP_GPIO_Port` | `GPIOF` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:64](../Inc/main.h#L64) | `LED_R_Pin` | `GPIO_PIN_12` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:65](../Inc/main.h#L65) | `LED_R_GPIO_Port` | `GPIOH` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:66](../Inc/main.h#L66) | `INT_MAG_Pin` | `GPIO_PIN_3` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:67](../Inc/main.h#L67) | `INT_MAG_GPIO_Port` | `GPIOG` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:68](../Inc/main.h#L68) | `INT_MAG_EXTI_IRQn` | `EXTI3_IRQn` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:69](../Inc/main.h#L69) | `LED_G_Pin` | `GPIO_PIN_11` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:70](../Inc/main.h#L70) | `LED_G_GPIO_Port` | `GPIOH` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:71](../Inc/main.h#L71) | `LED_B_Pin` | `GPIO_PIN_10` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:72](../Inc/main.h#L72) | `LED_B_GPIO_Port` | `GPIOH` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:73](../Inc/main.h#L73) | `BUZZER_Pin` | `GPIO_PIN_14` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:74](../Inc/main.h#L74) | `BUZZER_GPIO_Port` | `GPIOD` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:75](../Inc/main.h#L75) | `KEY_Pin` | `GPIO_PIN_0` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:76](../Inc/main.h#L76) | `KEY_GPIO_Port` | `GPIOA` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:77](../Inc/main.h#L77) | `CS1_ACCEL_Pin` | `GPIO_PIN_4` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:78](../Inc/main.h#L78) | `CS1_ACCEL_GPIO_Port` | `GPIOA` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:79](../Inc/main.h#L79) | `INT_ACC_Pin` | `GPIO_PIN_4` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:80](../Inc/main.h#L80) | `INT_ACC_GPIO_Port` | `GPIOC` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:81](../Inc/main.h#L81) | `INT_ACC_EXTI_IRQn` | `EXTI4_IRQn` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:82](../Inc/main.h#L82) | `INT_GYRO_Pin` | `GPIO_PIN_5` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:83](../Inc/main.h#L83) | `INT_GYRO_GPIO_Port` | `GPIOC` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:84](../Inc/main.h#L84) | `INT_GYRO_EXTI_IRQn` | `EXTI9_5_IRQn` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:85](../Inc/main.h#L85) | `SERVO_Pin` | `GPIO_PIN_9` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:86](../Inc/main.h#L86) | `SERVO_GPIO_Port` | `GPIOE` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:87](../Inc/main.h#L87) | `CS1_GYRO_Pin` | `GPIO_PIN_0` | `#ifndef __MAIN_H` |  |
| [Inc/main.h:88](../Inc/main.h#L88) | `CS1_GYRO_GPIO_Port` | `GPIOB` | `#ifndef __MAIN_H` |  |

## Inc/rng.h

源文件：[打开 Inc/rng.h](../Inc/rng.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/rng.h:22](../Inc/rng.h#L22) | `__RNG_H__` | 空定义 | `#ifndef __RNG_H__` |  |

## Inc/rtc.h

源文件：[打开 Inc/rtc.h](../Inc/rtc.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/rtc.h:22](../Inc/rtc.h#L22) | `__RTC_H__` | 空定义 | `#ifndef __RTC_H__` |  |

## Inc/spi.h

源文件：[打开 Inc/spi.h](../Inc/spi.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/spi.h:22](../Inc/spi.h#L22) | `__SPI_H__` | 空定义 | `#ifndef __SPI_H__` |  |

## Inc/stm32f4xx_hal_conf.h

源文件：[打开 Inc/stm32f4xx_hal_conf.h](../Inc/stm32f4xx_hal_conf.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/stm32f4xx_hal_conf.h:25](../Inc/stm32f4xx_hal_conf.h#L25) | `__STM32F4xx_HAL_CONF_H` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:38](../Inc/stm32f4xx_hal_conf.h#L38) | `HAL_MODULE_ENABLED` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:41](../Inc/stm32f4xx_hal_conf.h#L41) | `HAL_ADC_MODULE_ENABLED` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:42](../Inc/stm32f4xx_hal_conf.h#L42) | `HAL_CAN_MODULE_ENABLED` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:43](../Inc/stm32f4xx_hal_conf.h#L43) | `HAL_CRC_MODULE_ENABLED` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:45](../Inc/stm32f4xx_hal_conf.h#L45) | `HAL_DAC_MODULE_ENABLED` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:55](../Inc/stm32f4xx_hal_conf.h#L55) | `HAL_I2C_MODULE_ENABLED` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:59](../Inc/stm32f4xx_hal_conf.h#L59) | `HAL_RNG_MODULE_ENABLED` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:60](../Inc/stm32f4xx_hal_conf.h#L60) | `HAL_RTC_MODULE_ENABLED` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:64](../Inc/stm32f4xx_hal_conf.h#L64) | `HAL_SPI_MODULE_ENABLED` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:65](../Inc/stm32f4xx_hal_conf.h#L65) | `HAL_TIM_MODULE_ENABLED` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:66](../Inc/stm32f4xx_hal_conf.h#L66) | `HAL_UART_MODULE_ENABLED` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:72](../Inc/stm32f4xx_hal_conf.h#L72) | `HAL_PCD_MODULE_ENABLED` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:83](../Inc/stm32f4xx_hal_conf.h#L83) | `HAL_GPIO_MODULE_ENABLED` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:84](../Inc/stm32f4xx_hal_conf.h#L84) | `HAL_EXTI_MODULE_ENABLED` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:85](../Inc/stm32f4xx_hal_conf.h#L85) | `HAL_DMA_MODULE_ENABLED` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:86](../Inc/stm32f4xx_hal_conf.h#L86) | `HAL_RCC_MODULE_ENABLED` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:87](../Inc/stm32f4xx_hal_conf.h#L87) | `HAL_FLASH_MODULE_ENABLED` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:88](../Inc/stm32f4xx_hal_conf.h#L88) | `HAL_PWR_MODULE_ENABLED` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:89](../Inc/stm32f4xx_hal_conf.h#L89) | `HAL_CORTEX_MODULE_ENABLED` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:98](../Inc/stm32f4xx_hal_conf.h#L98) | `HSE_VALUE` | `12000000U` | `#ifndef __STM32F4xx_HAL_CONF_H；#if !defined (HSE_VALUE)` | !< Value of the External oscillator in Hz |
| [Inc/stm32f4xx_hal_conf.h:102](../Inc/stm32f4xx_hal_conf.h#L102) | `HSE_STARTUP_TIMEOUT` | `100U` | `#ifndef __STM32F4xx_HAL_CONF_H；#if !defined (HSE_STARTUP_TIMEOUT)` | !< Time out for HSE start up, in ms |
| [Inc/stm32f4xx_hal_conf.h:111](../Inc/stm32f4xx_hal_conf.h#L111) | `HSI_VALUE` | `((uint32_t)16000000U)` | `#ifndef __STM32F4xx_HAL_CONF_H；#if !defined (HSI_VALUE)` | !< Value of the Internal oscillator in Hz |
| [Inc/stm32f4xx_hal_conf.h:118](../Inc/stm32f4xx_hal_conf.h#L118) | `LSI_VALUE` | `32000U` | `#ifndef __STM32F4xx_HAL_CONF_H；#if !defined (LSI_VALUE)` | !< LSI Typical Value in Hz |
| [Inc/stm32f4xx_hal_conf.h:126](../Inc/stm32f4xx_hal_conf.h#L126) | `LSE_VALUE` | `32768U` | `#ifndef __STM32F4xx_HAL_CONF_H；#if !defined (LSE_VALUE)` | !< Value of the External Low Speed oscillator in Hz |
| [Inc/stm32f4xx_hal_conf.h:130](../Inc/stm32f4xx_hal_conf.h#L130) | `LSE_STARTUP_TIMEOUT` | `5000U` | `#ifndef __STM32F4xx_HAL_CONF_H；#if !defined (LSE_STARTUP_TIMEOUT)` | !< Time out for LSE start up, in ms |
| [Inc/stm32f4xx_hal_conf.h:139](../Inc/stm32f4xx_hal_conf.h#L139) | `EXTERNAL_CLOCK_VALUE` | `12288000U` | `#ifndef __STM32F4xx_HAL_CONF_H；#if !defined (EXTERNAL_CLOCK_VALUE)` | !< Value of the External audio frequency in Hz |
| [Inc/stm32f4xx_hal_conf.h:149](../Inc/stm32f4xx_hal_conf.h#L149) | `VDD_VALUE` | `3300U` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< Value of VDD in mv |
| [Inc/stm32f4xx_hal_conf.h:150](../Inc/stm32f4xx_hal_conf.h#L150) | `TICK_INT_PRIORITY` | `15U` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< tick interrupt priority |
| [Inc/stm32f4xx_hal_conf.h:151](../Inc/stm32f4xx_hal_conf.h#L151) | `USE_RTOS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:152](../Inc/stm32f4xx_hal_conf.h#L152) | `PREFETCH_ENABLE` | `1U` | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:153](../Inc/stm32f4xx_hal_conf.h#L153) | `INSTRUCTION_CACHE_ENABLE` | `1U` | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:154](../Inc/stm32f4xx_hal_conf.h#L154) | `DATA_CACHE_ENABLE` | `1U` | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:156](../Inc/stm32f4xx_hal_conf.h#L156) | `USE_HAL_ADC_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | ADC register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:157](../Inc/stm32f4xx_hal_conf.h#L157) | `USE_HAL_CAN_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | CAN register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:158](../Inc/stm32f4xx_hal_conf.h#L158) | `USE_HAL_CEC_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | CEC register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:159](../Inc/stm32f4xx_hal_conf.h#L159) | `USE_HAL_CRYP_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | CRYP register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:160](../Inc/stm32f4xx_hal_conf.h#L160) | `USE_HAL_DAC_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | DAC register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:161](../Inc/stm32f4xx_hal_conf.h#L161) | `USE_HAL_DCMI_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | DCMI register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:162](../Inc/stm32f4xx_hal_conf.h#L162) | `USE_HAL_DFSDM_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | DFSDM register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:163](../Inc/stm32f4xx_hal_conf.h#L163) | `USE_HAL_DMA2D_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | DMA2D register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:164](../Inc/stm32f4xx_hal_conf.h#L164) | `USE_HAL_DSI_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | DSI register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:165](../Inc/stm32f4xx_hal_conf.h#L165) | `USE_HAL_ETH_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | ETH register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:166](../Inc/stm32f4xx_hal_conf.h#L166) | `USE_HAL_HASH_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | HASH register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:167](../Inc/stm32f4xx_hal_conf.h#L167) | `USE_HAL_HCD_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | HCD register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:168](../Inc/stm32f4xx_hal_conf.h#L168) | `USE_HAL_I2C_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | I2C register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:169](../Inc/stm32f4xx_hal_conf.h#L169) | `USE_HAL_FMPI2C_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | FMPI2C register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:170](../Inc/stm32f4xx_hal_conf.h#L170) | `USE_HAL_FMPSMBUS_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | FMPSMBUS register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:171](../Inc/stm32f4xx_hal_conf.h#L171) | `USE_HAL_I2S_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | I2S register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:172](../Inc/stm32f4xx_hal_conf.h#L172) | `USE_HAL_IRDA_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | IRDA register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:173](../Inc/stm32f4xx_hal_conf.h#L173) | `USE_HAL_LPTIM_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | LPTIM register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:174](../Inc/stm32f4xx_hal_conf.h#L174) | `USE_HAL_LTDC_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | LTDC register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:175](../Inc/stm32f4xx_hal_conf.h#L175) | `USE_HAL_MMC_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | MMC register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:176](../Inc/stm32f4xx_hal_conf.h#L176) | `USE_HAL_NAND_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | NAND register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:177](../Inc/stm32f4xx_hal_conf.h#L177) | `USE_HAL_NOR_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | NOR register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:178](../Inc/stm32f4xx_hal_conf.h#L178) | `USE_HAL_PCCARD_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | PCCARD register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:179](../Inc/stm32f4xx_hal_conf.h#L179) | `USE_HAL_PCD_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | PCD register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:180](../Inc/stm32f4xx_hal_conf.h#L180) | `USE_HAL_QSPI_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | QSPI register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:181](../Inc/stm32f4xx_hal_conf.h#L181) | `USE_HAL_RNG_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | RNG register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:182](../Inc/stm32f4xx_hal_conf.h#L182) | `USE_HAL_RTC_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | RTC register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:183](../Inc/stm32f4xx_hal_conf.h#L183) | `USE_HAL_SAI_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | SAI register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:184](../Inc/stm32f4xx_hal_conf.h#L184) | `USE_HAL_SD_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | SD register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:185](../Inc/stm32f4xx_hal_conf.h#L185) | `USE_HAL_SMARTCARD_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | SMARTCARD register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:186](../Inc/stm32f4xx_hal_conf.h#L186) | `USE_HAL_SDRAM_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | SDRAM register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:187](../Inc/stm32f4xx_hal_conf.h#L187) | `USE_HAL_SRAM_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | SRAM register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:188](../Inc/stm32f4xx_hal_conf.h#L188) | `USE_HAL_SPDIFRX_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | SPDIFRX register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:189](../Inc/stm32f4xx_hal_conf.h#L189) | `USE_HAL_SMBUS_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | SMBUS register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:190](../Inc/stm32f4xx_hal_conf.h#L190) | `USE_HAL_SPI_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | SPI register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:191](../Inc/stm32f4xx_hal_conf.h#L191) | `USE_HAL_TIM_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | TIM register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:192](../Inc/stm32f4xx_hal_conf.h#L192) | `USE_HAL_UART_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | UART register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:193](../Inc/stm32f4xx_hal_conf.h#L193) | `USE_HAL_USART_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | USART register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:194](../Inc/stm32f4xx_hal_conf.h#L194) | `USE_HAL_WWDG_REGISTER_CALLBACKS` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` | WWDG register callback disabled |
| [Inc/stm32f4xx_hal_conf.h:208](../Inc/stm32f4xx_hal_conf.h#L208) | `MAC_ADDR0` | `2U` | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:209](../Inc/stm32f4xx_hal_conf.h#L209) | `MAC_ADDR1` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:210](../Inc/stm32f4xx_hal_conf.h#L210) | `MAC_ADDR2` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:211](../Inc/stm32f4xx_hal_conf.h#L211) | `MAC_ADDR3` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:212](../Inc/stm32f4xx_hal_conf.h#L212) | `MAC_ADDR4` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:213](../Inc/stm32f4xx_hal_conf.h#L213) | `MAC_ADDR5` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:216](../Inc/stm32f4xx_hal_conf.h#L216) | `ETH_RX_BUF_SIZE` | 空定义 | `#ifndef __STM32F4xx_HAL_CONF_H` | buffer size for receive |
| [Inc/stm32f4xx_hal_conf.h:217](../Inc/stm32f4xx_hal_conf.h#L217) | `ETH_TX_BUF_SIZE` | `ETH_MAX_PACKET_SIZE` | `#ifndef __STM32F4xx_HAL_CONF_H` | buffer size for transmit |
| [Inc/stm32f4xx_hal_conf.h:218](../Inc/stm32f4xx_hal_conf.h#L218) | `ETH_RXBUFNB` | `4U` | `#ifndef __STM32F4xx_HAL_CONF_H` | 4 Rx buffers of size ETH_RX_BUF_SIZE |
| [Inc/stm32f4xx_hal_conf.h:219](../Inc/stm32f4xx_hal_conf.h#L219) | `ETH_TXBUFNB` | `4U` | `#ifndef __STM32F4xx_HAL_CONF_H` | 4 Tx buffers of size ETH_TX_BUF_SIZE |
| [Inc/stm32f4xx_hal_conf.h:224](../Inc/stm32f4xx_hal_conf.h#L224) | `DP83848_PHY_ADDRESS` | `0x01U` | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:226](../Inc/stm32f4xx_hal_conf.h#L226) | `PHY_RESET_DELAY` | `0x000000FFU` | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:228](../Inc/stm32f4xx_hal_conf.h#L228) | `PHY_CONFIG_DELAY` | `0x00000FFFU` | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:230](../Inc/stm32f4xx_hal_conf.h#L230) | `PHY_READ_TO` | `0x0000FFFFU` | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:231](../Inc/stm32f4xx_hal_conf.h#L231) | `PHY_WRITE_TO` | `0x0000FFFFU` | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:235](../Inc/stm32f4xx_hal_conf.h#L235) | `PHY_BCR` | `((uint16_t)0x0000U)` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< Transceiver Basic Control Register |
| [Inc/stm32f4xx_hal_conf.h:236](../Inc/stm32f4xx_hal_conf.h#L236) | `PHY_BSR` | `((uint16_t)0x0001U)` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< Transceiver Basic Status Register |
| [Inc/stm32f4xx_hal_conf.h:238](../Inc/stm32f4xx_hal_conf.h#L238) | `PHY_RESET` | `((uint16_t)0x8000U)` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< PHY Reset |
| [Inc/stm32f4xx_hal_conf.h:239](../Inc/stm32f4xx_hal_conf.h#L239) | `PHY_LOOPBACK` | `((uint16_t)0x4000U)` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< Select loop-back mode |
| [Inc/stm32f4xx_hal_conf.h:240](../Inc/stm32f4xx_hal_conf.h#L240) | `PHY_FULLDUPLEX_100M` | `((uint16_t)0x2100U)` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< Set the full-duplex mode at 100 Mb/s |
| [Inc/stm32f4xx_hal_conf.h:241](../Inc/stm32f4xx_hal_conf.h#L241) | `PHY_HALFDUPLEX_100M` | `((uint16_t)0x2000U)` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< Set the half-duplex mode at 100 Mb/s |
| [Inc/stm32f4xx_hal_conf.h:242](../Inc/stm32f4xx_hal_conf.h#L242) | `PHY_FULLDUPLEX_10M` | `((uint16_t)0x0100U)` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< Set the full-duplex mode at 10 Mb/s |
| [Inc/stm32f4xx_hal_conf.h:243](../Inc/stm32f4xx_hal_conf.h#L243) | `PHY_HALFDUPLEX_10M` | `((uint16_t)0x0000U)` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< Set the half-duplex mode at 10 Mb/s |
| [Inc/stm32f4xx_hal_conf.h:244](../Inc/stm32f4xx_hal_conf.h#L244) | `PHY_AUTONEGOTIATION` | `((uint16_t)0x1000U)` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< Enable auto-negotiation function |
| [Inc/stm32f4xx_hal_conf.h:245](../Inc/stm32f4xx_hal_conf.h#L245) | `PHY_RESTART_AUTONEGOTIATION` | `((uint16_t)0x0200U)` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< Restart auto-negotiation function |
| [Inc/stm32f4xx_hal_conf.h:246](../Inc/stm32f4xx_hal_conf.h#L246) | `PHY_POWERDOWN` | `((uint16_t)0x0800U)` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< Select the power down mode |
| [Inc/stm32f4xx_hal_conf.h:247](../Inc/stm32f4xx_hal_conf.h#L247) | `PHY_ISOLATE` | `((uint16_t)0x0400U)` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< Isolate PHY from MII |
| [Inc/stm32f4xx_hal_conf.h:249](../Inc/stm32f4xx_hal_conf.h#L249) | `PHY_AUTONEGO_COMPLETE` | `((uint16_t)0x0020U)` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< Auto-Negotiation process completed |
| [Inc/stm32f4xx_hal_conf.h:250](../Inc/stm32f4xx_hal_conf.h#L250) | `PHY_LINKED_STATUS` | `((uint16_t)0x0004U)` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< Valid link established |
| [Inc/stm32f4xx_hal_conf.h:251](../Inc/stm32f4xx_hal_conf.h#L251) | `PHY_JABBER_DETECTION` | `((uint16_t)0x0002U)` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< Jabber condition detected |
| [Inc/stm32f4xx_hal_conf.h:254](../Inc/stm32f4xx_hal_conf.h#L254) | `PHY_SR` | `((uint16_t)0x10U)` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< PHY status register Offset |
| [Inc/stm32f4xx_hal_conf.h:256](../Inc/stm32f4xx_hal_conf.h#L256) | `PHY_SPEED_STATUS` | `((uint16_t)0x0002U)` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< PHY Speed mask |
| [Inc/stm32f4xx_hal_conf.h:257](../Inc/stm32f4xx_hal_conf.h#L257) | `PHY_DUPLEX_STATUS` | `((uint16_t)0x0004U)` | `#ifndef __STM32F4xx_HAL_CONF_H` | !< PHY Duplex mask |
| [Inc/stm32f4xx_hal_conf.h:266](../Inc/stm32f4xx_hal_conf.h#L266) | `USE_SPI_CRC` | `0U` | `#ifndef __STM32F4xx_HAL_CONF_H` |  |
| [Inc/stm32f4xx_hal_conf.h:479](../Inc/stm32f4xx_hal_conf.h#L479) | `assert_param(expr)` | `((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))` | `#ifndef __STM32F4xx_HAL_CONF_H；#ifdef USE_FULL_ASSERT` |  |
| [Inc/stm32f4xx_hal_conf.h:483](../Inc/stm32f4xx_hal_conf.h#L483) | `assert_param(expr)` | `((void)0U)` | `#ifndef __STM32F4xx_HAL_CONF_H；#ifdef USE_FULL_ASSERT → #else` |  |

### 已注释的宏选项（当前不定义）

| 行 | 原选项 |
| --- | --- |
| [Inc/stm32f4xx_hal_conf.h:40](../Inc/stm32f4xx_hal_conf.h#L40) | `#define HAL_CRYP_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:44](../Inc/stm32f4xx_hal_conf.h#L44) | `#define HAL_CAN_LEGACY_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:46](../Inc/stm32f4xx_hal_conf.h#L46) | `#define HAL_DCMI_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:47](../Inc/stm32f4xx_hal_conf.h#L47) | `#define HAL_DMA2D_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:48](../Inc/stm32f4xx_hal_conf.h#L48) | `#define HAL_ETH_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:49](../Inc/stm32f4xx_hal_conf.h#L49) | `#define HAL_NAND_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:50](../Inc/stm32f4xx_hal_conf.h#L50) | `#define HAL_NOR_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:51](../Inc/stm32f4xx_hal_conf.h#L51) | `#define HAL_PCCARD_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:52](../Inc/stm32f4xx_hal_conf.h#L52) | `#define HAL_SRAM_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:53](../Inc/stm32f4xx_hal_conf.h#L53) | `#define HAL_SDRAM_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:54](../Inc/stm32f4xx_hal_conf.h#L54) | `#define HAL_HASH_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:56](../Inc/stm32f4xx_hal_conf.h#L56) | `#define HAL_I2S_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:57](../Inc/stm32f4xx_hal_conf.h#L57) | `#define HAL_IWDG_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:58](../Inc/stm32f4xx_hal_conf.h#L58) | `#define HAL_LTDC_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:61](../Inc/stm32f4xx_hal_conf.h#L61) | `#define HAL_SAI_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:62](../Inc/stm32f4xx_hal_conf.h#L62) | `#define HAL_SD_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:63](../Inc/stm32f4xx_hal_conf.h#L63) | `#define HAL_MMC_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:67](../Inc/stm32f4xx_hal_conf.h#L67) | `#define HAL_USART_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:68](../Inc/stm32f4xx_hal_conf.h#L68) | `#define HAL_IRDA_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:69](../Inc/stm32f4xx_hal_conf.h#L69) | `#define HAL_SMARTCARD_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:70](../Inc/stm32f4xx_hal_conf.h#L70) | `#define HAL_SMBUS_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:71](../Inc/stm32f4xx_hal_conf.h#L71) | `#define HAL_WWDG_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:73](../Inc/stm32f4xx_hal_conf.h#L73) | `#define HAL_HCD_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:74](../Inc/stm32f4xx_hal_conf.h#L74) | `#define HAL_DSI_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:75](../Inc/stm32f4xx_hal_conf.h#L75) | `#define HAL_QSPI_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:76](../Inc/stm32f4xx_hal_conf.h#L76) | `#define HAL_QSPI_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:77](../Inc/stm32f4xx_hal_conf.h#L77) | `#define HAL_CEC_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:78](../Inc/stm32f4xx_hal_conf.h#L78) | `#define HAL_FMPI2C_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:79](../Inc/stm32f4xx_hal_conf.h#L79) | `#define HAL_FMPSMBUS_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:80](../Inc/stm32f4xx_hal_conf.h#L80) | `#define HAL_SPDIFRX_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:81](../Inc/stm32f4xx_hal_conf.h#L81) | `#define HAL_DFSDM_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:82](../Inc/stm32f4xx_hal_conf.h#L82) | `#define HAL_LPTIM_MODULE_ENABLED` |
| [Inc/stm32f4xx_hal_conf.h:201](../Inc/stm32f4xx_hal_conf.h#L201) | `#define USE_FULL_ASSERT 1U` |

## Inc/stm32f4xx_it.h

源文件：[打开 Inc/stm32f4xx_it.h](../Inc/stm32f4xx_it.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/stm32f4xx_it.h:22](../Inc/stm32f4xx_it.h#L22) | `__STM32F4xx_IT_H` | 空定义 | `#ifndef __STM32F4xx_IT_H` |  |

## Inc/tim.h

源文件：[打开 Inc/tim.h](../Inc/tim.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/tim.h:22](../Inc/tim.h#L22) | `__TIM_H__` | 空定义 | `#ifndef __TIM_H__` |  |

## Inc/usart.h

源文件：[打开 Inc/usart.h](../Inc/usart.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/usart.h:22](../Inc/usart.h#L22) | `__USART_H__` | 空定义 | `#ifndef __USART_H__` |  |

## Inc/usb_device.h

源文件：[打开 Inc/usb_device.h](../Inc/usb_device.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/usb_device.h:23](../Inc/usb_device.h#L23) | `__USB_DEVICE__H__` | 空定义 | `#ifndef __USB_DEVICE__H__` |  |

## Inc/usbd_cdc_if.h

源文件：[打开 Inc/usbd_cdc_if.h](../Inc/usbd_cdc_if.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/usbd_cdc_if.h:23](../Inc/usbd_cdc_if.h#L23) | `__USBD_CDC_IF_H__` | 空定义 | `#ifndef __USBD_CDC_IF_H__` |  |
| [Inc/usbd_cdc_if.h:51](../Inc/usbd_cdc_if.h#L51) | `APP_RX_DATA_SIZE` | `2048` | `#ifndef __USBD_CDC_IF_H__` |  |
| [Inc/usbd_cdc_if.h:52](../Inc/usbd_cdc_if.h#L52) | `APP_TX_DATA_SIZE` | `2048` | `#ifndef __USBD_CDC_IF_H__` |  |

## Inc/usbd_conf.h

源文件：[打开 Inc/usbd_conf.h](../Inc/usbd_conf.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/usbd_conf.h:23](../Inc/usbd_conf.h#L23) | `__USBD_CONF__H__` | 空定义 | `#ifndef __USBD_CONF__H__` |  |
| [Inc/usbd_conf.h:66](../Inc/usbd_conf.h#L66) | `USBD_MAX_NUM_INTERFACES` | `1U` | `#ifndef __USBD_CONF__H__` |  |
| [Inc/usbd_conf.h:68](../Inc/usbd_conf.h#L68) | `USBD_MAX_NUM_CONFIGURATION` | `1U` | `#ifndef __USBD_CONF__H__` |  |
| [Inc/usbd_conf.h:70](../Inc/usbd_conf.h#L70) | `USBD_MAX_STR_DESC_SIZ` | `512U` | `#ifndef __USBD_CONF__H__` |  |
| [Inc/usbd_conf.h:72](../Inc/usbd_conf.h#L72) | `USBD_DEBUG_LEVEL` | `0U` | `#ifndef __USBD_CONF__H__` |  |
| [Inc/usbd_conf.h:74](../Inc/usbd_conf.h#L74) | `USBD_LPM_ENABLED` | `0U` | `#ifndef __USBD_CONF__H__` |  |
| [Inc/usbd_conf.h:76](../Inc/usbd_conf.h#L76) | `USBD_SELF_POWERED` | `1U` | `#ifndef __USBD_CONF__H__` |  |
| [Inc/usbd_conf.h:80](../Inc/usbd_conf.h#L80) | `DEVICE_FS` | `0` | `#ifndef __USBD_CONF__H__` |  |
| [Inc/usbd_conf.h:81](../Inc/usbd_conf.h#L81) | `DEVICE_HS` | `1` | `#ifndef __USBD_CONF__H__` |  |
| [Inc/usbd_conf.h:94](../Inc/usbd_conf.h#L94) | `USBD_malloc` | `(void *)USBD_static_malloc` | `#ifndef __USBD_CONF__H__` |  |
| [Inc/usbd_conf.h:97](../Inc/usbd_conf.h#L97) | `USBD_free` | `USBD_static_free` | `#ifndef __USBD_CONF__H__` |  |
| [Inc/usbd_conf.h:100](../Inc/usbd_conf.h#L100) | `USBD_memset` | `memset` | `#ifndef __USBD_CONF__H__` |  |
| [Inc/usbd_conf.h:103](../Inc/usbd_conf.h#L103) | `USBD_memcpy` | `memcpy` | `#ifndef __USBD_CONF__H__` |  |
| [Inc/usbd_conf.h:106](../Inc/usbd_conf.h#L106) | `USBD_Delay` | `HAL_Delay` | `#ifndef __USBD_CONF__H__` |  |
| [Inc/usbd_conf.h:111](../Inc/usbd_conf.h#L111) | `USBD_UsrLog(...)` | `printf(__VA_ARGS__); printf("\n");` | `#ifndef __USBD_CONF__H__；#if (USBD_DEBUG_LEVEL > 0)` |  |
| [Inc/usbd_conf.h:114](../Inc/usbd_conf.h#L114) | `USBD_UsrLog(...)` | 空定义 | `#ifndef __USBD_CONF__H__；#if (USBD_DEBUG_LEVEL > 0) → #else` |  |
| [Inc/usbd_conf.h:119](../Inc/usbd_conf.h#L119) | `USBD_ErrLog(...)` | `printf("ERROR: "); printf(__VA_ARGS__); printf("\n");` | `#ifndef __USBD_CONF__H__；#if (USBD_DEBUG_LEVEL > 1)` |  |
| [Inc/usbd_conf.h:123](../Inc/usbd_conf.h#L123) | `USBD_ErrLog(...)` | 空定义 | `#ifndef __USBD_CONF__H__；#if (USBD_DEBUG_LEVEL > 1) → #else` |  |
| [Inc/usbd_conf.h:127](../Inc/usbd_conf.h#L127) | `USBD_DbgLog(...)` | `printf("DEBUG : "); printf(__VA_ARGS__); printf("\n");` | `#ifndef __USBD_CONF__H__；#if (USBD_DEBUG_LEVEL > 2)` |  |
| [Inc/usbd_conf.h:131](../Inc/usbd_conf.h#L131) | `USBD_DbgLog(...)` | 空定义 | `#ifndef __USBD_CONF__H__；#if (USBD_DEBUG_LEVEL > 2) → #else` |  |

### 已注释的宏选项（当前不定义）

| 行 | 原选项 |
| --- | --- |
| [Inc/usbd_conf.h:79](../Inc/usbd_conf.h#L79) | `#define for FS and HS identification` |

## Inc/usbd_desc.h

源文件：[打开 Inc/usbd_desc.h](../Inc/usbd_desc.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Inc/usbd_desc.h:22](../Inc/usbd_desc.h#L22) | `__USBD_DESC__C__` | 空定义 | `#ifndef __USBD_DESC__C__` |  |
| [Inc/usbd_desc.h:48](../Inc/usbd_desc.h#L48) | `DEVICE_ID1` | `(UID_BASE)` | `#ifndef __USBD_DESC__C__` |  |
| [Inc/usbd_desc.h:49](../Inc/usbd_desc.h#L49) | `DEVICE_ID2` | `(UID_BASE + 0x4)` | `#ifndef __USBD_DESC__C__` |  |
| [Inc/usbd_desc.h:50](../Inc/usbd_desc.h#L50) | `DEVICE_ID3` | `(UID_BASE + 0x8)` | `#ifndef __USBD_DESC__C__` |  |
| [Inc/usbd_desc.h:52](../Inc/usbd_desc.h#L52) | `USB_SIZ_STRING_SERIAL` | `0x1A` | `#ifndef __USBD_DESC__C__` |  |

## Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h

源文件：[打开 Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:59](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L59) | `SEGGER_RTT_CONF_H` | 空定义 | `#ifndef SEGGER_RTT_CONF_H` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:83](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L83) | `SEGGER_RTT_MAX_NUM_UP_BUFFERS` | `(3)` | `#ifndef SEGGER_RTT_CONF_H；#ifndef SEGGER_RTT_MAX_NUM_UP_BUFFERS` | Max. number of up-buffers (T->H) available on this target    (Default: 3) |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:91](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L91) | `SEGGER_RTT_MAX_NUM_DOWN_BUFFERS` | `(3)` | `#ifndef SEGGER_RTT_CONF_H；#ifndef SEGGER_RTT_MAX_NUM_DOWN_BUFFERS` | Max. number of down-buffers (H->T) available on this target  (Default: 3) |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:95](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L95) | `BUFFER_SIZE_UP` | `(1024)` | `#ifndef SEGGER_RTT_CONF_H；#ifndef BUFFER_SIZE_UP` | Size of the buffer for terminal output of target, up to host (Default: 1k) |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:99](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L99) | `BUFFER_SIZE_DOWN` | `(16)` | `#ifndef SEGGER_RTT_CONF_H；#ifndef BUFFER_SIZE_DOWN` | Size of the buffer for terminal input to target from host (Usually keyboard input) (Default: 16) |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:103](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L103) | `SEGGER_RTT_PRINTF_BUFFER_SIZE` | `(64u)` | `#ifndef SEGGER_RTT_CONF_H；#ifndef SEGGER_RTT_PRINTF_BUFFER_SIZE` | Size of buffer for RTT printf to bulk-send chars via RTT     (Default: 64) |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:107](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L107) | `SEGGER_RTT_MODE_DEFAULT` | `SEGGER_RTT_MODE_NO_BLOCK_SKIP` | `#ifndef SEGGER_RTT_CONF_H；#ifndef SEGGER_RTT_MODE_DEFAULT` | Mode for pre-initialized terminal channel (buffer 0) |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:123](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L123) | `SEGGER_RTT_MEMCPY_USE_BYTELOOP` | `0` | `#ifndef SEGGER_RTT_CONF_H；#ifndef SEGGER_RTT_MEMCPY_USE_BYTELOOP` | 0: Use memcpy/SEGGER_RTT_MEMCPY, 1: Use a simple byte-loop |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:147](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L147) | `SEGGER_RTT_MAX_INTERRUPT_PRIORITY` | `(0x20)` | `#ifndef SEGGER_RTT_CONF_H；#ifndef SEGGER_RTT_MAX_INTERRUPT_PRIORITY` | Interrupt priority to lock on SEGGER_RTT_LOCK on Cortex-M3/4 (Default: 0x20) |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:157](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L157) | `SEGGER_RTT_LOCK()` | `{ unsigned int _SEGGER_RTT__LockState; __asm volatile ("mrs %0, primask \n\t" "movs r1, #1 \n\t" "msr primask, r1 \n\t" : "=r" (_SEGGER_RTT__LockState) : : "r1", "cc" );` | `#ifndef SEGGER_RTT_CONF_H；#if ((defined(__SES_ARM) \|\| defined(__SES_RISCV) \|\| defined(__CROSSWORKS_ARM) \|\| defined(__GNUC__) \|\| defined(__clang__)) && !defined (__CC_ARM) && !defined(WIN32))；#if (defined(__ARM_ARCH_6M__) \|\| defined(__ARM_ARCH_8M_BASE__))` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:167](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L167) | `SEGGER_RTT_UNLOCK()` | `__asm volatile ("msr primask, %0 \n\t" : : "r" (_SEGGER_RTT__LockState) : ); }` | `#ifndef SEGGER_RTT_CONF_H；#if ((defined(__SES_ARM) \|\| defined(__SES_RISCV) \|\| defined(__CROSSWORKS_ARM) \|\| defined(__GNUC__) \|\| defined(__clang__)) && !defined (__CC_ARM) && !defined(WIN32))；#if (defined(__ARM_ARCH_6M__) \|\| defined(__ARM_ARCH_8M_BASE__))` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:175](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L175) | `SEGGER_RTT_MAX_INTERRUPT_PRIORITY` | `(0x20)` | `#ifndef SEGGER_RTT_CONF_H；#if ((defined(__SES_ARM) \|\| defined(__SES_RISCV) \|\| defined(__CROSSWORKS_ARM) \|\| defined(__GNUC__) \|\| defined(__clang__)) && !defined (__CC_ARM) && !defined(WIN32))；#if (defined(__ARM_ARCH_6M__) \|\| defined(__ARM_ARCH_8M_BASE__)) → #elif (defined(__ARM_ARCH_7M__) \|\| defined(__ARM_ARCH_7EM__) \|\| defined(__ARM_ARCH_8M_MAIN__))；#ifndef SEGGER_RTT_MAX_INTERRUPT_PRIORITY` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:177](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L177) | `SEGGER_RTT_LOCK()` | `{ unsigned int _SEGGER_RTT__LockState; __asm volatile ("mrs %0, basepri \n\t" "mov r1, %1 \n\t" "msr basepri, r1 \n\t" : "=r" (_SEGGER_RTT__LockState) : "i"(SEGGER_RTT_MAX_INTERRUPT_PRIORITY) : "r1", "cc" );` | `#ifndef SEGGER_RTT_CONF_H；#if ((defined(__SES_ARM) \|\| defined(__SES_RISCV) \|\| defined(__CROSSWORKS_ARM) \|\| defined(__GNUC__) \|\| defined(__clang__)) && !defined (__CC_ARM) && !defined(WIN32))；#if (defined(__ARM_ARCH_6M__) \|\| defined(__ARM_ARCH_8M_BASE__)) → #elif (defined(__ARM_ARCH_7M__) \|\| defined(__ARM_ARCH_7EM__) \|\| defined(__ARM_ARCH_8M_MAIN__))` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:187](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L187) | `SEGGER_RTT_UNLOCK()` | `__asm volatile ("msr basepri, %0 \n\t" : : "r" (_SEGGER_RTT__LockState) : ); }` | `#ifndef SEGGER_RTT_CONF_H；#if ((defined(__SES_ARM) \|\| defined(__SES_RISCV) \|\| defined(__CROSSWORKS_ARM) \|\| defined(__GNUC__) \|\| defined(__clang__)) && !defined (__CC_ARM) && !defined(WIN32))；#if (defined(__ARM_ARCH_6M__) \|\| defined(__ARM_ARCH_8M_BASE__)) → #elif (defined(__ARM_ARCH_7M__) \|\| defined(__ARM_ARCH_7EM__) \|\| defined(__ARM_ARCH_8M_MAIN__))` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:195](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L195) | `SEGGER_RTT_LOCK()` | `{ unsigned int _SEGGER_RTT__LockState; __asm volatile ("mrs r1, CPSR \n\t" "mov %0, r1 \n\t" "orr r1, r1, #0xC0 \n\t" "msr CPSR_c, r1 \n\t" : "=r" (_SEGGER_RTT__LockState) : : "r1", "cc" );` | `#ifndef SEGGER_RTT_CONF_H；#if ((defined(__SES_ARM) \|\| defined(__SES_RISCV) \|\| defined(__CROSSWORKS_ARM) \|\| defined(__GNUC__) \|\| defined(__clang__)) && !defined (__CC_ARM) && !defined(WIN32))；#if (defined(__ARM_ARCH_6M__) \|\| defined(__ARM_ARCH_8M_BASE__)) → #elif (defined(__ARM_ARCH_7M__) \|\| defined(__ARM_ARCH_7EM__) \|\| defined(__ARM_ARCH_8M_MAIN__)) → #elif (defined(__ARM_ARCH_7A__) \|\| defined(__ARM_ARCH_7R__))` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:206](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L206) | `SEGGER_RTT_UNLOCK()` | `__asm volatile ("mov r0, %0 \n\t" "mrs r1, CPSR \n\t" "bic r1, r1, #0xC0 \n\t" "and r0, r0, #0xC0 \n\t" "orr r1, r1, r0 \n\t" "msr CPSR_c, r1 \n\t" : : "r" (_SEGGER_RTT__LockState) : "r0", "r1", "cc" ); }` | `#ifndef SEGGER_RTT_CONF_H；#if ((defined(__SES_ARM) \|\| defined(__SES_RISCV) \|\| defined(__CROSSWORKS_ARM) \|\| defined(__GNUC__) \|\| defined(__clang__)) && !defined (__CC_ARM) && !defined(WIN32))；#if (defined(__ARM_ARCH_6M__) \|\| defined(__ARM_ARCH_8M_BASE__)) → #elif (defined(__ARM_ARCH_7M__) \|\| defined(__ARM_ARCH_7EM__) \|\| defined(__ARM_ARCH_8M_MAIN__)) → #elif (defined(__ARM_ARCH_7A__) \|\| defined(__ARM_ARCH_7R__))` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:218](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L218) | `SEGGER_RTT_LOCK()` | `{ unsigned int _SEGGER_RTT__LockState; __asm volatile ("csrr %0, mstatus \n\t" "csrci mstatus, 8 \n\t" "andi %0, %0, 8 \n\t" : "=r" (_SEGGER_RTT__LockState) : : );` | `#ifndef SEGGER_RTT_CONF_H；#if ((defined(__SES_ARM) \|\| defined(__SES_RISCV) \|\| defined(__CROSSWORKS_ARM) \|\| defined(__GNUC__) \|\| defined(__clang__)) && !defined (__CC_ARM) && !defined(WIN32))；#if (defined(__ARM_ARCH_6M__) \|\| defined(__ARM_ARCH_8M_BASE__)) → #elif (defined(__ARM_ARCH_7M__) \|\| defined(__ARM_ARCH_7EM__) \|\| defined(__ARM_ARCH_8M_MAIN__)) → #elif (defined(__ARM_ARCH_7A__) \|\| defined(__ARM_ARCH_7R__)) → #elif defined(__riscv) \|\| defined(__riscv_xlen)` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:228](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L228) | `SEGGER_RTT_UNLOCK()` | `__asm volatile ("csrr a1, mstatus \n\t" "or %0, %0, a1 \n\t" "csrs mstatus, %0 \n\t" : : "r" (_SEGGER_RTT__LockState) : "a1" ); }` | `#ifndef SEGGER_RTT_CONF_H；#if ((defined(__SES_ARM) \|\| defined(__SES_RISCV) \|\| defined(__CROSSWORKS_ARM) \|\| defined(__GNUC__) \|\| defined(__clang__)) && !defined (__CC_ARM) && !defined(WIN32))；#if (defined(__ARM_ARCH_6M__) \|\| defined(__ARM_ARCH_8M_BASE__)) → #elif (defined(__ARM_ARCH_7M__) \|\| defined(__ARM_ARCH_7EM__) \|\| defined(__ARM_ARCH_8M_MAIN__)) → #elif (defined(__ARM_ARCH_7A__) \|\| defined(__ARM_ARCH_7R__)) → #elif defined(__riscv) \|\| defined(__riscv_xlen)` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:237](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L237) | `SEGGER_RTT_LOCK()` | 空定义 | `#ifndef SEGGER_RTT_CONF_H；#if ((defined(__SES_ARM) \|\| defined(__SES_RISCV) \|\| defined(__CROSSWORKS_ARM) \|\| defined(__GNUC__) \|\| defined(__clang__)) && !defined (__CC_ARM) && !defined(WIN32))；#if (defined(__ARM_ARCH_6M__) \|\| defined(__ARM_ARCH_8M_BASE__)) → #elif (defined(__ARM_ARCH_7M__) \|\| defined(__ARM_ARCH_7EM__) \|\| defined(__ARM_ARCH_8M_MAIN__)) → #elif (defined(__ARM_ARCH_7A__) \|\| defined(__ARM_ARCH_7R__)) → #elif defined(__riscv) \|\| defined(__riscv_xlen) → #else` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:238](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L238) | `SEGGER_RTT_UNLOCK()` | 空定义 | `#ifndef SEGGER_RTT_CONF_H；#if ((defined(__SES_ARM) \|\| defined(__SES_RISCV) \|\| defined(__CROSSWORKS_ARM) \|\| defined(__GNUC__) \|\| defined(__clang__)) && !defined (__CC_ARM) && !defined(WIN32))；#if (defined(__ARM_ARCH_6M__) \|\| defined(__ARM_ARCH_8M_BASE__)) → #elif (defined(__ARM_ARCH_7M__) \|\| defined(__ARM_ARCH_7EM__) \|\| defined(__ARM_ARCH_8M_MAIN__)) → #elif (defined(__ARM_ARCH_7A__) \|\| defined(__ARM_ARCH_7R__)) → #elif defined(__riscv) \|\| defined(__riscv_xlen) → #else` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:249](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L249) | `SEGGER_RTT_LOCK()` | `{ unsigned int _SEGGER_RTT__LockState; _SEGGER_RTT__LockState = __get_PRIMASK(); __set_PRIMASK(1);` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __ICCARM__；#if (defined (__ARM6M__) && (__CORE__ == __ARM6M__)) \|\| (defined (__ARM8M_BASELINE__) && (__CORE__ == __ARM8M_BASELINE__))` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:254](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L254) | `SEGGER_RTT_UNLOCK()` | `__set_PRIMASK(_SEGGER_RTT__LockState); }` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __ICCARM__；#if (defined (__ARM6M__) && (__CORE__ == __ARM6M__)) \|\| (defined (__ARM8M_BASELINE__) && (__CORE__ == __ARM8M_BASELINE__))` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:261](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L261) | `SEGGER_RTT_MAX_INTERRUPT_PRIORITY` | `(0x20)` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __ICCARM__；#if (defined (__ARM6M__) && (__CORE__ == __ARM6M__)) \|\| (defined (__ARM8M_BASELINE__) && (__CORE__ == __ARM8M_BASELINE__)) → #elif (defined (__ARM7EM__) && (__CORE__ == __ARM7EM__)) \|\| (defined (__ARM7M__) && (__CORE__ == __ARM7M__)) \|\| (defined (__ARM8M_MAINLINE__) && (__CORE__ == __ARM8M_MAINLINE__)) \|\| (defined (__ARM8M_MAINLINE__) && (__CORE__ == __ARM8M_MAINLINE__))；#ifndef SEGGER_RTT_MAX_INTERRUPT_PRIORITY` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:263](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L263) | `SEGGER_RTT_LOCK()` | `{ unsigned int _SEGGER_RTT__LockState; _SEGGER_RTT__LockState = __get_BASEPRI(); __set_BASEPRI(SEGGER_RTT_MAX_INTERRUPT_PRIORITY);` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __ICCARM__；#if (defined (__ARM6M__) && (__CORE__ == __ARM6M__)) \|\| (defined (__ARM8M_BASELINE__) && (__CORE__ == __ARM8M_BASELINE__)) → #elif (defined (__ARM7EM__) && (__CORE__ == __ARM7EM__)) \|\| (defined (__ARM7M__) && (__CORE__ == __ARM7M__)) \|\| (defined (__ARM8M_MAINLINE__) && (__CORE__ == __ARM8M_MAINLINE__)) \|\| (defined (__ARM8M_MAINLINE__) && (__CORE__ == __ARM8M_MAINLINE__))` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:268](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L268) | `SEGGER_RTT_UNLOCK()` | `__set_BASEPRI(_SEGGER_RTT__LockState); }` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __ICCARM__；#if (defined (__ARM6M__) && (__CORE__ == __ARM6M__)) \|\| (defined (__ARM8M_BASELINE__) && (__CORE__ == __ARM8M_BASELINE__)) → #elif (defined (__ARM7EM__) && (__CORE__ == __ARM7EM__)) \|\| (defined (__ARM7M__) && (__CORE__ == __ARM7M__)) \|\| (defined (__ARM8M_MAINLINE__) && (__CORE__ == __ARM8M_MAINLINE__)) \|\| (defined (__ARM8M_MAINLINE__) && (__CORE__ == __ARM8M_MAINLINE__))` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:272](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L272) | `SEGGER_RTT_LOCK()` | `{ unsigned int _SEGGER_RTT__LockState; __asm volatile ("mrs r1, CPSR \n\t" "mov %0, r1 \n\t" "orr r1, r1, #0xC0 \n\t" "msr CPSR_c, r1 \n\t" : "=r" (_SEGGER_RTT__LockState) : : "r1", "cc" );` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __ICCARM__；#if (defined (__ARM6M__) && (__CORE__ == __ARM6M__)) \|\| (defined (__ARM8M_BASELINE__) && (__CORE__ == __ARM8M_BASELINE__)) → #elif (defined (__ARM7EM__) && (__CORE__ == __ARM7EM__)) \|\| (defined (__ARM7M__) && (__CORE__ == __ARM7M__)) \|\| (defined (__ARM8M_MAINLINE__) && (__CORE__ == __ARM8M_MAINLINE__)) \|\| (defined (__ARM8M_MAINLINE__) && (__CORE__ == __ARM8M_MAINLINE__)) → #elif (defined (__ARM7A__) && (__CORE__ == __ARM7A__)) \|\| (defined (__ARM7R__) && (__CORE__ == __ARM7R__))` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:283](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L283) | `SEGGER_RTT_UNLOCK()` | `__asm volatile ("mov r0, %0 \n\t" "mrs r1, CPSR \n\t" "bic r1, r1, #0xC0 \n\t" "and r0, r0, #0xC0 \n\t" "orr r1, r1, r0 \n\t" "msr CPSR_c, r1 \n\t" : : "r" (_SEGGER_RTT__LockState) : "r0", "r1", "cc" ); }` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __ICCARM__；#if (defined (__ARM6M__) && (__CORE__ == __ARM6M__)) \|\| (defined (__ARM8M_BASELINE__) && (__CORE__ == __ARM8M_BASELINE__)) → #elif (defined (__ARM7EM__) && (__CORE__ == __ARM7EM__)) \|\| (defined (__ARM7M__) && (__CORE__ == __ARM7M__)) \|\| (defined (__ARM8M_MAINLINE__) && (__CORE__ == __ARM8M_MAINLINE__)) \|\| (defined (__ARM8M_MAINLINE__) && (__CORE__ == __ARM8M_MAINLINE__)) → #elif (defined (__ARM7A__) && (__CORE__ == __ARM7A__)) \|\| (defined (__ARM7R__) && (__CORE__ == __ARM7R__))` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:302](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L302) | `SEGGER_RTT_LOCK()` | `{ unsigned long _SEGGER_RTT__LockState; _SEGGER_RTT__LockState = __get_interrupt_state(); __disable_interrupt();` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __ICCRX__` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:307](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L307) | `SEGGER_RTT_UNLOCK()` | `__set_interrupt_state(_SEGGER_RTT__LockState); }` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __ICCRX__` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:316](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L316) | `SEGGER_RTT_LOCK()` | `{ __istate_t _SEGGER_RTT__LockState; _SEGGER_RTT__LockState = __get_interrupt_state(); __disable_interrupt();` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __ICCRL78__` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:321](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L321) | `SEGGER_RTT_UNLOCK()` | `__set_interrupt_state(_SEGGER_RTT__LockState); }` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __ICCRL78__` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:331](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L331) | `SEGGER_RTT_LOCK()` | `{ unsigned int _SEGGER_RTT__LockState; register unsigned char _SEGGER_RTT__PRIMASK __asm( "primask"); _SEGGER_RTT__LockState = _SEGGER_RTT__PRIMASK; _SEGGER_RTT__PRIMASK = 1u; __schedule_barrier();` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __CC_ARM；#if (defined __TARGET_ARCH_6S_M)` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:338](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L338) | `SEGGER_RTT_UNLOCK()` | `_SEGGER_RTT__PRIMASK = _SEGGER_RTT__LockState; __schedule_barrier(); }` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __CC_ARM；#if (defined __TARGET_ARCH_6S_M)` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:343](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L343) | `SEGGER_RTT_MAX_INTERRUPT_PRIORITY` | `(0x20)` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __CC_ARM；#if (defined __TARGET_ARCH_6S_M) → #elif (defined(__TARGET_ARCH_7_M) \|\| defined(__TARGET_ARCH_7E_M))；#ifndef SEGGER_RTT_MAX_INTERRUPT_PRIORITY` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:345](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L345) | `SEGGER_RTT_LOCK()` | `{ unsigned int _SEGGER_RTT__LockState; register unsigned char BASEPRI __asm( "basepri"); _SEGGER_RTT__LockState = BASEPRI; BASEPRI = SEGGER_RTT_MAX_INTERRUPT_PRIORITY; __schedule_barrier();` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __CC_ARM；#if (defined __TARGET_ARCH_6S_M) → #elif (defined(__TARGET_ARCH_7_M) \|\| defined(__TARGET_ARCH_7E_M))` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:352](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L352) | `SEGGER_RTT_UNLOCK()` | `BASEPRI = _SEGGER_RTT__LockState; __schedule_barrier(); }` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __CC_ARM；#if (defined __TARGET_ARCH_6S_M) → #elif (defined(__TARGET_ARCH_7_M) \|\| defined(__TARGET_ARCH_7E_M))` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:364](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L364) | `SEGGER_RTT_LOCK()` | `{ unsigned int _SEGGER_RTT__LockState; _SEGGER_RTT__LockState = __get_PRIMASK(); __set_PRIMASK(1);` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __TI_ARM__；#if defined (__TI_ARM_V6M0__)` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:369](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L369) | `SEGGER_RTT_UNLOCK()` | `__set_PRIMASK(_SEGGER_RTT__LockState); }` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __TI_ARM__；#if defined (__TI_ARM_V6M0__)` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:373](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L373) | `SEGGER_RTT_MAX_INTERRUPT_PRIORITY` | `(0x20)` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __TI_ARM__；#if defined (__TI_ARM_V6M0__) → #elif (defined (__TI_ARM_V7M3__) \|\| defined (__TI_ARM_V7M4__))；#ifndef SEGGER_RTT_MAX_INTERRUPT_PRIORITY` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:375](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L375) | `SEGGER_RTT_LOCK()` | `{ unsigned int _SEGGER_RTT__LockState; _SEGGER_RTT__LockState = _set_interrupt_priority(SEGGER_RTT_MAX_INTERRUPT_PRIORITY);` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __TI_ARM__；#if defined (__TI_ARM_V6M0__) → #elif (defined (__TI_ARM_V7M3__) \|\| defined (__TI_ARM_V7M4__))` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:379](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L379) | `SEGGER_RTT_UNLOCK()` | `_set_interrupt_priority(_SEGGER_RTT__LockState); }` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __TI_ARM__；#if defined (__TI_ARM_V6M0__) → #elif (defined (__TI_ARM_V7M3__) \|\| defined (__TI_ARM_V7M4__))` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:390](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L390) | `SEGGER_RTT_LOCK()` | `{ unsigned long _SEGGER_RTT__LockState; _SEGGER_RTT__LockState = get_psw() & 0x010000; clrpsw_i();` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __RX` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:395](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L395) | `SEGGER_RTT_UNLOCK()` | `set_psw(get_psw() \| _SEGGER_RTT__LockState); }` | `#ifndef SEGGER_RTT_CONF_H；#ifdef __RX` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:409](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L409) | `SEGGER_RTT_LOCK()` | `{ OS_SIM_EnterCriticalSection();` | `#ifndef SEGGER_RTT_CONF_H；#if defined(WIN32) \|\| defined(SEGGER_RTT_LOCK_EMBOS)` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:412](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L412) | `SEGGER_RTT_UNLOCK()` | `OS_SIM_LeaveCriticalSection(); }` | `#ifndef SEGGER_RTT_CONF_H；#if defined(WIN32) \|\| defined(SEGGER_RTT_LOCK_EMBOS)` |  |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:421](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L421) | `SEGGER_RTT_LOCK()` | 空定义 | `#ifndef SEGGER_RTT_CONF_H；#ifndef SEGGER_RTT_LOCK` | Lock RTT (nestable)   (i.e. disable interrupts) |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:425](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L425) | `SEGGER_RTT_UNLOCK()` | 空定义 | `#ifndef SEGGER_RTT_CONF_H；#ifndef SEGGER_RTT_UNLOCK` | Unlock RTT (nestable) (i.e. enable previous interrupt lock state) |

### 已注释的宏选项（当前不定义）

| 行 | 原选项 |
| --- | --- |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:75](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L75) | `#define SEGGER_RTT_CPU_CACHE_LINE_SIZE (32) // Largest cache line size (in bytes) in the current system` |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:76](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L76) | `#define SEGGER_RTT_UNCACHED_OFF (0xFB000000) // Address alias where RTT CB and buffers can be accessed uncached` |
| [Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:129](../Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h#L129) | `#define SEGGER_RTT_MEMCPY(pDest, pSrc, NumBytes) SEGGER_memcpy((pDest), (pSrc), (NumBytes))` |

## modules/alarm/buzzer.c

源文件：[打开 modules/alarm/buzzer.c](../modules/alarm/buzzer.c)。

### 配置结构体初值

以下记录原始定义，不自动推断运行路径或后续覆盖。未填字段为零；指针/回调表示绑定关系。

[modules/alarm/buzzer.c:16](../modules/alarm/buzzer.c#L16)

```c
PWM_Init_Config_s buzzer_config = {
        .htim = &htim4,
        .channel = TIM_CHANNEL_3,
        .dutyratio = 0,
        .period = 0.001,
    };
```

## modules/alarm/buzzer.h

源文件：[打开 modules/alarm/buzzer.h](../modules/alarm/buzzer.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/alarm/buzzer.h:2](../modules/alarm/buzzer.h#L2) | `BUZZER_H` | 空定义 | `#ifndef BUZZER_H` |  |
| [modules/alarm/buzzer.h:4](../modules/alarm/buzzer.h#L4) | `BUZZER_DEVICE_CNT` | `5` | `#ifndef BUZZER_H` |  |
| [modules/alarm/buzzer.h:6](../modules/alarm/buzzer.h#L6) | `DoFreq` | `523` | `#ifndef BUZZER_H` |  |
| [modules/alarm/buzzer.h:7](../modules/alarm/buzzer.h#L7) | `ReFreq` | `587` | `#ifndef BUZZER_H` |  |
| [modules/alarm/buzzer.h:8](../modules/alarm/buzzer.h#L8) | `MiFreq` | `659` | `#ifndef BUZZER_H` |  |
| [modules/alarm/buzzer.h:9](../modules/alarm/buzzer.h#L9) | `FaFreq` | `698` | `#ifndef BUZZER_H` |  |
| [modules/alarm/buzzer.h:10](../modules/alarm/buzzer.h#L10) | `SoFreq` | `784` | `#ifndef BUZZER_H` |  |
| [modules/alarm/buzzer.h:11](../modules/alarm/buzzer.h#L11) | `LaFreq` | `880` | `#ifndef BUZZER_H` |  |
| [modules/alarm/buzzer.h:12](../modules/alarm/buzzer.h#L12) | `SiFreq` | `988` | `#ifndef BUZZER_H` |  |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[modules/alarm/buzzer.h:40](../modules/alarm/buzzer.h#L40)

```c
typedef struct
{
    AlarmLevel_e alarm_level;
    octave_e octave;
    float loudness;
}Buzzer_config_s;
```

## modules/algorithm/chassis_energy_config.h

源文件：[打开 modules/algorithm/chassis_energy_config.h](../modules/algorithm/chassis_energy_config.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/algorithm/chassis_energy_config.h:2](../modules/algorithm/chassis_energy_config.h#L2) | `CHASSIS_ENERGY_CONFIG_H` | 空定义 | `#ifndef CHASSIS_ENERGY_CONFIG_H` |  |
| [modules/algorithm/chassis_energy_config.h:6](../modules/algorithm/chassis_energy_config.h#L6) | `CHASSIS_BUFFER_POWER_ENABLE` | `1` | `#ifndef CHASSIS_ENERGY_CONFIG_H；#ifndef CHASSIS_BUFFER_POWER_ENABLE` |  |
| [modules/algorithm/chassis_energy_config.h:9](../modules/algorithm/chassis_energy_config.h#L9) | `CHASSIS_POWER_SOURCE_DIRECT` | `0` | `#ifndef CHASSIS_ENERGY_CONFIG_H` |  |
| [modules/algorithm/chassis_energy_config.h:10](../modules/algorithm/chassis_energy_config.h#L10) | `CHASSIS_POWER_SOURCE_SUPERCAP` | `1` | `#ifndef CHASSIS_ENERGY_CONFIG_H` |  |
| [modules/algorithm/chassis_energy_config.h:14](../modules/algorithm/chassis_energy_config.h#L14) | `CHASSIS_POWER_SOURCE_MODE` | `CHASSIS_POWER_SOURCE_DIRECT` | `#ifndef CHASSIS_ENERGY_CONFIG_H；#ifndef CHASSIS_POWER_SOURCE_MODE` |  |
| [modules/algorithm/chassis_energy_config.h:27](../modules/algorithm/chassis_energy_config.h#L27) | `CHASSIS_BUFFER_MAX_ENERGY_J` | `60.0f` | `#ifndef CHASSIS_ENERGY_CONFIG_H` | 缓冲能量满值(J):裁判原始读数超过它即判缓冲数据无效,不再参与增补 |
| [modules/algorithm/chassis_energy_config.h:28](../modules/algorithm/chassis_energy_config.h#L28) | `CHASSIS_BUFFER_RECOVERY_ENTER_J` | `15.0f` | `#ifndef CHASSIS_ENERGY_CONFIG_H` | 进入回充(J):估算下限降到它或以下就转入恢复 |
| [modules/algorithm/chassis_energy_config.h:29](../modules/algorithm/chassis_energy_config.h#L29) | `CHASSIS_BUFFER_RECOVERY_EXIT_J` | `30.0f` | `#ifndef CHASSIS_ENERGY_CONFIG_H` | 退出回充(J):估算下限回到它或以上并保持 RECOVERY_HOLD_S 才解除 |
| [modules/algorithm/chassis_energy_config.h:30](../modules/algorithm/chassis_energy_config.h#L30) | `CHASSIS_BUFFER_HARD_FLOOR_J` | `5.0f` | `#ifndef CHASSIS_ENERGY_CONFIG_H` | 能量硬底座(J):预测值跌破它强制转入恢复,也是增补功率的保底基准 |
| [modules/algorithm/chassis_energy_config.h:31](../modules/algorithm/chassis_energy_config.h#L31) | `CHASSIS_BUFFER_PREDICTION_WINDOW_S` | `0.5f` | `#ifndef CHASSIS_ENERGY_CONFIG_H` | 预测窗口(s):把高于进入门槛的富余能量按该时长摊成允许的额外输入功率 |
| [modules/algorithm/chassis_energy_config.h:32](../modules/algorithm/chassis_energy_config.h#L32) | `CHASSIS_BUFFER_GUARD_WINDOW_S` | `0.3f` | `#ifndef CHASSIS_ENERGY_CONFIG_H` | 保护窗口(s):把超限输入功率和硬底座余量换算成该时长内的能量预算 |
| [modules/algorithm/chassis_energy_config.h:33](../modules/algorithm/chassis_energy_config.h#L33) | `CHASSIS_BUFFER_SETTLEMENT_DELAY_S` | `0.1f` | `#ifndef CHASSIS_ENERGY_CONFIG_H` | 结算延迟(s):裁判帧接收时刻不等于结算时刻,额外预留这段时间的消耗 |
| [modules/algorithm/chassis_energy_config.h:34](../modules/algorithm/chassis_energy_config.h#L34) | `CHASSIS_BUFFER_ENERGY_MARGIN_J` | `3.0f` | `#ifndef CHASSIS_ENERGY_CONFIG_H` | 能量安全余量(J):计算估算下限时直接扣除 |
| [modules/algorithm/chassis_energy_config.h:35](../modules/algorithm/chassis_energy_config.h#L35) | `CHASSIS_BUFFER_POWER_MARGIN_W` | `5.0f` | `#ifndef CHASSIS_ENERGY_CONFIG_H` | 功率安全余量(W):估算下限按它预留消耗,判输入超限和增补时也扣掉 |
| [modules/algorithm/chassis_energy_config.h:36](../modules/algorithm/chassis_energy_config.h#L36) | `CHASSIS_BUFFER_BOOST_MAX_W` | `60.0f` | `#ifndef CHASSIS_ENERGY_CONFIG_H` | 输入预算增补上限(W):注意与外层 max_boost_w(电容负载增补上限)不是同一个量 |
| [modules/algorithm/chassis_energy_config.h:37](../modules/algorithm/chassis_energy_config.h#L37) | `CHASSIS_BUFFER_RECOVERY_POWER_W` | `20.0f` | `#ifndef CHASSIS_ENERGY_CONFIG_H` | 最大回充减让(W):按能量缺口调节,逼近硬底座时使用全部减让 |
| [modules/algorithm/chassis_energy_config.h:38](../modules/algorithm/chassis_energy_config.h#L38) | `CHASSIS_BUFFER_RECOVERY_TARGET_TIME_S` | `1.0f` | `#ifndef CHASSIS_ENERGY_CONFIG_H` | 目标恢复时间(s):能量缺口除以该时间得到回充减让功率 |
| [modules/algorithm/chassis_energy_config.h:39](../modules/algorithm/chassis_energy_config.h#L39) | `CHASSIS_BUFFER_OBSERVER_MAX_STEP_S` | `0.020f` | `#ifndef CHASSIS_ENERGY_CONFIG_H` | 最大连续积分间隔(s):超出时按最坏耗能扣减并撤销增补 |
| [modules/algorithm/chassis_energy_config.h:40](../modules/algorithm/chassis_energy_config.h#L40) | `CHASSIS_BUFFER_RECOVERY_HOLD_S` | `0.1f` | `#ifndef CHASSIS_ENERGY_CONFIG_H` | 退出回充的确认时长(s):在该时长内持续高于退出门槛才解除 |
| [modules/algorithm/chassis_energy_config.h:41](../modules/algorithm/chassis_energy_config.h#L41) | `CHASSIS_BUFFER_RISE_RATE_W_PER_S` | `600.0f` | `#ifndef CHASSIS_ENERGY_CONFIG_H` | 输入预算上升斜率(W/s);电容负载侧使用独立斜率,下降立即生效 |
| [modules/algorithm/chassis_energy_config.h:42](../modules/algorithm/chassis_energy_config.h#L42) | `CHASSIS_BUFFER_CORRECTION_RISE_J_PER_S` | `20.0f` | `#ifndef CHASSIS_ENERGY_CONFIG_H` | 新裁判帧向上校正估算的上限(J/s);向下立即校正 |
| [modules/algorithm/chassis_energy_config.h:43](../modules/algorithm/chassis_energy_config.h#L43) | `CHASSIS_POWER_DEMAND_MARGIN_W` | `5.0f` | `#ifndef CHASSIS_ENERGY_CONFIG_H` | 高于基础功率的真实驱动需求所允许的额外余量(W) |

## modules/algorithm/chassis_kinematics.c

源文件：[打开 modules/algorithm/chassis_kinematics.c](../modules/algorithm/chassis_kinematics.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/algorithm/chassis_kinematics.c:5](../modules/algorithm/chassis_kinematics.c#L5) | `OMNI_X_PROJECTION` | `0.70710678118654752440f` | 无外层条件 |  |

## modules/algorithm/chassis_kinematics.h

源文件：[打开 modules/algorithm/chassis_kinematics.h](../modules/algorithm/chassis_kinematics.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/algorithm/chassis_kinematics.h:2](../modules/algorithm/chassis_kinematics.h#L2) | `CHASSIS_KINEMATICS_H` | 空定义 | `#ifndef CHASSIS_KINEMATICS_H` |  |

## modules/algorithm/chassis_power_budget.h

源文件：[打开 modules/algorithm/chassis_power_budget.h](../modules/algorithm/chassis_power_budget.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/algorithm/chassis_power_budget.h:2](../modules/algorithm/chassis_power_budget.h#L2) | `CHASSIS_POWER_BUDGET_H` | 空定义 | `#ifndef CHASSIS_POWER_BUDGET_H` |  |
| [modules/algorithm/chassis_power_budget.h:7](../modules/algorithm/chassis_power_budget.h#L7) | `CHASSIS_BUFFER_HISTORY_LENGTH` | `96U` | `#ifndef CHASSIS_POWER_BUDGET_H` | 固定输入功率历史段数 |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[modules/algorithm/chassis_power_budget.h:14](../modules/algorithm/chassis_power_budget.h#L14)

```c
typedef struct
{
    float max_energy_j;
    float recovery_enter_j;
    float recovery_exit_j;
    float hard_floor_j;
    float prediction_window_s;
    float guard_window_s;
    float settlement_delay_s;
    float energy_margin_j;
    float power_margin_w;
    float max_boost_w;
    float recovery_power_w; /* Maximum recovery reduction, not a fixed decrement. */
    float recovery_target_time_s;
    float observer_max_step_s;
    float recovery_hold_s;
    float rise_rate_w_per_s;
    float safety_factor;
} ChassisBufferBudgetConfig_s;
```

[modules/algorithm/chassis_power_budget.h:34](../modules/algorithm/chassis_power_budget.h#L34)

```c
typedef struct
{
    float empty_energy_ratio;
    float full_energy_ratio;
    float max_boost_w;
    float rise_rate_w_per_s;
    ChassisBufferBudgetConfig_s buffer;
} ChassisPowerBudgetConfig_s;
```

## modules/algorithm/chassis_power_config.h

源文件：[打开 modules/algorithm/chassis_power_config.h](../modules/algorithm/chassis_power_config.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/algorithm/chassis_power_config.h:2](../modules/algorithm/chassis_power_config.h#L2) | `CHASSIS_POWER_CONFIG_H` | 空定义 | `#ifndef CHASSIS_POWER_CONFIG_H` |  |
| [modules/algorithm/chassis_power_config.h:10](../modules/algorithm/chassis_power_config.h#L10) | `CHASSIS_POWER_CONTROL_STAGE` | `3` | `#ifndef CHASSIS_POWER_CONFIG_H；#ifndef CHASSIS_POWER_CONTROL_STAGE` |  |
| [modules/algorithm/chassis_power_config.h:13](../modules/algorithm/chassis_power_config.h#L13) | `CHASSIS_POWER_CAPTURE_ENABLE` | `0` | `#ifndef CHASSIS_POWER_CONFIG_H；#ifndef CHASSIS_POWER_CAPTURE_ENABLE` |  |
| [modules/algorithm/chassis_power_config.h:16](../modules/algorithm/chassis_power_config.h#L16) | `POWER_METER_ENABLE` | `1` | `#ifndef CHASSIS_POWER_CONFIG_H；#ifndef POWER_METER_ENABLE` |  |

## modules/algorithm/chassis_power_outer_loop.h

源文件：[打开 modules/algorithm/chassis_power_outer_loop.h](../modules/algorithm/chassis_power_outer_loop.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/algorithm/chassis_power_outer_loop.h:2](../modules/algorithm/chassis_power_outer_loop.h#L2) | `CHASSIS_POWER_OUTER_LOOP_H` | 空定义 | `#ifndef CHASSIS_POWER_OUTER_LOOP_H` |  |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[modules/algorithm/chassis_power_outer_loop.h:14](../modules/algorithm/chassis_power_outer_loop.h#L14)

```c
typedef struct
{
    float kp;
    float ki;
    float measurement_lpf_tau_s;
    float activation_margin_w;
    float error_deadband_w;
    float max_positive_correction_w;
    float positive_slew_rate_w_per_s;
    float budget_drop_reset_w;
    float antiwindup_gain_per_s, low_demand_delay_s, low_demand_decay_w_per_s;
} ChassisPowerOuterLoopConfig_s;
```

## modules/algorithm/chassis_power_transient.c

源文件：[打开 modules/algorithm/chassis_power_transient.c](../modules/algorithm/chassis_power_transient.c)。

### 配置结构体初值

以下记录原始定义，不自动推断运行路径或后续覆盖。未填字段为零；指针/回调表示绑定关系。

[modules/algorithm/chassis_power_transient.c:28](../modules/algorithm/chassis_power_transient.c#L28)

```c
return (ChassisPowerTransientConfig_s){.eta = .95f, .fast_gain = .5f,
        .release_w_per_s = 60, .stale_release_w_per_s = 20,
        .meter_fresh_ms = 20, .feedback_fresh_ms = 5,
        .release_confirm_ms = 10, .stable_run_ms = 100,
        .max_current_command = 15000, .acceleration_tau_s = .010f, .prediction_s = .008f,
        .growth_time_s = .005f, .peak_ratio = 1.30f, .low_rpm = 100, .low_current = 500,
        .target_step_rpm = 300, .zero_enter_rpm = 50, .zero_exit_rpm = 100,
        .demand_enter_current = 200, .demand_exit_current = 100,
        .low_hold_ms = 100, .startup_window_ms = 30, .startup_cooldown_ms = 500,
        .guard_search_nodes = 256};
```

## modules/algorithm/chassis_power_transient.h

源文件：[打开 modules/algorithm/chassis_power_transient.h](../modules/algorithm/chassis_power_transient.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/algorithm/chassis_power_transient.h:2](../modules/algorithm/chassis_power_transient.h#L2) | `CHASSIS_POWER_TRANSIENT_H` | 空定义 | `#ifndef CHASSIS_POWER_TRANSIENT_H` |  |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[modules/algorithm/chassis_power_transient.h:14](../modules/algorithm/chassis_power_transient.h#L14)

```c
typedef struct {
    float eta, fast_gain, release_w_per_s, stale_release_w_per_s;
    uint32_t meter_fresh_ms, feedback_fresh_ms, release_confirm_ms, stable_run_ms;
    float max_current_command, acceleration_tau_s, prediction_s, growth_time_s;
    float peak_ratio, low_rpm, low_current, target_step_rpm;
    float zero_enter_rpm, zero_exit_rpm, demand_enter_current, demand_exit_current;
    uint32_t low_hold_ms, startup_window_ms, startup_cooldown_ms;
    uint16_t guard_search_nodes;
} ChassisPowerTransientConfig_s;
```

## modules/algorithm/chassis_spin_config.h

源文件：[打开 modules/algorithm/chassis_spin_config.h](../modules/algorithm/chassis_spin_config.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/algorithm/chassis_spin_config.h:2](../modules/algorithm/chassis_spin_config.h#L2) | `CHASSIS_SPIN_CONFIG_H` | 空定义 | `#ifndef CHASSIS_SPIN_CONFIG_H` |  |
| [modules/algorithm/chassis_spin_config.h:6](../modules/algorithm/chassis_spin_config.h#L6) | `CHASSIS_SPIN_ADAPTIVE_ENABLE` | `1` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_ADAPTIVE_ENABLE` |  |
| [modules/algorithm/chassis_spin_config.h:12](../modules/algorithm/chassis_spin_config.h#L12) | `CHASSIS_SPIN_MODE_FIXED` | `0` | `#ifndef CHASSIS_SPIN_CONFIG_H` |  |
| [modules/algorithm/chassis_spin_config.h:13](../modules/algorithm/chassis_spin_config.h#L13) | `CHASSIS_SPIN_MODE_ADAPTIVE` | `1` | `#ifndef CHASSIS_SPIN_CONFIG_H` |  |
| [modules/algorithm/chassis_spin_config.h:14](../modules/algorithm/chassis_spin_config.h#L14) | `CHASSIS_SPIN_MODE_SINE` | `2` | `#ifndef CHASSIS_SPIN_CONFIG_H` |  |
| [modules/algorithm/chassis_spin_config.h:17](../modules/algorithm/chassis_spin_config.h#L17) | `CHASSIS_SPIN_MODE` | `CHASSIS_SPIN_MODE_ADAPTIVE` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_MODE；#if CHASSIS_SPIN_ADAPTIVE_ENABLE` |  |
| [modules/algorithm/chassis_spin_config.h:19](../modules/algorithm/chassis_spin_config.h#L19) | `CHASSIS_SPIN_MODE` | `CHASSIS_SPIN_MODE_FIXED` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_MODE；#if CHASSIS_SPIN_ADAPTIVE_ENABLE → #else` |  |
| [modules/algorithm/chassis_spin_config.h:25](../modules/algorithm/chassis_spin_config.h#L25) | `CHASSIS_SPIN_DYNAMIC_ENABLED` | `(CHASSIS_SPIN_MODE != CHASSIS_SPIN_MODE_FIXED)` | `#ifndef CHASSIS_SPIN_CONFIG_H` |  |
| [modules/algorithm/chassis_spin_config.h:29](../modules/algorithm/chassis_spin_config.h#L29) | `CHASSIS_SPIN_FIXED_WZ` | `4000.0f` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_FIXED_WZ` |  |
| [modules/algorithm/chassis_spin_config.h:32](../modules/algorithm/chassis_spin_config.h#L32) | `CHASSIS_SPIN_MIN_WZ` | `1000.0f` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_MIN_WZ` |  |
| [modules/algorithm/chassis_spin_config.h:35](../modules/algorithm/chassis_spin_config.h#L35) | `CHASSIS_SPIN_MAX_WZ` | `6000.0f` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_MAX_WZ` |  |
| [modules/algorithm/chassis_spin_config.h:38](../modules/algorithm/chassis_spin_config.h#L38) | `CHASSIS_SPIN_POWER_GAIN` | `40.0f` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_POWER_GAIN` |  |
| [modules/algorithm/chassis_spin_config.h:41](../modules/algorithm/chassis_spin_config.h#L41) | `CHASSIS_SPIN_RISE_RATE` | `400.0f` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_RISE_RATE` |  |
| [modules/algorithm/chassis_spin_config.h:44](../modules/algorithm/chassis_spin_config.h#L44) | `CHASSIS_SPIN_FALL_RATE` | `2000.0f` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_FALL_RATE` |  |
| [modules/algorithm/chassis_spin_config.h:47](../modules/algorithm/chassis_spin_config.h#L47) | `CHASSIS_SPIN_POWER_LPF_TAU_S` | `0.1f` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_POWER_LPF_TAU_S` |  |
| [modules/algorithm/chassis_spin_config.h:50](../modules/algorithm/chassis_spin_config.h#L50) | `CHASSIS_SPIN_POWER_DEADBAND_W` | `1.0f` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_POWER_DEADBAND_W` |  |
| [modules/algorithm/chassis_spin_config.h:53](../modules/algorithm/chassis_spin_config.h#L53) | `CHASSIS_SPIN_SETTLE_TIME_S` | `0.2f` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_SETTLE_TIME_S` |  |
| [modules/algorithm/chassis_spin_config.h:56](../modules/algorithm/chassis_spin_config.h#L56) | `CHASSIS_SPIN_TRACKING_ERROR_RPM` | `500.0f` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_TRACKING_ERROR_RPM` |  |
| [modules/algorithm/chassis_spin_config.h:59](../modules/algorithm/chassis_spin_config.h#L59) | `CHASSIS_SPIN_BUDGET_DROP_RESET_W` | `1.0f` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_BUDGET_DROP_RESET_W` |  |
| [modules/algorithm/chassis_spin_config.h:65](../modules/algorithm/chassis_spin_config.h#L65) | `CHASSIS_SPIN_SINE_CENTER_WZ` | `800.0f` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_SINE_CENTER_WZ` |  |
| [modules/algorithm/chassis_spin_config.h:68](../modules/algorithm/chassis_spin_config.h#L68) | `CHASSIS_SPIN_SINE_MIN_WZ` | `200.0f` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_SINE_MIN_WZ` |  |
| [modules/algorithm/chassis_spin_config.h:71](../modules/algorithm/chassis_spin_config.h#L71) | `CHASSIS_SPIN_SINE_AMPLITUDE_WZ` | `200.0f` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_SINE_AMPLITUDE_WZ` |  |
| [modules/algorithm/chassis_spin_config.h:74](../modules/algorithm/chassis_spin_config.h#L74) | `CHASSIS_SPIN_SINE_PERIOD_S` | `4.0f` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_SINE_PERIOD_S` |  |
| [modules/algorithm/chassis_spin_config.h:77](../modules/algorithm/chassis_spin_config.h#L77) | `CHASSIS_SPIN_SINE_RAMP_S` | `1.0f` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_SINE_RAMP_S` |  |
| [modules/algorithm/chassis_spin_config.h:80](../modules/algorithm/chassis_spin_config.h#L80) | `CHASSIS_SPIN_SINE_RISE_RATE` | `400.0f` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_SINE_RISE_RATE` |  |
| [modules/algorithm/chassis_spin_config.h:83](../modules/algorithm/chassis_spin_config.h#L83) | `CHASSIS_SPIN_SINE_FALL_RATE` | `2000.0f` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_SINE_FALL_RATE` |  |
| [modules/algorithm/chassis_spin_config.h:86](../modules/algorithm/chassis_spin_config.h#L86) | `CHASSIS_SPIN_SINE_DIRECTION` | `1` | `#ifndef CHASSIS_SPIN_CONFIG_H；#ifndef CHASSIS_SPIN_SINE_DIRECTION` |  |

## modules/algorithm/chassis_spin.h

源文件：[打开 modules/algorithm/chassis_spin.h](../modules/algorithm/chassis_spin.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/algorithm/chassis_spin.h:2](../modules/algorithm/chassis_spin.h#L2) | `CHASSIS_SPIN_H` | 空定义 | `#ifndef CHASSIS_SPIN_H` |  |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[modules/algorithm/chassis_spin.h:8](../modules/algorithm/chassis_spin.h#L8)

```c
typedef struct
{
    float fixed_wz, min_wz, max_wz;
    float gain_wz_per_w_s, rise_wz_per_s, fall_wz_per_s;
    float power_lpf_tau_s, power_deadband_w, settle_time_s;
    float budget_drop_reset_w;
    float sine_amplitude_wz, sine_period_s, sine_ramp_s;
    float sine_rise_wz_per_s, sine_fall_wz_per_s;
    int8_t sine_direction;
} ChassisSpinConfig_s;
```

## modules/algorithm/controller.h

源文件：[打开 modules/algorithm/controller.h](../modules/algorithm/controller.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/algorithm/controller.h:14](../modules/algorithm/controller.h#L14) | `_CONTROLLER_H` | 空定义 | `#ifndef _CONTROLLER_H` |  |
| [modules/algorithm/controller.h:25](../modules/algorithm/controller.h#L25) | `abs(x)` | `((x > 0) ? x : -x)` | `#ifndef _CONTROLLER_H；#ifndef abs` |  |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[modules/algorithm/controller.h:100](../modules/algorithm/controller.h#L100)

```c
typedef struct // config parameter
{
    // basic parameter
    float Kp;
    float Ki;
    float Kd;
    float MaxOut;   // 输出限幅
    float DeadBand; // 死区

    // improve parameter
    PID_Improvement_e Improve;
    float IntegralLimit; // 积分限幅
    float CoefA;         // AB为变速积分参数,变速积分实际上就引入了积分分离
    float CoefB;         // ITerm = Err*((A-abs(err)+B)/A)  when B<|err|<A+B
    float Output_LPF_RC; // RC = 1/omegac
    float Derivative_LPF_RC;
} PID_Init_Config_s;
```

## modules/algorithm/crc16.h

源文件：[打开 modules/algorithm/crc16.h](../modules/algorithm/crc16.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/algorithm/crc16.h:2](../modules/algorithm/crc16.h#L2) | `__CRC16_H` | 空定义 | `#ifndef __CRC16_H` |  |
| [modules/algorithm/crc16.h:5](../modules/algorithm/crc16.h#L5) | `CRC_START_16` | `0xFFFF` | `#ifndef __CRC16_H` |  |
| [modules/algorithm/crc16.h:6](../modules/algorithm/crc16.h#L6) | `CRC_START_MODBUS` | `0xFFFF` | `#ifndef __CRC16_H` |  |
| [modules/algorithm/crc16.h:7](../modules/algorithm/crc16.h#L7) | `CRC_POLY_16` | `0xA001` | `#ifndef __CRC16_H` |  |

## modules/algorithm/crc8.h

源文件：[打开 modules/algorithm/crc8.h](../modules/algorithm/crc8.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/algorithm/crc8.h:2](../modules/algorithm/crc8.h#L2) | `__CRC8_H` | 空定义 | `#ifndef __CRC8_H` |  |
| [modules/algorithm/crc8.h:6](../modules/algorithm/crc8.h#L6) | `CRC_START_8` | `0x00` | `#ifndef __CRC8_H` |  |

## modules/algorithm/kalman_filter.h

源文件：[打开 modules/algorithm/kalman_filter.h](../modules/algorithm/kalman_filter.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/algorithm/kalman_filter.h:14](../modules/algorithm/kalman_filter.h#L14) | `__KALMAN_FILTER_H` | 空定义 | `#ifndef __KALMAN_FILTER_H` |  |
| [modules/algorithm/kalman_filter.h:34](../modules/algorithm/kalman_filter.h#L34) | `user_malloc` | `pvPortMalloc` | `#ifndef __KALMAN_FILTER_H；#ifndef user_malloc；#ifdef _CMSIS_OS_H` |  |
| [modules/algorithm/kalman_filter.h:36](../modules/algorithm/kalman_filter.h#L36) | `user_malloc` | `malloc` | `#ifndef __KALMAN_FILTER_H；#ifndef user_malloc；#ifdef _CMSIS_OS_H → #else` |  |
| [modules/algorithm/kalman_filter.h:41](../modules/algorithm/kalman_filter.h#L41) | `mat` | `arm_matrix_instance_f32` | `#ifndef __KALMAN_FILTER_H` |  |
| [modules/algorithm/kalman_filter.h:42](../modules/algorithm/kalman_filter.h#L42) | `Matrix_Init` | `arm_mat_init_f32` | `#ifndef __KALMAN_FILTER_H` |  |
| [modules/algorithm/kalman_filter.h:43](../modules/algorithm/kalman_filter.h#L43) | `Matrix_Add` | `arm_mat_add_f32` | `#ifndef __KALMAN_FILTER_H` |  |
| [modules/algorithm/kalman_filter.h:44](../modules/algorithm/kalman_filter.h#L44) | `Matrix_Subtract` | `arm_mat_sub_f32` | `#ifndef __KALMAN_FILTER_H` |  |
| [modules/algorithm/kalman_filter.h:45](../modules/algorithm/kalman_filter.h#L45) | `Matrix_Multiply` | `arm_mat_mult_f32` | `#ifndef __KALMAN_FILTER_H` |  |
| [modules/algorithm/kalman_filter.h:46](../modules/algorithm/kalman_filter.h#L46) | `Matrix_Transpose` | `arm_mat_trans_f32` | `#ifndef __KALMAN_FILTER_H` |  |
| [modules/algorithm/kalman_filter.h:47](../modules/algorithm/kalman_filter.h#L47) | `Matrix_Inverse` | `arm_mat_inverse_f32` | `#ifndef __KALMAN_FILTER_H` |  |

### 已注释的宏选项（当前不定义）

| 行 | 原选项 |
| --- | --- |
| [modules/algorithm/kalman_filter.h:18](../modules/algorithm/kalman_filter.h#L18) | `#define __CC_ARM // Keil` |
| [modules/algorithm/kalman_filter.h:19](../modules/algorithm/kalman_filter.h#L19) | `#define ARM_MATH_CM4` |
| [modules/algorithm/kalman_filter.h:20](../modules/algorithm/kalman_filter.h#L20) | `#define ARM_MATH_MATRIX_CHECK` |
| [modules/algorithm/kalman_filter.h:21](../modules/algorithm/kalman_filter.h#L21) | `#define ARM_MATH_ROUNDING` |
| [modules/algorithm/kalman_filter.h:22](../modules/algorithm/kalman_filter.h#L22) | `#define ARM_MATH_DSP // define in arm_math.h` |

## modules/algorithm/power_model.h

源文件：[打开 modules/algorithm/power_model.h](../modules/algorithm/power_model.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/algorithm/power_model.h:2](../modules/algorithm/power_model.h#L2) | `POWER_MODEL_H` | 空定义 | `#ifndef POWER_MODEL_H` |  |
| [modules/algorithm/power_model.h:7](../modules/algorithm/power_model.h#L7) | `POWER_MODEL_MOTOR_COUNT` | `4U` | `#ifndef POWER_MODEL_H` |  |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[modules/algorithm/power_model.h:18](../modules/algorithm/power_model.h#L18)

```c
typedef struct
{
    float k0;
    float k1;
    float k2;
    float k3;
    float k4;
    float k5;
    float current_conversion;
} MotorPowerModelConfig_s;
```

[modules/algorithm/power_model.h:29](../modules/algorithm/power_model.h#L29)

```c
typedef struct
{
    float safety_factor;
    float small_error_threshold_rpm;
    float reserved_power_threshold_w;
    float per_motor_reserved_power_w;
    float max_current_command;
} ChassisPowerAlgorithmConfig_s;
```

## modules/algorithm/QuaternionEKF.h

源文件：[打开 modules/algorithm/QuaternionEKF.h](../modules/algorithm/QuaternionEKF.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/algorithm/QuaternionEKF.h:14](../modules/algorithm/QuaternionEKF.h#L14) | `_QUAT_EKF_H` | 空定义 | `#ifndef _QUAT_EKF_H` |  |
| [modules/algorithm/QuaternionEKF.h:19](../modules/algorithm/QuaternionEKF.h#L19) | `TRUE` | `1` | `#ifndef _QUAT_EKF_H；#ifndef TRUE` | *< boolean true |
| [modules/algorithm/QuaternionEKF.h:23](../modules/algorithm/QuaternionEKF.h#L23) | `FALSE` | `0` | `#ifndef _QUAT_EKF_H；#ifndef FALSE` | *< boolean fails |

## modules/algorithm/user_lib.c

源文件：[打开 modules/algorithm/user_lib.c](../modules/algorithm/user_lib.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/algorithm/user_lib.c:21](../modules/algorithm/user_lib.c#L21) | `user_malloc` | `pvPortMalloc` | `#ifdef _CMSIS_OS_H` |  |
| [modules/algorithm/user_lib.c:23](../modules/algorithm/user_lib.c#L23) | `user_malloc` | `malloc` | `#ifdef _CMSIS_OS_H → #else` |  |

## modules/algorithm/user_lib.h

源文件：[打开 modules/algorithm/user_lib.h](../modules/algorithm/user_lib.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/algorithm/user_lib.h:14](../modules/algorithm/user_lib.h#L14) | `_USER_LIB_H` | 空定义 | `#ifndef _USER_LIB_H` |  |
| [modules/algorithm/user_lib.h:25](../modules/algorithm/user_lib.h#L25) | `user_malloc` | `pvPortMalloc` | `#ifndef _USER_LIB_H；#ifndef user_malloc；#ifdef _CMSIS_OS_H` |  |
| [modules/algorithm/user_lib.h:27](../modules/algorithm/user_lib.h#L27) | `user_malloc` | `malloc` | `#ifndef _USER_LIB_H；#ifndef user_malloc；#ifdef _CMSIS_OS_H → #else` |  |
| [modules/algorithm/user_lib.h:31](../modules/algorithm/user_lib.h#L31) | `msin(x)` | `(arm_sin_f32(x))` | `#ifndef _USER_LIB_H` |  |
| [modules/algorithm/user_lib.h:32](../modules/algorithm/user_lib.h#L32) | `mcos(x)` | `(arm_cos_f32(x))` | `#ifndef _USER_LIB_H` |  |
| [modules/algorithm/user_lib.h:36](../modules/algorithm/user_lib.h#L36) | `MatAdd` | `arm_mat_add_f32` | `#ifndef _USER_LIB_H` |  |
| [modules/algorithm/user_lib.h:37](../modules/algorithm/user_lib.h#L37) | `MatSubtract` | `arm_mat_sub_f32` | `#ifndef _USER_LIB_H` |  |
| [modules/algorithm/user_lib.h:38](../modules/algorithm/user_lib.h#L38) | `MatMultiply` | `arm_mat_mult_f32` | `#ifndef _USER_LIB_H` |  |
| [modules/algorithm/user_lib.h:39](../modules/algorithm/user_lib.h#L39) | `MatTranspose` | `arm_mat_trans_f32` | `#ifndef _USER_LIB_H` |  |
| [modules/algorithm/user_lib.h:40](../modules/algorithm/user_lib.h#L40) | `MatInverse` | `arm_mat_inverse_f32` | `#ifndef _USER_LIB_H` |  |
| [modules/algorithm/user_lib.h:45](../modules/algorithm/user_lib.h#L45) | `TRUE` | `1` | `#ifndef _USER_LIB_H；#ifndef TRUE` | *< boolean true |
| [modules/algorithm/user_lib.h:49](../modules/algorithm/user_lib.h#L49) | `FALSE` | `0` | `#ifndef _USER_LIB_H；#ifndef FALSE` | *< boolean fails |
| [modules/algorithm/user_lib.h:54](../modules/algorithm/user_lib.h#L54) | `PI` | `3.14159265354f` | `#ifndef _USER_LIB_H；#ifndef PI` |  |
| [modules/algorithm/user_lib.h:57](../modules/algorithm/user_lib.h#L57) | `VAL_LIMIT(val, min, max)` | `do { if ((val) <= (min)) { (val) = (min); } else if ((val) >= (max)) { (val) = (max); } } while (0)` | `#ifndef _USER_LIB_H` |  |
| [modules/algorithm/user_lib.h:70](../modules/algorithm/user_lib.h#L70) | `ANGLE_LIMIT_360(val, angle)` | `do { (val) = (angle) - (int)(angle); (val) += (int)(angle) % 360; } while (0)` | `#ifndef _USER_LIB_H` |  |
| [modules/algorithm/user_lib.h:77](../modules/algorithm/user_lib.h#L77) | `ANGLE_LIMIT_360_TO_180(val)` | `do { if ((val) > 180) (val) -= 360; } while (0)` | `#ifndef _USER_LIB_H` |  |
| [modules/algorithm/user_lib.h:84](../modules/algorithm/user_lib.h#L84) | `VAL_MIN(a, b)` | `((a) < (b) ? (a) : (b))` | `#ifndef _USER_LIB_H` |  |
| [modules/algorithm/user_lib.h:85](../modules/algorithm/user_lib.h#L85) | `VAL_MAX(a, b)` | `((a) > (b) ? (a) : (b))` | `#ifndef _USER_LIB_H` |  |
| [modules/algorithm/user_lib.h:124](../modules/algorithm/user_lib.h#L124) | `rad_format(Ang)` | `loop_float_constrain((Ang), -PI, PI)` | `#ifndef _USER_LIB_H` |  |

## modules/bluetooth/HC05.c

源文件：[打开 modules/bluetooth/HC05.c](../modules/bluetooth/HC05.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/bluetooth/HC05.c:4](../modules/bluetooth/HC05.c#L4) | `HC05_BUFFERSIZE` | `HC05_DATASIZE+2` | 无外层条件 | HC05发送和接收数据buffer大小，不得大于256 |
| [modules/bluetooth/HC05.c:6](../modules/bluetooth/HC05.c#L6) | `FRAME_HEAD` | `0XAA` | 无外层条件 | 帧头 |
| [modules/bluetooth/HC05.c:7](../modules/bluetooth/HC05.c#L7) | `FRAME_END` | `0X55` | 无外层条件 | 帧尾 |

## modules/bluetooth/HC05.h

源文件：[打开 modules/bluetooth/HC05.h](../modules/bluetooth/HC05.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/bluetooth/HC05.h:2](../modules/bluetooth/HC05.h#L2) | `HC05_H` | 空定义 | `#ifndef HC05_H` |  |
| [modules/bluetooth/HC05.h:8](../modules/bluetooth/HC05.h#L8) | `HC05_DATASIZE` | `4` | `#ifndef HC05_H` | HC05接收和发送数据大小，根据需要修改 |

## modules/BMI088/bmi088_regNdef.h

源文件：[打开 modules/BMI088/bmi088_regNdef.h](../modules/BMI088/bmi088_regNdef.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/BMI088/bmi088_regNdef.h:2](../modules/BMI088/bmi088_regNdef.h#L2) | `BMI088REG_H` | 空定义 | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:6](../modules/BMI088/bmi088_regNdef.h#L6) | `BMI088_ACC_CHIP_ID` | `0x00` | `#ifndef BMI088REG_H` | the register is  " Who am I " |
| [modules/BMI088/bmi088_regNdef.h:7](../modules/BMI088/bmi088_regNdef.h#L7) | `BMI088_ACC_CHIP_ID_VALUE` | `0x1E` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:9](../modules/BMI088/bmi088_regNdef.h#L9) | `BMI088_ACC_ERR_REG` | `0x02` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:10](../modules/BMI088/bmi088_regNdef.h#L10) | `BMI088_ACCEL_CONGIF_ERROR_SHFITS` | `0x2` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:11](../modules/BMI088/bmi088_regNdef.h#L11) | `BMI088_ACCEL_CONGIF_ERROR` | `(1 << BMI088_ACCEL_CONGIF_ERROR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:12](../modules/BMI088/bmi088_regNdef.h#L12) | `BMI088_FATAL_ERROR_SHFITS` | `0x0` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:13](../modules/BMI088/bmi088_regNdef.h#L13) | `BMI088_FATAL_ERROR` | `(1 << BMI088_FATAL_ERROR)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:15](../modules/BMI088/bmi088_regNdef.h#L15) | `BMI088_ACC_STATUS` | `0x03` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:16](../modules/BMI088/bmi088_regNdef.h#L16) | `BMI088_ACCEL_DRDY_SHFITS` | `0x7` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:17](../modules/BMI088/bmi088_regNdef.h#L17) | `BMI088_ACCEL_DRDY` | `(1 << BMI088_ACCEL_DRDY_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:19](../modules/BMI088/bmi088_regNdef.h#L19) | `BMI088_ACCEL_XOUT_L` | `0x12` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:20](../modules/BMI088/bmi088_regNdef.h#L20) | `BMI088_ACCEL_XOUT_M` | `0x13` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:21](../modules/BMI088/bmi088_regNdef.h#L21) | `BMI088_ACCEL_YOUT_L` | `0x14` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:22](../modules/BMI088/bmi088_regNdef.h#L22) | `BMI088_ACCEL_YOUT_M` | `0x15` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:23](../modules/BMI088/bmi088_regNdef.h#L23) | `BMI088_ACCEL_ZOUT_L` | `0x16` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:24](../modules/BMI088/bmi088_regNdef.h#L24) | `BMI088_ACCEL_ZOUT_M` | `0x17` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:26](../modules/BMI088/bmi088_regNdef.h#L26) | `BMI088_SENSORTIME_DATA_L` | `0x18` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:27](../modules/BMI088/bmi088_regNdef.h#L27) | `BMI088_SENSORTIME_DATA_M` | `0x19` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:28](../modules/BMI088/bmi088_regNdef.h#L28) | `BMI088_SENSORTIME_DATA_H` | `0x1A` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:30](../modules/BMI088/bmi088_regNdef.h#L30) | `BMI088_ACC_INT_STAT_1` | `0x1D` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:31](../modules/BMI088/bmi088_regNdef.h#L31) | `BMI088_ACCEL_DRDY_INTERRUPT_SHFITS` | `0x7` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:32](../modules/BMI088/bmi088_regNdef.h#L32) | `BMI088_ACCEL_DRDY_INTERRUPT` | `(1 << BMI088_ACCEL_DRDY_INTERRUPT_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:34](../modules/BMI088/bmi088_regNdef.h#L34) | `BMI088_TEMP_M` | `0x22` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:36](../modules/BMI088/bmi088_regNdef.h#L36) | `BMI088_TEMP_L` | `0x23` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:38](../modules/BMI088/bmi088_regNdef.h#L38) | `BMI088_ACC_CONF` | `0x40` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:39](../modules/BMI088/bmi088_regNdef.h#L39) | `BMI088_ACC_CONF_MUST_Set` | `0x80` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:40](../modules/BMI088/bmi088_regNdef.h#L40) | `BMI088_ACC_BWP_SHFITS` | `0x4` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:41](../modules/BMI088/bmi088_regNdef.h#L41) | `BMI088_ACC_OSR4` | `(0x0 << BMI088_ACC_BWP_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:42](../modules/BMI088/bmi088_regNdef.h#L42) | `BMI088_ACC_OSR2` | `(0x1 << BMI088_ACC_BWP_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:43](../modules/BMI088/bmi088_regNdef.h#L43) | `BMI088_ACC_NORMAL` | `(0x2 << BMI088_ACC_BWP_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:45](../modules/BMI088/bmi088_regNdef.h#L45) | `BMI088_ACC_ODR_SHFITS` | `0x0` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:46](../modules/BMI088/bmi088_regNdef.h#L46) | `BMI088_ACC_12_5_HZ` | `(0x5 << BMI088_ACC_ODR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:47](../modules/BMI088/bmi088_regNdef.h#L47) | `BMI088_ACC_25_HZ` | `(0x6 << BMI088_ACC_ODR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:48](../modules/BMI088/bmi088_regNdef.h#L48) | `BMI088_ACC_50_HZ` | `(0x7 << BMI088_ACC_ODR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:49](../modules/BMI088/bmi088_regNdef.h#L49) | `BMI088_ACC_100_HZ` | `(0x8 << BMI088_ACC_ODR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:50](../modules/BMI088/bmi088_regNdef.h#L50) | `BMI088_ACC_200_HZ` | `(0x9 << BMI088_ACC_ODR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:51](../modules/BMI088/bmi088_regNdef.h#L51) | `BMI088_ACC_400_HZ` | `(0xA << BMI088_ACC_ODR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:52](../modules/BMI088/bmi088_regNdef.h#L52) | `BMI088_ACC_800_HZ` | `(0xB << BMI088_ACC_ODR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:53](../modules/BMI088/bmi088_regNdef.h#L53) | `BMI088_ACC_1600_HZ` | `(0xC << BMI088_ACC_ODR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:55](../modules/BMI088/bmi088_regNdef.h#L55) | `BMI088_ACC_RANGE` | `0x41` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:57](../modules/BMI088/bmi088_regNdef.h#L57) | `BMI088_ACC_RANGE_SHFITS` | `0x0` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:58](../modules/BMI088/bmi088_regNdef.h#L58) | `BMI088_ACC_RANGE_3G` | `(0x0 << BMI088_ACC_RANGE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:59](../modules/BMI088/bmi088_regNdef.h#L59) | `BMI088_ACC_RANGE_6G` | `(0x1 << BMI088_ACC_RANGE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:60](../modules/BMI088/bmi088_regNdef.h#L60) | `BMI088_ACC_RANGE_12G` | `(0x2 << BMI088_ACC_RANGE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:61](../modules/BMI088/bmi088_regNdef.h#L61) | `BMI088_ACC_RANGE_24G` | `(0x3 << BMI088_ACC_RANGE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:63](../modules/BMI088/bmi088_regNdef.h#L63) | `BMI088_INT1_IO_CTRL` | `0x53` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:64](../modules/BMI088/bmi088_regNdef.h#L64) | `BMI088_ACC_INT1_IO_ENABLE_SHFITS` | `0x3` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:65](../modules/BMI088/bmi088_regNdef.h#L65) | `BMI088_ACC_INT1_IO_ENABLE` | `(0x1 << BMI088_ACC_INT1_IO_ENABLE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:66](../modules/BMI088/bmi088_regNdef.h#L66) | `BMI088_ACC_INT1_GPIO_MODE_SHFITS` | `0x2` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:67](../modules/BMI088/bmi088_regNdef.h#L67) | `BMI088_ACC_INT1_GPIO_PP` | `(0x0 << BMI088_ACC_INT1_GPIO_MODE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:68](../modules/BMI088/bmi088_regNdef.h#L68) | `BMI088_ACC_INT1_GPIO_OD` | `(0x1 << BMI088_ACC_INT1_GPIO_MODE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:69](../modules/BMI088/bmi088_regNdef.h#L69) | `BMI088_ACC_INT1_GPIO_LVL_SHFITS` | `0x1` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:70](../modules/BMI088/bmi088_regNdef.h#L70) | `BMI088_ACC_INT1_GPIO_LOW` | `(0x0 << BMI088_ACC_INT1_GPIO_LVL_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:71](../modules/BMI088/bmi088_regNdef.h#L71) | `BMI088_ACC_INT1_GPIO_HIGH` | `(0x1 << BMI088_ACC_INT1_GPIO_LVL_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:73](../modules/BMI088/bmi088_regNdef.h#L73) | `BMI088_INT2_IO_CTRL` | `0x54` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:74](../modules/BMI088/bmi088_regNdef.h#L74) | `BMI088_ACC_INT2_IO_ENABLE_SHFITS` | `0x3` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:75](../modules/BMI088/bmi088_regNdef.h#L75) | `BMI088_ACC_INT2_IO_ENABLE` | `(0x1 << BMI088_ACC_INT2_IO_ENABLE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:76](../modules/BMI088/bmi088_regNdef.h#L76) | `BMI088_ACC_INT2_GPIO_MODE_SHFITS` | `0x2` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:77](../modules/BMI088/bmi088_regNdef.h#L77) | `BMI088_ACC_INT2_GPIO_PP` | `(0x0 << BMI088_ACC_INT2_GPIO_MODE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:78](../modules/BMI088/bmi088_regNdef.h#L78) | `BMI088_ACC_INT2_GPIO_OD` | `(0x1 << BMI088_ACC_INT2_GPIO_MODE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:79](../modules/BMI088/bmi088_regNdef.h#L79) | `BMI088_ACC_INT2_GPIO_LVL_SHFITS` | `0x1` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:80](../modules/BMI088/bmi088_regNdef.h#L80) | `BMI088_ACC_INT2_GPIO_LOW` | `(0x0 << BMI088_ACC_INT2_GPIO_LVL_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:81](../modules/BMI088/bmi088_regNdef.h#L81) | `BMI088_ACC_INT2_GPIO_HIGH` | `(0x1 << BMI088_ACC_INT2_GPIO_LVL_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:83](../modules/BMI088/bmi088_regNdef.h#L83) | `BMI088_INT_MAP_DATA` | `0x58` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:84](../modules/BMI088/bmi088_regNdef.h#L84) | `BMI088_ACC_INT2_DRDY_INTERRUPT_SHFITS` | `0x6` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:85](../modules/BMI088/bmi088_regNdef.h#L85) | `BMI088_ACC_INT2_DRDY_INTERRUPT` | `(0x1 << BMI088_ACC_INT2_DRDY_INTERRUPT_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:86](../modules/BMI088/bmi088_regNdef.h#L86) | `BMI088_ACC_INT1_DRDY_INTERRUPT_SHFITS` | `0x2` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:87](../modules/BMI088/bmi088_regNdef.h#L87) | `BMI088_ACC_INT1_DRDY_INTERRUPT` | `(0x1 << BMI088_ACC_INT1_DRDY_INTERRUPT_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:89](../modules/BMI088/bmi088_regNdef.h#L89) | `BMI088_ACC_SELF_TEST` | `0x6D` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:90](../modules/BMI088/bmi088_regNdef.h#L90) | `BMI088_ACC_SELF_TEST_OFF` | `0x00` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:91](../modules/BMI088/bmi088_regNdef.h#L91) | `BMI088_ACC_SELF_TEST_POSITIVE_SIGNAL` | `0x0D` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:92](../modules/BMI088/bmi088_regNdef.h#L92) | `BMI088_ACC_SELF_TEST_NEGATIVE_SIGNAL` | `0x09` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:94](../modules/BMI088/bmi088_regNdef.h#L94) | `BMI088_ACC_PWR_CONF` | `0x7C` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:95](../modules/BMI088/bmi088_regNdef.h#L95) | `BMI088_ACC_PWR_SUSPEND_MODE` | `0x03` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:96](../modules/BMI088/bmi088_regNdef.h#L96) | `BMI088_ACC_PWR_ACTIVE_MODE` | `0x00` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:98](../modules/BMI088/bmi088_regNdef.h#L98) | `BMI088_ACC_PWR_CTRL` | `0x7D` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:99](../modules/BMI088/bmi088_regNdef.h#L99) | `BMI088_ACC_ENABLE_ACC_OFF` | `0x00` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:100](../modules/BMI088/bmi088_regNdef.h#L100) | `BMI088_ACC_ENABLE_ACC_ON` | `0x04` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:102](../modules/BMI088/bmi088_regNdef.h#L102) | `BMI088_ACC_SOFTRESET` | `0x7E` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:103](../modules/BMI088/bmi088_regNdef.h#L103) | `BMI088_ACC_SOFTRESET_VALUE` | `0xB6` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:105](../modules/BMI088/bmi088_regNdef.h#L105) | `BMI088_GYRO_CHIP_ID` | `0x00` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:106](../modules/BMI088/bmi088_regNdef.h#L106) | `BMI088_GYRO_CHIP_ID_VALUE` | `0x0F` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:108](../modules/BMI088/bmi088_regNdef.h#L108) | `BMI088_GYRO_X_L` | `0x02` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:109](../modules/BMI088/bmi088_regNdef.h#L109) | `BMI088_GYRO_X_H` | `0x03` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:110](../modules/BMI088/bmi088_regNdef.h#L110) | `BMI088_GYRO_Y_L` | `0x04` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:111](../modules/BMI088/bmi088_regNdef.h#L111) | `BMI088_GYRO_Y_H` | `0x05` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:112](../modules/BMI088/bmi088_regNdef.h#L112) | `BMI088_GYRO_Z_L` | `0x06` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:113](../modules/BMI088/bmi088_regNdef.h#L113) | `BMI088_GYRO_Z_H` | `0x07` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:115](../modules/BMI088/bmi088_regNdef.h#L115) | `BMI088_GYRO_INT_STAT_1` | `0x0A` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:116](../modules/BMI088/bmi088_regNdef.h#L116) | `BMI088_GYRO_DYDR_SHFITS` | `0x7` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:117](../modules/BMI088/bmi088_regNdef.h#L117) | `BMI088_GYRO_DYDR` | `(0x1 << BMI088_GYRO_DYDR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:119](../modules/BMI088/bmi088_regNdef.h#L119) | `BMI088_GYRO_RANGE` | `0x0F` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:120](../modules/BMI088/bmi088_regNdef.h#L120) | `BMI088_GYRO_RANGE_SHFITS` | `0x0` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:121](../modules/BMI088/bmi088_regNdef.h#L121) | `BMI088_GYRO_2000` | `(0x0 << BMI088_GYRO_RANGE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:122](../modules/BMI088/bmi088_regNdef.h#L122) | `BMI088_GYRO_1000` | `(0x1 << BMI088_GYRO_RANGE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:123](../modules/BMI088/bmi088_regNdef.h#L123) | `BMI088_GYRO_500` | `(0x2 << BMI088_GYRO_RANGE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:124](../modules/BMI088/bmi088_regNdef.h#L124) | `BMI088_GYRO_250` | `(0x3 << BMI088_GYRO_RANGE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:125](../modules/BMI088/bmi088_regNdef.h#L125) | `BMI088_GYRO_125` | `(0x4 << BMI088_GYRO_RANGE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:127](../modules/BMI088/bmi088_regNdef.h#L127) | `BMI088_GYRO_BANDWIDTH` | `0x10` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:129](../modules/BMI088/bmi088_regNdef.h#L129) | `BMI088_GYRO_BANDWIDTH_MUST_Set` | `0x80` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:130](../modules/BMI088/bmi088_regNdef.h#L130) | `BMI088_GYRO_2000_532_HZ` | `0x00` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:131](../modules/BMI088/bmi088_regNdef.h#L131) | `BMI088_GYRO_2000_230_HZ` | `0x01` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:132](../modules/BMI088/bmi088_regNdef.h#L132) | `BMI088_GYRO_1000_116_HZ` | `0x02` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:133](../modules/BMI088/bmi088_regNdef.h#L133) | `BMI088_GYRO_400_47_HZ` | `0x03` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:134](../modules/BMI088/bmi088_regNdef.h#L134) | `BMI088_GYRO_200_23_HZ` | `0x04` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:135](../modules/BMI088/bmi088_regNdef.h#L135) | `BMI088_GYRO_100_12_HZ` | `0x05` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:136](../modules/BMI088/bmi088_regNdef.h#L136) | `BMI088_GYRO_200_64_HZ` | `0x06` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:137](../modules/BMI088/bmi088_regNdef.h#L137) | `BMI088_GYRO_100_32_HZ` | `0x07` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:139](../modules/BMI088/bmi088_regNdef.h#L139) | `BMI088_GYRO_LPM1` | `0x11` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:140](../modules/BMI088/bmi088_regNdef.h#L140) | `BMI088_GYRO_NORMAL_MODE` | `0x00` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:141](../modules/BMI088/bmi088_regNdef.h#L141) | `BMI088_GYRO_SUSPEND_MODE` | `0x80` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:142](../modules/BMI088/bmi088_regNdef.h#L142) | `BMI088_GYRO_DEEP_SUSPEND_MODE` | `0x20` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:144](../modules/BMI088/bmi088_regNdef.h#L144) | `BMI088_GYRO_SOFTRESET` | `0x14` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:145](../modules/BMI088/bmi088_regNdef.h#L145) | `BMI088_GYRO_SOFTRESET_VALUE` | `0xB6` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:147](../modules/BMI088/bmi088_regNdef.h#L147) | `BMI088_GYRO_CTRL` | `0x15` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:148](../modules/BMI088/bmi088_regNdef.h#L148) | `BMI088_DRDY_OFF` | `0x00` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:149](../modules/BMI088/bmi088_regNdef.h#L149) | `BMI088_DRDY_ON` | `0x80` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:151](../modules/BMI088/bmi088_regNdef.h#L151) | `BMI088_GYRO_INT3_INT4_IO_CONF` | `0x16` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:152](../modules/BMI088/bmi088_regNdef.h#L152) | `BMI088_GYRO_INT4_GPIO_MODE_SHFITS` | `0x3` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:153](../modules/BMI088/bmi088_regNdef.h#L153) | `BMI088_GYRO_INT4_GPIO_PP` | `(0x0 << BMI088_GYRO_INT4_GPIO_MODE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:154](../modules/BMI088/bmi088_regNdef.h#L154) | `BMI088_GYRO_INT4_GPIO_OD` | `(0x1 << BMI088_GYRO_INT4_GPIO_MODE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:155](../modules/BMI088/bmi088_regNdef.h#L155) | `BMI088_GYRO_INT4_GPIO_LVL_SHFITS` | `0x2` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:156](../modules/BMI088/bmi088_regNdef.h#L156) | `BMI088_GYRO_INT4_GPIO_LOW` | `(0x0 << BMI088_GYRO_INT4_GPIO_LVL_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:157](../modules/BMI088/bmi088_regNdef.h#L157) | `BMI088_GYRO_INT4_GPIO_HIGH` | `(0x1 << BMI088_GYRO_INT4_GPIO_LVL_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:158](../modules/BMI088/bmi088_regNdef.h#L158) | `BMI088_GYRO_INT3_GPIO_MODE_SHFITS` | `0x1` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:159](../modules/BMI088/bmi088_regNdef.h#L159) | `BMI088_GYRO_INT3_GPIO_PP` | `(0x0 << BMI088_GYRO_INT3_GPIO_MODE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:160](../modules/BMI088/bmi088_regNdef.h#L160) | `BMI088_GYRO_INT3_GPIO_OD` | `(0x1 << BMI088_GYRO_INT3_GPIO_MODE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:161](../modules/BMI088/bmi088_regNdef.h#L161) | `BMI088_GYRO_INT3_GPIO_LVL_SHFITS` | `0x0` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:162](../modules/BMI088/bmi088_regNdef.h#L162) | `BMI088_GYRO_INT3_GPIO_LOW` | `(0x0 << BMI088_GYRO_INT3_GPIO_LVL_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:163](../modules/BMI088/bmi088_regNdef.h#L163) | `BMI088_GYRO_INT3_GPIO_HIGH` | `(0x1 << BMI088_GYRO_INT3_GPIO_LVL_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:165](../modules/BMI088/bmi088_regNdef.h#L165) | `BMI088_GYRO_INT3_INT4_IO_MAP` | `0x18` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:167](../modules/BMI088/bmi088_regNdef.h#L167) | `BMI088_GYRO_DRDY_IO_OFF` | `0x00` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:168](../modules/BMI088/bmi088_regNdef.h#L168) | `BMI088_GYRO_DRDY_IO_INT3` | `0x01` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:169](../modules/BMI088/bmi088_regNdef.h#L169) | `BMI088_GYRO_DRDY_IO_INT4` | `0x80` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:170](../modules/BMI088/bmi088_regNdef.h#L170) | `BMI088_GYRO_DRDY_IO_BOTH` | `(BMI088_GYRO_DRDY_IO_INT3 \| BMI088_GYRO_DRDY_IO_INT4)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:172](../modules/BMI088/bmi088_regNdef.h#L172) | `BMI088_GYRO_SELF_TEST` | `0x3C` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:173](../modules/BMI088/bmi088_regNdef.h#L173) | `BMI088_GYRO_RATE_OK_SHFITS` | `0x4` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:174](../modules/BMI088/bmi088_regNdef.h#L174) | `BMI088_GYRO_RATE_OK` | `(0x1 << BMI088_GYRO_RATE_OK_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:175](../modules/BMI088/bmi088_regNdef.h#L175) | `BMI088_GYRO_BIST_FAIL_SHFITS` | `0x2` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:176](../modules/BMI088/bmi088_regNdef.h#L176) | `BMI088_GYRO_BIST_FAIL` | `(0x1 << BMI088_GYRO_BIST_FAIL_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:177](../modules/BMI088/bmi088_regNdef.h#L177) | `BMI088_GYRO_BIST_RDY_SHFITS` | `0x1` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:178](../modules/BMI088/bmi088_regNdef.h#L178) | `BMI088_GYRO_BIST_RDY` | `(0x1 << BMI088_GYRO_BIST_RDY_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:179](../modules/BMI088/bmi088_regNdef.h#L179) | `BMI088_GYRO_TRIG_BIST_SHFITS` | `0x0` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:180](../modules/BMI088/bmi088_regNdef.h#L180) | `BMI088_GYRO_TRIG_BIST` | `(0x1 << BMI088_GYRO_TRIG_BIST_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:184](../modules/BMI088/bmi088_regNdef.h#L184) | `BMI088_TEMP_FACTOR` | `0.125f` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:185](../modules/BMI088/bmi088_regNdef.h#L185) | `BMI088_TEMP_OFFSET` | `23.0f` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:187](../modules/BMI088/bmi088_regNdef.h#L187) | `BMI088_WRITE_ACCEL_REG_NUM` | `6` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:188](../modules/BMI088/bmi088_regNdef.h#L188) | `BMI088_WRITE_GYRO_REG_NUM` | `6` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:190](../modules/BMI088/bmi088_regNdef.h#L190) | `BMI088_GYRO_DATA_READY_BIT` | `0` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:191](../modules/BMI088/bmi088_regNdef.h#L191) | `BMI088_ACCEL_DATA_READY_BIT` | `1` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:192](../modules/BMI088/bmi088_regNdef.h#L192) | `BMI088_ACCEL_TEMP_DATA_READY_BIT` | `2` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:194](../modules/BMI088/bmi088_regNdef.h#L194) | `BMI088_LONG_DELAY_TIME` | `80` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:195](../modules/BMI088/bmi088_regNdef.h#L195) | `BMI088_COM_WAIT_SENSOR_TIME` | `150` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:197](../modules/BMI088/bmi088_regNdef.h#L197) | `BMI088_ACCEL_IIC_ADDRESSE` | `(0x18 << 1)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:198](../modules/BMI088/bmi088_regNdef.h#L198) | `BMI088_GYRO_IIC_ADDRESSE` | `(0x68 << 1)` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:200](../modules/BMI088/bmi088_regNdef.h#L200) | `BMI088_ACCEL_3G_SEN` | `0.0008974358974f` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:201](../modules/BMI088/bmi088_regNdef.h#L201) | `BMI088_ACCEL_6G_SEN` | `0.00179443359375f` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:202](../modules/BMI088/bmi088_regNdef.h#L202) | `BMI088_ACCEL_12G_SEN` | `0.0035888671875f` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:203](../modules/BMI088/bmi088_regNdef.h#L203) | `BMI088_ACCEL_24G_SEN` | `0.007177734375f` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:205](../modules/BMI088/bmi088_regNdef.h#L205) | `BMI088_GYRO_2000_SEN` | `0.00106526443603169529841533860381f` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:206](../modules/BMI088/bmi088_regNdef.h#L206) | `BMI088_GYRO_1000_SEN` | `0.00053263221801584764920766930190693f` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:207](../modules/BMI088/bmi088_regNdef.h#L207) | `BMI088_GYRO_500_SEN` | `0.00026631610900792382460383465095346f` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:208](../modules/BMI088/bmi088_regNdef.h#L208) | `BMI088_GYRO_250_SEN` | `0.00013315805450396191230191732547673f` | `#ifndef BMI088REG_H` |  |
| [modules/BMI088/bmi088_regNdef.h:209](../modules/BMI088/bmi088_regNdef.h#L209) | `BMI088_GYRO_125_SEN` | `0.000066579027251980956150958662738366f` | `#ifndef BMI088REG_H` |  |

## modules/BMI088/bmi088.c

源文件：[打开 modules/BMI088/bmi088.c](../modules/BMI088/bmi088.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/BMI088/bmi088.c:82](../modules/BMI088/bmi088.c#L82) | `BMI088REG` | `0` | 无外层条件 |  |
| [modules/BMI088/bmi088.c:83](../modules/BMI088/bmi088.c#L83) | `BMI088DATA` | `1` | 无外层条件 |  |
| [modules/BMI088/bmi088.c:84](../modules/BMI088/bmi088.c#L84) | `BMI088ERROR` | `2` | 无外层条件 |  |
| [modules/BMI088/bmi088.c:299](../modules/BMI088/bmi088.c#L299) | `BMI088_PRE_CALI_ACC_X_OFFSET` | `0.0f` | 无外层条件 |  |
| [modules/BMI088/bmi088.c:300](../modules/BMI088/bmi088.c#L300) | `BMI088_PRE_CALI_ACC_Y_OFFSET` | `0.0f` | 无外层条件 |  |
| [modules/BMI088/bmi088.c:301](../modules/BMI088/bmi088.c#L301) | `BMI088_PRE_CALI_ACC_Z_OFFSET` | `0.0f` | 无外层条件 |  |
| [modules/BMI088/bmi088.c:302](../modules/BMI088/bmi088.c#L302) | `BMI088_PRE_CALI_G_NORM` | `9.805f` | 无外层条件 |  |

## modules/BMI088/bmi088.h

源文件：[打开 modules/BMI088/bmi088.h](../modules/BMI088/bmi088.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/BMI088/bmi088.h:2](../modules/BMI088/bmi088.h#L2) | `__BMI088_H__` | 空定义 | `#ifndef __BMI088_H__` |  |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[modules/BMI088/bmi088.h:80](../modules/BMI088/bmi088.h#L80)

```c
typedef struct
{
    BMI088_Work_Mode_e work_mode;
    BMI088_Calibrate_Mode_e cali_mode;
    SPI_Init_Config_s spi_gyro_config;
    SPI_Init_Config_s spi_acc_config;
    GPIO_Init_Config_s gyro_int_config;
    GPIO_Init_Config_s acc_int_config;
    PID_Init_Config_s heat_pid_config;
    PWM_Init_Config_s heat_pwm_config;
} BMI088_Init_Config_s;
```

## modules/can_comm/can_comm.c

源文件：[打开 modules/can_comm/can_comm.c](../modules/can_comm/can_comm.c)。

### 配置结构体初值

以下记录原始定义，不自动推断运行路径或后续覆盖。未填字段为零；指针/回调表示绑定关系。

[modules/can_comm/can_comm.c:97](../modules/can_comm/can_comm.c#L97)

```c
Daemon_Init_Config_s daemon_config = {
        .callback = CANCommLostCallback,
        .owner_id = (void *)ins,
        .reload_count = comm_config->daemon_count,
    };
```

## modules/can_comm/can_comm.h

源文件：[打开 modules/can_comm/can_comm.h](../modules/can_comm/can_comm.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/can_comm/can_comm.h:12](../modules/can_comm/can_comm.h#L12) | `CAN_COMM_H` | 空定义 | `#ifndef CAN_COMM_H` |  |
| [modules/can_comm/can_comm.h:17](../modules/can_comm/can_comm.h#L17) | `MX_CAN_COMM_COUNT` | `4` | `#ifndef CAN_COMM_H` | 注意均衡负载,一条总线上不要挂载过多的外设 |
| [modules/can_comm/can_comm.h:19](../modules/can_comm/can_comm.h#L19) | `CAN_COMM_MAX_BUFFSIZE` | `60` | `#ifndef CAN_COMM_H` | 最大发送/接收字节数,如果不够可以增加此数值 |
| [modules/can_comm/can_comm.h:20](../modules/can_comm/can_comm.h#L20) | `CAN_COMM_HEADER` | `'s'` | `#ifndef CAN_COMM_H` | 帧头 |
| [modules/can_comm/can_comm.h:21](../modules/can_comm/can_comm.h#L21) | `CAN_COMM_TAIL` | `'e'` | `#ifndef CAN_COMM_H` | 帧尾 |
| [modules/can_comm/can_comm.h:22](../modules/can_comm/can_comm.h#L22) | `CAN_COMM_OFFSET_BYTES` | `4` | `#ifndef CAN_COMM_H` | 's'+ datalen + 'e' + crc8 |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[modules/can_comm/can_comm.h:48](../modules/can_comm/can_comm.h#L48)

```c
typedef struct
{
    CAN_Init_Config_s can_config; // CAN初始化结构体
    uint8_t send_data_len;        // 发送数据长度
    uint8_t recv_data_len;        // 接收数据长度

    uint16_t daemon_count; // 守护进程计数,用于初始化守护进程
} CANComm_Init_Config_s;
```

## modules/daemon/daemon.h

源文件：[打开 modules/daemon/daemon.h](../modules/daemon/daemon.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/daemon/daemon.h:2](../modules/daemon/daemon.h#L2) | `MONITOR_H` | 空定义 | `#ifndef MONITOR_H` |  |
| [modules/daemon/daemon.h:7](../modules/daemon/daemon.h#L7) | `DAEMON_MX_CNT` | `64` | `#ifndef MONITOR_H` |  |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[modules/daemon/daemon.h:23](../modules/daemon/daemon.h#L23)

```c
typedef struct
{
    uint16_t reload_count;     // 实际上这是app唯一需要设置的值?
    uint16_t init_count;       // 上线等待时间,有些模块需要收到主控的指令才会反馈报文,或pc等需要开机时间
    offline_callback callback; // 异常处理函数,当模块发生异常时会被调用

    void *owner_id;            // id取拥有daemon的实例的地址,如DJIMotorInstance*,cast成void*类型
} Daemon_Init_Config_s;
```

## modules/general_def.h

源文件：[打开 modules/general_def.h](../modules/general_def.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/general_def.h:2](../modules/general_def.h#L2) | `GENERAL_DEF_H` | 空定义 | `#ifndef GENERAL_DEF_H` |  |
| [modules/general_def.h:7](../modules/general_def.h#L7) | `PI` | `3.1415926535f` | `#ifndef GENERAL_DEF_H；#ifndef PI` |  |
| [modules/general_def.h:9](../modules/general_def.h#L9) | `PI2` | `(PI * 2.0f)` | `#ifndef GENERAL_DEF_H` | 2 pi |
| [modules/general_def.h:11](../modules/general_def.h#L11) | `RAD_2_DEGREE` | `57.2957795f` | `#ifndef GENERAL_DEF_H` | 180/pi |
| [modules/general_def.h:12](../modules/general_def.h#L12) | `DEGREE_2_RAD` | `0.01745329252f` | `#ifndef GENERAL_DEF_H` | pi/180 |
| [modules/general_def.h:14](../modules/general_def.h#L14) | `RPM_2_ANGLE_PER_SEC` | `6.0f` | `#ifndef GENERAL_DEF_H` | ×360°/60sec |
| [modules/general_def.h:15](../modules/general_def.h#L15) | `RPM_2_RAD_PER_SEC` | `0.104719755f` | `#ifndef GENERAL_DEF_H` | ×2pi/60sec |

## modules/imu/BMI088driver.c

源文件：[打开 modules/imu/BMI088driver.c](../modules/imu/BMI088driver.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/imu/BMI088driver.c:25](../modules/imu/BMI088driver.c#L25) | `BMI088_accel_write_single_reg(reg, data)` | `{ BMI088_ACCEL_NS_L(); BMI088_write_single_reg((reg), (data)); BMI088_ACCEL_NS_H(); }` | `#if defined(BMI088_USE_SPI)` |  |
| [modules/imu/BMI088driver.c:31](../modules/imu/BMI088driver.c#L31) | `BMI088_accel_read_single_reg(reg, data)` | `{ BMI088_ACCEL_NS_L(); BMI088_read_write_byte((reg) \| 0x80); BMI088_read_write_byte(0x55); (data) = BMI088_read_write_byte(0x55); BMI088_ACCEL_NS_H(); }` | `#if defined(BMI088_USE_SPI)` |  |
| [modules/imu/BMI088driver.c:39](../modules/imu/BMI088driver.c#L39) | `BMI088_accel_read_muli_reg(reg, data, len)` | `{ BMI088_ACCEL_NS_L(); BMI088_read_write_byte((reg) \| 0x80); BMI088_read_muli_reg(reg, data, len); BMI088_ACCEL_NS_H(); }` | `#if defined(BMI088_USE_SPI)` |  |
| [modules/imu/BMI088driver.c:47](../modules/imu/BMI088driver.c#L47) | `BMI088_gyro_write_single_reg(reg, data)` | `{ BMI088_GYRO_NS_L(); BMI088_write_single_reg((reg), (data)); BMI088_GYRO_NS_H(); }` | `#if defined(BMI088_USE_SPI)` |  |
| [modules/imu/BMI088driver.c:53](../modules/imu/BMI088driver.c#L53) | `BMI088_gyro_read_single_reg(reg, data)` | `{ BMI088_GYRO_NS_L(); BMI088_read_single_reg((reg), &(data)); BMI088_GYRO_NS_H(); }` | `#if defined(BMI088_USE_SPI)` |  |
| [modules/imu/BMI088driver.c:59](../modules/imu/BMI088driver.c#L59) | `BMI088_gyro_read_muli_reg(reg, data, len)` | `{ BMI088_GYRO_NS_L(); BMI088_read_muli_reg((reg), (data), (len)); BMI088_GYRO_NS_H(); }` | `#if defined(BMI088_USE_SPI)` |  |

## modules/imu/BMI088driver.h

源文件：[打开 modules/imu/BMI088driver.h](../modules/imu/BMI088driver.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/imu/BMI088driver.h:2](../modules/imu/BMI088driver.h#L2) | `BMI088DRIVER_H` | 空定义 | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:7](../modules/imu/BMI088driver.h#L7) | `BMI088_TEMP_FACTOR` | `0.125f` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:8](../modules/imu/BMI088driver.h#L8) | `BMI088_TEMP_OFFSET` | `23.0f` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:10](../modules/imu/BMI088driver.h#L10) | `BMI088_WRITE_ACCEL_REG_NUM` | `6` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:11](../modules/imu/BMI088driver.h#L11) | `BMI088_WRITE_GYRO_REG_NUM` | `6` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:13](../modules/imu/BMI088driver.h#L13) | `BMI088_GYRO_DATA_READY_BIT` | `0` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:14](../modules/imu/BMI088driver.h#L14) | `BMI088_ACCEL_DATA_READY_BIT` | `1` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:15](../modules/imu/BMI088driver.h#L15) | `BMI088_ACCEL_TEMP_DATA_READY_BIT` | `2` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:17](../modules/imu/BMI088driver.h#L17) | `BMI088_LONG_DELAY_TIME` | `80` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:18](../modules/imu/BMI088driver.h#L18) | `BMI088_COM_WAIT_SENSOR_TIME` | `150` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:20](../modules/imu/BMI088driver.h#L20) | `BMI088_ACCEL_IIC_ADDRESSE` | `(0x18 << 1)` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:21](../modules/imu/BMI088driver.h#L21) | `BMI088_GYRO_IIC_ADDRESSE` | `(0x68 << 1)` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:23](../modules/imu/BMI088driver.h#L23) | `BMI088_ACCEL_3G_SEN` | `0.0008974358974f` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:24](../modules/imu/BMI088driver.h#L24) | `BMI088_ACCEL_6G_SEN` | `0.00179443359375f` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:25](../modules/imu/BMI088driver.h#L25) | `BMI088_ACCEL_12G_SEN` | `0.0035888671875f` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:26](../modules/imu/BMI088driver.h#L26) | `BMI088_ACCEL_24G_SEN` | `0.007177734375f` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:28](../modules/imu/BMI088driver.h#L28) | `BMI088_GYRO_2000_SEN` | `0.00106526443603169529841533860381f` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:29](../modules/imu/BMI088driver.h#L29) | `BMI088_GYRO_1000_SEN` | `0.00053263221801584764920766930190693f` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:30](../modules/imu/BMI088driver.h#L30) | `BMI088_GYRO_500_SEN` | `0.00026631610900792382460383465095346f` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:31](../modules/imu/BMI088driver.h#L31) | `BMI088_GYRO_250_SEN` | `0.00013315805450396191230191732547673f` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:32](../modules/imu/BMI088driver.h#L32) | `BMI088_GYRO_125_SEN` | `0.000066579027251980956150958662738366f` | `#ifndef BMI088DRIVER_H` |  |
| [modules/imu/BMI088driver.h:36](../modules/imu/BMI088driver.h#L36) | `GxOFFSET` | `0.00247530174f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0` |  |
| [modules/imu/BMI088driver.h:37](../modules/imu/BMI088driver.h#L37) | `GyOFFSET` | `0.000393082853f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0` |  |
| [modules/imu/BMI088driver.h:38](../modules/imu/BMI088driver.h#L38) | `GzOFFSET` | `0.000393082853f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0` |  |
| [modules/imu/BMI088driver.h:39](../modules/imu/BMI088driver.h#L39) | `gNORM` | `9.69293118f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0` |  |
| [modules/imu/BMI088driver.h:41](../modules/imu/BMI088driver.h#L41) | `GxOFFSET` | `0.0007222f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0 → #elif INFANTRY_ID == 1` |  |
| [modules/imu/BMI088driver.h:42](../modules/imu/BMI088driver.h#L42) | `GyOFFSET` | `-0.001786f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0 → #elif INFANTRY_ID == 1` |  |
| [modules/imu/BMI088driver.h:43](../modules/imu/BMI088driver.h#L43) | `GzOFFSET` | `0.0004346f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0 → #elif INFANTRY_ID == 1` |  |
| [modules/imu/BMI088driver.h:44](../modules/imu/BMI088driver.h#L44) | `gNORM` | `9.876785f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0 → #elif INFANTRY_ID == 1` |  |
| [modules/imu/BMI088driver.h:46](../modules/imu/BMI088driver.h#L46) | `GxOFFSET` | `0.0007222f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0 → #elif INFANTRY_ID == 1 → #elif INFANTRY_ID == 2` |  |
| [modules/imu/BMI088driver.h:47](../modules/imu/BMI088driver.h#L47) | `GyOFFSET` | `-0.001786f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0 → #elif INFANTRY_ID == 1 → #elif INFANTRY_ID == 2` |  |
| [modules/imu/BMI088driver.h:48](../modules/imu/BMI088driver.h#L48) | `GzOFFSET` | `0.0004346f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0 → #elif INFANTRY_ID == 1 → #elif INFANTRY_ID == 2` |  |
| [modules/imu/BMI088driver.h:49](../modules/imu/BMI088driver.h#L49) | `gNORM` | `9.876785f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0 → #elif INFANTRY_ID == 1 → #elif INFANTRY_ID == 2` |  |
| [modules/imu/BMI088driver.h:51](../modules/imu/BMI088driver.h#L51) | `GxOFFSET` | `0.00270364084f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0 → #elif INFANTRY_ID == 1 → #elif INFANTRY_ID == 2 → #elif INFANTRY_ID == 3` |  |
| [modules/imu/BMI088driver.h:52](../modules/imu/BMI088driver.h#L52) | `GyOFFSET` | `-0.000532632112f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0 → #elif INFANTRY_ID == 1 → #elif INFANTRY_ID == 2 → #elif INFANTRY_ID == 3` |  |
| [modules/imu/BMI088driver.h:53](../modules/imu/BMI088driver.h#L53) | `GzOFFSET` | `0.00478090625f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0 → #elif INFANTRY_ID == 1 → #elif INFANTRY_ID == 2 → #elif INFANTRY_ID == 3` |  |
| [modules/imu/BMI088driver.h:54](../modules/imu/BMI088driver.h#L54) | `gNORM` | `9.73574924f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0 → #elif INFANTRY_ID == 1 → #elif INFANTRY_ID == 2 → #elif INFANTRY_ID == 3` |  |
| [modules/imu/BMI088driver.h:56](../modules/imu/BMI088driver.h#L56) | `GxOFFSET` | `0.0007222f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0 → #elif INFANTRY_ID == 1 → #elif INFANTRY_ID == 2 → #elif INFANTRY_ID == 3 → #elif INFANTRY_ID == 4` |  |
| [modules/imu/BMI088driver.h:57](../modules/imu/BMI088driver.h#L57) | `GyOFFSET` | `-0.001786f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0 → #elif INFANTRY_ID == 1 → #elif INFANTRY_ID == 2 → #elif INFANTRY_ID == 3 → #elif INFANTRY_ID == 4` |  |
| [modules/imu/BMI088driver.h:58](../modules/imu/BMI088driver.h#L58) | `GzOFFSET` | `0.0004346f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0 → #elif INFANTRY_ID == 1 → #elif INFANTRY_ID == 2 → #elif INFANTRY_ID == 3 → #elif INFANTRY_ID == 4` |  |
| [modules/imu/BMI088driver.h:59](../modules/imu/BMI088driver.h#L59) | `gNORM` | `9.876785f` | `#ifndef BMI088DRIVER_H；#if INFANTRY_ID == 0 → #elif INFANTRY_ID == 1 → #elif INFANTRY_ID == 2 → #elif INFANTRY_ID == 3 → #elif INFANTRY_ID == 4` |  |

## modules/imu/BMI088Middleware.h

源文件：[打开 modules/imu/BMI088Middleware.h](../modules/imu/BMI088Middleware.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/imu/BMI088Middleware.h:2](../modules/imu/BMI088Middleware.h#L2) | `BMI088MIDDLEWARE_H` | 空定义 | `#ifndef BMI088MIDDLEWARE_H` |  |
| [modules/imu/BMI088Middleware.h:6](../modules/imu/BMI088Middleware.h#L6) | `BMI088_USE_SPI` | 空定义 | `#ifndef BMI088MIDDLEWARE_H` |  |

### 已注释的宏选项（当前不定义）

| 行 | 原选项 |
| --- | --- |
| [modules/imu/BMI088Middleware.h:7](../modules/imu/BMI088Middleware.h#L7) | `#define BMI088_USE_IIC` |
| [modules/imu/BMI088Middleware.h:10](../modules/imu/BMI088Middleware.h#L10) | `#define CS1_ACCEL_GPIO_Port ACCEL_NSS_GPIO_Port` |
| [modules/imu/BMI088Middleware.h:11](../modules/imu/BMI088Middleware.h#L11) | `#define CS1_ACCEL_Pin ACCEL_NSS_Pin` |
| [modules/imu/BMI088Middleware.h:12](../modules/imu/BMI088Middleware.h#L12) | `#define CS1_GYRO_GPIO_Port GYRO_NSS_GPIO_Port` |
| [modules/imu/BMI088Middleware.h:13](../modules/imu/BMI088Middleware.h#L13) | `#define CS1_GYRO_Pin GYRO_NSS_Pin` |

## modules/imu/BMI088reg.h

源文件：[打开 modules/imu/BMI088reg.h](../modules/imu/BMI088reg.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/imu/BMI088reg.h:2](../modules/imu/BMI088reg.h#L2) | `BMI088REG_H` | 空定义 | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:4](../modules/imu/BMI088reg.h#L4) | `BMI088_ACC_CHIP_ID` | `0x00` | `#ifndef BMI088REG_H` | the register is  " Who am I " |
| [modules/imu/BMI088reg.h:5](../modules/imu/BMI088reg.h#L5) | `BMI088_ACC_CHIP_ID_VALUE` | `0x1E` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:7](../modules/imu/BMI088reg.h#L7) | `BMI088_ACC_ERR_REG` | `0x02` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:8](../modules/imu/BMI088reg.h#L8) | `BMI088_ACCEL_CONGIF_ERROR_SHFITS` | `0x2` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:9](../modules/imu/BMI088reg.h#L9) | `BMI088_ACCEL_CONGIF_ERROR` | `(1 << BMI088_ACCEL_CONGIF_ERROR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:10](../modules/imu/BMI088reg.h#L10) | `BMI088_FATAL_ERROR_SHFITS` | `0x0` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:11](../modules/imu/BMI088reg.h#L11) | `BMI088_FATAL_ERROR` | `(1 << BMI088_FATAL_ERROR)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:13](../modules/imu/BMI088reg.h#L13) | `BMI088_ACC_STATUS` | `0x03` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:14](../modules/imu/BMI088reg.h#L14) | `BMI088_ACCEL_DRDY_SHFITS` | `0x7` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:15](../modules/imu/BMI088reg.h#L15) | `BMI088_ACCEL_DRDY` | `(1 << BMI088_ACCEL_DRDY_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:17](../modules/imu/BMI088reg.h#L17) | `BMI088_ACCEL_XOUT_L` | `0x12` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:18](../modules/imu/BMI088reg.h#L18) | `BMI088_ACCEL_XOUT_M` | `0x13` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:19](../modules/imu/BMI088reg.h#L19) | `BMI088_ACCEL_YOUT_L` | `0x14` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:20](../modules/imu/BMI088reg.h#L20) | `BMI088_ACCEL_YOUT_M` | `0x15` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:21](../modules/imu/BMI088reg.h#L21) | `BMI088_ACCEL_ZOUT_L` | `0x16` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:22](../modules/imu/BMI088reg.h#L22) | `BMI088_ACCEL_ZOUT_M` | `0x17` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:24](../modules/imu/BMI088reg.h#L24) | `BMI088_SENSORTIME_DATA_L` | `0x18` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:25](../modules/imu/BMI088reg.h#L25) | `BMI088_SENSORTIME_DATA_M` | `0x19` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:26](../modules/imu/BMI088reg.h#L26) | `BMI088_SENSORTIME_DATA_H` | `0x1A` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:28](../modules/imu/BMI088reg.h#L28) | `BMI088_ACC_INT_STAT_1` | `0x1D` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:29](../modules/imu/BMI088reg.h#L29) | `BMI088_ACCEL_DRDY_INTERRUPT_SHFITS` | `0x7` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:30](../modules/imu/BMI088reg.h#L30) | `BMI088_ACCEL_DRDY_INTERRUPT` | `(1 << BMI088_ACCEL_DRDY_INTERRUPT_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:32](../modules/imu/BMI088reg.h#L32) | `BMI088_TEMP_M` | `0x22` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:34](../modules/imu/BMI088reg.h#L34) | `BMI088_TEMP_L` | `0x23` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:36](../modules/imu/BMI088reg.h#L36) | `BMI088_ACC_CONF` | `0x40` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:37](../modules/imu/BMI088reg.h#L37) | `BMI088_ACC_CONF_MUST_Set` | `0x80` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:38](../modules/imu/BMI088reg.h#L38) | `BMI088_ACC_BWP_SHFITS` | `0x4` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:39](../modules/imu/BMI088reg.h#L39) | `BMI088_ACC_OSR4` | `(0x0 << BMI088_ACC_BWP_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:40](../modules/imu/BMI088reg.h#L40) | `BMI088_ACC_OSR2` | `(0x1 << BMI088_ACC_BWP_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:41](../modules/imu/BMI088reg.h#L41) | `BMI088_ACC_NORMAL` | `(0x2 << BMI088_ACC_BWP_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:43](../modules/imu/BMI088reg.h#L43) | `BMI088_ACC_ODR_SHFITS` | `0x0` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:44](../modules/imu/BMI088reg.h#L44) | `BMI088_ACC_12_5_HZ` | `(0x5 << BMI088_ACC_ODR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:45](../modules/imu/BMI088reg.h#L45) | `BMI088_ACC_25_HZ` | `(0x6 << BMI088_ACC_ODR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:46](../modules/imu/BMI088reg.h#L46) | `BMI088_ACC_50_HZ` | `(0x7 << BMI088_ACC_ODR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:47](../modules/imu/BMI088reg.h#L47) | `BMI088_ACC_100_HZ` | `(0x8 << BMI088_ACC_ODR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:48](../modules/imu/BMI088reg.h#L48) | `BMI088_ACC_200_HZ` | `(0x9 << BMI088_ACC_ODR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:49](../modules/imu/BMI088reg.h#L49) | `BMI088_ACC_400_HZ` | `(0xA << BMI088_ACC_ODR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:50](../modules/imu/BMI088reg.h#L50) | `BMI088_ACC_800_HZ` | `(0xB << BMI088_ACC_ODR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:51](../modules/imu/BMI088reg.h#L51) | `BMI088_ACC_1600_HZ` | `(0xC << BMI088_ACC_ODR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:53](../modules/imu/BMI088reg.h#L53) | `BMI088_ACC_RANGE` | `0x41` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:55](../modules/imu/BMI088reg.h#L55) | `BMI088_ACC_RANGE_SHFITS` | `0x0` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:56](../modules/imu/BMI088reg.h#L56) | `BMI088_ACC_RANGE_3G` | `(0x0 << BMI088_ACC_RANGE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:57](../modules/imu/BMI088reg.h#L57) | `BMI088_ACC_RANGE_6G` | `(0x1 << BMI088_ACC_RANGE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:58](../modules/imu/BMI088reg.h#L58) | `BMI088_ACC_RANGE_12G` | `(0x2 << BMI088_ACC_RANGE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:59](../modules/imu/BMI088reg.h#L59) | `BMI088_ACC_RANGE_24G` | `(0x3 << BMI088_ACC_RANGE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:61](../modules/imu/BMI088reg.h#L61) | `BMI088_INT1_IO_CTRL` | `0x53` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:62](../modules/imu/BMI088reg.h#L62) | `BMI088_ACC_INT1_IO_ENABLE_SHFITS` | `0x3` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:63](../modules/imu/BMI088reg.h#L63) | `BMI088_ACC_INT1_IO_ENABLE` | `(0x1 << BMI088_ACC_INT1_IO_ENABLE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:64](../modules/imu/BMI088reg.h#L64) | `BMI088_ACC_INT1_GPIO_MODE_SHFITS` | `0x2` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:65](../modules/imu/BMI088reg.h#L65) | `BMI088_ACC_INT1_GPIO_PP` | `(0x0 << BMI088_ACC_INT1_GPIO_MODE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:66](../modules/imu/BMI088reg.h#L66) | `BMI088_ACC_INT1_GPIO_OD` | `(0x1 << BMI088_ACC_INT1_GPIO_MODE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:67](../modules/imu/BMI088reg.h#L67) | `BMI088_ACC_INT1_GPIO_LVL_SHFITS` | `0x1` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:68](../modules/imu/BMI088reg.h#L68) | `BMI088_ACC_INT1_GPIO_LOW` | `(0x0 << BMI088_ACC_INT1_GPIO_LVL_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:69](../modules/imu/BMI088reg.h#L69) | `BMI088_ACC_INT1_GPIO_HIGH` | `(0x1 << BMI088_ACC_INT1_GPIO_LVL_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:71](../modules/imu/BMI088reg.h#L71) | `BMI088_INT2_IO_CTRL` | `0x54` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:72](../modules/imu/BMI088reg.h#L72) | `BMI088_ACC_INT2_IO_ENABLE_SHFITS` | `0x3` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:73](../modules/imu/BMI088reg.h#L73) | `BMI088_ACC_INT2_IO_ENABLE` | `(0x1 << BMI088_ACC_INT2_IO_ENABLE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:74](../modules/imu/BMI088reg.h#L74) | `BMI088_ACC_INT2_GPIO_MODE_SHFITS` | `0x2` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:75](../modules/imu/BMI088reg.h#L75) | `BMI088_ACC_INT2_GPIO_PP` | `(0x0 << BMI088_ACC_INT2_GPIO_MODE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:76](../modules/imu/BMI088reg.h#L76) | `BMI088_ACC_INT2_GPIO_OD` | `(0x1 << BMI088_ACC_INT2_GPIO_MODE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:77](../modules/imu/BMI088reg.h#L77) | `BMI088_ACC_INT2_GPIO_LVL_SHFITS` | `0x1` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:78](../modules/imu/BMI088reg.h#L78) | `BMI088_ACC_INT2_GPIO_LOW` | `(0x0 << BMI088_ACC_INT2_GPIO_LVL_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:79](../modules/imu/BMI088reg.h#L79) | `BMI088_ACC_INT2_GPIO_HIGH` | `(0x1 << BMI088_ACC_INT2_GPIO_LVL_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:81](../modules/imu/BMI088reg.h#L81) | `BMI088_INT_MAP_DATA` | `0x58` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:82](../modules/imu/BMI088reg.h#L82) | `BMI088_ACC_INT2_DRDY_INTERRUPT_SHFITS` | `0x6` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:83](../modules/imu/BMI088reg.h#L83) | `BMI088_ACC_INT2_DRDY_INTERRUPT` | `(0x1 << BMI088_ACC_INT2_DRDY_INTERRUPT_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:84](../modules/imu/BMI088reg.h#L84) | `BMI088_ACC_INT1_DRDY_INTERRUPT_SHFITS` | `0x2` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:85](../modules/imu/BMI088reg.h#L85) | `BMI088_ACC_INT1_DRDY_INTERRUPT` | `(0x1 << BMI088_ACC_INT1_DRDY_INTERRUPT_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:87](../modules/imu/BMI088reg.h#L87) | `BMI088_ACC_SELF_TEST` | `0x6D` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:88](../modules/imu/BMI088reg.h#L88) | `BMI088_ACC_SELF_TEST_OFF` | `0x00` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:89](../modules/imu/BMI088reg.h#L89) | `BMI088_ACC_SELF_TEST_POSITIVE_SIGNAL` | `0x0D` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:90](../modules/imu/BMI088reg.h#L90) | `BMI088_ACC_SELF_TEST_NEGATIVE_SIGNAL` | `0x09` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:92](../modules/imu/BMI088reg.h#L92) | `BMI088_ACC_PWR_CONF` | `0x7C` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:93](../modules/imu/BMI088reg.h#L93) | `BMI088_ACC_PWR_SUSPEND_MODE` | `0x03` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:94](../modules/imu/BMI088reg.h#L94) | `BMI088_ACC_PWR_ACTIVE_MODE` | `0x00` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:96](../modules/imu/BMI088reg.h#L96) | `BMI088_ACC_PWR_CTRL` | `0x7D` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:97](../modules/imu/BMI088reg.h#L97) | `BMI088_ACC_ENABLE_ACC_OFF` | `0x00` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:98](../modules/imu/BMI088reg.h#L98) | `BMI088_ACC_ENABLE_ACC_ON` | `0x04` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:100](../modules/imu/BMI088reg.h#L100) | `BMI088_ACC_SOFTRESET` | `0x7E` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:101](../modules/imu/BMI088reg.h#L101) | `BMI088_ACC_SOFTRESET_VALUE` | `0xB6` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:103](../modules/imu/BMI088reg.h#L103) | `BMI088_GYRO_CHIP_ID` | `0x00` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:104](../modules/imu/BMI088reg.h#L104) | `BMI088_GYRO_CHIP_ID_VALUE` | `0x0F` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:106](../modules/imu/BMI088reg.h#L106) | `BMI088_GYRO_X_L` | `0x02` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:107](../modules/imu/BMI088reg.h#L107) | `BMI088_GYRO_X_H` | `0x03` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:108](../modules/imu/BMI088reg.h#L108) | `BMI088_GYRO_Y_L` | `0x04` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:109](../modules/imu/BMI088reg.h#L109) | `BMI088_GYRO_Y_H` | `0x05` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:110](../modules/imu/BMI088reg.h#L110) | `BMI088_GYRO_Z_L` | `0x06` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:111](../modules/imu/BMI088reg.h#L111) | `BMI088_GYRO_Z_H` | `0x07` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:113](../modules/imu/BMI088reg.h#L113) | `BMI088_GYRO_INT_STAT_1` | `0x0A` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:114](../modules/imu/BMI088reg.h#L114) | `BMI088_GYRO_DYDR_SHFITS` | `0x7` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:115](../modules/imu/BMI088reg.h#L115) | `BMI088_GYRO_DYDR` | `(0x1 << BMI088_GYRO_DYDR_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:117](../modules/imu/BMI088reg.h#L117) | `BMI088_GYRO_RANGE` | `0x0F` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:118](../modules/imu/BMI088reg.h#L118) | `BMI088_GYRO_RANGE_SHFITS` | `0x0` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:119](../modules/imu/BMI088reg.h#L119) | `BMI088_GYRO_2000` | `(0x0 << BMI088_GYRO_RANGE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:120](../modules/imu/BMI088reg.h#L120) | `BMI088_GYRO_1000` | `(0x1 << BMI088_GYRO_RANGE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:121](../modules/imu/BMI088reg.h#L121) | `BMI088_GYRO_500` | `(0x2 << BMI088_GYRO_RANGE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:122](../modules/imu/BMI088reg.h#L122) | `BMI088_GYRO_250` | `(0x3 << BMI088_GYRO_RANGE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:123](../modules/imu/BMI088reg.h#L123) | `BMI088_GYRO_125` | `(0x4 << BMI088_GYRO_RANGE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:125](../modules/imu/BMI088reg.h#L125) | `BMI088_GYRO_BANDWIDTH` | `0x10` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:127](../modules/imu/BMI088reg.h#L127) | `BMI088_GYRO_BANDWIDTH_MUST_Set` | `0x80` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:128](../modules/imu/BMI088reg.h#L128) | `BMI088_GYRO_2000_532_HZ` | `0x00` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:129](../modules/imu/BMI088reg.h#L129) | `BMI088_GYRO_2000_230_HZ` | `0x01` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:130](../modules/imu/BMI088reg.h#L130) | `BMI088_GYRO_1000_116_HZ` | `0x02` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:131](../modules/imu/BMI088reg.h#L131) | `BMI088_GYRO_400_47_HZ` | `0x03` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:132](../modules/imu/BMI088reg.h#L132) | `BMI088_GYRO_200_23_HZ` | `0x04` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:133](../modules/imu/BMI088reg.h#L133) | `BMI088_GYRO_100_12_HZ` | `0x05` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:134](../modules/imu/BMI088reg.h#L134) | `BMI088_GYRO_200_64_HZ` | `0x06` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:135](../modules/imu/BMI088reg.h#L135) | `BMI088_GYRO_100_32_HZ` | `0x07` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:137](../modules/imu/BMI088reg.h#L137) | `BMI088_GYRO_LPM1` | `0x11` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:138](../modules/imu/BMI088reg.h#L138) | `BMI088_GYRO_NORMAL_MODE` | `0x00` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:139](../modules/imu/BMI088reg.h#L139) | `BMI088_GYRO_SUSPEND_MODE` | `0x80` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:140](../modules/imu/BMI088reg.h#L140) | `BMI088_GYRO_DEEP_SUSPEND_MODE` | `0x20` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:142](../modules/imu/BMI088reg.h#L142) | `BMI088_GYRO_SOFTRESET` | `0x14` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:143](../modules/imu/BMI088reg.h#L143) | `BMI088_GYRO_SOFTRESET_VALUE` | `0xB6` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:145](../modules/imu/BMI088reg.h#L145) | `BMI088_GYRO_CTRL` | `0x15` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:146](../modules/imu/BMI088reg.h#L146) | `BMI088_DRDY_OFF` | `0x00` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:147](../modules/imu/BMI088reg.h#L147) | `BMI088_DRDY_ON` | `0x80` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:149](../modules/imu/BMI088reg.h#L149) | `BMI088_GYRO_INT3_INT4_IO_CONF` | `0x16` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:150](../modules/imu/BMI088reg.h#L150) | `BMI088_GYRO_INT4_GPIO_MODE_SHFITS` | `0x3` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:151](../modules/imu/BMI088reg.h#L151) | `BMI088_GYRO_INT4_GPIO_PP` | `(0x0 << BMI088_GYRO_INT4_GPIO_MODE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:152](../modules/imu/BMI088reg.h#L152) | `BMI088_GYRO_INT4_GPIO_OD` | `(0x1 << BMI088_GYRO_INT4_GPIO_MODE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:153](../modules/imu/BMI088reg.h#L153) | `BMI088_GYRO_INT4_GPIO_LVL_SHFITS` | `0x2` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:154](../modules/imu/BMI088reg.h#L154) | `BMI088_GYRO_INT4_GPIO_LOW` | `(0x0 << BMI088_GYRO_INT4_GPIO_LVL_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:155](../modules/imu/BMI088reg.h#L155) | `BMI088_GYRO_INT4_GPIO_HIGH` | `(0x1 << BMI088_GYRO_INT4_GPIO_LVL_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:156](../modules/imu/BMI088reg.h#L156) | `BMI088_GYRO_INT3_GPIO_MODE_SHFITS` | `0x1` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:157](../modules/imu/BMI088reg.h#L157) | `BMI088_GYRO_INT3_GPIO_PP` | `(0x0 << BMI088_GYRO_INT3_GPIO_MODE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:158](../modules/imu/BMI088reg.h#L158) | `BMI088_GYRO_INT3_GPIO_OD` | `(0x1 << BMI088_GYRO_INT3_GPIO_MODE_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:159](../modules/imu/BMI088reg.h#L159) | `BMI088_GYRO_INT3_GPIO_LVL_SHFITS` | `0x0` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:160](../modules/imu/BMI088reg.h#L160) | `BMI088_GYRO_INT3_GPIO_LOW` | `(0x0 << BMI088_GYRO_INT3_GPIO_LVL_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:161](../modules/imu/BMI088reg.h#L161) | `BMI088_GYRO_INT3_GPIO_HIGH` | `(0x1 << BMI088_GYRO_INT3_GPIO_LVL_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:163](../modules/imu/BMI088reg.h#L163) | `BMI088_GYRO_INT3_INT4_IO_MAP` | `0x18` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:165](../modules/imu/BMI088reg.h#L165) | `BMI088_GYRO_DRDY_IO_OFF` | `0x00` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:166](../modules/imu/BMI088reg.h#L166) | `BMI088_GYRO_DRDY_IO_INT3` | `0x01` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:167](../modules/imu/BMI088reg.h#L167) | `BMI088_GYRO_DRDY_IO_INT4` | `0x80` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:168](../modules/imu/BMI088reg.h#L168) | `BMI088_GYRO_DRDY_IO_BOTH` | `(BMI088_GYRO_DRDY_IO_INT3 \| BMI088_GYRO_DRDY_IO_INT4)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:170](../modules/imu/BMI088reg.h#L170) | `BMI088_GYRO_SELF_TEST` | `0x3C` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:171](../modules/imu/BMI088reg.h#L171) | `BMI088_GYRO_RATE_OK_SHFITS` | `0x4` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:172](../modules/imu/BMI088reg.h#L172) | `BMI088_GYRO_RATE_OK` | `(0x1 << BMI088_GYRO_RATE_OK_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:173](../modules/imu/BMI088reg.h#L173) | `BMI088_GYRO_BIST_FAIL_SHFITS` | `0x2` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:174](../modules/imu/BMI088reg.h#L174) | `BMI088_GYRO_BIST_FAIL` | `(0x1 << BMI088_GYRO_BIST_FAIL_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:175](../modules/imu/BMI088reg.h#L175) | `BMI088_GYRO_BIST_RDY_SHFITS` | `0x1` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:176](../modules/imu/BMI088reg.h#L176) | `BMI088_GYRO_BIST_RDY` | `(0x1 << BMI088_GYRO_BIST_RDY_SHFITS)` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:177](../modules/imu/BMI088reg.h#L177) | `BMI088_GYRO_TRIG_BIST_SHFITS` | `0x0` | `#ifndef BMI088REG_H` |  |
| [modules/imu/BMI088reg.h:178](../modules/imu/BMI088reg.h#L178) | `BMI088_GYRO_TRIG_BIST` | `(0x1 << BMI088_GYRO_TRIG_BIST_SHFITS)` | `#ifndef BMI088REG_H` |  |

## modules/imu/ins_task.c

源文件：[打开 modules/imu/ins_task.c](../modules/imu/ins_task.c)。

### 配置结构体初值

以下记录原始定义，不自动推断运行路径或后续覆盖。未填字段为零；指针/回调表示绑定关系。

[modules/imu/ins_task.c:103](../modules/imu/ins_task.c#L103)

```c
PID_Init_Config_s config = {.MaxOut = 2000,
                                .IntegralLimit = 300,
                                .DeadBand = 0,
                                .Kp = 1000,
                                .Ki = 20,
                                .Kd = 0,
                                .Improve = 0x01};
```

## modules/imu/ins_task.h

源文件：[打开 modules/imu/ins_task.h](../modules/imu/ins_task.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/imu/ins_task.h:16](../modules/imu/ins_task.h#L16) | `__INS_TASK_H` | 空定义 | `#ifndef __INS_TASK_H` |  |
| [modules/imu/ins_task.h:22](../modules/imu/ins_task.h#L22) | `X` | `0` | `#ifndef __INS_TASK_H` |  |
| [modules/imu/ins_task.h:23](../modules/imu/ins_task.h#L23) | `Y` | `1` | `#ifndef __INS_TASK_H` |  |
| [modules/imu/ins_task.h:24](../modules/imu/ins_task.h#L24) | `Z` | `2` | `#ifndef __INS_TASK_H` |  |
| [modules/imu/ins_task.h:26](../modules/imu/ins_task.h#L26) | `INS_TASK_PERIOD` | `1` | `#ifndef __INS_TASK_H` |  |

## modules/ist8310/ist8310.c

源文件：[打开 modules/ist8310/ist8310.c](../modules/ist8310/ist8310.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/ist8310/ist8310.c:10](../modules/ist8310/ist8310.c#L10) | `IST8310_WRITE_REG_NUM` | `4` | 无外层条件 | 方便阅读 |
| [modules/ist8310/ist8310.c:11](../modules/ist8310/ist8310.c#L11) | `IST8310_DATA_REG` | `0x03` | 无外层条件 | ist8310的数据寄存器 |
| [modules/ist8310/ist8310.c:12](../modules/ist8310/ist8310.c#L12) | `IST8310_WHO_AM_I` | `0x00` | 无外层条件 | ist8310 id 寄存器值 |
| [modules/ist8310/ist8310.c:13](../modules/ist8310/ist8310.c#L13) | `IST8310_WHO_AM_I_VALUE` | `0x10` | 无外层条件 | 用于检测是否连接成功,读取ist8310的whoami会返回该值 |

## modules/ist8310/ist8310.h

源文件：[打开 modules/ist8310/ist8310.h](../modules/ist8310/ist8310.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/ist8310/ist8310.h:8](../modules/ist8310/ist8310.h#L8) | `MAG_SEN` | `0.3f` | 无外层条件 | raw int16 data change to uT unit. 原始整型数据变成 单位ut |
| [modules/ist8310/ist8310.h:11](../modules/ist8310/ist8310.h#L11) | `IST8310_NO_ERROR` | `0x00` | 无外层条件 | seldom used. 一般不会用到 |
| [modules/ist8310/ist8310.h:12](../modules/ist8310/ist8310.h#L12) | `IST8310_NO_SENSOR` | `0x40` | 无外层条件 | seldom used. 一般不会用到 |
| [modules/ist8310/ist8310.h:13](../modules/ist8310/ist8310.h#L13) | `IST8310_IIC_ADDRESS` | `0x0E` | 无外层条件 | the I2C slave address of IST8310 |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[modules/ist8310/ist8310.h:38](../modules/ist8310/ist8310.h#L38)

```c
typedef struct
{
    IIC_Init_Config_s iic_config;      // iic初始化配置
    GPIO_Init_Config_s gpio_conf_rst;  // gpio初始化配置,用于复位ist8310,看数据手册
    GPIO_Init_Config_s gpio_conf_exti; // gpio初始化配置,用于获取MAG_DRDY引脚状态,判断数据是否准备好(EXTI外部中断)
} IST8310_Init_Config_s;
```

## modules/master_machine/master_process.c

源文件：[打开 modules/master_machine/master_process.c](../modules/master_machine/master_process.c)。

### 配置结构体初值

以下记录原始定义，不自动推断运行路径或后续覆盖。未填字段为零；指针/回调表示绑定关系。

[modules/master_machine/master_process.c:79](../modules/master_machine/master_process.c#L79)

```c
Daemon_Init_Config_s daemon_conf = {
        .callback = VisionOfflineCallback, // 离线时调用的回调函数,会重启串口接收
        .owner_id = vision_usart_instance,
        .reload_count = 10,
    };
```

[modules/master_machine/master_process.c:130](../modules/master_machine/master_process.c#L130)

```c
USB_Init_Config_s conf = {.rx_cbk = DecodeVision};
```

[modules/master_machine/master_process.c:134](../modules/master_machine/master_process.c#L134)

```c
Daemon_Init_Config_s daemon_conf = {
        .callback = VisionOfflineCallback, // 离线时调用的回调函数,会重启串口接收
        .owner_id = NULL,
        .reload_count = 5, // 50ms
    };
```

## modules/master_machine/master_process.h

源文件：[打开 modules/master_machine/master_process.h](../modules/master_machine/master_process.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/master_machine/master_process.h:2](../modules/master_machine/master_process.h#L2) | `MASTER_PROCESS_H` | 空定义 | `#ifndef MASTER_PROCESS_H` |  |
| [modules/master_machine/master_process.h:7](../modules/master_machine/master_process.h#L7) | `VISION_RECV_SIZE` | `18u` | `#ifndef MASTER_PROCESS_H` | 当前为固定值,36字节 |
| [modules/master_machine/master_process.h:8](../modules/master_machine/master_process.h#L8) | `VISION_SEND_SIZE` | `36u` | `#ifndef MASTER_PROCESS_H` |  |

## modules/master_machine/seasky_protocol.h

源文件：[打开 modules/master_machine/seasky_protocol.h](../modules/master_machine/seasky_protocol.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/master_machine/seasky_protocol.h:2](../modules/master_machine/seasky_protocol.h#L2) | `__SEASKY_PROTOCOL_H` | 空定义 | `#ifndef __SEASKY_PROTOCOL_H` |  |
| [modules/master_machine/seasky_protocol.h:7](../modules/master_machine/seasky_protocol.h#L7) | `PROTOCOL_CMD_ID` | `0XA5` | `#ifndef __SEASKY_PROTOCOL_H` |  |
| [modules/master_machine/seasky_protocol.h:8](../modules/master_machine/seasky_protocol.h#L8) | `OFFSET_BYTE` | `8` | `#ifndef __SEASKY_PROTOCOL_H` | 出数据段外，其他部分所占字节数 |

## modules/message_center/message_center.h

源文件：[打开 modules/message_center/message_center.h](../modules/message_center/message_center.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/message_center/message_center.h:13](../modules/message_center/message_center.h#L13) | `PUBSUB_H` | 空定义 | `#ifndef PUBSUB_H` |  |
| [modules/message_center/message_center.h:17](../modules/message_center/message_center.h#L17) | `MAX_TOPIC_NAME_LEN` | `32` | `#ifndef PUBSUB_H` | 最大的话题名长度,每个话题都有字符串来命名 |
| [modules/message_center/message_center.h:18](../modules/message_center/message_center.h#L18) | `MAX_TOPIC_COUNT` | `12` | `#ifndef PUBSUB_H` | 最多支持的话题数量 |
| [modules/message_center/message_center.h:19](../modules/message_center/message_center.h#L19) | `QUEUE_SIZE` | `1` | `#ifndef PUBSUB_H` |  |

## modules/motor/DJImotor/dji_motor.c

源文件：[打开 modules/motor/DJImotor/dji_motor.c](../modules/motor/DJImotor/dji_motor.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/motor/DJImotor/dji_motor.c:20](../modules/motor/DJImotor/dji_motor.c#L20) | `CHASSIS_POWER_RTT_DIVIDER` | `40U` | 无外层条件 |  |

### 配置结构体初值

以下记录原始定义，不自动推断运行路径或后续覆盖。未填字段为零；指针/回调表示绑定关系。

[modules/motor/DJImotor/dji_motor.c:221](../modules/motor/DJImotor/dji_motor.c#L221)

```c
Daemon_Init_Config_s daemon_config = {
        .callback = DJIMotorLostCallback,
        .owner_id = instance,
        .reload_count = 2, // 20ms未收到数据则丢失
    };
```

## modules/motor/DJImotor/dji_motor.h

源文件：[打开 modules/motor/DJImotor/dji_motor.h](../modules/motor/DJImotor/dji_motor.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/motor/DJImotor/dji_motor.h:16](../modules/motor/DJImotor/dji_motor.h#L16) | `DJI_MOTOR_H` | 空定义 | `#ifndef DJI_MOTOR_H` |  |
| [modules/motor/DJImotor/dji_motor.h:29](../modules/motor/DJImotor/dji_motor.h#L29) | `DJI_MOTOR_CNT` | `12` | `#ifndef DJI_MOTOR_H` |  |
| [modules/motor/DJImotor/dji_motor.h:32](../modules/motor/DJImotor/dji_motor.h#L32) | `SPEED_SMOOTH_COEF` | `0.85f` | `#ifndef DJI_MOTOR_H` | 最好大于0.85 |
| [modules/motor/DJImotor/dji_motor.h:33](../modules/motor/DJImotor/dji_motor.h#L33) | `CURRENT_SMOOTH_COEF` | `0.9f` | `#ifndef DJI_MOTOR_H` | 必须大于0.9 |
| [modules/motor/DJImotor/dji_motor.h:34](../modules/motor/DJImotor/dji_motor.h#L34) | `ECD_ANGLE_COEF_DJI` | `0.043945f` | `#ifndef DJI_MOTOR_H` | (360/8192),将编码器值转化为角度制 |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[modules/motor/DJImotor/dji_motor.h:75](../modules/motor/DJImotor/dji_motor.h#L75)

```c
typedef struct
{
    DJIMotorInstance *motors[POWER_MODEL_MOTOR_COUNT];
    MotorPowerModelConfig_s models[POWER_MODEL_MOTOR_COUNT];
    ChassisPowerAlgorithmConfig_s algorithm;
} DJIChassisPowerConfig_s;
```

## modules/motor/DMmotor/dmmotor.c

源文件：[打开 modules/motor/DMmotor/dmmotor.c](../modules/motor/DMmotor/dmmotor.c)。

### 配置结构体初值

以下记录原始定义，不自动推断运行路径或后续覆盖。未填字段为零；指针/回调表示绑定关系。

[modules/motor/DMmotor/dmmotor.c:82](../modules/motor/DMmotor/dmmotor.c#L82)

```c
Daemon_Init_Config_s conf = {
        .callback = DMMotorLostCallback,
        .owner_id = motor,
        .reload_count = 10,
    };
```

## modules/motor/DMmotor/dmmotor.h

源文件：[打开 modules/motor/DMmotor/dmmotor.h](../modules/motor/DMmotor/dmmotor.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/motor/DMmotor/dmmotor.h:2](../modules/motor/DMmotor/dmmotor.h#L2) | `DMMOTOR_H` | 空定义 | `#ifndef DMMOTOR_H` |  |
| [modules/motor/DMmotor/dmmotor.h:9](../modules/motor/DMmotor/dmmotor.h#L9) | `DM_MOTOR_CNT` | `4` | `#ifndef DMMOTOR_H` |  |
| [modules/motor/DMmotor/dmmotor.h:11](../modules/motor/DMmotor/dmmotor.h#L11) | `DM_P_MIN` | `(-12.5f)` | `#ifndef DMMOTOR_H` |  |
| [modules/motor/DMmotor/dmmotor.h:12](../modules/motor/DMmotor/dmmotor.h#L12) | `DM_P_MAX` | `12.5f` | `#ifndef DMMOTOR_H` |  |
| [modules/motor/DMmotor/dmmotor.h:13](../modules/motor/DMmotor/dmmotor.h#L13) | `DM_V_MIN` | `(-45.0f)` | `#ifndef DMMOTOR_H` |  |
| [modules/motor/DMmotor/dmmotor.h:14](../modules/motor/DMmotor/dmmotor.h#L14) | `DM_V_MAX` | `45.0f` | `#ifndef DMMOTOR_H` |  |
| [modules/motor/DMmotor/dmmotor.h:15](../modules/motor/DMmotor/dmmotor.h#L15) | `DM_T_MIN` | `(-18.0f)` | `#ifndef DMMOTOR_H` |  |
| [modules/motor/DMmotor/dmmotor.h:16](../modules/motor/DMmotor/dmmotor.h#L16) | `DM_T_MAX` | `18.0f` | `#ifndef DMMOTOR_H` |  |

## modules/motor/HTmotor/HT04.c

源文件：[打开 modules/motor/HTmotor/HT04.c](../modules/motor/HTmotor/HT04.c)。

### 配置结构体初值

以下记录原始定义，不自动推断运行路径或后续覆盖。未填字段为零；指针/回调表示绑定关系。

[modules/motor/HTmotor/HT04.c:121](../modules/motor/HTmotor/HT04.c#L121)

```c
Daemon_Init_Config_s conf = {
        .callback = HTMotorLostCallback,
        .owner_id = motor,
        .reload_count = 5, // 50ms
    };
```

## modules/motor/HTmotor/HT04.h

源文件：[打开 modules/motor/HTmotor/HT04.h](../modules/motor/HTmotor/HT04.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/motor/HTmotor/HT04.h:2](../modules/motor/HTmotor/HT04.h#L2) | `HT04_H` | 空定义 | `#ifndef HT04_H` |  |
| [modules/motor/HTmotor/HT04.h:10](../modules/motor/HTmotor/HT04.h#L10) | `HT_MOTOR_CNT` | `4` | `#ifndef HT04_H` |  |
| [modules/motor/HTmotor/HT04.h:11](../modules/motor/HTmotor/HT04.h#L11) | `CURRENT_SMOOTH_COEF` | `0.9f` | `#ifndef HT04_H` |  |
| [modules/motor/HTmotor/HT04.h:12](../modules/motor/HTmotor/HT04.h#L12) | `SPEED_BUFFER_SIZE` | `5` | `#ifndef HT04_H` |  |
| [modules/motor/HTmotor/HT04.h:13](../modules/motor/HTmotor/HT04.h#L13) | `HT_SPEED_BIAS` | `-0.0109901428f` | `#ifndef HT04_H` | 电机速度偏差,单位rad/s |
| [modules/motor/HTmotor/HT04.h:14](../modules/motor/HTmotor/HT04.h#L14) | `TORQUE_COEF_HT` | `3.5` | `#ifndef HT04_H` | 扭矩系数,单位N.m/A |
| [modules/motor/HTmotor/HT04.h:16](../modules/motor/HTmotor/HT04.h#L16) | `P_MIN` | `-95.5f` | `#ifndef HT04_H` | Radians |
| [modules/motor/HTmotor/HT04.h:17](../modules/motor/HTmotor/HT04.h#L17) | `P_MAX` | `95.5f` | `#ifndef HT04_H` |  |
| [modules/motor/HTmotor/HT04.h:18](../modules/motor/HTmotor/HT04.h#L18) | `V_MIN` | `-45.0f` | `#ifndef HT04_H` | Rad/s |
| [modules/motor/HTmotor/HT04.h:19](../modules/motor/HTmotor/HT04.h#L19) | `V_MAX` | `45.0f` | `#ifndef HT04_H` |  |
| [modules/motor/HTmotor/HT04.h:20](../modules/motor/HTmotor/HT04.h#L20) | `T_MIN` | `-18.0f` | `#ifndef HT04_H` | N·m |
| [modules/motor/HTmotor/HT04.h:21](../modules/motor/HTmotor/HT04.h#L21) | `T_MAX` | `18.0f` | `#ifndef HT04_H` |  |
| [modules/motor/HTmotor/HT04.h:22](../modules/motor/HTmotor/HT04.h#L22) | `KP_MIN` | `0.0f` | `#ifndef HT04_H` | N-m/rad |
| [modules/motor/HTmotor/HT04.h:23](../modules/motor/HTmotor/HT04.h#L23) | `KP_MAX` | `500.0f` | `#ifndef HT04_H` |  |
| [modules/motor/HTmotor/HT04.h:24](../modules/motor/HTmotor/HT04.h#L24) | `KD_MIN` | `0.0f` | `#ifndef HT04_H` | N-m/rad/s |
| [modules/motor/HTmotor/HT04.h:25](../modules/motor/HTmotor/HT04.h#L25) | `KD_MAX` | `5.0f` | `#ifndef HT04_H` |  |

### 已注释的宏选项（当前不定义）

| 行 | 原选项 |
| --- | --- |
| [modules/motor/HTmotor/HT04.h:124](../modules/motor/HTmotor/HT04.h#L124) | `#define HT04_H` |

## modules/motor/LKmotor/LK9025.c

源文件：[打开 modules/motor/LKmotor/LK9025.c](../modules/motor/LKmotor/LK9025.c)。

### 配置结构体初值

以下记录原始定义，不自动推断运行路径或后续覆盖。未填字段为零；指针/回调表示绑定关系。

[modules/motor/LKmotor/LK9025.c:82](../modules/motor/LKmotor/LK9025.c#L82)

```c
Daemon_Init_Config_s daemon_config = {
        .callback = LKMotorLostCallback,
        .owner_id = motor,
        .reload_count = 5, // 50ms
    };
```

## modules/motor/LKmotor/LK9025.h

源文件：[打开 modules/motor/LKmotor/LK9025.h](../modules/motor/LKmotor/LK9025.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/motor/LKmotor/LK9025.h:2](../modules/motor/LKmotor/LK9025.h#L2) | `LK9025_H` | 空定义 | `#ifndef LK9025_H` |  |
| [modules/motor/LKmotor/LK9025.h:10](../modules/motor/LKmotor/LK9025.h#L10) | `LK_MOTOR_MX_CNT` | `4` | `#ifndef LK9025_H` | 最多允许4个LK电机使用多电机指令,挂载在一条总线上 |
| [modules/motor/LKmotor/LK9025.h:12](../modules/motor/LKmotor/LK9025.h#L12) | `I_MIN` | `-2000` | `#ifndef LK9025_H` |  |
| [modules/motor/LKmotor/LK9025.h:13](../modules/motor/LKmotor/LK9025.h#L13) | `I_MAX` | `2000` | `#ifndef LK9025_H` |  |
| [modules/motor/LKmotor/LK9025.h:14](../modules/motor/LKmotor/LK9025.h#L14) | `CURRENT_SMOOTH_COEF` | `0.9f` | `#ifndef LK9025_H` |  |
| [modules/motor/LKmotor/LK9025.h:15](../modules/motor/LKmotor/LK9025.h#L15) | `SPEED_SMOOTH_COEF` | `0.85f` | `#ifndef LK9025_H` |  |
| [modules/motor/LKmotor/LK9025.h:16](../modules/motor/LKmotor/LK9025.h#L16) | `REDUCTION_RATIO_DRIVEN` | `1` | `#ifndef LK9025_H` |  |
| [modules/motor/LKmotor/LK9025.h:17](../modules/motor/LKmotor/LK9025.h#L17) | `ECD_ANGLE_COEF_LK` | `(360.0f / 65536.0f)` | `#ifndef LK9025_H` |  |
| [modules/motor/LKmotor/LK9025.h:18](../modules/motor/LKmotor/LK9025.h#L18) | `CURRENT_TORQUE_COEF_LK` | `0.00512f` | `#ifndef LK9025_H` | 电流设定值转换成扭矩的系数，这里对应的是16T |

## modules/motor/motor_def.h

源文件：[打开 modules/motor/motor_def.h](../modules/motor/motor_def.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/motor/motor_def.h:13](../modules/motor/motor_def.h#L13) | `MOTOR_DEF_H` | 空定义 | `#ifndef MOTOR_DEF_H` |  |
| [modules/motor/motor_def.h:18](../modules/motor/motor_def.h#L18) | `LIMIT_MIN_MAX(x, min, max)` | `(x) = (((x) <= (min)) ? (min) : (((x) >= (max)) ? (max) : (x)))` | `#ifndef MOTOR_DEF_H` |  |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[modules/motor/motor_def.h:73](../modules/motor/motor_def.h#L73)

```c
typedef struct
{
    Closeloop_Type_e outer_loop_type;              // 最外层的闭环,未设置时默认为最高级的闭环
    Closeloop_Type_e close_loop_type;              // 使用几个闭环(串级)
    Motor_Reverse_Flag_e motor_reverse_flag;       // 是否反转
    Feedback_Reverse_Flag_e feedback_reverse_flag; // 反馈是否反向
    Feedback_Source_e angle_feedback_source;       // 角度反馈类型
    Feedback_Source_e speed_feedback_source;       // 速度反馈类型
    Feedfoward_Type_e feedforward_flag;            // 前馈标志

} Motor_Control_Setting_s;
```

[modules/motor/motor_def.h:117](../modules/motor/motor_def.h#L117)

```c
typedef struct
{
    float *other_angle_feedback_ptr; // 角度反馈数据指针,注意电机使用total_angle
    float *other_speed_feedback_ptr; // 速度反馈数据指针,单位为angle per sec

    float *speed_feedforward_ptr;   // 速度前馈数据指针
    float *current_feedforward_ptr; // 电流前馈数据指针

    PID_Init_Config_s current_PID;
    PID_Init_Config_s speed_PID;
    PID_Init_Config_s angle_PID;
} Motor_Controller_Init_s;
```

[modules/motor/motor_def.h:131](../modules/motor/motor_def.h#L131)

```c
typedef struct
{
    Motor_Controller_Init_s controller_param_init_config;
    Motor_Control_Setting_s controller_setting_init_config;
    Motor_Type_e motor_type;
    CAN_Init_Config_s can_init_config;
} Motor_Init_Config_s;
```

## modules/motor/motor_task.h

源文件：[打开 modules/motor/motor_task.h](../modules/motor/motor_task.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/motor/motor_task.h:12](../modules/motor/motor_task.h#L12) | `MOTOR_TASK_H` | 空定义 | `#ifndef MOTOR_TASK_H` |  |

## modules/motor/power_control.c

源文件：[打开 modules/motor/power_control.c](../modules/motor/power_control.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/motor/power_control.c:6](../modules/motor/power_control.c#L6) | `TORQUE_COEF` | `0.0003662109375f` | 无外层条件 | (20/16384)*(0.3), 电机转矩系数，与电流和转矩相关 |
| [modules/motor/power_control.c:7](../modules/motor/power_control.c#L7) | `POWER_COEF` | `187.0f / 3591.0f / 9.55f` | 无外层条件 | 电机机械功率系数，与力矩和转速相关，注意框架中速度单位为aps |

### 配置结构体初值

以下记录原始定义，不自动推断运行路径或后续覆盖。未填字段为零；指针/回调表示绑定关系。

[modules/motor/power_control.c:207](../modules/motor/power_control.c#L207)

```c
Daemon_Init_Config_s daemon_config = {
        .callback = DJIMotorLostCallback,
        .owner_id = instance,
        .reload_count = 2, // 20ms未收到数据则丢失
    };
```

## modules/motor/power_control.h

源文件：[打开 modules/motor/power_control.h](../modules/motor/power_control.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/motor/power_control.h:16](../modules/motor/power_control.h#L16) | `POWER_CONTROL_H` | 空定义 | `#ifndef POWER_CONTROL_H` |  |

## modules/motor/servo_motor/servo_motor.h

源文件：[打开 modules/motor/servo_motor/servo_motor.h](../modules/motor/servo_motor/servo_motor.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/motor/servo_motor/servo_motor.h:2](../modules/motor/servo_motor/servo_motor.h#L2) | `SERVO_MOTOR_H` | 空定义 | `#ifndef SERVO_MOTOR_H` |  |
| [modules/motor/servo_motor/servo_motor.h:10](../modules/motor/servo_motor/servo_motor.h#L10) | `SERVO_MOTOR_CNT` | `7` | `#ifndef SERVO_MOTOR_H` |  |
| [modules/motor/servo_motor/servo_motor.h:11](../modules/motor/servo_motor/servo_motor.h#L11) | `Servo_Frame_First` | `0x55` | `#ifndef SERVO_MOTOR_H` |  |
| [modules/motor/servo_motor/servo_motor.h:12](../modules/motor/servo_motor/servo_motor.h#L12) | `Servo_Frame_Second` | `0x55` | `#ifndef SERVO_MOTOR_H` |  |
| [modules/motor/servo_motor/servo_motor.h:13](../modules/motor/servo_motor/servo_motor.h#L13) | `Servo_MAX_BUFF` | `10` | `#ifndef SERVO_MOTOR_H` |  |
| [modules/motor/servo_motor/servo_motor.h:14](../modules/motor/servo_motor/servo_motor.h#L14) | `SERVO_MOVE_CMD` | `0x03` | `#ifndef SERVO_MOTOR_H` |  |
| [modules/motor/servo_motor/servo_motor.h:15](../modules/motor/servo_motor/servo_motor.h#L15) | `SERVO_UNLOAD_CMD` | `0x14` | `#ifndef SERVO_MOTOR_H` |  |
| [modules/motor/servo_motor/servo_motor.h:16](../modules/motor/servo_motor/servo_motor.h#L16) | `SERVO_POS_READ_CMD` | `0x15` | `#ifndef SERVO_MOTOR_H` |  |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[modules/motor/servo_motor/servo_motor.h:26](../modules/motor/servo_motor/servo_motor.h#L26)

```c
typedef struct
{
    PWM_Init_Config_s pwm_init_config;
    ServoType_e servo_type;
    UART_HandleTypeDef *_handle;
    uint8_t servo_id;
}Servo_Init_Config_s;
```

## modules/oled/oled.h

源文件：[打开 modules/oled/oled.h](../modules/oled/oled.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/oled/oled.h:14](../modules/oled/oled.h#L14) | `OLED_H` | 空定义 | `#ifndef OLED_H` |  |
| [modules/oled/oled.h:18](../modules/oled/oled.h#L18) | `OLED_I2C_ADDRESS` | `0x78` | `#ifndef OLED_H` |  |
| [modules/oled/oled.h:21](../modules/oled/oled.h#L21) | `MAX_COLUMN` | `128` | `#ifndef OLED_H` |  |
| [modules/oled/oled.h:22](../modules/oled/oled.h#L22) | `MAX_ROW` | `64` | `#ifndef OLED_H` |  |
| [modules/oled/oled.h:24](../modules/oled/oled.h#L24) | `X_WIDTH` | `MAX_COLUMN` | `#ifndef OLED_H` |  |
| [modules/oled/oled.h:25](../modules/oled/oled.h#L25) | `Y_WIDTH` | `MAX_ROW` | `#ifndef OLED_H` |  |
| [modules/oled/oled.h:27](../modules/oled/oled.h#L27) | `OLED_CMD` | `0x00` | `#ifndef OLED_H` |  |
| [modules/oled/oled.h:28](../modules/oled/oled.h#L28) | `OLED_DATA` | `0x01` | `#ifndef OLED_H` |  |
| [modules/oled/oled.h:30](../modules/oled/oled.h#L30) | `CHAR_SIZE_WIDTH` | `6` | `#ifndef OLED_H` |  |
| [modules/oled/oled.h:31](../modules/oled/oled.h#L31) | `CHAR_SIZE_HIGHT` | `12` | `#ifndef OLED_H` |  |

## modules/oled/oledfont.h

源文件：[打开 modules/oled/oledfont.h](../modules/oled/oledfont.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/oled/oledfont.h:12](../modules/oled/oledfont.h#L12) | `__OLED__FONT__H` | 空定义 | `#ifndef __OLED__FONT__H` |  |

## modules/power_meter/power_meter_protocol.h

源文件：[打开 modules/power_meter/power_meter_protocol.h](../modules/power_meter/power_meter_protocol.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/power_meter/power_meter_protocol.h:2](../modules/power_meter/power_meter_protocol.h#L2) | `POWER_METER_PROTOCOL_H` | 空定义 | `#ifndef POWER_METER_PROTOCOL_H` |  |
| [modules/power_meter/power_meter_protocol.h:7](../modules/power_meter/power_meter_protocol.h#L7) | `POWER_METER_STATUS_CAN_ID` | `0x605U` | `#ifndef POWER_METER_PROTOCOL_H` |  |
| [modules/power_meter/power_meter_protocol.h:8](../modules/power_meter/power_meter_protocol.h#L8) | `POWER_METER_CAN_DLC` | `8U` | `#ifndef POWER_METER_PROTOCOL_H` |  |

## modules/power_meter/power_meter.c

源文件：[打开 modules/power_meter/power_meter.c](../modules/power_meter/power_meter.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/power_meter/power_meter.c:9](../modules/power_meter/power_meter.c#L9) | `POWER_METER_DEFAULT_OFFLINE_RELOAD_COUNT` | `10U` | 无外层条件 | 默认离线重载值,即100ms内没有接收到数据则认为离线 |
| [modules/power_meter/power_meter.c:10](../modules/power_meter/power_meter.c#L10) | `POWER_METER_DAEMON_PERIOD_MS` | `10U` | 无外层条件 | 守护进程周期,即每10ms调用一次守护进程的回调函数 |

### 配置结构体初值

以下记录原始定义，不自动推断运行路径或后续覆盖。未填字段为零；指针/回调表示绑定关系。

[modules/power_meter/power_meter.c:80](../modules/power_meter/power_meter.c#L80)

```c
Daemon_Init_Config_s daemon_config = {
        .reload_count = reload_count,
        .init_count = reload_count,
        .callback = PowerMeterOffline,
        .owner_id = instance,
    };
```

## modules/power_meter/power_meter.h

源文件：[打开 modules/power_meter/power_meter.h](../modules/power_meter/power_meter.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/power_meter/power_meter.h:2](../modules/power_meter/power_meter.h#L2) | `POWER_METER_H` | 空定义 | `#ifndef POWER_METER_H` |  |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[modules/power_meter/power_meter.h:32](../modules/power_meter/power_meter.h#L32)

```c
typedef struct
{
    CAN_Init_Config_s can_config;
    uint16_t offline_reload_count;
} PowerMeter_Init_Config_s;
```

## modules/referee/crc_ref.h

源文件：[打开 modules/referee/crc_ref.h](../modules/referee/crc_ref.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/referee/crc_ref.h:2](../modules/referee/crc_ref.h#L2) | `__CRC_H_` | 空定义 | `#ifndef __CRC_H_` |  |
| [modules/referee/crc_ref.h:8](../modules/referee/crc_ref.h#L8) | `TRUE` | `1` | `#ifndef __CRC_H_` |  |
| [modules/referee/crc_ref.h:9](../modules/referee/crc_ref.h#L9) | `FALSE` | `0` | `#ifndef __CRC_H_` |  |

## modules/referee/referee_power.h

源文件：[打开 modules/referee/referee_power.h](../modules/referee/referee_power.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/referee/referee_power.h:2](../modules/referee/referee_power.h#L2) | `REFEREE_POWER_H` | 空定义 | `#ifndef REFEREE_POWER_H` |  |
| [modules/referee/referee_power.h:7](../modules/referee/referee_power.h#L7) | `REFEREE_ROBOT_STATE_COMMAND_ID` | `0x0201U` | `#ifndef REFEREE_POWER_H` |  |
| [modules/referee/referee_power.h:8](../modules/referee/referee_power.h#L8) | `REFEREE_POWER_HEAT_COMMAND_ID` | `0x0202U` | `#ifndef REFEREE_POWER_H` |  |
| [modules/referee/referee_power.h:9](../modules/referee/referee_power.h#L9) | `REFEREE_ROBOT_STATE_PAYLOAD_LENGTH` | `13U` | `#ifndef REFEREE_POWER_H` |  |
| [modules/referee/referee_power.h:10](../modules/referee/referee_power.h#L10) | `REFEREE_ROBOT_STATE_EXTENDED_PAYLOAD_LENGTH` | `17U` | `#ifndef REFEREE_POWER_H` |  |
| [modules/referee/referee_power.h:11](../modules/referee/referee_power.h#L11) | `REFEREE_POWER_HEAT_PAYLOAD_LENGTH` | `14U` | `#ifndef REFEREE_POWER_H` |  |
| [modules/referee/referee_power.h:12](../modules/referee/referee_power.h#L12) | `REFEREE_POWER_HEAT_LEGACY_PAYLOAD_LENGTH` | `16U` | `#ifndef REFEREE_POWER_H` |  |
| [modules/referee/referee_power.h:13](../modules/referee/referee_power.h#L13) | `REFEREE_POWER_DATA_TIMEOUT_MS` | `300U` | `#ifndef REFEREE_POWER_H` |  |
| [modules/referee/referee_power.h:18](../modules/referee/referee_power.h#L18) | `REFEREE_LEGACY_POWER_FIELDS_ENABLED` | `0` | `#ifndef REFEREE_POWER_H；#ifndef REFEREE_LEGACY_POWER_FIELDS_ENABLED` |  |

## modules/referee/referee_protocol.h

源文件：[打开 modules/referee/referee_protocol.h](../modules/referee/referee_protocol.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/referee/referee_protocol.h:12](../modules/referee/referee_protocol.h#L12) | `referee_protocol_H` | 空定义 | `#ifndef referee_protocol_H` |  |
| [modules/referee/referee_protocol.h:18](../modules/referee/referee_protocol.h#L18) | `REFEREE_SOF` | `0xA5` | `#ifndef referee_protocol_H` | 起始字节,协议固定为0xA5 |
| [modules/referee/referee_protocol.h:19](../modules/referee/referee_protocol.h#L19) | `Robot_Red` | `0` | `#ifndef referee_protocol_H` |  |
| [modules/referee/referee_protocol.h:20](../modules/referee/referee_protocol.h#L20) | `Robot_Blue` | `1` | `#ifndef referee_protocol_H` |  |
| [modules/referee/referee_protocol.h:21](../modules/referee/referee_protocol.h#L21) | `Communicate_Data_LEN` | `5` | `#ifndef referee_protocol_H` | 自定义交互数据长度，该长度决定了我方发送和他方接收，自定义交互数据协议更改时只需要更改此宏定义即可 |

## modules/referee/referee_stream_parser.c

源文件：[打开 modules/referee/referee_stream_parser.c](../modules/referee/referee_stream_parser.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/referee/referee_stream_parser.c:7](../modules/referee/referee_stream_parser.c#L7) | `REFEREE_SOF` | `0xA5U` | 无外层条件 |  |
| [modules/referee/referee_stream_parser.c:8](../modules/referee/referee_stream_parser.c#L8) | `REFEREE_HEADER_LENGTH` | `5U` | 无外层条件 |  |
| [modules/referee/referee_stream_parser.c:9](../modules/referee/referee_stream_parser.c#L9) | `REFEREE_COMMAND_LENGTH` | `2U` | 无外层条件 |  |
| [modules/referee/referee_stream_parser.c:10](../modules/referee/referee_stream_parser.c#L10) | `REFEREE_TAIL_LENGTH` | `2U` | 无外层条件 |  |
| [modules/referee/referee_stream_parser.c:11](../modules/referee/referee_stream_parser.c#L11) | `REFEREE_FRAME_OVERHEAD` | `(REFEREE_HEADER_LENGTH + REFEREE_COMMAND_LENGTH + REFEREE_TAIL_LENGTH)` | 无外层条件 |  |

## modules/referee/referee_stream_parser.h

源文件：[打开 modules/referee/referee_stream_parser.h](../modules/referee/referee_stream_parser.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/referee/referee_stream_parser.h:2](../modules/referee/referee_stream_parser.h#L2) | `REFEREE_STREAM_PARSER_H` | 空定义 | `#ifndef REFEREE_STREAM_PARSER_H` |  |
| [modules/referee/referee_stream_parser.h:8](../modules/referee/referee_stream_parser.h#L8) | `REFEREE_STREAM_BUFFER_CAPACITY` | `256U` | `#ifndef REFEREE_STREAM_PARSER_H` |  |

## modules/referee/referee_task.c

源文件：[打开 modules/referee/referee_task.c](../modules/referee/referee_task.c)。

### 已注释的宏选项（当前不定义）

| 行 | 原选项 |
| --- | --- |
| [modules/referee/referee_task.c:31](../modules/referee/referee_task.c#L31) | `#define Robot_Red 0 #define Robot_Blue 1` |

## modules/referee/referee_task.h

源文件：[打开 modules/referee/referee_task.h](../modules/referee/referee_task.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/referee/referee_task.h:2](../modules/referee/referee_task.h#L2) | `REFEREE_H` | 空定义 | `#ifndef REFEREE_H` |  |

## modules/referee/referee_UI.h

源文件：[打开 modules/referee/referee_UI.h](../modules/referee/referee_UI.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/referee/referee_UI.h:2](../modules/referee/referee_UI.h#L2) | `REFEREE_UI_H` | 空定义 | `#ifndef REFEREE_UI_H` |  |

## modules/referee/rm_referee.c

源文件：[打开 modules/referee/rm_referee.c](../modules/referee/rm_referee.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/referee/rm_referee.c:21](../modules/referee/rm_referee.c#L21) | `RE_RX_BUFFER_SIZE` | `255u` | 无外层条件 | 裁判系统接收缓冲区大小 |

### 配置结构体初值

以下记录原始定义，不自动推断运行路径或后续覆盖。未填字段为零；指针/回调表示绑定关系。

[modules/referee/rm_referee.c:223](../modules/referee/rm_referee.c#L223)

```c
Daemon_Init_Config_s daemon_conf = {
		.callback = RefereeLostCallback,
		.owner_id = referee_usart_instance,
		.reload_count = 30, // 0.3s没有收到数据,则认为丢失,重启串口接收
	};
```

## modules/referee/rm_referee.h

源文件：[打开 modules/referee/rm_referee.h](../modules/referee/rm_referee.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/referee/rm_referee.h:2](../modules/referee/rm_referee.h#L2) | `RM_REFEREE_H` | 空定义 | `#ifndef RM_REFEREE_H` |  |

## modules/remote/remote_control.c

源文件：[打开 modules/remote/remote_control.c](../modules/remote/remote_control.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/remote/remote_control.c:9](../modules/remote/remote_control.c#L9) | `REMOTE_CONTROL_FRAME_SIZE` | `18u` | 无外层条件 | 遥控器接收的buffer大小 |

### 配置结构体初值

以下记录原始定义，不自动推断运行路径或后续覆盖。未填字段为零；指针/回调表示绑定关系。

[modules/remote/remote_control.c:120](../modules/remote/remote_control.c#L120)

```c
Daemon_Init_Config_s daemon_conf = {
        .reload_count = 10, // 100ms未收到数据视为离线,遥控器的接收频率实际上是1000/14Hz(大约70Hz)
        .callback = RCLostCallback,
        .owner_id = NULL, // 只有1个遥控器,不需要owner_id
    };
```

## modules/remote/remote_control.h

源文件：[打开 modules/remote/remote_control.h](../modules/remote/remote_control.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/remote/remote_control.h:14](../modules/remote/remote_control.h#L14) | `REMOTE_CONTROL_H` | 空定义 | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:21](../modules/remote/remote_control.h#L21) | `LAST` | `1` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:22](../modules/remote/remote_control.h#L22) | `TEMP` | `0` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:25](../modules/remote/remote_control.h#L25) | `KEY_PRESS` | `0` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:26](../modules/remote/remote_control.h#L26) | `KEY_STATE` | `1` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:27](../modules/remote/remote_control.h#L27) | `KEY_PRESS_WITH_CTRL` | `1` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:28](../modules/remote/remote_control.h#L28) | `KEY_PRESS_WITH_SHIFT` | `2` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:31](../modules/remote/remote_control.h#L31) | `RC_CH_VALUE_MIN` | `((uint16_t)364)` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:32](../modules/remote/remote_control.h#L32) | `RC_CH_VALUE_OFFSET` | `((uint16_t)1024)` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:33](../modules/remote/remote_control.h#L33) | `RC_CH_VALUE_MAX` | `((uint16_t)1684)` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:36](../modules/remote/remote_control.h#L36) | `RC_SW_UP` | `((uint16_t)1)` | `#ifndef REMOTE_CONTROL_H` | 开关向上时的值 |
| [modules/remote/remote_control.h:37](../modules/remote/remote_control.h#L37) | `RC_SW_MID` | `((uint16_t)3)` | `#ifndef REMOTE_CONTROL_H` | 开关中间时的值 |
| [modules/remote/remote_control.h:38](../modules/remote/remote_control.h#L38) | `RC_SW_DOWN` | `((uint16_t)2)` | `#ifndef REMOTE_CONTROL_H` | 开关向下时的值 |
| [modules/remote/remote_control.h:40](../modules/remote/remote_control.h#L40) | `switch_is_down(s)` | `(s == RC_SW_DOWN)` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:41](../modules/remote/remote_control.h#L41) | `switch_is_mid(s)` | `(s == RC_SW_MID)` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:42](../modules/remote/remote_control.h#L42) | `switch_is_up(s)` | `(s == RC_SW_UP)` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:46](../modules/remote/remote_control.h#L46) | `Key_W` | `0` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:47](../modules/remote/remote_control.h#L47) | `Key_S` | `1` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:48](../modules/remote/remote_control.h#L48) | `Key_D` | `2` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:49](../modules/remote/remote_control.h#L49) | `Key_A` | `3` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:50](../modules/remote/remote_control.h#L50) | `Key_Shift` | `4` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:51](../modules/remote/remote_control.h#L51) | `Key_Ctrl` | `5` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:52](../modules/remote/remote_control.h#L52) | `Key_Q` | `6` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:53](../modules/remote/remote_control.h#L53) | `Key_E` | `7` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:54](../modules/remote/remote_control.h#L54) | `Key_R` | `8` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:55](../modules/remote/remote_control.h#L55) | `Key_F` | `9` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:56](../modules/remote/remote_control.h#L56) | `Key_G` | `10` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:57](../modules/remote/remote_control.h#L57) | `Key_Z` | `11` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:58](../modules/remote/remote_control.h#L58) | `Key_X` | `12` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:59](../modules/remote/remote_control.h#L59) | `Key_C` | `13` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:60](../modules/remote/remote_control.h#L60) | `Key_V` | `14` | `#ifndef REMOTE_CONTROL_H` |  |
| [modules/remote/remote_control.h:61](../modules/remote/remote_control.h#L61) | `Key_B` | `15` | `#ifndef REMOTE_CONTROL_H` |  |

## modules/super_cap/chassis_power_link.h

源文件：[打开 modules/super_cap/chassis_power_link.h](../modules/super_cap/chassis_power_link.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/super_cap/chassis_power_link.h:2](../modules/super_cap/chassis_power_link.h#L2) | `CHASSIS_POWER_LINK_H` | 空定义 | `#ifndef CHASSIS_POWER_LINK_H` |  |

## modules/super_cap/super_cap_protocol.h

源文件：[打开 modules/super_cap/super_cap_protocol.h](../modules/super_cap/super_cap_protocol.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/super_cap/super_cap_protocol.h:2](../modules/super_cap/super_cap_protocol.h#L2) | `SUPER_CAP_PROTOCOL_H` | 空定义 | `#ifndef SUPER_CAP_PROTOCOL_H` |  |
| [modules/super_cap/super_cap_protocol.h:7](../modules/super_cap/super_cap_protocol.h#L7) | `SUPERCAP_COMMAND_CAN_ID` | `0x061U` | `#ifndef SUPER_CAP_PROTOCOL_H` |  |
| [modules/super_cap/super_cap_protocol.h:8](../modules/super_cap/super_cap_protocol.h#L8) | `SUPERCAP_STATUS_CAN_ID` | `0x051U` | `#ifndef SUPER_CAP_PROTOCOL_H` |  |
| [modules/super_cap/super_cap_protocol.h:9](../modules/super_cap/super_cap_protocol.h#L9) | `SUPERCAP_INPUT_POWER_CAN_ID` | `0x052U` | `#ifndef SUPER_CAP_PROTOCOL_H` |  |
| [modules/super_cap/super_cap_protocol.h:10](../modules/super_cap/super_cap_protocol.h#L10) | `SUPERCAP_INPUT_COMMAND_TAG` | `0xA5U` | `#ifndef SUPER_CAP_PROTOCOL_H` |  |
| [modules/super_cap/super_cap_protocol.h:11](../modules/super_cap/super_cap_protocol.h#L11) | `SUPERCAP_INPUT_FEEDBACK_VERSION` | `1U` | `#ifndef SUPER_CAP_PROTOCOL_H` |  |
| [modules/super_cap/super_cap_protocol.h:12](../modules/super_cap/super_cap_protocol.h#L12) | `SUPERCAP_CAN_DLC` | `8U` | `#ifndef SUPER_CAP_PROTOCOL_H` |  |
| [modules/super_cap/super_cap_protocol.h:14](../modules/super_cap/super_cap_protocol.h#L14) | `SUPERCAP_ERROR_UNDER_VOLTAGE` | `0x01U` | `#ifndef SUPER_CAP_PROTOCOL_H` |  |
| [modules/super_cap/super_cap_protocol.h:15](../modules/super_cap/super_cap_protocol.h#L15) | `SUPERCAP_ERROR_OVER_VOLTAGE` | `0x02U` | `#ifndef SUPER_CAP_PROTOCOL_H` |  |
| [modules/super_cap/super_cap_protocol.h:16](../modules/super_cap/super_cap_protocol.h#L16) | `SUPERCAP_ERROR_BUCK_BOOST` | `0x04U` | `#ifndef SUPER_CAP_PROTOCOL_H` |  |
| [modules/super_cap/super_cap_protocol.h:17](../modules/super_cap/super_cap_protocol.h#L17) | `SUPERCAP_ERROR_SHORT_CIRCUIT` | `0x08U` | `#ifndef SUPER_CAP_PROTOCOL_H` |  |
| [modules/super_cap/super_cap_protocol.h:18](../modules/super_cap/super_cap_protocol.h#L18) | `SUPERCAP_ERROR_HIGH_TEMPERATURE` | `0x10U` | `#ifndef SUPER_CAP_PROTOCOL_H` |  |
| [modules/super_cap/super_cap_protocol.h:19](../modules/super_cap/super_cap_protocol.h#L19) | `SUPERCAP_ERROR_NO_POWER_INPUT` | `0x20U` | `#ifndef SUPER_CAP_PROTOCOL_H` |  |
| [modules/super_cap/super_cap_protocol.h:20](../modules/super_cap/super_cap_protocol.h#L20) | `SUPERCAP_ERROR_CAPACITOR` | `0x40U` | `#ifndef SUPER_CAP_PROTOCOL_H` |  |
| [modules/super_cap/super_cap_protocol.h:21](../modules/super_cap/super_cap_protocol.h#L21) | `SUPERCAP_OUTPUT_DISABLED_MASK` | `0x80U` | `#ifndef SUPER_CAP_PROTOCOL_H` |  |
| [modules/super_cap/super_cap_protocol.h:22](../modules/super_cap/super_cap_protocol.h#L22) | `SUPERCAP_ERROR_MASK` | `0x7FU` | `#ifndef SUPER_CAP_PROTOCOL_H` |  |

## modules/super_cap/super_cap.c

源文件：[打开 modules/super_cap/super_cap.c](../modules/super_cap/super_cap.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/super_cap/super_cap.c:9](../modules/super_cap/super_cap.c#L9) | `SUPERCAP_DEFAULT_OFFLINE_RELOAD_COUNT` | `20U` | 无外层条件 |  |

### 配置结构体初值

以下记录原始定义，不自动推断运行路径或后续覆盖。未填字段为零；指针/回调表示绑定关系。

[modules/super_cap/super_cap.c:77](../modules/super_cap/super_cap.c#L77)

```c
Daemon_Init_Config_s daemon_config = {
        .reload_count = reload_count,
        .init_count = reload_count,
        .callback = SuperCapOffline,
        .owner_id = instance,
    };
```

## modules/super_cap/super_cap.h

源文件：[打开 modules/super_cap/super_cap.h](../modules/super_cap/super_cap.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/super_cap/super_cap.h:2](../modules/super_cap/super_cap.h#L2) | `SUPER_CAP_H` | 空定义 | `#ifndef SUPER_CAP_H` |  |

### 可设置的结构体字段

这是配置接口的字段声明，不表示当前已创建该模块实例。字段对应类型、指针和原注释如下。

[modules/super_cap/super_cap.h:23](../modules/super_cap/super_cap.h#L23)

```c
typedef struct
{
    CAN_Init_Config_s can_config;
    uint16_t offline_reload_count;
    bool input_power_feedback_enabled;
} SuperCap_Init_Config_s;
```

## modules/telemetry/chassis_power_capture.h

源文件：[打开 modules/telemetry/chassis_power_capture.h](../modules/telemetry/chassis_power_capture.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/telemetry/chassis_power_capture.h:2](../modules/telemetry/chassis_power_capture.h#L2) | `CHASSIS_POWER_CAPTURE_H` | 空定义 | `#ifndef CHASSIS_POWER_CAPTURE_H` |  |
| [modules/telemetry/chassis_power_capture.h:7](../modules/telemetry/chassis_power_capture.h#L7) | `CHASSIS_POWER_CAPTURE_RECORDS` | `150U` | `#ifndef CHASSIS_POWER_CAPTURE_H` |  |
| [modules/telemetry/chassis_power_capture.h:8](../modules/telemetry/chassis_power_capture.h#L8) | `CHASSIS_POWER_CAPTURE_RECORD_BYTES` | `96U` | `#ifndef CHASSIS_POWER_CAPTURE_H` |  |
| [modules/telemetry/chassis_power_capture.h:9](../modules/telemetry/chassis_power_capture.h#L9) | `CHASSIS_POWER_CAPTURE_FRAME_BYTES` | `102U` | `#ifndef CHASSIS_POWER_CAPTURE_H` |  |
| [modules/telemetry/chassis_power_capture.h:10](../modules/telemetry/chassis_power_capture.h#L10) | `CHASSIS_POWER_CAPTURE_QUANTIZED` | `0x80U` | `#ifndef CHASSIS_POWER_CAPTURE_H` |  |

## modules/telemetry/rtt_telemetry.h

源文件：[打开 modules/telemetry/rtt_telemetry.h](../modules/telemetry/rtt_telemetry.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/telemetry/rtt_telemetry.h:2](../modules/telemetry/rtt_telemetry.h#L2) | `RTT_TELEMETRY_H` | 空定义 | `#ifndef RTT_TELEMETRY_H` |  |
| [modules/telemetry/rtt_telemetry.h:12](../modules/telemetry/rtt_telemetry.h#L12) | `CHASSIS_POWER_CALIBRATION_ENABLE` | `0` | `#ifndef RTT_TELEMETRY_H；#ifndef CHASSIS_POWER_CALIBRATION_ENABLE` |  |
| [modules/telemetry/rtt_telemetry.h:19](../modules/telemetry/rtt_telemetry.h#L19) | `RTT_TELEMETRY_CHASSIS_POWER_CSV_MAX_LENGTH` | `320U` | `#ifndef RTT_TELEMETRY_H` | 预留的缓冲区大小 |

## modules/TFminiPlus/tfminiplus.c

源文件：[打开 modules/TFminiPlus/tfminiplus.c](../modules/TFminiPlus/tfminiplus.c)。

### 配置结构体初值

以下记录原始定义，不自动推断运行路径或后续覆盖。未填字段为零；指针/回调表示绑定关系。

[modules/TFminiPlus/tfminiplus.c:10](../modules/TFminiPlus/tfminiplus.c#L10)

```c
IIC_Init_Config_s conf = {
		.handle = hi2c,
		.dev_address = 0x10,
		.id = tfmini,
		.work_mode = IIC_BLOCK_MODE,
	};
```

## modules/TFminiPlus/tfminiplus.h

源文件：[打开 modules/TFminiPlus/tfminiplus.h](../modules/TFminiPlus/tfminiplus.h)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [modules/TFminiPlus/tfminiplus.h:2](../modules/TFminiPlus/tfminiplus.h#L2) | `__TFMINIPLUS_H__` | 空定义 | `#ifndef __TFMINIPLUS_H__` |  |

## Src/adc.c

源文件：[打开 Src/adc.c](../Src/adc.c)。

### HAL 设置赋值与中断优先级

同一变量可能在不同外设初始化中重复使用，按源代码行区分。

| 行 | 设置 |
| --- | --- |
| [Src/adc.c:45](../Src/adc.c#L45) | `hadc1.Instance = ADC1;` |
| [Src/adc.c:46](../Src/adc.c#L46) | `hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV6;` |
| [Src/adc.c:47](../Src/adc.c#L47) | `hadc1.Init.Resolution = ADC_RESOLUTION_12B;` |
| [Src/adc.c:48](../Src/adc.c#L48) | `hadc1.Init.ScanConvMode = DISABLE;` |
| [Src/adc.c:49](../Src/adc.c#L49) | `hadc1.Init.ContinuousConvMode = DISABLE;` |
| [Src/adc.c:50](../Src/adc.c#L50) | `hadc1.Init.DiscontinuousConvMode = DISABLE;` |
| [Src/adc.c:51](../Src/adc.c#L51) | `hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;` |
| [Src/adc.c:52](../Src/adc.c#L52) | `hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;` |
| [Src/adc.c:53](../Src/adc.c#L53) | `hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;` |
| [Src/adc.c:54](../Src/adc.c#L54) | `hadc1.Init.NbrOfConversion = 1;` |
| [Src/adc.c:55](../Src/adc.c#L55) | `hadc1.Init.DMAContinuousRequests = DISABLE;` |
| [Src/adc.c:56](../Src/adc.c#L56) | `hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;` |
| [Src/adc.c:64](../Src/adc.c#L64) | `sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;` |
| [Src/adc.c:65](../Src/adc.c#L65) | `sConfig.Rank = 1;` |
| [Src/adc.c:66](../Src/adc.c#L66) | `sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;` |

## Src/can.c

源文件：[打开 Src/can.c](../Src/can.c)。

### HAL 设置赋值与中断优先级

同一变量可能在不同外设初始化中重复使用，按源代码行区分。

| 行 | 设置 |
| --- | --- |
| [Src/can.c:41](../Src/can.c#L41) | `hcan1.Instance = CAN1;` |
| [Src/can.c:42](../Src/can.c#L42) | `hcan1.Init.Prescaler = 3;` |
| [Src/can.c:43](../Src/can.c#L43) | `hcan1.Init.Mode = CAN_MODE_NORMAL;` |
| [Src/can.c:44](../Src/can.c#L44) | `hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;` |
| [Src/can.c:45](../Src/can.c#L45) | `hcan1.Init.TimeSeg1 = CAN_BS1_10TQ;` |
| [Src/can.c:46](../Src/can.c#L46) | `hcan1.Init.TimeSeg2 = CAN_BS2_3TQ;` |
| [Src/can.c:47](../Src/can.c#L47) | `hcan1.Init.TimeTriggeredMode = DISABLE;` |
| [Src/can.c:48](../Src/can.c#L48) | `hcan1.Init.AutoBusOff = DISABLE;` |
| [Src/can.c:49](../Src/can.c#L49) | `hcan1.Init.AutoWakeUp = DISABLE;` |
| [Src/can.c:50](../Src/can.c#L50) | `hcan1.Init.AutoRetransmission = ENABLE;` |
| [Src/can.c:51](../Src/can.c#L51) | `hcan1.Init.ReceiveFifoLocked = DISABLE;` |
| [Src/can.c:52](../Src/can.c#L52) | `hcan1.Init.TransmitFifoPriority = ENABLE;` |
| [Src/can.c:73](../Src/can.c#L73) | `hcan2.Instance = CAN2;` |
| [Src/can.c:74](../Src/can.c#L74) | `hcan2.Init.Prescaler = 3;` |
| [Src/can.c:75](../Src/can.c#L75) | `hcan2.Init.Mode = CAN_MODE_NORMAL;` |
| [Src/can.c:76](../Src/can.c#L76) | `hcan2.Init.SyncJumpWidth = CAN_SJW_1TQ;` |
| [Src/can.c:77](../Src/can.c#L77) | `hcan2.Init.TimeSeg1 = CAN_BS1_10TQ;` |
| [Src/can.c:78](../Src/can.c#L78) | `hcan2.Init.TimeSeg2 = CAN_BS2_3TQ;` |
| [Src/can.c:79](../Src/can.c#L79) | `hcan2.Init.TimeTriggeredMode = DISABLE;` |
| [Src/can.c:80](../Src/can.c#L80) | `hcan2.Init.AutoBusOff = DISABLE;` |
| [Src/can.c:81](../Src/can.c#L81) | `hcan2.Init.AutoWakeUp = DISABLE;` |
| [Src/can.c:82](../Src/can.c#L82) | `hcan2.Init.AutoRetransmission = ENABLE;` |
| [Src/can.c:83](../Src/can.c#L83) | `hcan2.Init.ReceiveFifoLocked = DISABLE;` |
| [Src/can.c:84](../Src/can.c#L84) | `hcan2.Init.TransmitFifoPriority = ENABLE;` |
| [Src/can.c:117](../Src/can.c#L117) | `GPIO_InitStruct.Pin = GPIO_PIN_0\|GPIO_PIN_1;` |
| [Src/can.c:118](../Src/can.c#L118) | `GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;` |
| [Src/can.c:119](../Src/can.c#L119) | `GPIO_InitStruct.Pull = GPIO_NOPULL;` |
| [Src/can.c:120](../Src/can.c#L120) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;` |
| [Src/can.c:121](../Src/can.c#L121) | `GPIO_InitStruct.Alternate = GPIO_AF9_CAN1;` |
| [Src/can.c:125](../Src/can.c#L125) | `HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 5, 0);` |
| [Src/can.c:127](../Src/can.c#L127) | `HAL_NVIC_SetPriority(CAN1_RX1_IRQn, 5, 0);` |
| [Src/can.c:150](../Src/can.c#L150) | `GPIO_InitStruct.Pin = GPIO_PIN_5\|GPIO_PIN_6;` |
| [Src/can.c:151](../Src/can.c#L151) | `GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;` |
| [Src/can.c:152](../Src/can.c#L152) | `GPIO_InitStruct.Pull = GPIO_NOPULL;` |
| [Src/can.c:153](../Src/can.c#L153) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;` |
| [Src/can.c:154](../Src/can.c#L154) | `GPIO_InitStruct.Alternate = GPIO_AF9_CAN2;` |
| [Src/can.c:158](../Src/can.c#L158) | `HAL_NVIC_SetPriority(CAN2_RX0_IRQn, 5, 0);` |
| [Src/can.c:160](../Src/can.c#L160) | `HAL_NVIC_SetPriority(CAN2_RX1_IRQn, 5, 0);` |

## Src/crc.c

源文件：[打开 Src/crc.c](../Src/crc.c)。

### HAL 设置赋值与中断优先级

同一变量可能在不同外设初始化中重复使用，按源代码行区分。

| 行 | 设置 |
| --- | --- |
| [Src/crc.c:40](../Src/crc.c#L40) | `hcrc.Instance = CRC;` |

## Src/dac.c

源文件：[打开 Src/dac.c](../Src/dac.c)。

### HAL 设置赋值与中断优先级

同一变量可能在不同外设初始化中重复使用，按源代码行区分。

| 行 | 设置 |
| --- | --- |
| [Src/dac.c:45](../Src/dac.c#L45) | `hdac.Instance = DAC;` |
| [Src/dac.c:53](../Src/dac.c#L53) | `sConfig.DAC_Trigger = DAC_TRIGGER_NONE;` |
| [Src/dac.c:54](../Src/dac.c#L54) | `sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;` |
| [Src/dac.c:81](../Src/dac.c#L81) | `GPIO_InitStruct.Pin = GPIO_PIN_5;` |
| [Src/dac.c:82](../Src/dac.c#L82) | `GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;` |
| [Src/dac.c:83](../Src/dac.c#L83) | `GPIO_InitStruct.Pull = GPIO_NOPULL;` |

## Src/dma.c

源文件：[打开 Src/dma.c](../Src/dma.c)。

### HAL 设置赋值与中断优先级

同一变量可能在不同外设初始化中重复使用，按源代码行区分。

| 行 | 设置 |
| --- | --- |
| [Src/dma.c:48](../Src/dma.c#L48) | `HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 5, 0);` |
| [Src/dma.c:51](../Src/dma.c#L51) | `HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 5, 0);` |
| [Src/dma.c:54](../Src/dma.c#L54) | `HAL_NVIC_SetPriority(DMA1_Stream7_IRQn, 5, 0);` |
| [Src/dma.c:57](../Src/dma.c#L57) | `HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);` |
| [Src/dma.c:60](../Src/dma.c#L60) | `HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 5, 0);` |
| [Src/dma.c:63](../Src/dma.c#L63) | `HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 0, 0);` |
| [Src/dma.c:66](../Src/dma.c#L66) | `HAL_NVIC_SetPriority(DMA2_Stream5_IRQn, 0, 0);` |
| [Src/dma.c:69](../Src/dma.c#L69) | `HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 5, 0);` |
| [Src/dma.c:72](../Src/dma.c#L72) | `HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 5, 0);` |

## Src/gpio.c

源文件：[打开 Src/gpio.c](../Src/gpio.c)。

### HAL 设置赋值与中断优先级

同一变量可能在不同外设初始化中重复使用，按源代码行区分。

| 行 | 设置 |
| --- | --- |
| [Src/gpio.c:68](../Src/gpio.c#L68) | `GPIO_InitStruct.Pin = MAG_RST_Pin;` |
| [Src/gpio.c:69](../Src/gpio.c#L69) | `GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;` |
| [Src/gpio.c:70](../Src/gpio.c#L70) | `GPIO_InitStruct.Pull = GPIO_NOPULL;` |
| [Src/gpio.c:71](../Src/gpio.c#L71) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;` |
| [Src/gpio.c:75](../Src/gpio.c#L75) | `GPIO_InitStruct.Pin = INT_MAG_Pin;` |
| [Src/gpio.c:76](../Src/gpio.c#L76) | `GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;` |
| [Src/gpio.c:77](../Src/gpio.c#L77) | `GPIO_InitStruct.Pull = GPIO_PULLUP;` |
| [Src/gpio.c:81](../Src/gpio.c#L81) | `GPIO_InitStruct.Pin = KEY_Pin;` |
| [Src/gpio.c:82](../Src/gpio.c#L82) | `GPIO_InitStruct.Mode = GPIO_MODE_INPUT;` |
| [Src/gpio.c:83](../Src/gpio.c#L83) | `GPIO_InitStruct.Pull = GPIO_PULLUP;` |
| [Src/gpio.c:87](../Src/gpio.c#L87) | `GPIO_InitStruct.Pin = CS1_ACCEL_Pin;` |
| [Src/gpio.c:88](../Src/gpio.c#L88) | `GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;` |
| [Src/gpio.c:89](../Src/gpio.c#L89) | `GPIO_InitStruct.Pull = GPIO_PULLUP;` |
| [Src/gpio.c:90](../Src/gpio.c#L90) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;` |
| [Src/gpio.c:94](../Src/gpio.c#L94) | `GPIO_InitStruct.Pin = INT_ACC_Pin\|INT_GYRO_Pin;` |
| [Src/gpio.c:95](../Src/gpio.c#L95) | `GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;` |
| [Src/gpio.c:96](../Src/gpio.c#L96) | `GPIO_InitStruct.Pull = GPIO_NOPULL;` |
| [Src/gpio.c:100](../Src/gpio.c#L100) | `GPIO_InitStruct.Pin = CS1_GYRO_Pin;` |
| [Src/gpio.c:101](../Src/gpio.c#L101) | `GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;` |
| [Src/gpio.c:102](../Src/gpio.c#L102) | `GPIO_InitStruct.Pull = GPIO_PULLUP;` |
| [Src/gpio.c:103](../Src/gpio.c#L103) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;` |
| [Src/gpio.c:107](../Src/gpio.c#L107) | `HAL_NVIC_SetPriority(EXTI3_IRQn, 5, 0);` |
| [Src/gpio.c:110](../Src/gpio.c#L110) | `HAL_NVIC_SetPriority(EXTI4_IRQn, 5, 0);` |
| [Src/gpio.c:113](../Src/gpio.c#L113) | `HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);` |

## Src/i2c.c

源文件：[打开 Src/i2c.c](../Src/i2c.c)。

### HAL 设置赋值与中断优先级

同一变量可能在不同外设初始化中重复使用，按源代码行区分。

| 行 | 设置 |
| --- | --- |
| [Src/i2c.c:43](../Src/i2c.c#L43) | `hi2c2.Instance = I2C2;` |
| [Src/i2c.c:44](../Src/i2c.c#L44) | `hi2c2.Init.ClockSpeed = 400000;` |
| [Src/i2c.c:45](../Src/i2c.c#L45) | `hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;` |
| [Src/i2c.c:46](../Src/i2c.c#L46) | `hi2c2.Init.OwnAddress1 = 0;` |
| [Src/i2c.c:47](../Src/i2c.c#L47) | `hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;` |
| [Src/i2c.c:48](../Src/i2c.c#L48) | `hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;` |
| [Src/i2c.c:49](../Src/i2c.c#L49) | `hi2c2.Init.OwnAddress2 = 0;` |
| [Src/i2c.c:50](../Src/i2c.c#L50) | `hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;` |
| [Src/i2c.c:51](../Src/i2c.c#L51) | `hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;` |
| [Src/i2c.c:72](../Src/i2c.c#L72) | `hi2c3.Instance = I2C3;` |
| [Src/i2c.c:73](../Src/i2c.c#L73) | `hi2c3.Init.ClockSpeed = 400000;` |
| [Src/i2c.c:74](../Src/i2c.c#L74) | `hi2c3.Init.DutyCycle = I2C_DUTYCYCLE_2;` |
| [Src/i2c.c:75](../Src/i2c.c#L75) | `hi2c3.Init.OwnAddress1 = 0;` |
| [Src/i2c.c:76](../Src/i2c.c#L76) | `hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;` |
| [Src/i2c.c:77](../Src/i2c.c#L77) | `hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;` |
| [Src/i2c.c:78](../Src/i2c.c#L78) | `hi2c3.Init.OwnAddress2 = 0;` |
| [Src/i2c.c:79](../Src/i2c.c#L79) | `hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;` |
| [Src/i2c.c:80](../Src/i2c.c#L80) | `hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;` |
| [Src/i2c.c:106](../Src/i2c.c#L106) | `GPIO_InitStruct.Pin = GPIO_PIN_0\|GPIO_PIN_1;` |
| [Src/i2c.c:107](../Src/i2c.c#L107) | `GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;` |
| [Src/i2c.c:108](../Src/i2c.c#L108) | `GPIO_InitStruct.Pull = GPIO_NOPULL;` |
| [Src/i2c.c:109](../Src/i2c.c#L109) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;` |
| [Src/i2c.c:110](../Src/i2c.c#L110) | `GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;` |
| [Src/i2c.c:118](../Src/i2c.c#L118) | `hdma_i2c2_rx.Instance = DMA1_Stream2;` |
| [Src/i2c.c:119](../Src/i2c.c#L119) | `hdma_i2c2_rx.Init.Channel = DMA_CHANNEL_7;` |
| [Src/i2c.c:120](../Src/i2c.c#L120) | `hdma_i2c2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;` |
| [Src/i2c.c:121](../Src/i2c.c#L121) | `hdma_i2c2_rx.Init.PeriphInc = DMA_PINC_DISABLE;` |
| [Src/i2c.c:122](../Src/i2c.c#L122) | `hdma_i2c2_rx.Init.MemInc = DMA_MINC_ENABLE;` |
| [Src/i2c.c:123](../Src/i2c.c#L123) | `hdma_i2c2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;` |
| [Src/i2c.c:124](../Src/i2c.c#L124) | `hdma_i2c2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;` |
| [Src/i2c.c:125](../Src/i2c.c#L125) | `hdma_i2c2_rx.Init.Mode = DMA_NORMAL;` |
| [Src/i2c.c:126](../Src/i2c.c#L126) | `hdma_i2c2_rx.Init.Priority = DMA_PRIORITY_LOW;` |
| [Src/i2c.c:127](../Src/i2c.c#L127) | `hdma_i2c2_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;` |
| [Src/i2c.c:136](../Src/i2c.c#L136) | `hdma_i2c2_tx.Instance = DMA1_Stream7;` |
| [Src/i2c.c:137](../Src/i2c.c#L137) | `hdma_i2c2_tx.Init.Channel = DMA_CHANNEL_7;` |
| [Src/i2c.c:138](../Src/i2c.c#L138) | `hdma_i2c2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;` |
| [Src/i2c.c:139](../Src/i2c.c#L139) | `hdma_i2c2_tx.Init.PeriphInc = DMA_PINC_DISABLE;` |
| [Src/i2c.c:140](../Src/i2c.c#L140) | `hdma_i2c2_tx.Init.MemInc = DMA_MINC_ENABLE;` |
| [Src/i2c.c:141](../Src/i2c.c#L141) | `hdma_i2c2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;` |
| [Src/i2c.c:142](../Src/i2c.c#L142) | `hdma_i2c2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;` |
| [Src/i2c.c:143](../Src/i2c.c#L143) | `hdma_i2c2_tx.Init.Mode = DMA_NORMAL;` |
| [Src/i2c.c:144](../Src/i2c.c#L144) | `hdma_i2c2_tx.Init.Priority = DMA_PRIORITY_LOW;` |
| [Src/i2c.c:145](../Src/i2c.c#L145) | `hdma_i2c2_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;` |
| [Src/i2c.c:154](../Src/i2c.c#L154) | `HAL_NVIC_SetPriority(I2C2_EV_IRQn, 5, 0);` |
| [Src/i2c.c:156](../Src/i2c.c#L156) | `HAL_NVIC_SetPriority(I2C2_ER_IRQn, 5, 0);` |
| [Src/i2c.c:174](../Src/i2c.c#L174) | `GPIO_InitStruct.Pin = GPIO_PIN_9;` |
| [Src/i2c.c:175](../Src/i2c.c#L175) | `GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;` |
| [Src/i2c.c:176](../Src/i2c.c#L176) | `GPIO_InitStruct.Pull = GPIO_NOPULL;` |
| [Src/i2c.c:177](../Src/i2c.c#L177) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;` |
| [Src/i2c.c:178](../Src/i2c.c#L178) | `GPIO_InitStruct.Alternate = GPIO_AF4_I2C3;` |
| [Src/i2c.c:181](../Src/i2c.c#L181) | `GPIO_InitStruct.Pin = GPIO_PIN_8;` |
| [Src/i2c.c:182](../Src/i2c.c#L182) | `GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;` |
| [Src/i2c.c:183](../Src/i2c.c#L183) | `GPIO_InitStruct.Pull = GPIO_NOPULL;` |
| [Src/i2c.c:184](../Src/i2c.c#L184) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;` |
| [Src/i2c.c:185](../Src/i2c.c#L185) | `GPIO_InitStruct.Alternate = GPIO_AF4_I2C3;` |
| [Src/i2c.c:192](../Src/i2c.c#L192) | `HAL_NVIC_SetPriority(I2C3_EV_IRQn, 5, 0);` |
| [Src/i2c.c:194](../Src/i2c.c#L194) | `HAL_NVIC_SetPriority(I2C3_ER_IRQn, 5, 0);` |

## Src/main.c

源文件：[打开 Src/main.c](../Src/main.c)。

### HAL 设置赋值与中断优先级

同一变量可能在不同外设初始化中重复使用，按源代码行区分。

| 行 | 设置 |
| --- | --- |
| [Src/main.c:162](../Src/main.c#L162) | `RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;` |
| [Src/main.c:163](../Src/main.c#L163) | `RCC_OscInitStruct.HSEState = RCC_HSE_ON;` |
| [Src/main.c:164](../Src/main.c#L164) | `RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;` |
| [Src/main.c:165](../Src/main.c#L165) | `RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;` |
| [Src/main.c:166](../Src/main.c#L166) | `RCC_OscInitStruct.PLL.PLLM = 6;` |
| [Src/main.c:167](../Src/main.c#L167) | `RCC_OscInitStruct.PLL.PLLN = 168;` |
| [Src/main.c:168](../Src/main.c#L168) | `RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;` |
| [Src/main.c:169](../Src/main.c#L169) | `RCC_OscInitStruct.PLL.PLLQ = 7;` |
| [Src/main.c:177](../Src/main.c#L177) | `RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK\|RCC_CLOCKTYPE_SYSCLK \|RCC_CLOCKTYPE_PCLK1\|RCC_CLOCKTYPE_PCLK2;` |
| [Src/main.c:179](../Src/main.c#L179) | `RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;` |
| [Src/main.c:180](../Src/main.c#L180) | `RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;` |
| [Src/main.c:181](../Src/main.c#L181) | `RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;` |
| [Src/main.c:182](../Src/main.c#L182) | `RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;` |

## Src/rng.c

源文件：[打开 Src/rng.c](../Src/rng.c)。

### HAL 设置赋值与中断优先级

同一变量可能在不同外设初始化中重复使用，按源代码行区分。

| 行 | 设置 |
| --- | --- |
| [Src/rng.c:40](../Src/rng.c#L40) | `hrng.Instance = RNG;` |

## Src/rtc.c

源文件：[打开 Src/rtc.c](../Src/rtc.c)。

### HAL 设置赋值与中断优先级

同一变量可能在不同外设初始化中重复使用，按源代码行区分。

| 行 | 设置 |
| --- | --- |
| [Src/rtc.c:43](../Src/rtc.c#L43) | `hrtc.Instance = RTC;` |
| [Src/rtc.c:44](../Src/rtc.c#L44) | `hrtc.Init.HourFormat = RTC_HOURFORMAT_24;` |
| [Src/rtc.c:45](../Src/rtc.c#L45) | `hrtc.Init.AsynchPrediv = 124;` |
| [Src/rtc.c:46](../Src/rtc.c#L46) | `hrtc.Init.SynchPrediv = 3199;` |
| [Src/rtc.c:47](../Src/rtc.c#L47) | `hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;` |
| [Src/rtc.c:48](../Src/rtc.c#L48) | `hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;` |
| [Src/rtc.c:49](../Src/rtc.c#L49) | `hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;` |
| [Src/rtc.c:72](../Src/rtc.c#L72) | `PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;` |
| [Src/rtc.c:73](../Src/rtc.c#L73) | `PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_HSE_DIV30;` |

## Src/spi.c

源文件：[打开 Src/spi.c](../Src/spi.c)。

### HAL 设置赋值与中断优先级

同一变量可能在不同外设初始化中重复使用，按源代码行区分。

| 行 | 设置 |
| --- | --- |
| [Src/spi.c:43](../Src/spi.c#L43) | `hspi1.Instance = SPI1;` |
| [Src/spi.c:44](../Src/spi.c#L44) | `hspi1.Init.Mode = SPI_MODE_MASTER;` |
| [Src/spi.c:45](../Src/spi.c#L45) | `hspi1.Init.Direction = SPI_DIRECTION_2LINES;` |
| [Src/spi.c:46](../Src/spi.c#L46) | `hspi1.Init.DataSize = SPI_DATASIZE_8BIT;` |
| [Src/spi.c:47](../Src/spi.c#L47) | `hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;` |
| [Src/spi.c:48](../Src/spi.c#L48) | `hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;` |
| [Src/spi.c:49](../Src/spi.c#L49) | `hspi1.Init.NSS = SPI_NSS_SOFT;` |
| [Src/spi.c:50](../Src/spi.c#L50) | `hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;` |
| [Src/spi.c:51](../Src/spi.c#L51) | `hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;` |
| [Src/spi.c:52](../Src/spi.c#L52) | `hspi1.Init.TIMode = SPI_TIMODE_DISABLE;` |
| [Src/spi.c:53](../Src/spi.c#L53) | `hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;` |
| [Src/spi.c:54](../Src/spi.c#L54) | `hspi1.Init.CRCPolynomial = 10;` |
| [Src/spi.c:75](../Src/spi.c#L75) | `hspi2.Instance = SPI2;` |
| [Src/spi.c:76](../Src/spi.c#L76) | `hspi2.Init.Mode = SPI_MODE_MASTER;` |
| [Src/spi.c:77](../Src/spi.c#L77) | `hspi2.Init.Direction = SPI_DIRECTION_2LINES;` |
| [Src/spi.c:78](../Src/spi.c#L78) | `hspi2.Init.DataSize = SPI_DATASIZE_8BIT;` |
| [Src/spi.c:79](../Src/spi.c#L79) | `hspi2.Init.CLKPolarity = SPI_POLARITY_HIGH;` |
| [Src/spi.c:80](../Src/spi.c#L80) | `hspi2.Init.CLKPhase = SPI_PHASE_2EDGE;` |
| [Src/spi.c:81](../Src/spi.c#L81) | `hspi2.Init.NSS = SPI_NSS_SOFT;` |
| [Src/spi.c:82](../Src/spi.c#L82) | `hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;` |
| [Src/spi.c:83](../Src/spi.c#L83) | `hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;` |
| [Src/spi.c:84](../Src/spi.c#L84) | `hspi2.Init.TIMode = SPI_TIMODE_DISABLE;` |
| [Src/spi.c:85](../Src/spi.c#L85) | `hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;` |
| [Src/spi.c:86](../Src/spi.c#L86) | `hspi2.Init.CRCPolynomial = 10;` |
| [Src/spi.c:116](../Src/spi.c#L116) | `GPIO_InitStruct.Pin = GPIO_PIN_4\|GPIO_PIN_3;` |
| [Src/spi.c:117](../Src/spi.c#L117) | `GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;` |
| [Src/spi.c:118](../Src/spi.c#L118) | `GPIO_InitStruct.Pull = GPIO_PULLUP;` |
| [Src/spi.c:119](../Src/spi.c#L119) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;` |
| [Src/spi.c:120](../Src/spi.c#L120) | `GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;` |
| [Src/spi.c:123](../Src/spi.c#L123) | `GPIO_InitStruct.Pin = GPIO_PIN_7;` |
| [Src/spi.c:124](../Src/spi.c#L124) | `GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;` |
| [Src/spi.c:125](../Src/spi.c#L125) | `GPIO_InitStruct.Pull = GPIO_PULLUP;` |
| [Src/spi.c:126](../Src/spi.c#L126) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;` |
| [Src/spi.c:127](../Src/spi.c#L127) | `GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;` |
| [Src/spi.c:132](../Src/spi.c#L132) | `hdma_spi1_rx.Instance = DMA2_Stream0;` |
| [Src/spi.c:133](../Src/spi.c#L133) | `hdma_spi1_rx.Init.Channel = DMA_CHANNEL_3;` |
| [Src/spi.c:134](../Src/spi.c#L134) | `hdma_spi1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;` |
| [Src/spi.c:135](../Src/spi.c#L135) | `hdma_spi1_rx.Init.PeriphInc = DMA_PINC_DISABLE;` |
| [Src/spi.c:136](../Src/spi.c#L136) | `hdma_spi1_rx.Init.MemInc = DMA_MINC_ENABLE;` |
| [Src/spi.c:137](../Src/spi.c#L137) | `hdma_spi1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;` |
| [Src/spi.c:138](../Src/spi.c#L138) | `hdma_spi1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;` |
| [Src/spi.c:139](../Src/spi.c#L139) | `hdma_spi1_rx.Init.Mode = DMA_NORMAL;` |
| [Src/spi.c:140](../Src/spi.c#L140) | `hdma_spi1_rx.Init.Priority = DMA_PRIORITY_VERY_HIGH;` |
| [Src/spi.c:141](../Src/spi.c#L141) | `hdma_spi1_rx.Init.FIFOMode = DMA_FIFOMODE_ENABLE;` |
| [Src/spi.c:142](../Src/spi.c#L142) | `hdma_spi1_rx.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;` |
| [Src/spi.c:143](../Src/spi.c#L143) | `hdma_spi1_rx.Init.MemBurst = DMA_MBURST_SINGLE;` |
| [Src/spi.c:144](../Src/spi.c#L144) | `hdma_spi1_rx.Init.PeriphBurst = DMA_PBURST_SINGLE;` |
| [Src/spi.c:153](../Src/spi.c#L153) | `hdma_spi1_tx.Instance = DMA2_Stream3;` |
| [Src/spi.c:154](../Src/spi.c#L154) | `hdma_spi1_tx.Init.Channel = DMA_CHANNEL_3;` |
| [Src/spi.c:155](../Src/spi.c#L155) | `hdma_spi1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;` |
| [Src/spi.c:156](../Src/spi.c#L156) | `hdma_spi1_tx.Init.PeriphInc = DMA_PINC_DISABLE;` |
| [Src/spi.c:157](../Src/spi.c#L157) | `hdma_spi1_tx.Init.MemInc = DMA_MINC_ENABLE;` |
| [Src/spi.c:158](../Src/spi.c#L158) | `hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;` |
| [Src/spi.c:159](../Src/spi.c#L159) | `hdma_spi1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;` |
| [Src/spi.c:160](../Src/spi.c#L160) | `hdma_spi1_tx.Init.Mode = DMA_NORMAL;` |
| [Src/spi.c:161](../Src/spi.c#L161) | `hdma_spi1_tx.Init.Priority = DMA_PRIORITY_VERY_HIGH;` |
| [Src/spi.c:162](../Src/spi.c#L162) | `hdma_spi1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;` |
| [Src/spi.c:171](../Src/spi.c#L171) | `HAL_NVIC_SetPriority(SPI1_IRQn, 5, 0);` |
| [Src/spi.c:191](../Src/spi.c#L191) | `GPIO_InitStruct.Pin = GPIO_PIN_13\|GPIO_PIN_14\|GPIO_PIN_15;` |
| [Src/spi.c:192](../Src/spi.c#L192) | `GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;` |
| [Src/spi.c:193](../Src/spi.c#L193) | `GPIO_InitStruct.Pull = GPIO_NOPULL;` |
| [Src/spi.c:194](../Src/spi.c#L194) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;` |
| [Src/spi.c:195](../Src/spi.c#L195) | `GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;` |
| [Src/spi.c:199](../Src/spi.c#L199) | `HAL_NVIC_SetPriority(SPI2_IRQn, 5, 0);` |

## Src/stm32f4xx_hal_msp.c

源文件：[打开 Src/stm32f4xx_hal_msp.c](../Src/stm32f4xx_hal_msp.c)。

### HAL 设置赋值与中断优先级

同一变量可能在不同外设初始化中重复使用，按源代码行区分。

| 行 | 设置 |
| --- | --- |
| [Src/stm32f4xx_hal_msp.c:74](../Src/stm32f4xx_hal_msp.c#L74) | `HAL_NVIC_SetPriority(PendSV_IRQn, 15, 0);` |
| [Src/stm32f4xx_hal_msp.c:78](../Src/stm32f4xx_hal_msp.c#L78) | `HAL_NVIC_SetPriority(FLASH_IRQn, 6, 0);` |

## Src/stm32f4xx_hal_timebase_tim.c

源文件：[打开 Src/stm32f4xx_hal_timebase_tim.c](../Src/stm32f4xx_hal_timebase_tim.c)。

### HAL 设置赋值与中断优先级

同一变量可能在不同外设初始化中重复使用，按源代码行区分。

| 行 | 设置 |
| --- | --- |
| [Src/stm32f4xx_hal_timebase_tim.c:72](../Src/stm32f4xx_hal_timebase_tim.c#L72) | `htim14.Instance = TIM14;` |
| [Src/stm32f4xx_hal_timebase_tim.c:80](../Src/stm32f4xx_hal_timebase_tim.c#L80) | `htim14.Init.Period = (1000000U / 1000U) - 1U;` |
| [Src/stm32f4xx_hal_timebase_tim.c:81](../Src/stm32f4xx_hal_timebase_tim.c#L81) | `htim14.Init.Prescaler = uwPrescalerValue;` |
| [Src/stm32f4xx_hal_timebase_tim.c:82](../Src/stm32f4xx_hal_timebase_tim.c#L82) | `htim14.Init.ClockDivision = 0;` |
| [Src/stm32f4xx_hal_timebase_tim.c:83](../Src/stm32f4xx_hal_timebase_tim.c#L83) | `htim14.Init.CounterMode = TIM_COUNTERMODE_UP;` |
| [Src/stm32f4xx_hal_timebase_tim.c:84](../Src/stm32f4xx_hal_timebase_tim.c#L84) | `htim14.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;` |
| [Src/stm32f4xx_hal_timebase_tim.c:99](../Src/stm32f4xx_hal_timebase_tim.c#L99) | `HAL_NVIC_SetPriority(TIM8_TRG_COM_TIM14_IRQn, TickPriority, 0U);` |

## Src/system_stm32f4xx.c

源文件：[打开 Src/system_stm32f4xx.c](../Src/system_stm32f4xx.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Src/system_stm32f4xx.c:51](../Src/system_stm32f4xx.c#L51) | `HSE_VALUE` | `((uint32_t)25000000)` | `#if !defined (HSE_VALUE)` | !< Default value of the External oscillator in Hz |
| [Src/system_stm32f4xx.c:55](../Src/system_stm32f4xx.c#L55) | `HSI_VALUE` | `((uint32_t)16000000)` | `#if !defined (HSI_VALUE)` | !< Value of the Internal oscillator in Hz |
| [Src/system_stm32f4xx.c:101](../Src/system_stm32f4xx.c#L101) | `VECT_TAB_BASE_ADDRESS` | `SRAM_BASE` | `#if defined(USER_VECT_TAB_ADDRESS)；#if defined(VECT_TAB_SRAM)` |  |
| [Src/system_stm32f4xx.c:103](../Src/system_stm32f4xx.c#L103) | `VECT_TAB_OFFSET` | `0x00000000U` | `#if defined(USER_VECT_TAB_ADDRESS)；#if defined(VECT_TAB_SRAM)` |  |
| [Src/system_stm32f4xx.c:106](../Src/system_stm32f4xx.c#L106) | `VECT_TAB_BASE_ADDRESS` | `FLASH_BASE` | `#if defined(USER_VECT_TAB_ADDRESS)；#if defined(VECT_TAB_SRAM) → #else` |  |
| [Src/system_stm32f4xx.c:108](../Src/system_stm32f4xx.c#L108) | `VECT_TAB_OFFSET` | `0x00000000U` | `#if defined(USER_VECT_TAB_ADDRESS)；#if defined(VECT_TAB_SRAM) → #else` |  |

### 已注释的宏选项（当前不定义）

| 行 | 原选项 |
| --- | --- |
| [Src/system_stm32f4xx.c:79](../Src/system_stm32f4xx.c#L79) | `#define DATA_IN_ExtSRAM` |
| [Src/system_stm32f4xx.c:85](../Src/system_stm32f4xx.c#L85) | `#define DATA_IN_ExtSDRAM` |
| [Src/system_stm32f4xx.c:94](../Src/system_stm32f4xx.c#L94) | `#define USER_VECT_TAB_ADDRESS` |
| [Src/system_stm32f4xx.c:99](../Src/system_stm32f4xx.c#L99) | `#define VECT_TAB_SRAM` |

## Src/tim.c

源文件：[打开 Src/tim.c](../Src/tim.c)。

### HAL 设置赋值与中断优先级

同一变量可能在不同外设初始化中重复使用，按源代码行区分。

| 行 | 设置 |
| --- | --- |
| [Src/tim.c:49](../Src/tim.c#L49) | `htim1.Instance = TIM1;` |
| [Src/tim.c:50](../Src/tim.c#L50) | `htim1.Init.Prescaler = 167;` |
| [Src/tim.c:51](../Src/tim.c#L51) | `htim1.Init.CounterMode = TIM_COUNTERMODE_UP;` |
| [Src/tim.c:52](../Src/tim.c#L52) | `htim1.Init.Period = 19999;` |
| [Src/tim.c:53](../Src/tim.c#L53) | `htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;` |
| [Src/tim.c:54](../Src/tim.c#L54) | `htim1.Init.RepetitionCounter = 0;` |
| [Src/tim.c:55](../Src/tim.c#L55) | `htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;` |
| [Src/tim.c:60](../Src/tim.c#L60) | `sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;` |
| [Src/tim.c:69](../Src/tim.c#L69) | `sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;` |
| [Src/tim.c:70](../Src/tim.c#L70) | `sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;` |
| [Src/tim.c:75](../Src/tim.c#L75) | `sConfigOC.OCMode = TIM_OCMODE_PWM1;` |
| [Src/tim.c:76](../Src/tim.c#L76) | `sConfigOC.Pulse = 0;` |
| [Src/tim.c:77](../Src/tim.c#L77) | `sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;` |
| [Src/tim.c:78](../Src/tim.c#L78) | `sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;` |
| [Src/tim.c:79](../Src/tim.c#L79) | `sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;` |
| [Src/tim.c:80](../Src/tim.c#L80) | `sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;` |
| [Src/tim.c:81](../Src/tim.c#L81) | `sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;` |
| [Src/tim.c:98](../Src/tim.c#L98) | `sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;` |
| [Src/tim.c:99](../Src/tim.c#L99) | `sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;` |
| [Src/tim.c:100](../Src/tim.c#L100) | `sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;` |
| [Src/tim.c:101](../Src/tim.c#L101) | `sBreakDeadTimeConfig.DeadTime = 0;` |
| [Src/tim.c:102](../Src/tim.c#L102) | `sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;` |
| [Src/tim.c:103](../Src/tim.c#L103) | `sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;` |
| [Src/tim.c:104](../Src/tim.c#L104) | `sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;` |
| [Src/tim.c:130](../Src/tim.c#L130) | `htim4.Instance = TIM4;` |
| [Src/tim.c:131](../Src/tim.c#L131) | `htim4.Init.Prescaler = 167;` |
| [Src/tim.c:132](../Src/tim.c#L132) | `htim4.Init.CounterMode = TIM_COUNTERMODE_UP;` |
| [Src/tim.c:133](../Src/tim.c#L133) | `htim4.Init.Period = 65535;` |
| [Src/tim.c:134](../Src/tim.c#L134) | `htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;` |
| [Src/tim.c:135](../Src/tim.c#L135) | `htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;` |
| [Src/tim.c:140](../Src/tim.c#L140) | `sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;` |
| [Src/tim.c:149](../Src/tim.c#L149) | `sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;` |
| [Src/tim.c:150](../Src/tim.c#L150) | `sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;` |
| [Src/tim.c:155](../Src/tim.c#L155) | `sConfigOC.OCMode = TIM_OCMODE_PWM1;` |
| [Src/tim.c:156](../Src/tim.c#L156) | `sConfigOC.Pulse = 0;` |
| [Src/tim.c:157](../Src/tim.c#L157) | `sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;` |
| [Src/tim.c:158](../Src/tim.c#L158) | `sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;` |
| [Src/tim.c:184](../Src/tim.c#L184) | `htim5.Instance = TIM5;` |
| [Src/tim.c:185](../Src/tim.c#L185) | `htim5.Init.Prescaler = 0;` |
| [Src/tim.c:186](../Src/tim.c#L186) | `htim5.Init.CounterMode = TIM_COUNTERMODE_UP;` |
| [Src/tim.c:187](../Src/tim.c#L187) | `htim5.Init.Period = 65535;` |
| [Src/tim.c:188](../Src/tim.c#L188) | `htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;` |
| [Src/tim.c:189](../Src/tim.c#L189) | `htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;` |
| [Src/tim.c:194](../Src/tim.c#L194) | `sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;` |
| [Src/tim.c:203](../Src/tim.c#L203) | `sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;` |
| [Src/tim.c:204](../Src/tim.c#L204) | `sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;` |
| [Src/tim.c:209](../Src/tim.c#L209) | `sConfigOC.OCMode = TIM_OCMODE_PWM1;` |
| [Src/tim.c:210](../Src/tim.c#L210) | `sConfigOC.Pulse = 0;` |
| [Src/tim.c:211](../Src/tim.c#L211) | `sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;` |
| [Src/tim.c:212](../Src/tim.c#L212) | `sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;` |
| [Src/tim.c:247](../Src/tim.c#L247) | `htim8.Instance = TIM8;` |
| [Src/tim.c:248](../Src/tim.c#L248) | `htim8.Init.Prescaler = 167;` |
| [Src/tim.c:249](../Src/tim.c#L249) | `htim8.Init.CounterMode = TIM_COUNTERMODE_UP;` |
| [Src/tim.c:250](../Src/tim.c#L250) | `htim8.Init.Period = 19999;` |
| [Src/tim.c:251](../Src/tim.c#L251) | `htim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;` |
| [Src/tim.c:252](../Src/tim.c#L252) | `htim8.Init.RepetitionCounter = 0;` |
| [Src/tim.c:253](../Src/tim.c#L253) | `htim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;` |
| [Src/tim.c:258](../Src/tim.c#L258) | `sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;` |
| [Src/tim.c:267](../Src/tim.c#L267) | `sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;` |
| [Src/tim.c:268](../Src/tim.c#L268) | `sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;` |
| [Src/tim.c:273](../Src/tim.c#L273) | `sConfigOC.OCMode = TIM_OCMODE_TIMING;` |
| [Src/tim.c:274](../Src/tim.c#L274) | `sConfigOC.Pulse = 0;` |
| [Src/tim.c:275](../Src/tim.c#L275) | `sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;` |
| [Src/tim.c:276](../Src/tim.c#L276) | `sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;` |
| [Src/tim.c:277](../Src/tim.c#L277) | `sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;` |
| [Src/tim.c:278](../Src/tim.c#L278) | `sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;` |
| [Src/tim.c:279](../Src/tim.c#L279) | `sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;` |
| [Src/tim.c:292](../Src/tim.c#L292) | `sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;` |
| [Src/tim.c:293](../Src/tim.c#L293) | `sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;` |
| [Src/tim.c:294](../Src/tim.c#L294) | `sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;` |
| [Src/tim.c:295](../Src/tim.c#L295) | `sBreakDeadTimeConfig.DeadTime = 0;` |
| [Src/tim.c:296](../Src/tim.c#L296) | `sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;` |
| [Src/tim.c:297](../Src/tim.c#L297) | `sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;` |
| [Src/tim.c:298](../Src/tim.c#L298) | `sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;` |
| [Src/tim.c:322](../Src/tim.c#L322) | `htim10.Instance = TIM10;` |
| [Src/tim.c:323](../Src/tim.c#L323) | `htim10.Init.Prescaler = 167;` |
| [Src/tim.c:324](../Src/tim.c#L324) | `htim10.Init.CounterMode = TIM_COUNTERMODE_UP;` |
| [Src/tim.c:325](../Src/tim.c#L325) | `htim10.Init.Period = 5000-1;` |
| [Src/tim.c:326](../Src/tim.c#L326) | `htim10.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;` |
| [Src/tim.c:327](../Src/tim.c#L327) | `htim10.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;` |
| [Src/tim.c:336](../Src/tim.c#L336) | `sConfigOC.OCMode = TIM_OCMODE_PWM1;` |
| [Src/tim.c:337](../Src/tim.c#L337) | `sConfigOC.Pulse = 0;` |
| [Src/tim.c:338](../Src/tim.c#L338) | `sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;` |
| [Src/tim.c:339](../Src/tim.c#L339) | `sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;` |
| [Src/tim.c:396](../Src/tim.c#L396) | `HAL_NVIC_SetPriority(TIM8_TRG_COM_TIM14_IRQn, 15, 0);` |
| [Src/tim.c:430](../Src/tim.c#L430) | `GPIO_InitStruct.Pin = GPIO_PIN_13\|SERVO_Pin\|GPIO_PIN_11\|GPIO_PIN_14;` |
| [Src/tim.c:431](../Src/tim.c#L431) | `GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;` |
| [Src/tim.c:432](../Src/tim.c#L432) | `GPIO_InitStruct.Pull = GPIO_NOPULL;` |
| [Src/tim.c:433](../Src/tim.c#L433) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;` |
| [Src/tim.c:434](../Src/tim.c#L434) | `GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;` |
| [Src/tim.c:451](../Src/tim.c#L451) | `GPIO_InitStruct.Pin = BUZZER_Pin;` |
| [Src/tim.c:452](../Src/tim.c#L452) | `GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;` |
| [Src/tim.c:453](../Src/tim.c#L453) | `GPIO_InitStruct.Pull = GPIO_PULLUP;` |
| [Src/tim.c:454](../Src/tim.c#L454) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;` |
| [Src/tim.c:455](../Src/tim.c#L455) | `GPIO_InitStruct.Alternate = GPIO_AF2_TIM4;` |
| [Src/tim.c:474](../Src/tim.c#L474) | `GPIO_InitStruct.Pin = LED_R_Pin\|LED_G_Pin\|LED_B_Pin;` |
| [Src/tim.c:475](../Src/tim.c#L475) | `GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;` |
| [Src/tim.c:476](../Src/tim.c#L476) | `GPIO_InitStruct.Pull = GPIO_PULLUP;` |
| [Src/tim.c:477](../Src/tim.c#L477) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;` |
| [Src/tim.c:478](../Src/tim.c#L478) | `GPIO_InitStruct.Alternate = GPIO_AF2_TIM5;` |
| [Src/tim.c:498](../Src/tim.c#L498) | `GPIO_InitStruct.Pin = GPIO_PIN_7\|GPIO_PIN_6;` |
| [Src/tim.c:499](../Src/tim.c#L499) | `GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;` |
| [Src/tim.c:500](../Src/tim.c#L500) | `GPIO_InitStruct.Pull = GPIO_NOPULL;` |
| [Src/tim.c:501](../Src/tim.c#L501) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;` |
| [Src/tim.c:502](../Src/tim.c#L502) | `GPIO_InitStruct.Alternate = GPIO_AF3_TIM8;` |
| [Src/tim.c:505](../Src/tim.c#L505) | `GPIO_InitStruct.Pin = GPIO_PIN_6;` |
| [Src/tim.c:506](../Src/tim.c#L506) | `GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;` |
| [Src/tim.c:507](../Src/tim.c#L507) | `GPIO_InitStruct.Pull = GPIO_NOPULL;` |
| [Src/tim.c:508](../Src/tim.c#L508) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;` |
| [Src/tim.c:509](../Src/tim.c#L509) | `GPIO_InitStruct.Alternate = GPIO_AF3_TIM8;` |
| [Src/tim.c:526](../Src/tim.c#L526) | `GPIO_InitStruct.Pin = IMU_TEMP_Pin;` |
| [Src/tim.c:527](../Src/tim.c#L527) | `GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;` |
| [Src/tim.c:528](../Src/tim.c#L528) | `GPIO_InitStruct.Pull = GPIO_PULLUP;` |
| [Src/tim.c:529](../Src/tim.c#L529) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;` |
| [Src/tim.c:530](../Src/tim.c#L530) | `GPIO_InitStruct.Alternate = GPIO_AF3_TIM10;` |

## Src/usart.c

源文件：[打开 Src/usart.c](../Src/usart.c)。

### HAL 设置赋值与中断优先级

同一变量可能在不同外设初始化中重复使用，按源代码行区分。

| 行 | 设置 |
| --- | --- |
| [Src/usart.c:48](../Src/usart.c#L48) | `huart1.Instance = USART1;` |
| [Src/usart.c:49](../Src/usart.c#L49) | `huart1.Init.BaudRate = 115200;` |
| [Src/usart.c:50](../Src/usart.c#L50) | `huart1.Init.WordLength = UART_WORDLENGTH_8B;` |
| [Src/usart.c:51](../Src/usart.c#L51) | `huart1.Init.StopBits = UART_STOPBITS_1;` |
| [Src/usart.c:52](../Src/usart.c#L52) | `huart1.Init.Parity = UART_PARITY_NONE;` |
| [Src/usart.c:53](../Src/usart.c#L53) | `huart1.Init.Mode = UART_MODE_TX_RX;` |
| [Src/usart.c:54](../Src/usart.c#L54) | `huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;` |
| [Src/usart.c:55](../Src/usart.c#L55) | `huart1.Init.OverSampling = UART_OVERSAMPLING_16;` |
| [Src/usart.c:77](../Src/usart.c#L77) | `huart3.Instance = USART3;` |
| [Src/usart.c:78](../Src/usart.c#L78) | `huart3.Init.BaudRate = 100000;` |
| [Src/usart.c:79](../Src/usart.c#L79) | `huart3.Init.WordLength = UART_WORDLENGTH_9B;` |
| [Src/usart.c:80](../Src/usart.c#L80) | `huart3.Init.StopBits = UART_STOPBITS_1;` |
| [Src/usart.c:81](../Src/usart.c#L81) | `huart3.Init.Parity = UART_PARITY_EVEN;` |
| [Src/usart.c:82](../Src/usart.c#L82) | `huart3.Init.Mode = UART_MODE_TX_RX;` |
| [Src/usart.c:83](../Src/usart.c#L83) | `huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;` |
| [Src/usart.c:84](../Src/usart.c#L84) | `huart3.Init.OverSampling = UART_OVERSAMPLING_16;` |
| [Src/usart.c:106](../Src/usart.c#L106) | `huart6.Instance = USART6;` |
| [Src/usart.c:107](../Src/usart.c#L107) | `huart6.Init.BaudRate = 115200;` |
| [Src/usart.c:108](../Src/usart.c#L108) | `huart6.Init.WordLength = UART_WORDLENGTH_8B;` |
| [Src/usart.c:109](../Src/usart.c#L109) | `huart6.Init.StopBits = UART_STOPBITS_1;` |
| [Src/usart.c:110](../Src/usart.c#L110) | `huart6.Init.Parity = UART_PARITY_NONE;` |
| [Src/usart.c:111](../Src/usart.c#L111) | `huart6.Init.Mode = UART_MODE_TX_RX;` |
| [Src/usart.c:112](../Src/usart.c#L112) | `huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;` |
| [Src/usart.c:113](../Src/usart.c#L113) | `huart6.Init.OverSampling = UART_OVERSAMPLING_16;` |
| [Src/usart.c:142](../Src/usart.c#L142) | `GPIO_InitStruct.Pin = GPIO_PIN_7;` |
| [Src/usart.c:143](../Src/usart.c#L143) | `GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;` |
| [Src/usart.c:144](../Src/usart.c#L144) | `GPIO_InitStruct.Pull = GPIO_NOPULL;` |
| [Src/usart.c:145](../Src/usart.c#L145) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;` |
| [Src/usart.c:146](../Src/usart.c#L146) | `GPIO_InitStruct.Alternate = GPIO_AF7_USART1;` |
| [Src/usart.c:149](../Src/usart.c#L149) | `GPIO_InitStruct.Pin = GPIO_PIN_9;` |
| [Src/usart.c:150](../Src/usart.c#L150) | `GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;` |
| [Src/usart.c:151](../Src/usart.c#L151) | `GPIO_InitStruct.Pull = GPIO_NOPULL;` |
| [Src/usart.c:152](../Src/usart.c#L152) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;` |
| [Src/usart.c:153](../Src/usart.c#L153) | `GPIO_InitStruct.Alternate = GPIO_AF7_USART1;` |
| [Src/usart.c:158](../Src/usart.c#L158) | `hdma_usart1_tx.Instance = DMA2_Stream7;` |
| [Src/usart.c:159](../Src/usart.c#L159) | `hdma_usart1_tx.Init.Channel = DMA_CHANNEL_4;` |
| [Src/usart.c:160](../Src/usart.c#L160) | `hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;` |
| [Src/usart.c:161](../Src/usart.c#L161) | `hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;` |
| [Src/usart.c:162](../Src/usart.c#L162) | `hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;` |
| [Src/usart.c:163](../Src/usart.c#L163) | `hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;` |
| [Src/usart.c:164](../Src/usart.c#L164) | `hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;` |
| [Src/usart.c:165](../Src/usart.c#L165) | `hdma_usart1_tx.Init.Mode = DMA_NORMAL;` |
| [Src/usart.c:166](../Src/usart.c#L166) | `hdma_usart1_tx.Init.Priority = DMA_PRIORITY_VERY_HIGH;` |
| [Src/usart.c:167](../Src/usart.c#L167) | `hdma_usart1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;` |
| [Src/usart.c:176](../Src/usart.c#L176) | `hdma_usart1_rx.Instance = DMA2_Stream5;` |
| [Src/usart.c:177](../Src/usart.c#L177) | `hdma_usart1_rx.Init.Channel = DMA_CHANNEL_4;` |
| [Src/usart.c:178](../Src/usart.c#L178) | `hdma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;` |
| [Src/usart.c:179](../Src/usart.c#L179) | `hdma_usart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;` |
| [Src/usart.c:180](../Src/usart.c#L180) | `hdma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;` |
| [Src/usart.c:181](../Src/usart.c#L181) | `hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;` |
| [Src/usart.c:182](../Src/usart.c#L182) | `hdma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;` |
| [Src/usart.c:183](../Src/usart.c#L183) | `hdma_usart1_rx.Init.Mode = DMA_NORMAL;` |
| [Src/usart.c:184](../Src/usart.c#L184) | `hdma_usart1_rx.Init.Priority = DMA_PRIORITY_VERY_HIGH;` |
| [Src/usart.c:185](../Src/usart.c#L185) | `hdma_usart1_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;` |
| [Src/usart.c:194](../Src/usart.c#L194) | `HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);` |
| [Src/usart.c:213](../Src/usart.c#L213) | `GPIO_InitStruct.Pin = GPIO_PIN_11\|GPIO_PIN_10;` |
| [Src/usart.c:214](../Src/usart.c#L214) | `GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;` |
| [Src/usart.c:215](../Src/usart.c#L215) | `GPIO_InitStruct.Pull = GPIO_NOPULL;` |
| [Src/usart.c:216](../Src/usart.c#L216) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;` |
| [Src/usart.c:217](../Src/usart.c#L217) | `GPIO_InitStruct.Alternate = GPIO_AF7_USART3;` |
| [Src/usart.c:222](../Src/usart.c#L222) | `hdma_usart3_rx.Instance = DMA1_Stream1;` |
| [Src/usart.c:223](../Src/usart.c#L223) | `hdma_usart3_rx.Init.Channel = DMA_CHANNEL_4;` |
| [Src/usart.c:224](../Src/usart.c#L224) | `hdma_usart3_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;` |
| [Src/usart.c:225](../Src/usart.c#L225) | `hdma_usart3_rx.Init.PeriphInc = DMA_PINC_DISABLE;` |
| [Src/usart.c:226](../Src/usart.c#L226) | `hdma_usart3_rx.Init.MemInc = DMA_MINC_ENABLE;` |
| [Src/usart.c:227](../Src/usart.c#L227) | `hdma_usart3_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;` |
| [Src/usart.c:228](../Src/usart.c#L228) | `hdma_usart3_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;` |
| [Src/usart.c:229](../Src/usart.c#L229) | `hdma_usart3_rx.Init.Mode = DMA_NORMAL;` |
| [Src/usart.c:230](../Src/usart.c#L230) | `hdma_usart3_rx.Init.Priority = DMA_PRIORITY_LOW;` |
| [Src/usart.c:231](../Src/usart.c#L231) | `hdma_usart3_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;` |
| [Src/usart.c:240](../Src/usart.c#L240) | `HAL_NVIC_SetPriority(USART3_IRQn, 5, 0);` |
| [Src/usart.c:259](../Src/usart.c#L259) | `GPIO_InitStruct.Pin = GPIO_PIN_14\|GPIO_PIN_9;` |
| [Src/usart.c:260](../Src/usart.c#L260) | `GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;` |
| [Src/usart.c:261](../Src/usart.c#L261) | `GPIO_InitStruct.Pull = GPIO_NOPULL;` |
| [Src/usart.c:262](../Src/usart.c#L262) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;` |
| [Src/usart.c:263](../Src/usart.c#L263) | `GPIO_InitStruct.Alternate = GPIO_AF8_USART6;` |
| [Src/usart.c:268](../Src/usart.c#L268) | `hdma_usart6_rx.Instance = DMA2_Stream2;` |
| [Src/usart.c:269](../Src/usart.c#L269) | `hdma_usart6_rx.Init.Channel = DMA_CHANNEL_5;` |
| [Src/usart.c:270](../Src/usart.c#L270) | `hdma_usart6_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;` |
| [Src/usart.c:271](../Src/usart.c#L271) | `hdma_usart6_rx.Init.PeriphInc = DMA_PINC_DISABLE;` |
| [Src/usart.c:272](../Src/usart.c#L272) | `hdma_usart6_rx.Init.MemInc = DMA_MINC_ENABLE;` |
| [Src/usart.c:273](../Src/usart.c#L273) | `hdma_usart6_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;` |
| [Src/usart.c:274](../Src/usart.c#L274) | `hdma_usart6_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;` |
| [Src/usart.c:275](../Src/usart.c#L275) | `hdma_usart6_rx.Init.Mode = DMA_NORMAL;` |
| [Src/usart.c:276](../Src/usart.c#L276) | `hdma_usart6_rx.Init.Priority = DMA_PRIORITY_HIGH;` |
| [Src/usart.c:277](../Src/usart.c#L277) | `hdma_usart6_rx.Init.FIFOMode = DMA_FIFOMODE_ENABLE;` |
| [Src/usart.c:278](../Src/usart.c#L278) | `hdma_usart6_rx.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;` |
| [Src/usart.c:279](../Src/usart.c#L279) | `hdma_usart6_rx.Init.MemBurst = DMA_MBURST_SINGLE;` |
| [Src/usart.c:280](../Src/usart.c#L280) | `hdma_usart6_rx.Init.PeriphBurst = DMA_PBURST_SINGLE;` |
| [Src/usart.c:289](../Src/usart.c#L289) | `hdma_usart6_tx.Instance = DMA2_Stream6;` |
| [Src/usart.c:290](../Src/usart.c#L290) | `hdma_usart6_tx.Init.Channel = DMA_CHANNEL_5;` |
| [Src/usart.c:291](../Src/usart.c#L291) | `hdma_usart6_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;` |
| [Src/usart.c:292](../Src/usart.c#L292) | `hdma_usart6_tx.Init.PeriphInc = DMA_PINC_DISABLE;` |
| [Src/usart.c:293](../Src/usart.c#L293) | `hdma_usart6_tx.Init.MemInc = DMA_MINC_ENABLE;` |
| [Src/usart.c:294](../Src/usart.c#L294) | `hdma_usart6_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;` |
| [Src/usart.c:295](../Src/usart.c#L295) | `hdma_usart6_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;` |
| [Src/usart.c:296](../Src/usart.c#L296) | `hdma_usart6_tx.Init.Mode = DMA_NORMAL;` |
| [Src/usart.c:297](../Src/usart.c#L297) | `hdma_usart6_tx.Init.Priority = DMA_PRIORITY_HIGH;` |
| [Src/usart.c:298](../Src/usart.c#L298) | `hdma_usart6_tx.Init.FIFOMode = DMA_FIFOMODE_ENABLE;` |
| [Src/usart.c:299](../Src/usart.c#L299) | `hdma_usart6_tx.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;` |
| [Src/usart.c:300](../Src/usart.c#L300) | `hdma_usart6_tx.Init.MemBurst = DMA_MBURST_SINGLE;` |
| [Src/usart.c:301](../Src/usart.c#L301) | `hdma_usart6_tx.Init.PeriphBurst = DMA_PBURST_SINGLE;` |
| [Src/usart.c:310](../Src/usart.c#L310) | `HAL_NVIC_SetPriority(USART6_IRQn, 5, 0);` |

## Src/usbd_conf.c

源文件：[打开 Src/usbd_conf.c](../Src/usbd_conf.c)。

### HAL 设置赋值与中断优先级

同一变量可能在不同外设初始化中重复使用，按源代码行区分。

| 行 | 设置 |
| --- | --- |
| [Src/usbd_conf.c:84](../Src/usbd_conf.c#L84) | `GPIO_InitStruct.Pin = GPIO_PIN_12\|GPIO_PIN_11\|GPIO_PIN_10;` |
| [Src/usbd_conf.c:85](../Src/usbd_conf.c#L85) | `GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;` |
| [Src/usbd_conf.c:86](../Src/usbd_conf.c#L86) | `GPIO_InitStruct.Pull = GPIO_NOPULL;` |
| [Src/usbd_conf.c:87](../Src/usbd_conf.c#L87) | `GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;` |
| [Src/usbd_conf.c:88](../Src/usbd_conf.c#L88) | `GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;` |
| [Src/usbd_conf.c:95](../Src/usbd_conf.c#L95) | `HAL_NVIC_SetPriority(OTG_FS_IRQn, 5, 0);` |
| [Src/usbd_conf.c:340](../Src/usbd_conf.c#L340) | `hpcd_USB_OTG_FS.pData = pdev;` |
| [Src/usbd_conf.c:343](../Src/usbd_conf.c#L343) | `hpcd_USB_OTG_FS.Instance = USB_OTG_FS;` |
| [Src/usbd_conf.c:344](../Src/usbd_conf.c#L344) | `hpcd_USB_OTG_FS.Init.dev_endpoints = 4;` |
| [Src/usbd_conf.c:345](../Src/usbd_conf.c#L345) | `hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;` |
| [Src/usbd_conf.c:346](../Src/usbd_conf.c#L346) | `hpcd_USB_OTG_FS.Init.dma_enable = DISABLE;` |
| [Src/usbd_conf.c:347](../Src/usbd_conf.c#L347) | `hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;` |
| [Src/usbd_conf.c:348](../Src/usbd_conf.c#L348) | `hpcd_USB_OTG_FS.Init.Sof_enable = DISABLE;` |
| [Src/usbd_conf.c:349](../Src/usbd_conf.c#L349) | `hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;` |
| [Src/usbd_conf.c:350](../Src/usbd_conf.c#L350) | `hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;` |
| [Src/usbd_conf.c:351](../Src/usbd_conf.c#L351) | `hpcd_USB_OTG_FS.Init.vbus_sensing_enable = DISABLE;` |
| [Src/usbd_conf.c:352](../Src/usbd_conf.c#L352) | `hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;` |

## Src/usbd_desc.c

源文件：[打开 Src/usbd_desc.c](../Src/usbd_desc.c)。

### 宏定义

| 行 | 宏 | 源码替换文本 | 条件路径 | 原行注释 |
| --- | --- | --- | --- | --- |
| [Src/usbd_desc.c:65](../Src/usbd_desc.c#L65) | `USBD_VID` | `1155` | 无外层条件 |  |
| [Src/usbd_desc.c:66](../Src/usbd_desc.c#L66) | `USBD_LANGID_STRING` | `1033` | 无外层条件 |  |
| [Src/usbd_desc.c:67](../Src/usbd_desc.c#L67) | `USBD_MANUFACTURER_STRING` | `"YueLuEmbedded"` | 无外层条件 |  |
| [Src/usbd_desc.c:68](../Src/usbd_desc.c#L68) | `USBD_PID_FS` | `202` | 无外层条件 |  |
| [Src/usbd_desc.c:69](../Src/usbd_desc.c#L69) | `USBD_PRODUCT_STRING_FS` | `"Vision Comm port"` | 无外层条件 |  |
| [Src/usbd_desc.c:70](../Src/usbd_desc.c#L70) | `USBD_CONFIGURATION_STRING_FS` | `"CDC Config"` | 无外层条件 |  |
| [Src/usbd_desc.c:71](../Src/usbd_desc.c#L71) | `USBD_INTERFACE_STRING_FS` | `"CDC Interface"` | 无外层条件 |  |
| [Src/usbd_desc.c:73](../Src/usbd_desc.c#L73) | `USB_SIZ_BOS_DESC` | `0x0C` | 无外层条件 |  |
