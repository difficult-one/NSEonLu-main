#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../../modules/power_meter/power_meter.h"

#include "../../modules/motor/DJImotor/dji_motor.c"

static int failures;
static char captured_data[RTT_TELEMETRY_CHASSIS_POWER_CSV_MAX_LENGTH];
static PowerMeterStatus_s test_meter;
static bool publish_stop_during_pid;
static uint32_t test_tick=123;

static void SetMeter(float power, bool online, uint32_t tick)
{
    test_meter = (PowerMeterStatus_s){.power_w = power, .online = online, .last_rx_tick_ms = tick};
}
static void PublishTelemetry(const ChassisPowerControlTelemetry_s *telemetry)
{
    if (telemetry == NULL) return;
    DJIChassisPowerControlSnapshot_s control = chassis_power->control;
    control.legacy_budget_w = chassis_power->input.referee_power_limit_w;
    control.telemetry = *telemetry;
    DJIChassisPowerPublishControl(&control);
}

bool PowerMeterGetStatus(PowerMeterStatus_s *status)
{
    *status = test_meter;
    return true;
}


#define ASSERT_TRUE(condition)                                                   \
    do                                                                           \
    {                                                                            \
        if (!(condition))                                                        \
        {                                                                        \
            printf("FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition);         \
            ++failures;                                                          \
        }                                                                        \
    } while (0)

#define ASSERT_STREQ(expected, actual) ASSERT_TRUE(strcmp((expected), (actual)) == 0)

static void AssertControlColumnsMatch(const char *expected)
{
#if CHASSIS_POWER_CALIBRATION_ENABLE
    const size_t prefix_length = strlen(expected) - 2U;
    if (strncmp(expected, captured_data, prefix_length) != 0)
        printf("expected %sactual %s", expected, captured_data);
    ASSERT_TRUE(strncmp(expected, captured_data, prefix_length) == 0);
    ASSERT_TRUE(captured_data[prefix_length] == ',');
#else
    if (strcmp(expected, captured_data) != 0) printf("expected %sactual %s", expected, captured_data);
    ASSERT_STREQ(expected, captured_data);
#endif
}

CAN_HandleTypeDef hcan1;
CAN_HandleTypeDef hcan2;

CANInstance *CANRegister(CAN_Init_Config_s *config)
{
    (void)config;
    return NULL;
}

uint8_t CANTransmit(CANInstance *instance, float timeout)
{
    (void)instance;
    (void)timeout;
    return 1U;
}

uint32_t HAL_GetTick(void)
{
    return test_tick;
}

void PIDInit(PIDInstance *pid, PID_Init_Config_s *config)
{
    (void)pid;
    (void)config;
}

float PIDCalculate(PIDInstance *pid, float measure, float ref)
{
    (void)pid;
    (void)measure;
    if (publish_stop_during_pid) {
        publish_stop_during_pid = false;
        DJIChassisPowerControlSnapshot_s next = chassis_power->control;
        ++next.version; next.control_enabled = false;
        DJIChassisPowerPublishControl(&next);
    }
    return ref;
}

DaemonInstance *DaemonRegister(Daemon_Init_Config_s *config)
{
    (void)config;
    return NULL;
}

void DaemonReload(DaemonInstance *instance)
{
    (void)instance;
}

uint8_t DaemonIsOnline(DaemonInstance *instance)
{
    return instance != NULL ? instance->online : 0U;
}

float DWT_GetDeltaT(uint32_t *count)
{
    *count=1234;
    return 0.001f;
}

unsigned BSPLogWrite(const void *data, unsigned length)
{
    if (length >= sizeof(captured_data))
        return 0U;
    memcpy(captured_data, data, length);
    captured_data[length] = '\0';
    return length;
}

unsigned BSPLogWriteString(const char *text)
{
    return BSPLogWrite(text, (unsigned)strlen(text));
}

static void PreparePowerGroup(void)
{
    static DJIChassisPowerInstance_s power_group;
    static DJIMotorInstance motors[POWER_MODEL_MOTOR_COUNT];
    static DaemonInstance daemons[POWER_MODEL_MOTOR_COUNT];

    memset(&power_group, 0, sizeof(power_group));
    memset(motors, 0, sizeof(motors));
    memset(daemons, 0, sizeof(daemons));
    memset(dji_motor_instance, 0, sizeof(dji_motor_instance));
    memset(motor_output, 0, sizeof(motor_output));
    memset(speed_reference_aps, 0, sizeof(speed_reference_aps));
    memset(speed_feedback_aps, 0, sizeof(speed_feedback_aps));
    memset(captured_data, 0, sizeof(captured_data));
    memset(&test_meter, 0, sizeof(test_meter));
    idx = 4;

    power_group.registered = true;
    power_group.input.referee_power_limit_w = 100.0f;
    power_group.control = (DJIChassisPowerControlSnapshot_s){
        .nominal_budget_w = 100, .target_power_w = 95,
        .legacy_budget_w = 100, .control_enabled = true};
    power_group.input.attenuation = 1.0f;
    power_group.algorithm = (ChassisPowerAlgorithmConfig_s){
        .safety_factor = 1.0f,
        .small_error_threshold_rpm = 0.0f,
        .reserved_power_threshold_w = 4.0f,
        .per_motor_reserved_power_w = 1.0f,
        .max_current_command = 100.0f,
    };

    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
    {
        power_group.motor_indices[i] = (uint8_t)i;
        power_group.models[i].current_conversion = 1.0f;
        daemons[i].online = 1U;
        motors[i].daemon = &daemons[i];
        motors[i].stop_flag = MOTOR_ENALBED;
        motors[i].sender_group = 1;
        motors[i].message_num = (uint8_t)i;
        dji_motor_instance[i] = &motors[i];
        motor_output[i] = (float)(i + 1U);
    }

    chassis_power = &power_group;
    chassis_power_rtt_count = 0U;
}

static void EmitOneRow(void)
{
    for (size_t i = 0; i < CHASSIS_POWER_RTT_DIVIDER; ++i)
        ApplyChassisPowerGroup();
}

static void TestAtomicPublicationAndFinalIntegerCommands(void)
{
    PreparePowerGroup();
    const DJIChassisPowerControlSnapshot_s control = {
        .version = 42, .tick_ms = 120, .nominal_budget_w = 20,
        .target_power_w = 19, .legacy_budget_w = 20, .control_enabled = true,
        .target_rpm = {1000, -1000, 500, -500},
    };
    DJIChassisPowerPublishControl(&control);
    motor_output[0] = 3.9f;
    ApplyChassisPowerGroup();
    DJIChassisPowerState_s state;
    ASSERT_TRUE(DJIChassisPowerGetSnapshot(&state));
    ASSERT_TRUE(state.control_version == 42);
    ASSERT_TRUE(state.raw_current[0] == 3.9f);
    ASSERT_TRUE(state.power.limited_current[0] == 3);
    ASSERT_TRUE(dji_motor_instance[1]->motor_controller.pid_ref == -6000);
    DJIMotorStop(dji_motor_instance[0]);
    ApplyChassisPowerGroup();
    ASSERT_TRUE(chassis_power->state.power.limited_current[0] == 0);
}

static void TestZeroDemandIsNotActualBaseLimiting(void)
{
    PreparePowerGroup();
    for (unsigned w = 0; w < 4; ++w) {
        chassis_power->models[w].k0 = 30;
        motor_output[w] = 0;
    }
    ApplyChassisPowerGroup();
    ASSERT_TRUE(!chassis_power->state.base_limited);
}

static void TestTargetSnapshotDoesNotDependOnFeedbackRounding(void)
{
    PreparePowerGroup();
    for (unsigned w = 0; w < 4; ++w) speed_reference_aps[w] = 6000;
    const float feedback[] = {5.10000038f, 2700.3f, 5399.99f};
    for (unsigned n = 0; n < 3; ++n) {
        speed_feedback_aps[0] = feedback[n];
        ApplyChassisPowerGroup();
        ASSERT_TRUE(chassis_power->state.telemetry.target_speed_rpm[0] == 1000);
    }
    motor_output[0] = NAN;
    ApplyChassisPowerGroup();
    ASSERT_TRUE(chassis_power->state.numeric_error && chassis_power->state.transient.fault);
    for (unsigned w = 0; w < 4; ++w) ASSERT_TRUE(chassis_power->state.power.limited_current[w] == 0);
}

static void TestStopPublicationDoesNotMixCanAndDiagnostics(void)
{
    PreparePowerGroup(); idx = 4;
    DJIChassisPowerControlSnapshot_s control = chassis_power->control;
    control.version = 10;
    for (unsigned i = 0; i < 4; ++i) {
        control.target_rpm[i] = 1000;
        dji_motor_instance[i]->motor_settings.close_loop_type = SPEED_LOOP;
        dji_motor_instance[i]->motor_settings.outer_loop_type = SPEED_LOOP;
    }
    DJIChassisPowerPublishControl(&control);
    publish_stop_during_pid = true;
    DJIMotorControl();
    ASSERT_TRUE(chassis_power->state.control_version == 10);
    for (unsigned i = 0; i < 4; ++i) {
        int16_t sent = (int16_t)((uint16_t)sender_assignment[1].tx_buff[2*i] << 8 |
                                sender_assignment[1].tx_buff[2*i+1]);
        ASSERT_TRUE(sent == chassis_power->state.power.limited_current[i]);
    }
    DJIMotorControl();
    ASSERT_TRUE(chassis_power->state.control_version == 11);
    for (unsigned i = 0; i < 4; ++i) ASSERT_TRUE(chassis_power->state.power.limited_current[i] == 0);
    idx = 0;
}

static void TestFastProtectionObservesOrAppliesByStage(void)
{
    PreparePowerGroup();
    DJIChassisPowerControlSnapshot_s control = chassis_power->control;
    control.nominal_budget_w = control.legacy_budget_w = 20;
    control.target_power_w = 19; control.slow_correction_w = 3;
    DJIChassisPowerPublishControl(&control);
    for (unsigned i = 0; i < 4; ++i) {
        chassis_power->models[i].k4 = .01f;
        motor_output[i] = 40;
    }
    SetMeter(36.1f, true, 123);
    ApplyChassisPowerGroup();
#if POWER_METER_ENABLE
    ASSERT_TRUE(chassis_power->state.transient.fast_w < -8);
    ASSERT_TRUE(chassis_power->state.proposed_current[0] <= 16);
#endif
#if CHASSIS_POWER_CONTROL_STAGE == 0 || !POWER_METER_ENABLE
    ASSERT_TRUE(chassis_power->state.power.limited_current[0] == 22);
#else
    ASSERT_TRUE(chassis_power->state.power.limited_current[0] <= 16);
#endif
}

static void TestControlAndMeasurementSnapshotsReachOneRttRow(void)
{
    PreparePowerGroup();
    const ChassisPowerControlTelemetry_s control = {
        .nominal_budget_w = 30.0f,
        .target_power_w = 28.5f,
        .corrected_budget_w = 27.0f,
        .filtered_power_w = 29.0f,
        .error_w = -0.5f,
        .correction_w = -3.0f,
        .outer_mode = 2U,
    };
    PublishTelemetry(&control);
    SetMeter(12.34f, true, 77U);

    EmitOneRow();

    AssertControlColumnsMatch(
        "123,3000,2850,2700,10000,0,0,1234,2900,-50,-300,"
        "1000,0,2,1,46,15\r\n");
}

static void TestOfflineMeasurementKeepsValueAndTick(void)
{
    PreparePowerGroup();
    SetMeter(9.0f, false, 88U);

    EmitOneRow();

    AssertControlColumnsMatch(
        "123,0,0,0,10000,0,0,900,0,0,0,1000,0,0,0,35,15\r\n");
}

static void TestNonfiniteMeasurementBecomesZero(void)
{
    PreparePowerGroup();
    SetMeter(NAN, true, 99U);

    EmitOneRow();

    AssertControlColumnsMatch(
        "123,0,0,0,10000,0,0,0,0,0,0,1000,0,0,1,24,15\r\n");
}

static void TestNullControlSnapshotLeavesPreviousValue(void)
{
    PreparePowerGroup();
    const ChassisPowerControlTelemetry_s control = {
        .nominal_budget_w = 1.0f,
        .outer_mode = 1U,
    };
    PublishTelemetry(&control);
    PublishTelemetry(NULL);

    EmitOneRow();

    AssertControlColumnsMatch(
        "123,100,0,0,10000,0,0,0,0,0,0,1000,0,1,0,0,15\r\n");
}

static void TestRttDividerRemainsTenMotorCycles(void)
{
    PreparePowerGroup();

    for (size_t i = 1; i < CHASSIS_POWER_RTT_DIVIDER; ++i)
        ApplyChassisPowerGroup();
    ASSERT_STREQ("", captured_data);

    ApplyChassisPowerGroup();
    ASSERT_TRUE(captured_data[0] != '\0');
}

static void TestCalibrationUsesOutputCommandsAndActualRpm(void)
{
    PreparePowerGroup();
    SetMeter(18.0f, true, 117U);
    motor_output[0] = -1.0f;
    motor_output[2] = -3.0f;
    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
        speed_feedback_aps[i] = motor_output[i] * 600.0f;
    dji_motor_instance[2]->stop_flag = MOTOR_STOP;
    dji_motor_instance[3]->daemon->online = 0U;

    EmitOneRow();
    const char expected[] =
        "123,0,0,0,10000,0,0,1800,0,0,0,1000,0,0,1,6,3"
#if CHASSIS_POWER_CALIBRATION_ENABLE
        ",-1,2,0,0,-100,200,-300,400,117"
#endif
        "\r\n";
    ASSERT_STREQ(expected, captured_data);
}

static void TestCalibrationPrintsLimitedCurrentsInsteadOfPidDemand(void)
{
    PreparePowerGroup();
    chassis_power->input.referee_power_limit_w = 4.0f;
    const ChassisPowerControlTelemetry_s control = {
        .nominal_budget_w = 4.0f,
        .target_power_w = 4.0f,
        .corrected_budget_w = 4.0f,
        .filtered_power_w = 3.9f,
        .error_w = 0.1f,
        .outer_mode = 2U,
    };
    PublishTelemetry(&control);
    SetMeter(3.9f, true, 117U);
    const float demands[] = {2.0f, -3.0f, 4.0f, -5.0f};
    const float feedback_aps[] = {600.0f, -1200.0f, 1800.0f, -2400.0f};
    for (size_t i = 0; i < POWER_MODEL_MOTOR_COUNT; ++i)
    {
        chassis_power->models[i].k4 = 1.0f;
        motor_output[i] = demands[i];
        speed_feedback_aps[i] = feedback_aps[i];
    }

    chassis_power_rtt_count = CHASSIS_POWER_RTT_DIVIDER - 1U;
    ApplyChassisPowerGroup();
    const char expected[] =
        "123,400,400,400,400,5400,400,390,390,10,0,200,15,2,1,6,15"
#if CHASSIS_POWER_CALIBRATION_ENABLE
        ",1,-1,1,-1,100,-200,300,-400,117"
#endif
        "\r\n";
    ASSERT_STREQ(expected, captured_data);
}

static void TestAutotuneIsolationAndExplicitZeroForceRearm(void)
{
    PreparePowerGroup();
    AutotuneCancel();
    PIDAutotuneFeedback f={.now_ms=123,.permitted=true,.feedback_ok=true,.can_ok=true};
    AutotuneDrive unused;AutotuneMotorStep(&f,&unused);
    chassis_power->control.tick_ms=123;
    chassis_power->control.autotune_permitted=true;
    for(unsigned w=0;w<4;++w) {
        dji_motor_instance[w]->measure.rx_tick_ms=123;
        PIDInstance *pid=&dji_motor_instance[w]->motor_controller.speed_PID;
        pid->Kp=.75f;pid->MaxOut=2000;pid->IntegralLimit=1000;pid->DWT_CNT=7;
    }
    RuntimeTuningCancel();RegisterPIDTuning();
    AutotuneSubmit("@A1,1,BEGIN,1");ReadMotorCycle();ApplyAutotuneCycle();
    char reply[512];ASSERT_TRUE(AutotuneReply(reply,sizeof reply)>0);AutotuneReplySent();
    ASSERT_TRUE(autotune_owned);
    AutotuneSubmit("@A1,2,TRIAL,1,2,500,500,3000,3f800000,3f000000,00000000");
    ReadMotorCycle();ApplyAutotuneCycle();AutotuneReply(reply,sizeof reply);AutotuneReplySent();
    ASSERT_TRUE(dji_motor_instance[2]->motor_controller.speed_PID.Kp==1);
    ASSERT_TRUE(dji_motor_instance[2]->motor_controller.speed_PID.MaxOut==2000);
    ASSERT_TRUE(dji_motor_instance[2]->motor_controller.speed_PID.DWT_CNT==1234);
    for(unsigned w=0;w<4;++w) ASSERT_TRUE(cycle_stop[w]==(w==2?MOTOR_ENALBED:MOTOR_STOP));
    AutotuneCancel();ReadMotorCycle();ApplyAutotuneCycle();
    ASSERT_TRUE(!autotune_owned && autotune_inhibit);
    ASSERT_TRUE(dji_motor_instance[2]->motor_controller.speed_PID.Kp==.75f);
    chassis_power->control.control_enabled=false;
    ReadMotorCycle();ApplyAutotuneCycle();ASSERT_TRUE(autotune_inhibit);
    chassis_power->control.control_enabled=true;
    ReadMotorCycle();ApplyAutotuneCycle();
    for(unsigned w=0;w<4;++w) ASSERT_TRUE(cycle_stop[w]==MOTOR_STOP);
    chassis_power->control.zero_force=true;chassis_power->control.control_enabled=false;
    ReadMotorCycle();ApplyAutotuneCycle();ASSERT_TRUE(!autotune_inhibit);
    chassis_power->control.zero_force=false;chassis_power->control.control_enabled=true;
    AutotuneSubmit("@A1,3,BEGIN,1");DJIMotorControl();
    ASSERT_TRUE(AutotuneReply(reply,sizeof reply)>0);AutotuneReplySent();
    chassis_power->control.autotune_permitted=false;
    DJIMotorControl();DJIMotorControl();
    AutotuneSubmit("@A1,4,STATUS,2");DJIMotorControl();
    ASSERT_TRUE(AutotuneReply(reply,sizeof reply)>0);
    ASSERT_TRUE(strstr(reply,"OK,2,aborted,0,0,guard")!=NULL);
    AutotuneReplySent();
    chassis_power->control.zero_force=true;chassis_power->control.control_enabled=false;
    DJIMotorControl();
    chassis_power->control.zero_force=false;chassis_power->control.control_enabled=true;
    chassis_power->control.autotune_permitted=true;
    AutotuneSubmit("@A1,5,BEGIN,1");DJIMotorControl();AutotuneReply(reply,sizeof reply);AutotuneReplySent();
    AutotuneSubmit("@A1,6,TRIAL,3,0,500,500,3000,3f800000,00000000,00000000");
    DJIMotorControl();AutotuneReply(reply,sizeof reply);AutotuneReplySent();
    test_tick=1124;chassis_power->control.tick_ms=test_tick;
    for(unsigned w=0;w<4;++w)dji_motor_instance[w]->measure.rx_tick_ms=test_tick;
    AutotuneSubmit("@A1,7,BEGIN,1");DJIMotorControl();
    ASSERT_TRUE(!autotune_owned && autotune_inhibit);
    ASSERT_TRUE(dji_motor_instance[0]->motor_controller.speed_PID.MaxOut==2000);
    ASSERT_TRUE(AutotuneReply(reply,sizeof reply)>0 && strstr(reply,"ERR,guard"));AutotuneReplySent();
    test_tick=123;chassis_power->control.tick_ms=test_tick;
    chassis_power->control.zero_force=true;chassis_power->control.control_enabled=false;DJIMotorControl();
}

int main(void)
{
    PreparePowerGroup();
    for (unsigned tick=1000;tick<=1100;++tick) {
        for (unsigned i=0;i<idx;++i) {
            cycle_stop[i]=MOTOR_STOP; cycle_online[i]=true;
            cycle_measure[i].rx_tick_ms=tick; cycle_measure[i].speed_aps=0;
            dji_motor_instance[i]->stop_flag=MOTOR_STOP;
        }
        UpdateTuningStopState(tick,true);
        ASSERT_TRUE(DJIMotorCanSaveParameters(tick)==(tick>=1100));
    }
    ASSERT_TRUE(!DJIMotorCanSaveParameters(1106)); /* Stale publication. */
    dji_motor_instance[0]->stop_flag=MOTOR_ENALBED;
    ASSERT_TRUE(!DJIMotorCanSaveParameters(1101)); /* Enable raced with snapshot. */
    dji_motor_instance[0]->stop_flag=MOTOR_STOP;
    UpdateTuningStopState(1101,false);
    ASSERT_TRUE(!DJIMotorCanSaveParameters(1101)); /* Zero CAN frame failed. */
    cycle_measure[0].speed_aps=1000;
    UpdateTuningStopState(1102,true);
    ASSERT_TRUE(!DJIMotorCanSaveParameters(1102)); /* Coasting motor. */
    ASSERT_TRUE(!tuning_stop_seen);
    cycle_measure[0].speed_aps=0; cycle_online[0]=false;
    UpdateTuningStopState(1103,true);
    ASSERT_TRUE(!DJIMotorCanSaveParameters(1103));
    ASSERT_TRUE(!tuning_stop_seen);
    cycle_online[0]=true;cycle_measure[0].rx_tick_ms=1000;
    UpdateTuningStopState(1104,true);
    ASSERT_TRUE(!tuning_stop_seen); /* Per-motor feedback age is independent of task age. */
    /* Live PID writes apply in the motor cycle and reject a bad whole group. */
    PreparePowerGroup();
    chassis_power->algorithm.max_current_command = 15000;
    RegisterPIDTuning();
    char tuning_set[512] = "@T1,1,SET,pid,1";
    const float wheel[] = {1.25f, .1f, 0, 12000, 2000, 1};
    for (unsigned i = 0; i < 24; ++i) {
        uint32_t bits; memcpy(&bits, &wheel[i % 6], sizeof bits);
        char cell[16]; snprintf(cell, sizeof cell, ",%08lx", (unsigned long)bits);
        strcat(tuning_set, cell);
    }
    RuntimeTuningSubmit(tuning_set);
    DJIMotorControl();
    for (unsigned i = 0; i < 4; ++i) {
        ASSERT_TRUE(dji_motor_instance[i]->motor_controller.speed_PID.Kp == 1.25f);
        ASSERT_TRUE(dji_motor_instance[i]->motor_controller.speed_PID.MaxOut == 12000);
    }
    char tuning_reply[512];
    ASSERT_TRUE(RuntimeTuningReply(tuning_reply, sizeof tuning_reply) > 0);
    ASSERT_TRUE(strstr(tuning_reply, "@T1,1,OK,pid,2") != NULL);
    RuntimeTuningReplySent();
    /* Direct consumer validation also guarantees all-or-nothing updates. */
    float invalid[24];
    for (unsigned i = 0; i < 24; ++i) invalid[i] = wheel[i % 6];
    invalid[21] = 16000;
    ASSERT_TRUE(!ApplyPIDTuning(invalid));
    ASSERT_TRUE(dji_motor_instance[0]->motor_controller.speed_PID.MaxOut == 12000);
    RuntimeTuningCancel();
    PreparePowerGroup();
    chassis_power->algorithm.max_current_command = 15000;
    chassis_power->control.power_ceiling_w = 4;
    SetMeter(0, true, 123);
    for (unsigned w = 0; w < 4; ++w) {
        chassis_power->models[w].k4 = 0.0001f;
        motor_output[w] = 1000;
        speed_reference_aps[w] = speed_feedback_aps[w] = 6000;
    }
    ApplyChassisPowerGroup();
    float ceiling_power = 0;
    for (unsigned w = 0; w < 4; ++w)
        ceiling_power += PowerModelPredict(&chassis_power->models[w],
            chassis_power->state.power.limited_current[w], 1000);
    ASSERT_TRUE(ceiling_power <= 4.0001f);
    TestZeroDemandIsNotActualBaseLimiting();
    TestTargetSnapshotDoesNotDependOnFeedbackRounding();
    TestFastProtectionObservesOrAppliesByStage();
    TestStopPublicationDoesNotMixCanAndDiagnostics();
    TestAtomicPublicationAndFinalIntegerCommands();
    TestAutotuneIsolationAndExplicitZeroForceRearm();
    if (CHASSIS_POWER_CONTROL_STAGE == 0 && POWER_METER_ENABLE) {
        TestControlAndMeasurementSnapshotsReachOneRttRow();
        TestOfflineMeasurementKeepsValueAndTick();
        TestNonfiniteMeasurementBecomesZero();
        TestNullControlSnapshotLeavesPreviousValue();
        TestRttDividerRemainsTenMotorCycles();
        TestCalibrationUsesOutputCommandsAndActualRpm();
        TestCalibrationPrintsLimitedCurrentsInsteadOfPidDemand();
    }

    if (failures != 0)
        return 1;

    puts("PASS: DJI chassis-power RTT integration tests");
    return 0;
}
