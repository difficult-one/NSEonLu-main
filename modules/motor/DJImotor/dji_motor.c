#include "dji_motor.h"
#include "general_def.h"
#include "bsp_dwt.h"
#include "bsp_log.h"
#include "rtt_telemetry.h"
#include "FreeRTOS.h"
#include "task.h"
#include "runtime_tuning.h"
#include "autotune_protocol.h"
#include "power_meter.h"
#if CHASSIS_POWER_CAPTURE_ENABLE
#include "bsp_uart_log.h"
#include "board_config.h"
#endif

#include <math.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(POWER_MODEL_MOTOR_COUNT == 4U, "DJI chassis power group must contain four motors");

#define CHASSIS_POWER_RTT_DIVIDER 40U

static uint8_t idx = 0; // register idx,是该文件的全局电机索引,在注册时使用
/* DJI电机的实例,此处仅保存指针,内存的分配将通过电机实例初始化时通过malloc()进行 */
static DJIMotorInstance *dji_motor_instance[DJI_MOTOR_CNT] = {NULL}; // 会在control任务中遍历该指针数组进行pid计算

/**
 * @brief 由于DJI电机发送以四个一组的形式进行,故对其进行特殊处理,用6个(2can*3group)can_instance专门负责发送
 *        该变量将在 DJIMotorControl() 中使用,分组在 MotorSenderGrouping()中进行
 *
 * @note  因为只用于发送,所以不需要在bsp_can中注册
 *
 * C610(m2006)/C620(m3508):0x1ff,0x200;
 * GM6020:0x1ff,0x2ff
 * 反馈(rx_id): GM6020: 0x204+id ; C610/C620: 0x200+id
 * can1: [0]:0x1FF,[1]:0x200,[2]:0x2FF
 * can2: [3]:0x1FF,[4]:0x200,[5]:0x2FF
 */
static CANInstance sender_assignment[6] = {
    [0] = {.can_handle = &hcan1, .txconf.StdId = 0x1ff, .txconf.IDE = CAN_ID_STD, .txconf.RTR = CAN_RTR_DATA, .txconf.DLC = 0x08, .tx_buff = {0}},
    [1] = {.can_handle = &hcan1, .txconf.StdId = 0x200, .txconf.IDE = CAN_ID_STD, .txconf.RTR = CAN_RTR_DATA, .txconf.DLC = 0x08, .tx_buff = {0}},
    [2] = {.can_handle = &hcan1, .txconf.StdId = 0x2ff, .txconf.IDE = CAN_ID_STD, .txconf.RTR = CAN_RTR_DATA, .txconf.DLC = 0x08, .tx_buff = {0}},
    [3] = {.can_handle = &hcan2, .txconf.StdId = 0x1ff, .txconf.IDE = CAN_ID_STD, .txconf.RTR = CAN_RTR_DATA, .txconf.DLC = 0x08, .tx_buff = {0}},
    [4] = {.can_handle = &hcan2, .txconf.StdId = 0x200, .txconf.IDE = CAN_ID_STD, .txconf.RTR = CAN_RTR_DATA, .txconf.DLC = 0x08, .tx_buff = {0}},
    [5] = {.can_handle = &hcan2, .txconf.StdId = 0x2ff, .txconf.IDE = CAN_ID_STD, .txconf.RTR = CAN_RTR_DATA, .txconf.DLC = 0x08, .tx_buff = {0}},
};

/**
 * @brief 6个用于确认是否有电机注册到sender_assignment中的标志位,防止发送空帧,此变量将在DJIMotorControl()使用
 *        flag的初始化在 MotorSenderGrouping()中进行
 */
static uint8_t sender_enable_flag[6] = {0};

static DJIChassisPowerInstance_s *chassis_power;

static float motor_output[DJI_MOTOR_CNT];
static float speed_reference_aps[DJI_MOTOR_CNT];
static float speed_feedback_aps[DJI_MOTOR_CNT];
static uint8_t chassis_power_rtt_count;
static DJI_Motor_Measure_s cycle_measure[DJI_MOTOR_CNT];
static float cycle_reference[DJI_MOTOR_CNT];
static Motor_Working_Type_e cycle_stop[DJI_MOTOR_CNT];
static bool cycle_online[DJI_MOTOR_CNT];
static DJIChassisPowerControlSnapshot_s cycle_control;
static bool cycle_ready;
static uint32_t tuning_stop_since, tuning_stop_tick;
static bool tuning_stopped, tuning_stop_seen;
static bool autotune_owned,autotune_inhibit;
static uint32_t autotune_trial;
static PIDInstance autotune_original[4];
static AutotuneDrive autotune_drive;

