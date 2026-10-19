/**
 * @file chassis.c
 * @author NeoZeng neozng1@hnu.edu.cn
 * @brief 底盘应用,负责接收robot_cmd的控制命令并根据命令进行运动学解算,得到输出
 *        注意底盘采取右手系,对于平面视图,底盘纵向运动的正前方为x正方向;横向运动的右侧为y正方向
 *
 * @version 0.1
 * @date 2022-12-04
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "chassis.h"
#include "chassis_kinematics.h"
#include "chassis_power_budget.h"
#include "chassis_energy_config.h"
#include "chassis_power_outer_loop.h"
#include "chassis_power_link.h"
#include "chassis_spin_config.h"
#include "chassis_spin.h"
#include "runtime_tuning.h"
#include "remote_control.h"
#include "robot_def.h"
#include "board_config.h"
#if POWER_METER_ENABLE
#include "power_meter.h"
#endif
#include "dji_motor.h"
#include "super_cap.h"
#include "message_center.h"
#include "referee_task.h"

#include "general_def.h"
#include "bsp_dwt.h"
#include "referee_UI.h"
#include "arm_math.h"
#include <math.h>
#include "FreeRTOS.h"
#include "task.h"

/* 根据robot_def.h中的macro自动计算的参数 */
#define HALF_WHEEL_BASE (WHEEL_BASE / 2.0f)     // 半轴距
#define HALF_TRACK_WIDTH (TRACK_WIDTH / 2.0f)   // 半轮距
#define PERIMETER_WHEEL (RADIUS_WHEEL * 2 * PI) // 轮子周长
#define CHASSIS_POWER_SAFETY_FACTOR 0.98f   // 功率预算安全系数,用于降低功率预算,避免功率计和底盘功率模型的误差导致底盘超功率

/* 底盘应用包含的模块和信息存储,底盘是单例模式,因此不需要为底盘建立单独的结构体 */
#ifdef CHASSIS_BOARD // 如果是底盘板,使用板载IMU获取底盘转动角速度
#include "can_comm.h"
#include "ins_task.h"
static CANCommInstance *chasiss_can_comm; // 双板通信CAN comm
attitude_t *Chassis_IMU_data;
#endif // CHASSIS_BOARD
#ifdef ONE_BOARD
static Publisher_t *chassis_pub;                    // 用于发布底盘的数据
static Subscriber_t *chassis_sub;                   // 用于订阅底盘的控制命令
#endif                                              // !ONE_BOARD
static Chassis_Ctrl_Cmd_s chassis_cmd_recv;         // 底盘接收到的控制命令
static Chassis_Upload_Data_s chassis_feedback_data; // 底盘回传的反馈数据

static PIDInstance buffer_PID;             // 用于底盘的缓冲能量PID
static Referee_Interactive_info_t ui_data; // UI数据，将底盘中的数据传入此结构体的对应变量中，UI会自动检测是否变化，对应显示UI

 DJIMotorInstance *motor_lf, *motor_rf, *motor_lb, *motor_rb; // left right forward back
static bool chassis_power_ready;
static bool chassis_power_feedback_ready;
static ChassisPowerBudgetState_s chassis_budget_state;// 底盘功率预算状态
static ChassisPowerBudgetOutput_s chassis_budget_output;// 底盘功率预算输出
static ChassisPowerOuterLoopState_s chassis_outer_loop_state;
static ChassisPowerOuterLoopOutput_s chassis_outer_loop_output;
static uint32_t chassis_budget_dwt_count;
static DJIChassisPowerControlSnapshot_s pending_power_control;
static uint32_t power_control_version;
static ChassisTelemetrySnapshot_s chassis_telemetry;

bool ChassisGetTelemetrySnapshot(ChassisTelemetrySnapshot_s *snapshot)
{
    if (snapshot == NULL) return false;
    taskENTER_CRITICAL();
    *snapshot = chassis_telemetry;
    taskEXIT_CRITICAL();
    return snapshot->valid;
}

