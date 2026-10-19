#ifndef CHASSIS_ENERGY_CONFIG_H
#define CHASSIS_ENERGY_CONFIG_H

/* 0: 沿用原始功率预算; 1: 动态消耗裁判缓冲能量。 */
#ifndef CHASSIS_BUFFER_POWER_ENABLE
#define CHASSIS_BUFFER_POWER_ENABLE 1
#endif

#define CHASSIS_POWER_SOURCE_DIRECT 0
#define CHASSIS_POWER_SOURCE_SUPERCAP 1
/* 当前未接超级电容。烧录带输入功率反馈(CAN 0x052)的匹配 F334 固件后,
 * 再改为 CHASSIS_POWER_SOURCE_SUPERCAP。 */
#ifndef CHASSIS_POWER_SOURCE_MODE
#define CHASSIS_POWER_SOURCE_MODE CHASSIS_POWER_SOURCE_DIRECT
#endif

#if CHASSIS_BUFFER_POWER_ENABLE != 0 && CHASSIS_BUFFER_POWER_ENABLE != 1
#error CHASSIS_BUFFER_POWER_ENABLE must be 0 or 1
#endif
#if CHASSIS_POWER_SOURCE_MODE != CHASSIS_POWER_SOURCE_DIRECT && \
    CHASSIS_POWER_SOURCE_MODE != CHASSIS_POWER_SOURCE_SUPERCAP
#error Invalid CHASSIS_POWER_SOURCE_MODE
#endif

/* 以下阈值都针对缓冲能量的保守估计值,不等同于裁判原始焦耳读数。
 * 除 MAX_ENERGY_J 是原始读数上限外,其余都作用在估算下限 / 预测值上。 */
#define CHASSIS_BUFFER_MAX_ENERGY_J 60.0f       // 缓冲能量满值(J):裁判原始读数超过它即判缓冲数据无效,不再参与增补
#define CHASSIS_BUFFER_RECOVERY_ENTER_J 15.0f   // 进入回充(J):估算下限降到它或以下就转入恢复
#define CHASSIS_BUFFER_RECOVERY_EXIT_J 30.0f    // 退出回充(J):估算下限回到它或以上并保持 RECOVERY_HOLD_S 才解除
#define CHASSIS_BUFFER_HARD_FLOOR_J 5.0f        // 能量硬底座(J):预测值跌破它强制转入恢复,也是增补功率的保底基准
#define CHASSIS_BUFFER_PREDICTION_WINDOW_S 0.5f // 预测窗口(s):把高于进入门槛的富余能量按该时长摊成允许的额外输入功率
#define CHASSIS_BUFFER_GUARD_WINDOW_S 0.3f      // 保护窗口(s):把超限输入功率和硬底座余量换算成该时长内的能量预算
#define CHASSIS_BUFFER_SETTLEMENT_DELAY_S 0.1f  // 结算延迟(s):裁判帧接收时刻不等于结算时刻,额外预留这段时间的消耗
#define CHASSIS_BUFFER_ENERGY_MARGIN_J 3.0f     // 能量安全余量(J):计算估算下限时直接扣除
#define CHASSIS_BUFFER_POWER_MARGIN_W 5.0f      // 功率安全余量(W):估算下限按它预留消耗,判输入超限和增补时也扣掉
#define CHASSIS_BUFFER_BOOST_MAX_W 60.0f        // 输入预算增补上限(W):注意与外层 max_boost_w(电容负载增补上限)不是同一个量
#define CHASSIS_BUFFER_RECOVERY_POWER_W 20.0f   // 最大回充减让(W):按能量缺口调节,逼近硬底座时使用全部减让
#define CHASSIS_BUFFER_RECOVERY_TARGET_TIME_S 1.0f // 目标恢复时间(s):能量缺口除以该时间得到回充减让功率
#define CHASSIS_BUFFER_OBSERVER_MAX_STEP_S 0.020f // 最大连续积分间隔(s):超出时按最坏耗能扣减并撤销增补
#define CHASSIS_BUFFER_RECOVERY_HOLD_S 0.1f     // 退出回充的确认时长(s):在该时长内持续高于退出门槛才解除
#define CHASSIS_BUFFER_RISE_RATE_W_PER_S 600.0f // 输入预算上升斜率(W/s);电容负载侧使用独立斜率,下降立即生效
#define CHASSIS_BUFFER_CORRECTION_RISE_J_PER_S 20.0f // 新裁判帧向上校正估算的上限(J/s);向下立即校正
#define CHASSIS_POWER_DEMAND_MARGIN_W 5.0f     // 高于基础功率的真实驱动需求所允许的额外余量(W)

#endif