static void ResetTrialPID(PIDInstance *pid,float measured)
{
    pid->Iout=pid->ITerm=pid->Last_ITerm=0;
    pid->Dout=pid->Last_Dout=0;
    pid->Output=pid->Last_Output=0;
    pid->Last_Measure=measured;pid->Last_Err=-measured;
    (void)DWT_GetDeltaT(&pid->DWT_CNT);
}
static void ApplyAutotuneCycle(void)
{
    if (!chassis_power) return;
    const uint32_t now=HAL_GetTick();
    PIDAutotuneFeedback f={.now_ms=now,.can_ok=true,.feedback_ok=true,
        .permitted=cycle_control.autotune_permitted && cycle_control.control_enabled &&
            (uint32_t)(now-cycle_control.tick_ms)<=10 && !autotune_inhibit};
    for(unsigned w=0;w<4;++w) {
        unsigned i=chassis_power->motor_indices[w];
        f.rpm[w]=cycle_measure[i].speed_aps/6;
        f.feedback_ok &= cycle_online[i] && (uint32_t)(now-cycle_measure[i].rx_tick_ms)<=20;
        f.permitted &= fabsf(cycle_control.target_rpm[w])<=5;
    }
    AutotuneMotorStep(&f,&autotune_drive);
    if(autotune_drive.owned && !autotune_owned) {
        for(unsigned w=0;w<4;++w) autotune_original[w]=dji_motor_instance[chassis_power->motor_indices[w]]->motor_controller.speed_PID;
        autotune_trial=0;
    }
    if(!autotune_drive.owned && autotune_owned) {
        for(unsigned w=0;w<4;++w) {
            PIDInstance *pid=&dji_motor_instance[chassis_power->motor_indices[w]]->motor_controller.speed_PID;
            *pid=autotune_original[w];ResetTrialPID(pid,cycle_measure[chassis_power->motor_indices[w]].speed_aps);
        }
        autotune_inhibit=true;
    }
    autotune_owned=autotune_drive.owned;
    if(autotune_owned && autotune_drive.running && autotune_drive.trial!=autotune_trial) {
        for(unsigned w=0;w<4;++w) {
            PIDInstance *pid=&dji_motor_instance[chassis_power->motor_indices[w]]->motor_controller.speed_PID;
            *pid=autotune_original[w];ResetTrialPID(pid,cycle_measure[chassis_power->motor_indices[w]].speed_aps);
        }
        PIDInstance *pid=&dji_motor_instance[chassis_power->motor_indices[autotune_drive.test.wheel]]->motor_controller.speed_PID;
        pid->Kp=autotune_drive.test.kp;pid->Ki=autotune_drive.test.ki;pid->Kd=autotune_drive.test.kd;
        pid->MaxOut=fminf(pid->MaxOut,autotune_drive.test.current_limit);
        pid->IntegralLimit=fminf(pid->IntegralLimit,pid->MaxOut);
        autotune_trial=autotune_drive.trial;
    }
    if(autotune_owned || autotune_inhibit) for(unsigned w=0;w<4;++w) {
        unsigned i=chassis_power->motor_indices[w];
        bool selected=autotune_owned && autotune_drive.running && w==autotune_drive.test.wheel;
        cycle_stop[i]=selected ? MOTOR_ENALBED : MOTOR_STOP;
        cycle_reference[i]=selected ? autotune_drive.target_rpm*6 : 0;
    }
    if(autotune_inhibit && cycle_control.zero_force && (uint32_t)(now-cycle_control.tick_ms)<=10) autotune_inhibit=false;
}
static void UpdateTuningStopState(uint32_t now,bool transmitted)
{
    bool stopped=idx>0 && transmitted;
    for (unsigned i=0;i<idx;++i)
        stopped &= cycle_stop[i]==MOTOR_STOP && cycle_online[i] &&
            (uint32_t)(now-cycle_measure[i].rx_tick_ms)<=20 &&
            isfinite(cycle_measure[i].speed_aps) && fabsf(cycle_measure[i].speed_aps)<=300;
    taskENTER_CRITICAL();
    if (!stopped) {tuning_stop_seen=false; tuning_stopped=false;}
    else {
        if (!tuning_stop_seen || (uint32_t)(now-tuning_stop_tick)>5)
            tuning_stop_since=now;
        tuning_stopped=(uint32_t)(now-tuning_stop_since)>=100;
        tuning_stop_seen=true;
    }
    tuning_stop_tick=now;
    taskEXIT_CRITICAL();
}
bool DJIMotorCanSaveParameters(uint32_t now)
{
    taskENTER_CRITICAL();
    bool safe=tuning_stopped && (uint32_t)(now-tuning_stop_tick)<=5;
    for (unsigned i=0;i<idx;++i) safe &= dji_motor_instance[i]->stop_flag==MOTOR_STOP;
    taskEXIT_CRITICAL();
    return safe;
}
static void RegisterPIDTuning(void)
{
    float values[24];
    for (unsigned i = 0; i < 4; ++i) {
        PIDInstance *pid = &dji_motor_instance[chassis_power->motor_indices[i]]->motor_controller.speed_PID;
        const float wheel[] = {pid->Kp, pid->Ki, pid->Kd, pid->MaxOut, pid->IntegralLimit, pid->DeadBand};
        memcpy(&values[i * 6], wheel, sizeof wheel);
    }
    RuntimeTuningRegister(TUNING_PID, values, 0);
}
static bool ApplyPIDTuning(const float *v)
{
    if (!chassis_power) return false;
    for (unsigned i = 0; i < 4; ++i) {
        const float *w = &v[i * 6];
        for (unsigned j = 0; j < 6; ++j)
            if (!isfinite(w[j]) || w[j] < 0) return false;
        if (w[0] > 1000 || w[1] > 1000 || w[2] > 1000 ||
            w[3] <= 0 || w[3] > chassis_power->algorithm.max_current_command ||
            w[4] > w[3] || w[5] > 10000) return false;
    }
    /* Runs in the motor task, before any wheel computes its next PID output. */
    for (unsigned i = 0; i < 4; ++i) {
        const float *w = &v[i * 6];
        PIDInstance *pid = &dji_motor_instance[chassis_power->motor_indices[i]]->motor_controller.speed_PID;
        pid->Kp=w[0]; pid->Ki=w[1]; pid->Kd=w[2];
        pid->MaxOut=w[3]; pid->IntegralLimit=w[4]; pid->DeadBand=w[5];
        pid->Iout=pid->ITerm=pid->Last_ITerm=0;
        pid->Dout=pid->Last_Dout=0;
        pid->Output=fminf(fmaxf(pid->Output, -pid->MaxOut), pid->MaxOut);
        pid->Last_Output=pid->Output;
    }
    return true;
}
#if CHASSIS_POWER_CAPTURE_ENABLE
static ChassisPowerCapture_s chassis_capture;
#endif