#if CHASSIS_SPIN_DYNAMIC_ENABLED
static ChassisSpinState_s chassis_spin_state;
static ChassisSpinInput_s chassis_spin_input;
static uint32_t chassis_spin_dwt_count;
#endif
static ChassisSpinConfig_s chassis_spin_config = {
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

static ChassisPowerBudgetConfig_s chassis_budget_config = {
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

/* 功率外环:用功率计实测功率对比模型预测值,在"基准预算"上叠加一个有界的修正量 */
static ChassisPowerOuterLoopConfig_s chassis_outer_loop_config = {
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

static const MotorPowerModelConfig_s m3508_power_model = {
    .k0 = 1.3123816,
    .k1 = -0.25836727,
    .k2 = 0.0007424524,
    .k3 = 0.0024271158,
    .k4 = 0.081198552,
    .k5 = 3.890596e-08,
    .current_conversion = 1000.0f,// 电流单位转换,功率模型使用A,电机模块使用mA
};

/* 用于自旋变速策略的时间变量 */
// static float t;

/* 私有函数计算的中介变量,设为静态避免参数传递的开销 */
static float chassis_vx, chassis_vy;                      // 将云台系的速度投影到底盘
static float vt_lf, vt_rf, vt_lb, vt_rb;                  // 底盘速度解算后的临时输出,待进行限幅

static void RegisterChassisTuning(void)
{
    const ChassisPowerOuterLoopConfig_s *o = &chassis_outer_loop_config;
    const ChassisPowerBudgetConfig_s *b = &chassis_budget_config;
    const float power[] = {o->kp, o->ki, o->measurement_lpf_tau_s,
        o->activation_margin_w, o->error_deadband_w, o->max_positive_correction_w,
        o->positive_slew_rate_w_per_s, o->budget_drop_reset_w, o->antiwindup_gain_per_s,
        o->low_demand_delay_s, o->low_demand_decay_w_per_s,
        b->max_boost_w, b->rise_rate_w_per_s, b->demand_margin_w,
        b->empty_energy_ratio, b->full_energy_ratio, b->buffer.max_boost_w,
        b->buffer.recovery_power_w, b->buffer.recovery_target_time_s};
    RuntimeTuningRegister(TUNING_POWER, power, CHASSIS_POWER_CONTROL_STAGE);
    const ChassisSpinConfig_s *s = &chassis_spin_config;
    const float spin[] = {s->fixed_wz, s->min_wz, s->max_wz, s->gain_wz_per_w_s,
        s->rise_wz_per_s, s->fall_wz_per_s, s->power_lpf_tau_s, s->power_deadband_w,
        s->settle_time_s, s->budget_drop_reset_w, s->sine_amplitude_wz,
        s->sine_period_s, s->sine_ramp_s, s->sine_rise_wz_per_s,
        s->sine_fall_wz_per_s, (float)s->sine_direction};
    RuntimeTuningRegister(TUNING_SPIN, spin, CHASSIS_SPIN_MODE);
}
static bool ApplyPowerTuning(const float *v)
{
    const float upper[] = {100, 100, 10, 1000, 100, 1000, 10000, 1000,
        1000, 60, 1000, 1000, 10000, 1000, 1, 1, 1000, 1000, 60};
    for (unsigned i = 0; i < 19; ++i)
        if (!isfinite(v[i]) || v[i] < 0 || v[i] > upper[i]) return false;
    ChassisPowerOuterLoopConfig_s outer = {
        .kp=v[0], .ki=v[1], .measurement_lpf_tau_s=v[2],
        .activation_margin_w=v[3], .error_deadband_w=v[4],
        .max_positive_correction_w=v[5], .positive_slew_rate_w_per_s=v[6],
        .budget_drop_reset_w=v[7], .antiwindup_gain_per_s=v[8],
        .low_demand_delay_s=v[9], .low_demand_decay_w_per_s=v[10]};
    ChassisPowerBudgetConfig_s budget = chassis_budget_config;
    budget.max_boost_w=v[11]; budget.rise_rate_w_per_s=v[12];
    budget.demand_margin_w=v[13]; budget.empty_energy_ratio=v[14];
    budget.full_energy_ratio=v[15]; budget.buffer.max_boost_w=v[16];
    budget.buffer.recovery_power_w=v[17]; budget.buffer.recovery_target_time_s=v[18];
    if (!ChassisPowerOuterLoopConfigIsValid(&outer) ||
        !ChassisPowerBudgetConfigIsValid(&budget) ||
        !ChassisBufferBudgetConfigIsValid(&budget.buffer)) return false;
    chassis_outer_loop_config = outer;
    chassis_budget_config = budget;
    ChassisPowerOuterLoopReset(&chassis_outer_loop_state);
    return true;
}
static bool ApplySpinTuning(const float *v)
{
    const float upper[] = {6000,6000,6000,1000,10000,10000,10,1000,60,
        1000,6000,120,60,10000,10000};
    for (unsigned i = 0; i < 15; ++i)
        if (!isfinite(v[i]) || v[i] < 0 || v[i] > upper[i]) return false;
    if (v[15] != -1 && v[15] != 1) return false;
    ChassisSpinConfig_s spin = {
        .fixed_wz=v[0], .min_wz=v[1], .max_wz=v[2], .gain_wz_per_w_s=v[3],
        .rise_wz_per_s=v[4], .fall_wz_per_s=v[5], .power_lpf_tau_s=v[6],
        .power_deadband_w=v[7], .settle_time_s=v[8], .budget_drop_reset_w=v[9],
        .sine_amplitude_wz=v[10], .sine_period_s=v[11], .sine_ramp_s=v[12],
        .sine_rise_wz_per_s=v[13], .sine_fall_wz_per_s=v[14],
        .sine_direction=(int8_t)v[15]};
    if (!ChassisSpinConfigIsValid(&spin)) return false;
#if CHASSIS_SPIN_DYNAMIC_ENABLED
    if (chassis_cmd_recv.chassis_mode == CHASSIS_ROTATE) {
        if (spin.sine_direction != chassis_spin_config.sine_direction ||
            !ChassisSpinRetune(&spin, &chassis_spin_state)) return false;
    } else chassis_spin_state = (ChassisSpinState_s){0};
#endif
    chassis_spin_config = spin;
    return true;
}

void ChassisInit()
{
    RegisterChassisTuning();
    RuntimeTuningRestore(TUNING_POWER, ApplyPowerTuning);
    RuntimeTuningRestore(TUNING_SPIN, ApplySpinTuning);
    // 四个轮子的参数一样,改tx_id和反转标志位即可
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
    //  @todo: 当前还没有设置电机的正反转,仍然需要手动添加reference的正负号,需要电机module的支持,待修改.
    chassis_motor_config.can_init_config.tx_id = 1;
    chassis_motor_config.controller_setting_init_config.motor_reverse_flag = MOTOR_DIRECTION_NORMAL;
    motor_lf = DJIMotorInit(&chassis_motor_config);

    chassis_motor_config.can_init_config.tx_id = 2;
    chassis_motor_config.controller_setting_init_config.motor_reverse_flag = MOTOR_DIRECTION_NORMAL;
    motor_rf = DJIMotorInit(&chassis_motor_config);

    chassis_motor_config.can_init_config.tx_id = 4;
    chassis_motor_config.controller_setting_init_config.motor_reverse_flag = MOTOR_DIRECTION_NORMAL;
    motor_lb = DJIMotorInit(&chassis_motor_config);

    chassis_motor_config.can_init_config.tx_id = 3;
    chassis_motor_config.controller_setting_init_config.motor_reverse_flag = MOTOR_DIRECTION_NORMAL;
    motor_rb = DJIMotorInit(&chassis_motor_config);

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
    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
        power_config.models[i] = m3508_power_model;

    chassis_power_ready = DJIChassisPowerRegister(&power_config);
    chassis_power_feedback_ready = ChassisPowerFeedbackInit();
    DJIChassisPowerSetAttenuation(1.0f);
    if (!chassis_power_ready)
    {
        DJIMotorStop(motor_lf);
        DJIMotorStop(motor_rf);
        DJIMotorStop(motor_lb);
        DJIMotorStop(motor_rb);
    }

#if UART1_REFEREE_ENABLED
    (void)UITaskInit(&huart1, &ui_data); // 裁判系统初始化,会同时初始化UI
#endif

/* Buffer环暂未测试，逻辑是计算期望buffer与实际buffer的差值，转换为冗余的功率，todo：输入给功率控制部分，待完善 */
    PID_Init_Config_s Buffer_pid_conf = {
        .Kp = 0.1,
        .Ki = 0,
        .Kd = 0,
        .IntegralLimit = 1000,
        .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit | PID_Derivative_On_Measurement,
        .MaxOut = 1000,
    };
    PIDInit(&buffer_PID, &Buffer_pid_conf); // 缓冲能量PID初始化
#if CHASSIS_POWER_SOURCE_MODE == CHASSIS_POWER_SOURCE_SUPERCAP
    SuperCap_Init_Config_s cap_conf = {
        .can_config = {
            .can_handle = &hcan1,
            .tx_id = SUPERCAP_COMMAND_CAN_ID,
            .rx_id = SUPERCAP_STATUS_CAN_ID,
        },
        .offline_reload_count = 20U,
        .input_power_feedback_enabled = CHASSIS_BUFFER_POWER_ENABLE != 0,
    };
    (void)SuperCapInit(&cap_conf); // 超级电容初始化
#endif
#if POWER_METER_ENABLE
    const PowerMeter_Init_Config_s power_meter_config = {
        .can_config = {
            .can_handle = &hcan1,
            .tx_id = POWER_METER_STATUS_CAN_ID,
            .rx_id = POWER_METER_STATUS_CAN_ID,
        },
        .offline_reload_count = 10U,
    };
    (void)PowerMeterInit(&power_meter_config);
#endif
    ChassisPowerBudgetReset(&chassis_budget_state, 0.0f);
    ChassisPowerOuterLoopReset(&chassis_outer_loop_state);
    DWT_GetDeltaT(&chassis_budget_dwt_count);

    // 发布订阅初始化,如果为双板,则需要can comm来传递消息
#ifdef CHASSIS_BOARD
    Chassis_IMU_data = INS_Init(); // 底盘IMU初始化

    CANComm_Init_Config_s comm_conf = {
        .can_config = {
            .can_handle = &hcan2,
            .tx_id = 0x311,
            .rx_id = 0x312,
        },
        .recv_data_len = sizeof(Chassis_Ctrl_Cmd_s),
        .send_data_len = sizeof(Chassis_Upload_Data_s),
    };
    chasiss_can_comm = CANCommInit(&comm_conf); // can comm初始化
#endif                                          // CHASSIS_BOARD

#ifdef ONE_BOARD // 单板控制整车,则通过pubsub来传递消息
    chassis_sub = SubRegister("chassis_cmd", sizeof(Chassis_Ctrl_Cmd_s));
    chassis_pub = PubRegister("chassis_feed", sizeof(Chassis_Upload_Data_s));
#endif // ONE_BOARD
}

#define LF_ROTATION_LEVER_SUM ((HALF_TRACK_WIDTH + CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE - CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)
#define RF_ROTATION_LEVER_SUM ((HALF_TRACK_WIDTH - CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE - CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)
#define LB_ROTATION_LEVER_SUM ((HALF_TRACK_WIDTH + CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE + CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)
#define RB_ROTATION_LEVER_SUM ((HALF_TRACK_WIDTH - CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE + CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)
/**
 * @brief 计算每个轮毂电机的输出,正运动学解算
 *        用宏进行预替换减小开销,运动解算具体过程参考教程
 */
static void CalculateWheelSpeed(void)
{
    const ChassisKinematicsInput_s input = {
        .vx = chassis_vx,
        .vy = chassis_vy,
        .wz = chassis_cmd_recv.wz,
        .rotation_lever_sum = {
            LF_ROTATION_LEVER_SUM,
            RF_ROTATION_LEVER_SUM,
            LB_ROTATION_LEVER_SUM,
            RB_ROTATION_LEVER_SUM,
        },
    };
    ChassisWheelOutput_s output = {0};

#if CHASSIS_DRIVE_TYPE == CHASSIS_DRIVE_OMNI_X
    const float wheel_direction[CHASSIS_WHEEL_COUNT] = {
        OMNI_X_LF_DIRECTION,
        OMNI_X_RF_DIRECTION,
        OMNI_X_LB_DIRECTION,
        OMNI_X_RB_DIRECTION,
    };
    (void)ChassisOmniXCalculate(&input, wheel_direction, &output);
#else
    (void)ChassisMecanumCalculate(&input, &output);
#endif

    vt_lf = output.rpm[CHASSIS_WHEEL_LEFT_FRONT];
    vt_rf = output.rpm[CHASSIS_WHEEL_RIGHT_FRONT];
    vt_lb = output.rpm[CHASSIS_WHEEL_LEFT_BACK];
    vt_rb = output.rpm[CHASSIS_WHEEL_RIGHT_BACK];
}

/**
 * @brief 根据裁判系统和电容剩余容量对输出进行限制并设置电机参考值
 *
 */
static void LimitChassisOutput()
{
    // 底盘解算输出沿用 rpm；统一 DJI 驱动的速度环使用度/秒。
    pending_power_control.target_rpm[0] = vt_lf;
    pending_power_control.target_rpm[1] = vt_rf;
    pending_power_control.target_rpm[2] = vt_lb;
    pending_power_control.target_rpm[3] = vt_rb;
    pending_power_control.version = ++power_control_version;
    pending_power_control.tick_ms = HAL_GetTick();
    DJIChassisPowerPublishControl(&pending_power_control);
}

/**
 * @brief 根据每个轮子的速度反馈,计算底盘的实际运动速度,逆运动解算
 *        对于双板的情况,考虑增加来自底盘板IMU的数据
 *
 */
static void EstimateSpeed()
{
    // 根据电机速度和陀螺仪的角速度进行解算,还可以利用加速度计判断是否打滑(如果有)
    // chassis_feedback_data.vx vy wz =
    //  ...
}

static void UpdateChassisPowerBudget(void)
{
    bool control_config_valid = true;
#if CHASSIS_SPIN_DYNAMIC_ENABLED
    chassis_spin_input = (ChassisSpinInput_s){
        .spin_enabled = chassis_cmd_recv.chassis_mode == CHASSIS_ROTATE,
    };
#endif
    RefereePowerSnapshot_s referee_snapshot = {0};
    (void)RefereeGetPowerSnapshot(&referee_snapshot);

    const ChassisPowerLinkInput_s link_input = {
        .referee = referee_snapshot,
#if CHASSIS_POWER_BENCH_TEST
        .bench_enabled = true,
#else
        .bench_enabled = false,
#endif
        .bench_limit_w = CHASSIS_POWER_BENCH_LIMIT_W,
        .chassis_zero_force =
            chassis_cmd_recv.chassis_mode == CHASSIS_ZERO_FORCE,
    };
    ChassisPowerLinkOutput_s link_output = {0};
    (void)ChassisPowerLinkBuild(&link_input, &link_output);

    const uint32_t energy_now_ms = HAL_GetTick();
    SuperCapStatus_s cap_status = {0};
    SuperCapInputPower_s cap_input_power = {0};
#if CHASSIS_POWER_SOURCE_MODE == CHASSIS_POWER_SOURCE_SUPERCAP
    (void)SuperCapGetStatus(&cap_status);
    (void)SuperCapGetInputPower(&cap_input_power);
#if CHASSIS_BUFFER_POWER_ENABLE
    cap_status.online = cap_status.online &&
        (uint32_t)(HAL_GetTick() - cap_status.last_rx_tick_ms) <= 20U;
#endif
#endif
    bool input_power_valid = false;
    bool load_power_valid = false;
    float measured_input_power_w = 0;
#if POWER_METER_ENABLE
    PowerMeterStatus_s input_meter = {0};
    if (PowerMeterGetStatus(&input_meter))
        load_power_valid = input_meter.online &&
            (uint32_t)(HAL_GetTick() - input_meter.last_rx_tick_ms) <= 20U;
#endif
#if CHASSIS_POWER_SOURCE_MODE == CHASSIS_POWER_SOURCE_SUPERCAP
    input_power_valid = cap_input_power.online;
    measured_input_power_w = cap_input_power.input_power_w;
#elif POWER_METER_ENABLE
    input_power_valid = load_power_valid;
    measured_input_power_w = input_meter.power_w;
#endif

    DJIChassisPowerState_s power_state = {0};
    const bool power_state_valid = DJIChassisPowerGetSnapshot(&power_state);
    const uint32_t demand_now_ms = HAL_GetTick();
    bool demand_valid = power_state_valid && power_state.registered &&
        power_state.power.status == POWER_MODEL_STATUS_OK && !power_state.numeric_error &&
        !power_state.transient.fault && power_state.online_mask == 0x0f &&
        power_state.telemetry.motor_enabled_mask == 0x0f &&
        power_state.transient.drive_mask != 0 && power_state.transient.brake_mask == 0 &&
        (uint32_t)(demand_now_ms - power_state.tick_ms) <= 5 &&
        chassis_cmd_recv.chassis_mode != CHASSIS_ZERO_FORCE;
    float requested_load_power_w = 0;
    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
    {
        const float predicted_w = power_state.power.predicted_unlimited_power_w[i];
        demand_valid &= isfinite(predicted_w) &&
            (uint32_t)(demand_now_ms - power_state.feedback_tick_ms[i]) <= 5;
        requested_load_power_w += predicted_w;
    }
#if POWER_METER_ENABLE
    if (load_power_valid && isfinite(input_meter.power_w))
        requested_load_power_w = fmaxf(requested_load_power_w, input_meter.power_w);
#endif

    const ChassisPowerBudgetInput_s input = {
        .referee_limit_w = link_output.base_power_limit_w,
        .cap_online = cap_status.online,
        .cap_output_enabled = cap_status.output_enabled,
        .cap_error_code = cap_status.error_code,
        .cap_energy_ratio = cap_status.energy_ratio,
        .cap_reported_limit_w = (float)cap_status.available_power_limit_w,
        .buffer_control_enabled = CHASSIS_BUFFER_POWER_ENABLE != 0,
        .use_supercap = CHASSIS_POWER_SOURCE_MODE == CHASSIS_POWER_SOURCE_SUPERCAP,
        .buffer_data_valid = referee_snapshot.valid &&
            referee_snapshot.chassis_output_enabled,
        .buffer_energy_j = (float)referee_snapshot.buffer_energy_j,
        .buffer_tick_ms = referee_snapshot.buffer_tick_ms,
        .buffer_age_s = (float)(uint32_t)(energy_now_ms -
            referee_snapshot.buffer_tick_ms) * 0.001f,
        .input_power_valid = input_power_valid,
        .measured_input_power_w = measured_input_power_w,
        .cap_input_control_ready = cap_input_power.online &&
            cap_input_power.input_control_active,
        .load_power_valid = load_power_valid,
        .demand_valid = demand_valid,
        .requested_load_power_w = requested_load_power_w,
        .confirmed_input_target_w = (float)cap_input_power.input_power_target_w,
    };
    const float dt_s = DWT_GetDeltaT(&chassis_budget_dwt_count);
    if (!ChassisPowerBudgetUpdate(&chassis_budget_config,
                                  &input,
                                  dt_s,
                                  &chassis_budget_state,
                                  &chassis_budget_output))
    {
        chassis_budget_output.applied_budget_w = 0.0f;
        control_config_valid = false;
    }

    if (referee_snapshot.chassis_output_valid && !referee_snapshot.chassis_output_enabled)
        control_config_valid = false;
#if CHASSIS_POWER_SOURCE_MODE == CHASSIS_POWER_SOURCE_SUPERCAP
    link_output.command.input_control_enabled = CHASSIS_BUFFER_POWER_ENABLE &&
        referee_snapshot.valid && link_output.command.enable_dcdc;
    link_output.command.input_power_target_w = (uint16_t)fminf(65535,
        fmaxf(0, chassis_budget_output.input_target_w));
    (void)SuperCapSendCommand(&link_output.command);
#endif

#if POWER_METER_ENABLE
    PowerMeterStatus_s meter_status = {0};
    bool meter_valid = false;
    if (PowerMeterGetStatus(&meter_status))
        meter_valid = meter_status.online;

    float predicted_unlimited_power_w = 0.0f;
#if CHASSIS_POWER_CONTROL_STAGE == 0
    float motoring_indicator = 0.0f;
#endif
    if (power_state_valid && power_state.registered &&
        power_state.power.status == POWER_MODEL_STATUS_OK)
    {
        for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
        {
            predicted_unlimited_power_w +=
                power_state.power.predicted_unlimited_power_w[i];
#if CHASSIS_POWER_CONTROL_STAGE == 0
            motoring_indicator +=
                power_state.telemetry.current_command[i] *
                power_state.telemetry.actual_speed_rpm[i];
#endif
        }
    }

#if CHASSIS_POWER_CONTROL_STAGE >= 1
    meter_status.power_w = power_state.measured_power_w;
    meter_status.last_rx_tick_ms = power_state.meter_tick_ms;
    meter_valid = power_state_valid && power_state.meter_online &&
        (uint32_t)(HAL_GetTick() - meter_status.last_rx_tick_ms) <= 20;
#endif
    const ChassisPowerOuterLoopInput_s outer_input = {
        .nominal_budget_w = chassis_budget_output.applied_budget_w,
        .target_power_w = chassis_budget_output.applied_budget_w *
                          CHASSIS_POWER_SAFETY_FACTOR,
        .measured_power_w = meter_status.power_w,
        .predicted_unlimited_power_w = predicted_unlimited_power_w,
        .meter_online = meter_valid,
        .allow_positive_correction =
#if CHASSIS_POWER_CONTROL_STAGE == 0
            meter_valid && meter_status.power_w >= 0.0f &&
            motoring_indicator >= 0.0f,
#else
            meter_valid && power_state.transient.positive_allowed &&
            meter_status.power_w <= chassis_budget_output.applied_budget_w,
#endif
#if CHASSIS_SPIN_MODE == CHASSIS_SPIN_MODE_ADAPTIVE
        .hold_positive_learning = chassis_cmd_recv.chassis_mode == CHASSIS_ROTATE &&
            !power_state.base_limited,
#endif
        .learning_allowed = meter_valid && power_state.transient.learning_allowed &&
            meter_status.power_w <= chassis_budget_output.applied_budget_w &&
            (uint32_t)(HAL_GetTick() - power_state.tick_ms) <= 5,
        .negative_learning_allowed = CHASSIS_POWER_CONTROL_STAGE == 3 && meter_valid &&
            power_state.transient.negative_learning_allowed &&
            power_state.transient.negative_learning_threshold_w >=
                chassis_budget_output.applied_budget_w * CHASSIS_POWER_SAFETY_FACTOR +
                    chassis_outer_loop_config.error_deadband_w &&
            (uint32_t)(HAL_GetTick() - power_state.tick_ms) <= 5,
        .complete_slow_control = CHASSIS_POWER_CONTROL_STAGE == 3,
        .base_limited = power_state.base_limited,
        .demand_sufficient = power_state.transient.drive_mask != 0 && predicted_unlimited_power_w >=
            chassis_budget_output.applied_budget_w * CHASSIS_POWER_SAFETY_FACTOR -
            chassis_outer_loop_config.activation_margin_w,
        .braking = power_state.transient.brake_mask != 0,
        .meter_tick_ms = meter_status.last_rx_tick_ms,
        .control_enabled =
            chassis_power_ready &&
            chassis_cmd_recv.chassis_mode != CHASSIS_ZERO_FORCE,
    };

#if CHASSIS_POWER_CONTROL_STAGE == 0
    if (!ChassisPowerOuterLoopUpdate(&chassis_outer_loop_config,
#else
    if (!ChassisPowerOuterLoopUpdateIsolated(&chassis_outer_loop_config,
#endif
                                     &outer_input,
                                     dt_s,
                                     &chassis_outer_loop_state,
                                     &chassis_outer_loop_output))
    {
        chassis_outer_loop_output.corrected_budget_w = 0.0f;
        control_config_valid = false;
    }

    const ChassisPowerControlTelemetry_s control_telemetry = {
        .nominal_budget_w = chassis_budget_output.applied_budget_w,
        .target_power_w = outer_input.target_power_w,
        .corrected_budget_w = chassis_outer_loop_output.corrected_budget_w,
        .filtered_power_w = chassis_outer_loop_output.filtered_power_w,
        .error_w = chassis_outer_loop_output.error_w,
        .correction_w = chassis_outer_loop_output.correction_w,
        .outer_mode = (uint8_t)chassis_outer_loop_output.mode,
    };
    pending_power_control.telemetry = control_telemetry;
    pending_power_control.legacy_budget_w = chassis_outer_loop_output.corrected_budget_w;
    pending_power_control.slow_correction_w = chassis_outer_loop_output.correction_w;
#if CHASSIS_SPIN_DYNAMIC_ENABLED
    const uint32_t spin_now_ms = HAL_GetTick();
    bool feedback_valid = power_state_valid && power_state.registered &&
        power_state.power.status == POWER_MODEL_STATUS_OK && !power_state.numeric_error &&
        !power_state.transient.fault && power_state.online_mask == 0x0f &&
        power_state.telemetry.motor_enabled_mask == 0x0f &&
        (uint32_t)(spin_now_ms - power_state.tick_ms) <= 5;
    bool tracking = true;
    bool current_limited = power_state.base_limited;
#if CHASSIS_POWER_CONTROL_STAGE >= 2
    current_limited |= !ChassisSpinGuardAllowsIncrease(&power_state.transient);
#endif
    DJIMotorInstance *spin_motors[POWER_MODEL_MOTOR_COUNT] = {
        motor_lf, motor_rf, motor_lb, motor_rb,
    };
    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
    {
        const float actual_rpm = power_state.telemetry.actual_speed_rpm[i];
        const float target_rpm = power_state.telemetry.target_speed_rpm[i];
        feedback_valid &= (uint32_t)(spin_now_ms - power_state.feedback_tick_ms[i]) <= 5 &&
            isfinite(actual_rpm) && isfinite(target_rpm) && isfinite(power_state.raw_current[i]);
        tracking &= fabsf(target_rpm - actual_rpm) <= CHASSIS_SPIN_TRACKING_ERROR_RPM;
        const float max_current = spin_motors[i]->motor_controller.speed_PID.MaxOut;
        current_limited |= !isfinite(max_current) || max_current <= 0 ||
            fabsf(power_state.raw_current[i]) >= max_current;
    }
    chassis_spin_input.feedback_valid = feedback_valid && meter_valid &&
        (uint32_t)(spin_now_ms - meter_status.last_rx_tick_ms) <= 20;
    chassis_spin_input.measured_power_w = meter_status.power_w;
    chassis_spin_input.power_limited = current_limited;
    chassis_spin_input.increase_allowed = tracking && !current_limited &&
        ChassisSpinTransientAllowsIncrease(&power_state.transient);
#endif
#else
    const ChassisPowerControlTelemetry_s control_telemetry = {
        .nominal_budget_w = chassis_budget_output.applied_budget_w,
        .target_power_w = chassis_budget_output.applied_budget_w *
                          CHASSIS_POWER_SAFETY_FACTOR,
        .corrected_budget_w = chassis_budget_output.applied_budget_w,
        .outer_mode = (uint8_t)CHASSIS_POWER_OUTER_OFFLINE,
    };
    pending_power_control.telemetry = control_telemetry;
    pending_power_control.legacy_budget_w = chassis_budget_output.applied_budget_w;
    pending_power_control.slow_correction_w = 0;
#endif
    pending_power_control.nominal_budget_w = chassis_budget_output.applied_budget_w;
    pending_power_control.target_power_w = chassis_budget_output.applied_budget_w * CHASSIS_POWER_SAFETY_FACTOR;
    pending_power_control.power_deadband_w = chassis_outer_loop_config.error_deadband_w;
    pending_power_control.power_ceiling_w = CHASSIS_BUFFER_POWER_ENABLE ?
        chassis_budget_output.applied_budget_w : 0;
#if CHASSIS_BUFFER_POWER_ENABLE && CHASSIS_POWER_SOURCE_MODE == CHASSIS_POWER_SOURCE_SUPERCAP
    if (cap_status.online && cap_status.output_enabled && cap_status.error_code == 0)
        pending_power_control.power_ceiling_w = fminf(pending_power_control.power_ceiling_w,
            (float)cap_status.available_power_limit_w);
#endif
    pending_power_control.control_enabled = control_config_valid && chassis_power_ready &&
        chassis_cmd_recv.chassis_mode != CHASSIS_ZERO_FORCE;
    RemoteControlSnapshot_s bench_remote={0};
    pending_power_control.zero_force=chassis_cmd_recv.chassis_mode==CHASSIS_ZERO_FORCE;
    pending_power_control.autotune_permitted = pending_power_control.control_enabled &&
        chassis_cmd_recv.chassis_mode==CHASSIS_NO_FOLLOW && RemoteControlGetSnapshot(&bench_remote) &&
        bench_remote.online && bench_remote.has_sample &&
        (uint32_t)(HAL_GetTick()-bench_remote.last_rx_tick_ms)<=100 &&
        bench_remote.control.rc.switch_left==2 && bench_remote.control.rc.switch_right==3 &&
        abs(bench_remote.control.rc.rocker_l_)<=30 && abs(bench_remote.control.rc.rocker_l1)<=30 &&
        abs(bench_remote.control.rc.rocker_r_)<=30 && abs(bench_remote.control.rc.rocker_r1)<=30 &&
        abs(bench_remote.control.rc.dial)<=30 && !bench_remote.control.key[KEY_PRESS].keys &&
        !bench_remote.control.mouse.press_l && !bench_remote.control.mouse.press_r;
#if CHASSIS_SPIN_DYNAMIC_ENABLED
    chassis_spin_input.target_power_w = pending_power_control.target_power_w;
    chassis_spin_input.control_enabled = pending_power_control.control_enabled;
    chassis_spin_input.energy_recovering = chassis_budget_output.recovering;
#endif
}

/* 机器人底盘控制核心任务 */
void ChassisTask()
{
    RuntimeTuningProcess(TUNING_POWER, ApplyPowerTuning);
    RuntimeTuningProcess(TUNING_SPIN, ApplySpinTuning);
    // 后续增加没收到消息的处理(双板的情况)
    // 获取新的控制信息
#ifdef ONE_BOARD
    SubGetMessage(chassis_sub, &chassis_cmd_recv);
#endif
#ifdef CHASSIS_BOARD
    chassis_cmd_recv = *(Chassis_Ctrl_Cmd_s *)CANCommGet(chasiss_can_comm);
#endif // CHASSIS_BOARD

    UpdateChassisPowerBudget();

#if CHASSIS_SPIN_DYNAMIC_ENABLED
    float spin_target_wz = 0;
    (void)ChassisSpinUpdate(&chassis_spin_config, &chassis_spin_input,
        DWT_GetDeltaT(&chassis_spin_dwt_count), &chassis_spin_state, &spin_target_wz);
#endif

    // 根据控制模式设定旋转速度
    switch (chassis_cmd_recv.chassis_mode)
    {
    case CHASSIS_NO_FOLLOW: // 底盘不旋转,但维持全向机动,一般用于调整云台姿态
        chassis_cmd_recv.wz = 0;
        break;
    case CHASSIS_FOLLOW_GIMBAL_YAW: // 跟随云台,不单独设置pid,以误差角度平方为速度输出
        chassis_cmd_recv.wz = -1.5f * chassis_cmd_recv.offset_angle * abs(chassis_cmd_recv.offset_angle);
        break;
    case CHASSIS_ROTATE: // 自旋并保持全向机动;模式选择固定、功率自适应或同向正弦
#if CHASSIS_SPIN_DYNAMIC_ENABLED
        chassis_cmd_recv.wz = spin_target_wz;
#else
        chassis_cmd_recv.wz = chassis_spin_config.fixed_wz;
#endif
        break;
    default:
        break;
    }

    // 根据云台和底盘的角度offset将控制量映射到底盘坐标系上
    // 底盘逆时针旋转为角度正方向;云台命令的方向以云台指向的方向为x,采用右手系(x指向正北时y在正东)
    static float sin_theta, cos_theta;
    cos_theta = arm_cos_f32(chassis_cmd_recv.offset_angle * DEGREE_2_RAD);
    sin_theta = arm_sin_f32(chassis_cmd_recv.offset_angle * DEGREE_2_RAD);
    chassis_vx = chassis_cmd_recv.vx * cos_theta - chassis_cmd_recv.vy * sin_theta;
    chassis_vy = chassis_cmd_recv.vx * sin_theta + chassis_cmd_recv.vy * cos_theta;

    // 根据控制模式进行正运动学解算,计算底盘输出
    CalculateWheelSpeed();

    // 根据裁判系统的反馈数据和电容数据对输出限幅并设定闭环参考值
    LimitChassisOutput();

    // 根据电机的反馈速度和IMU(如果有)计算真实速度
    EstimateSpeed();

    // // 获取裁判系统数据   建议将裁判系统与底盘分离，所以此处数据应使用消息中心发送
    // // 我方颜色id小于7是红色,大于7是蓝色,注意这里发送的是对方的颜色, 0:blue , 1:red
    // chassis_feedback_data.enemy_color = referee_data->GameRobotState.robot_id > 7 ? 1 : 0;
    // // 当前只做了17mm热量的数据获取,后续根据robot_def中的宏切换双枪管和英雄42mm的情况
    // chassis_feedback_data.bullet_speed = referee_data->GameRobotState.shooter_id1_17mm_speed_limit;
    // chassis_feedback_data.rest_heat = referee_data->PowerHeatData.shooter_heat0;

    DJIChassisPowerState_s power_state = {0};
    const bool power_state_valid = DJIChassisPowerGetSnapshot(&power_state);
    ChassisPowerFeedbackUpdate(power_state_valid && power_state.registered,
                               power_state_valid ? &power_state.power : NULL);
    (void)ChassisPowerFeedbackGet(&chassis_feedback_data.power_control);

    const ChassisTelemetrySnapshot_s telemetry = {
        .valid = true,
        .tick_ms = HAL_GetTick(),
        .mode = (uint8_t)chassis_cmd_recv.chassis_mode,
        .vx_cmd = chassis_vx, .vy_cmd = chassis_vy, .wz_cmd = chassis_cmd_recv.wz,
        .nominal_budget_w = pending_power_control.nominal_budget_w,
        .target_power_w = pending_power_control.target_power_w,
        .buffer_control_enabled = CHASSIS_BUFFER_POWER_ENABLE != 0,
        .buffer_estimate_valid = chassis_budget_state.buffer_estimate_valid,
        .buffer_recovering = chassis_budget_output.recovering,
        .buffer_lower_j = chassis_budget_output.buffer_lower_j,
        .buffer_predicted_j = chassis_budget_output.buffer_predicted_j,
    };
    taskENTER_CRITICAL();
    chassis_telemetry = telemetry;
    taskEXIT_CRITICAL();

    // 推送反馈消息
#ifdef ONE_BOARD
    PubPushMessage(chassis_pub, (void *)&chassis_feedback_data);
#endif
#ifdef CHASSIS_BOARD
    CANCommSend(chasiss_can_comm, (void *)&chassis_feedback_data);
#endif // CHASSIS_BOARD
}
