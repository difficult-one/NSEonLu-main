#ifndef CHASSIS_POWER_CONFIG_H
#define CHASSIS_POWER_CONFIG_H
/*CHASSIS_POWER_CONTROL_STAGE
0    维持现有控制，新增算法只计算诊断结果 
1    启用独立快保护、恢复及慢积分隔离 
2    增加启动额度、预测约束和反馈不足时的增长兜底 
3    增加完整慢环学习门槛、连续死区和积分回算*/
/* 0 observe, 1 fast recovery, 2 startup guard, 3 complete slow loop. */
#ifndef CHASSIS_POWER_CONTROL_STAGE
#define CHASSIS_POWER_CONTROL_STAGE 3
#endif
#ifndef CHASSIS_POWER_CAPTURE_ENABLE
#define CHASSIS_POWER_CAPTURE_ENABLE 0
#endif
#ifndef POWER_METER_ENABLE
#define POWER_METER_ENABLE 1
#endif
#if CHASSIS_POWER_CONTROL_STAGE < 0 || CHASSIS_POWER_CONTROL_STAGE > 3
#error CHASSIS_POWER_CONTROL_STAGE must be 0, 1, 2 or 3
#endif
#if CHASSIS_POWER_CAPTURE_ENABLE != 0 && CHASSIS_POWER_CAPTURE_ENABLE != 1
#error CHASSIS_POWER_CAPTURE_ENABLE must be 0 or 1
#endif
#endif