/**
 * @brief 根据电调/拨码开关上的ID,根据说明书的默认id分配方式计算发送ID和接收ID,
 *        并对电机进行分组以便处理多电机控制命令
 */
static void MotorSenderGrouping(DJIMotorInstance *motor, CAN_Init_Config_s *config)
{
    uint8_t motor_id = config->tx_id - 1; // 下标从零开始,先减一方便赋值
    uint8_t motor_send_num;
    uint8_t motor_grouping;

    switch (motor->motor_type)
    {
    case M2006:
    case M3508:
        if (motor_id < 4) // 根据ID分组
        {
            motor_send_num = motor_id;
            motor_grouping = config->can_handle == &hcan1 ? 1 : 4;
        }
        else
        {
            motor_send_num = motor_id - 4;
            motor_grouping = config->can_handle == &hcan1 ? 0 : 3;
        }

        // 计算接收id并设置分组发送id
        config->rx_id = 0x200 + motor_id + 1;   // 把ID+1,进行分组设置
        sender_enable_flag[motor_grouping] = 1; // 设置发送标志位,防止发送空帧
        motor->message_num = motor_send_num;
        motor->sender_group = motor_grouping;

        // 检查是否发生id冲突
        for (size_t i = 0; i < idx; ++i)
        {
            if (dji_motor_instance[i]->motor_can_instance->can_handle == config->can_handle && dji_motor_instance[i]->motor_can_instance->rx_id == config->rx_id)
            {
                LOGERROR("[dji_motor] ID crash. Check in debug mode, add dji_motor_instance to watch to get more information.");
                uint16_t can_bus = config->can_handle == &hcan1 ? 1 : 2;
                while (1) // 6020的id 1-4和2006/3508的id 5-8会发生冲突(若有注册,即1!5,2!6,3!7,4!8) (1!5!,LTC! (((不是)
                    LOGERROR("[dji_motor] id [%d], can_bus [%d]", config->rx_id, can_bus);
            }
        }
        break;

    case GM6020:
        if (motor_id < 4)
        {
            motor_send_num = motor_id;
            motor_grouping = config->can_handle == &hcan1 ? 0 : 3;
        }
        else
        {
            motor_send_num = motor_id - 4;
            motor_grouping = config->can_handle == &hcan1 ? 2 : 5;
        }

        config->rx_id = 0x204 + motor_id + 1;   // 把ID+1,进行分组设置
        sender_enable_flag[motor_grouping] = 1; // 只要有电机注册到这个分组,置为1;在发送函数中会通过此标志判断是否有电机注册
        motor->message_num = motor_send_num;
        motor->sender_group = motor_grouping;

        for (size_t i = 0; i < idx; ++i)
        {
            if (dji_motor_instance[i]->motor_can_instance->can_handle == config->can_handle && dji_motor_instance[i]->motor_can_instance->rx_id == config->rx_id)
            {
                LOGERROR("[dji_motor] ID crash. Check in debug mode, add dji_motor_instance to watch to get more information.");
                uint16_t can_bus = config->can_handle == &hcan1 ? 1 : 2;
                while (1) // 6020的id 1-4和2006/3508的id 5-8会发生冲突(若有注册,即1!5,2!6,3!7,4!8) (1!5!,LTC! (((不是)
                    LOGERROR("[dji_motor] id [%d], can_bus [%d]", config->rx_id, can_bus);
            }
        }
        break;

    default: // other motors should not be registered here
        while (1)
            LOGERROR("[dji_motor]You must not register other motors using the API of DJI motor."); // 其他电机不应该在这里注册
    }
}

/**
 * @todo  是否可以简化多圈角度的计算？
 * @brief 根据返回的can_instance对反馈报文进行解析
 *
 * @param _instance 收到数据的instance,通过遍历与所有电机进行对比以选择正确的实例
 */
static void DecodeDJIMotor(CANInstance *_instance)
{
    // 这里对can instance的id进行了强制转换,从而获得电机的instance实例地址
    // _instance指针指向的id是对应电机instance的地址,通过强制转换为电机instance的指针,再通过->运算符访问电机的成员motor_measure,最后取地址获得指针
    uint8_t *rxbuff = _instance->rx_buff;
    DJIMotorInstance *motor = (DJIMotorInstance *)_instance->id;
    DJI_Motor_Measure_s *measure = &motor->measure; // measure要多次使用,保存指针减小访存开销

    DaemonReload(motor->daemon);
    motor->dt = DWT_GetDeltaT(&motor->feed_cnt);

    // 解析数据并对电流和速度进行滤波,电机的反馈报文具体格式见电机说明手册
    measure->last_ecd = measure->ecd;
    measure->ecd = ((uint16_t)rxbuff[0]) << 8 | rxbuff[1];
    measure->angle_single_round = ECD_ANGLE_COEF_DJI * (float)measure->ecd;
    measure->speed_aps = (1.0f - SPEED_SMOOTH_COEF) * measure->speed_aps +
                         RPM_2_ANGLE_PER_SEC * SPEED_SMOOTH_COEF * (float)((int16_t)(rxbuff[2] << 8 | rxbuff[3]));
    measure->real_current = (1.0f - CURRENT_SMOOTH_COEF) * measure->real_current +
                            CURRENT_SMOOTH_COEF * (float)((int16_t)(rxbuff[4] << 8 | rxbuff[5]));
    measure->temperature = rxbuff[6];

    // 多圈角度计算,前提是假设两次采样间电机转过的角度小于180°,自己画个图就清楚计算过程了
    if (measure->ecd - measure->last_ecd > 4096)
        measure->total_round--;
    else if (measure->ecd - measure->last_ecd < -4096)
        measure->total_round++;
    measure->total_angle = measure->total_round * 360 + measure->angle_single_round;
    measure->rx_tick_ms = HAL_GetTick();
    ++measure->rx_sequence;
}

static void DJIMotorLostCallback(void *motor_ptr)
{
    DJIMotorInstance *motor = (DJIMotorInstance *)motor_ptr;
    uint16_t can_bus = motor->motor_can_instance->can_handle == &hcan1 ? 1 : 2;
    LOGWARNING("[dji_motor] Motor lost, can bus [%d] , id [%d]", can_bus, motor->motor_can_instance->tx_id);
}

// 电机初始化,返回一个电机实例
DJIMotorInstance *DJIMotorInit(Motor_Init_Config_s *config)
{
    DJIMotorInstance *instance = (DJIMotorInstance *)malloc(sizeof(DJIMotorInstance));
    memset(instance, 0, sizeof(DJIMotorInstance));

    // motor basic setting 电机基本设置
    instance->motor_type = config->motor_type;                         // 6020 or 2006 or 3508
    instance->motor_settings = config->controller_setting_init_config; // 正反转,闭环类型等

    // motor controller init 电机控制器初始化
    PIDInit(&instance->motor_controller.current_PID, &config->controller_param_init_config.current_PID);
    PIDInit(&instance->motor_controller.speed_PID, &config->controller_param_init_config.speed_PID);
    PIDInit(&instance->motor_controller.angle_PID, &config->controller_param_init_config.angle_PID);
    instance->motor_controller.other_angle_feedback_ptr = config->controller_param_init_config.other_angle_feedback_ptr;
    instance->motor_controller.other_speed_feedback_ptr = config->controller_param_init_config.other_speed_feedback_ptr;
    instance->motor_controller.current_feedforward_ptr = config->controller_param_init_config.current_feedforward_ptr;
    instance->motor_controller.speed_feedforward_ptr = config->controller_param_init_config.speed_feedforward_ptr;
    // 后续增加电机前馈控制器(速度和电流)

    // 电机分组,因为至多4个电机可以共用一帧CAN控制报文
    MotorSenderGrouping(instance, &config->can_init_config);

    // 注册电机到CAN总线
    config->can_init_config.can_module_callback = DecodeDJIMotor; // set callback
    config->can_init_config.id = instance;                        // set id,eq to address(it is identity)
    instance->motor_can_instance = CANRegister(&config->can_init_config);

    // 注册守护线程
    Daemon_Init_Config_s daemon_config = {
        .callback = DJIMotorLostCallback,
        .owner_id = instance,
        .reload_count = 2, // 20ms未收到数据则丢失
    };
    instance->daemon = DaemonRegister(&daemon_config);

    DJIMotorEnable(instance);
    dji_motor_instance[idx++] = instance;
    return instance;
}

/* 电流只能通过电机自带传感器监测,后续考虑加入力矩传感器应变片等 */
void DJIMotorChangeFeed(DJIMotorInstance *motor, Closeloop_Type_e loop, Feedback_Source_e type)
{
    if (loop == ANGLE_LOOP)
        motor->motor_settings.angle_feedback_source = type;
    else if (loop == SPEED_LOOP)
        motor->motor_settings.speed_feedback_source = type;
    else
        LOGERROR("[dji_motor] loop type error, check memory access and func param"); // 检查是否传入了正确的LOOP类型,或发生了指针越界
}

void DJIMotorStop(DJIMotorInstance *motor)
{
    motor->stop_flag = MOTOR_STOP;
}

void DJIMotorEnable(DJIMotorInstance *motor)
{
    motor->stop_flag = MOTOR_ENALBED;
}

/* 修改电机的实际闭环对象 */
void DJIMotorOuterLoop(DJIMotorInstance *motor, Closeloop_Type_e outer_loop)
{
    motor->motor_settings.outer_loop_type = outer_loop;
}

// 设置参考值
void DJIMotorSetRef(DJIMotorInstance *motor, float ref)
{
    motor->motor_controller.pid_ref = ref;
}

static bool FindDJIMotorIndex(const DJIMotorInstance *motor, uint8_t *motor_index)
{
    for (uint8_t i = 0; i < idx; ++i)
    {
        if (dji_motor_instance[i] == motor)
        {
            *motor_index = i;
            return true;
        }
    }
    return false;
}

bool DJIChassisPowerRegister(const DJIChassisPowerConfig_s *config)
{
    uint8_t motor_indices[POWER_MODEL_MOTOR_COUNT];

    if (config == NULL || chassis_power != NULL ||
        !ChassisPowerAlgorithmConfigIsValid(&config->algorithm))
        return false;

    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
    {
        if (config->motors[i] == NULL || config->motors[i]->motor_type != M3508 ||
            !PowerModelConfigIsValid(&config->models[i]) ||
            !FindDJIMotorIndex(config->motors[i], &motor_indices[i]))
            return false;

        for (size_t j = 0; j < i; ++j)
        {
            if (config->motors[i] == config->motors[j])
                return false;
        }
    }

    DJIChassisPowerInstance_s *instance =
        (DJIChassisPowerInstance_s *)malloc(sizeof(*instance));
    if (instance == NULL)
        return false;
    memset(instance, 0, sizeof(*instance));

    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
    {
        instance->motor_indices[i] = motor_indices[i];
        instance->models[i] = config->models[i];
    }
    instance->algorithm = config->algorithm;
    instance->input.attenuation = 1.0f;
    instance->state.registered = true;
    instance->registered = true;
    chassis_power = instance;
    RegisterPIDTuning();
    RuntimeTuningRestore(TUNING_PID, ApplyPIDTuning);
    (void)DWT_GetDeltaT(&instance->control_dwt_count);
#if CHASSIS_POWER_CAPTURE_ENABLE
    ChassisPowerCaptureReset(&chassis_capture);
#endif
    chassis_power_rtt_count = 0U;
    RTTTelemetryWriteChassisPowerHeader();
    return true;
}

void DJIChassisPowerPublishControl(const DJIChassisPowerControlSnapshot_s *control)
{
    if (chassis_power == NULL || control == NULL) return;
    taskENTER_CRITICAL();
    chassis_power->control = *control;
    for (unsigned i = 0; i < 4; ++i) {
        DJIMotorInstance *motor = dji_motor_instance[chassis_power->motor_indices[i]];
        motor->motor_controller.pid_ref = control->target_rpm[i] * 6.0f;
        motor->stop_flag = control->control_enabled ? MOTOR_ENALBED : MOTOR_STOP;
    }
    taskEXIT_CRITICAL();
}

static void ReadMotorCycle(void)
{
    taskENTER_CRITICAL();
    if (chassis_power != NULL) cycle_control = chassis_power->control;
    for (unsigned i = 0; i < idx; ++i) {
        cycle_measure[i] = dji_motor_instance[i]->measure;
        cycle_reference[i] = dji_motor_instance[i]->motor_controller.pid_ref;
        cycle_stop[i] = dji_motor_instance[i]->stop_flag;
        cycle_online[i] = DaemonIsOnline(dji_motor_instance[i]->daemon) != 0;
    }
    taskEXIT_CRITICAL();
    cycle_ready = true;
}

void DJIChassisPowerSetAttenuation(float attenuation)
{
    if (chassis_power != NULL)
        chassis_power->input.attenuation = attenuation;
}

const DJIChassisPowerState_s *DJIChassisPowerGetState(void)
{
    return chassis_power != NULL ? &chassis_power->state : NULL;
}

bool DJIChassisPowerGetSnapshot(DJIChassisPowerState_s *snapshot)
{
    if (chassis_power == NULL || snapshot == NULL)
        return false;

    taskENTER_CRITICAL();
    *snapshot = chassis_power->state;
    taskEXIT_CRITICAL();
    return true;
}

static void ApplyChassisPowerGroup(void)
{
    bool output_enabled[POWER_MODEL_MOTOR_COUNT];
    ChassisPowerOutput_s power_output;
    ChassisPowerTelemetry_s telemetry;
    ChassisPowerControlTelemetry_s control_telemetry;
    float measured_chassis_power_w;
    bool meter_online;
    uint32_t meter_tick_ms;

    if (chassis_power == NULL || !chassis_power->registered)
        return;

    if (!cycle_ready) {
        ReadMotorCycle();
    }
    cycle_ready = false;
    DJIChassisPowerState_s state = {.registered = true, .control_version = cycle_control.version,
        .control_tick_ms = cycle_control.tick_ms};
    PowerMeterStatus_s meter = {0};
#if POWER_METER_ENABLE
    (void)PowerMeterGetStatus(&meter);
#endif
    const uint32_t now_ms = HAL_GetTick();
    state.tick_ms = now_ms;
    measured_chassis_power_w = meter.power_w;
    meter_online = meter.online;
    meter_tick_ms = meter.last_rx_tick_ms;
    state.measured_power_w = meter.power_w;
    state.meter_online = meter.online;
    state.meter_tick_ms = meter.last_rx_tick_ms;
    chassis_power->input.referee_power_limit_w = cycle_control.legacy_budget_w;
    ChassisPowerTransientInput_s transient_input = {
        .budget_w = cycle_control.nominal_budget_w, .target_w = cycle_control.target_power_w,
        .slow_w = cycle_control.slow_correction_w, .meter_w = meter.power_w,
        .power_deadband_w = cycle_control.power_deadband_w,
        .power_ceiling_w = cycle_control.power_ceiling_w,
        .now_ms = now_ms, .meter_tick_ms = meter.last_rx_tick_ms,
        .meter_enabled = POWER_METER_ENABLE != 0, .meter_online = meter.online,
        .startup_enabled = CHASSIS_POWER_CONTROL_STAGE != 1,
        .control_enabled = cycle_control.control_enabled};

    for (size_t slot = 0; slot < POWER_MODEL_MOTOR_COUNT; ++slot)
    {
        const uint8_t index = chassis_power->motor_indices[slot];
        chassis_power->input.desired_current[slot] = motor_output[index];
        chassis_power->input.speed_rpm[slot] = speed_feedback_aps[index] / 6.0f;
        chassis_power->input.speed_error_rpm[slot] =
            (speed_reference_aps[index] - speed_feedback_aps[index]) / 6.0f;
        state.raw_current[slot] = motor_output[index];
        state.feedback_current[slot] = cycle_measure[index].real_current;
        state.feedback_sequence[slot] = cycle_measure[index].rx_sequence;
        state.feedback_tick_ms[slot] = cycle_measure[index].rx_tick_ms;
        state.numeric_error |= !isfinite(motor_output[index]);
        transient_input.raw_current[slot] = motor_output[index];
        transient_input.actual_rpm[slot] = chassis_power->input.speed_rpm[slot];
        transient_input.target_rpm[slot] = speed_reference_aps[index] / 6.0f;
        state.numeric_error |= !isfinite(transient_input.actual_rpm[slot]) ||
                               !isfinite(transient_input.target_rpm[slot]);
        transient_input.feedback_tick_ms[slot] = cycle_measure[index].rx_tick_ms;
        transient_input.feedback_sequence[slot] = cycle_measure[index].rx_sequence;
        if (cycle_online[index]) transient_input.online_mask |= (uint8_t)(1U << slot);
        if (cycle_stop[index] != MOTOR_STOP) transient_input.enabled_mask |= (uint8_t)(1U << slot);
    }

    ChassisPowerTransientConfig_s transient_config = ChassisPowerTransientDefaultConfig();
    transient_config.eta = chassis_power->algorithm.safety_factor;
    transient_config.max_current_command = chassis_power->algorithm.max_current_command;
    (void)ChassisPowerTransientUpdate(&transient_config, &transient_input,
        DWT_GetDeltaT(&chassis_power->control_dwt_count), &chassis_power->transient, &state.transient);
#if CHASSIS_POWER_CONTROL_STAGE >= 1
    chassis_power->input.referee_power_limit_w = state.transient.model_budget_w;
#endif

    const bool power_valid = PowerModelApply(chassis_power->models,
                                             &chassis_power->algorithm,
                                             &chassis_power->input,
                                             &power_output);
    ChassisPowerOutput_s proposed_power;
    const float applied_budget = chassis_power->input.referee_power_limit_w;
    chassis_power->input.referee_power_limit_w = state.transient.model_budget_w;
    const bool proposed_valid = PowerModelApply(chassis_power->models, &chassis_power->algorithm,
                                                &chassis_power->input, &proposed_power);
    chassis_power->input.referee_power_limit_w = applied_budget;
    for (unsigned w = 0; w < 4; ++w) {
        const unsigned index = chassis_power->motor_indices[w];
        if (!cycle_online[index] || cycle_stop[index] == MOTOR_STOP || !cycle_control.control_enabled)
            proposed_power.limited_current[w] = 0;
    }
    if (proposed_valid && !state.transient.fault && cycle_control.control_enabled)
        (void)ChassisPowerTransientGuard(&transient_config, chassis_power->models, &transient_input,
                                        proposed_power.limited_current, &chassis_power->transient, &state.transient);

    for (size_t slot = 0; slot < POWER_MODEL_MOTOR_COUNT; ++slot)
    {
        const uint8_t index = chassis_power->motor_indices[slot];
        const bool motor_online = cycle_online[index];
        output_enabled[slot] = power_valid && motor_online && cycle_control.control_enabled &&
                               cycle_stop[index] != MOTOR_STOP && !state.transient.fault;
        if (motor_online) state.online_mask |= (uint8_t)(1U << slot);
        state.base_limited |= output_enabled[slot] && (state.transient.drive_mask & (1U << slot)) &&
            fabsf(power_output.limited_current[slot]) < fabsf(state.raw_current[slot]);
        float chosen = power_output.limited_current[slot];
#if CHASSIS_POWER_CONTROL_STAGE >= 2
        chosen = state.transient.guarded_current[slot];
#else
        /* A supplied source ceiling is mandatory even in observe/fast-only
         * configurations. Zero preserves those stages' legacy behavior. */
        if (cycle_control.power_ceiling_w > 0)
            chosen = state.transient.guarded_current[slot];
#endif
        const float final = output_enabled[slot] ? (float)(int16_t)chosen : 0;
        motor_output[index] = final;
        power_output.limited_current[slot] = final;
        state.proposed_current[slot] = proposed_valid && output_enabled[slot] ?
            state.transient.guarded_current[slot] : 0;
    }

    if (power_valid) (void)PowerModelRecomputeFinal(chassis_power->models,
        &chassis_power->input, power_output.limited_current, &power_output);

    (void)ChassisPowerTelemetryBuild(&chassis_power->input,
                                     &power_output,
                                     output_enabled,
                                     &telemetry);
    for (unsigned i = 0; i < 4; ++i) {
        telemetry.target_speed_rpm[i] = transient_input.target_rpm[i];
        telemetry.actual_speed_rpm[i] = transient_input.actual_rpm[i];
    }
    ChassisPowerTransientRecordSent(&chassis_power->transient, power_output.limited_current);

    state.power = power_output;
    state.telemetry = telemetry;
    control_telemetry = cycle_control.telemetry;
#if CHASSIS_POWER_CONTROL_STAGE >= 1
    control_telemetry.corrected_budget_w = applied_budget;
    control_telemetry.correction_w = state.transient.slow_applied_w + state.transient.fast_w;
#endif
    taskENTER_CRITICAL();
    chassis_power->state = state;
    taskEXIT_CRITICAL();

#if CHASSIS_POWER_CAPTURE_ENABLE
    ChassisPowerCaptureSample_s sample = {.tick_ms = now_ms, .meter_tick_ms = meter_tick_ms,
        .budget_w = cycle_control.nominal_budget_w, .measured_w = measured_chassis_power_w,
        .slow_w = state.transient.slow_applied_w, .fast_w = state.transient.fast_w,
        .state = (uint8_t)state.transient.mode,
        .excess_j = state.transient.excess_j, .allowance_j = state.transient.allowance_j,
        .drive_mask = state.transient.drive_mask, .brake_mask = state.transient.brake_mask,
        .enabled_mask = telemetry.motor_enabled_mask,
        .meter_age_ms = (uint8_t)fminf(255, (float)(uint32_t)(now_ms - meter_tick_ms)),
        .flags = (uint8_t)((state.numeric_error ? 1 : 0) | (state.transient.guard_infeasible ? 2 : 0) |
                          (state.transient.growth_fallback ? 4 : 0) |
                          (state.transient.guard_search_exhausted ? 8 : 0) | (CHASSIS_POWER_CONTROL_STAGE << 4))};
    for (unsigned i = 0; i < 4; ++i) {
        sample.raw_current[i] = state.raw_current[i];
        sample.final_current[i] = power_output.limited_current[i];
        sample.feedback_current[i] = state.feedback_current[i];
        sample.actual_rpm[i] = telemetry.actual_speed_rpm[i];
        sample.target_rpm[i] = telemetry.target_speed_rpm[i];
        sample.motor_age_ms[i] = (uint8_t)fminf(255, (float)(uint32_t)(now_ms - state.feedback_tick_ms[i]));
        sample.actual_model_w += power_output.predicted_limited_power_w[i];
        sample.guard_model_w += PowerModelPredict(&chassis_power->models[i],
                                                  power_output.limited_current[i], state.transient.guard_rpm[i]);
    }
    ChassisPowerCaptureUpdate(&chassis_capture, &sample);
#endif

    if (++chassis_power_rtt_count >= CHASSIS_POWER_RTT_DIVIDER)
    {
        chassis_power_rtt_count = 0U;
        RTTTelemetryWriteChassisPowerRow(
            &power_output,
            &telemetry,
            &control_telemetry,
            HAL_GetTick(),
            measured_chassis_power_w,
            meter_online,
            meter_tick_ms);
    }
}

// 为所有电机实例计算三环PID,发送控制报文
void DJIMotorControl()
{
    RuntimeTuningProcess(TUNING_PID, ApplyPIDTuning);
    uint8_t group, num;
    int16_t set;
    DJIMotorInstance *motor;
    Motor_Control_Setting_s *motor_setting;
    Motor_Controller_s *motor_controller;
    DJI_Motor_Measure_s *measure;
    float pid_measure, pid_ref;
    ReadMotorCycle();
    ApplyAutotuneCycle();

    // 第一阶段：计算所有 DJI 电机的原始输出，暂不写入 CAN 缓冲区。
    for (size_t i = 0; i < idx; ++i)
    {
        motor = dji_motor_instance[i];
        motor_setting = &motor->motor_settings;
        motor_controller = &motor->motor_controller;
        measure = &cycle_measure[i];
        pid_ref = cycle_reference[i];
        speed_reference_aps[i] = 0.0f;
        speed_feedback_aps[i] = 0.0f;

        if (motor_setting->motor_reverse_flag == MOTOR_DIRECTION_REVERSE)
            pid_ref *= -1;

        if ((motor_setting->close_loop_type & ANGLE_LOOP) && motor_setting->outer_loop_type == ANGLE_LOOP)
        {
            if (motor_setting->angle_feedback_source == OTHER_FEED)
                pid_measure = *motor_controller->other_angle_feedback_ptr;
            else
                pid_measure = measure->total_angle;
            pid_ref = PIDCalculate(&motor_controller->angle_PID, pid_measure, pid_ref);
        }

        if ((motor_setting->close_loop_type & SPEED_LOOP) && (motor_setting->outer_loop_type & (ANGLE_LOOP | SPEED_LOOP)))
        {
            if (motor_setting->feedforward_flag & SPEED_FEEDFORWARD)
                pid_ref += *motor_controller->speed_feedforward_ptr;

            if (motor_setting->speed_feedback_source == OTHER_FEED)
                pid_measure = *motor_controller->other_speed_feedback_ptr;
            else
                pid_measure = measure->speed_aps;

            speed_reference_aps[i] = pid_ref;
            speed_feedback_aps[i] = pid_measure;
            pid_ref = PIDCalculate(&motor_controller->speed_PID, pid_measure, pid_ref);
        }

        if (motor_setting->feedforward_flag & CURRENT_FEEDFORWARD)
            pid_ref += *motor_controller->current_feedforward_ptr;
        if (motor_setting->close_loop_type & CURRENT_LOOP)
            pid_ref = PIDCalculate(&motor_controller->current_PID, measure->real_current, pid_ref);

        if (motor_setting->feedback_reverse_flag == FEEDBACK_DIRECTION_REVERSE)
            pid_ref *= -1;

        motor_output[i] = pid_ref;
    }

    // 第二阶段：只修改已注册的四个底盘电机输出。
    ApplyChassisPowerGroup();

    /* A new stop/control message can arrive while PID/power calculations run. */
    if(chassis_power && (autotune_owned || autotune_inhibit)) {
        taskENTER_CRITICAL();
        bool permitted=chassis_power->control.autotune_permitted && chassis_power->control.control_enabled &&
            (uint32_t)(HAL_GetTick()-chassis_power->control.tick_ms)<=10;
        taskEXIT_CRITICAL();
        if(!permitted) {
            if(autotune_owned) AutotuneCancel();
            for(unsigned w=0;w<4;++w) {unsigned i=chassis_power->motor_indices[w];cycle_stop[i]=MOTOR_STOP;motor_output[i]=0;}
        }
    }

    // 第三阶段：所有 DJI 电机共用同一套 CAN 打包和发送路径。
    for (size_t i = 0; i < idx; ++i)
    {
        motor = dji_motor_instance[i];
        set = isfinite(motor_output[i]) ? (int16_t)motor_output[i] : 0;
        group = motor->sender_group;
        num = motor->message_num;
        sender_assignment[group].tx_buff[2 * num] = (uint8_t)(set >> 8);         // 低八位
        sender_assignment[group].tx_buff[2 * num + 1] = (uint8_t)(set & 0x00ff); // 高八位

        if (cycle_stop[i] == MOTOR_STOP)
            memset(sender_assignment[group].tx_buff + 2 * num, 0, sizeof(uint16_t));
    }

    bool transmitted = true;
    for (size_t i = 0; i < 6; ++i)
    {
        if (sender_enable_flag[i])
        {
            transmitted &= CANTransmit(&sender_assignment[i], 1)!=0;
        }
    }
    UpdateTuningStopState(HAL_GetTick(),transmitted);
    if(chassis_power && autotune_owned) {
        unsigned w=autotune_drive.test.wheel,i=chassis_power->motor_indices[w];
        AutotuneObserve(HAL_GetTick(),cycle_measure[i].speed_aps/6,chassis_power->state.raw_current[w],
            motor_output[i],fabsf(motor_output[i])>=dji_motor_instance[i]->motor_controller.speed_PID.MaxOut*.98f,transmitted);
    }
}

void DJIChassisPowerCaptureDrain(void)
{
#if CHASSIS_POWER_CAPTURE_ENABLE && UART1_LOG_ENABLED
    BSPUARTLogPoll();
    uint8_t frame[CHASSIS_POWER_CAPTURE_FRAME_BYTES];
    const uint32_t now = HAL_GetTick();
    taskENTER_CRITICAL();
    if (BSPUARTLogIsIdle()) ChassisPowerCaptureDrainComplete(&chassis_capture);
    const bool ready = chassis_power != NULL &&
        ChassisPowerCaptureNextFrame(&chassis_capture, now,
                                    !chassis_power->control.control_enabled, frame);
    taskEXIT_CRITICAL();
    if (ready && BSPUARTLogWrite(frame, sizeof(frame)) == sizeof(frame)) {
        taskENTER_CRITICAL();
        ChassisPowerCaptureFrameAccepted(&chassis_capture, now);
        taskEXIT_CRITICAL();
    }
#endif
}
