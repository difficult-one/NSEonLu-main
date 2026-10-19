# 底盘功率控制反馈数据设计

## 目标

在框架公共数据定义中加入底盘功率控制反馈结构体，使底盘应用能够向其他应用或双板通信对端提供三个以 W 为单位的总量：

- 限流前四个底盘电机的预测功率之和；
- 限流后四个底盘电机的预测功率之和；
- 功率模型当前实际采用的有效总功率上限。

## 数据结构

保留裁判 UI 正在使用的 `Chassis_Power_Data_s.chassis_power_mx`，避免破坏现有界面逻辑。另行在 `application/robot_def.h` 中定义功率控制反馈类型：

```c
typedef struct
{
    float predicted_unlimited_power_w;
    float predicted_limited_power_w;
    float effective_limit_w;
} Chassis_Power_Control_Data_s;
```

在 `Chassis_Upload_Data_s` 中增加 `Chassis_Power_Control_Data_s power_control` 成员。该上传结构体已经同时用于单板消息中心和双板 CAN 通信，因此不建立新的通信主题或协议层。

## 数据流

功率模型继续在 DJI 电机模块中计算每轮的限流前预测功率、限流后预测功率及有效功率上限。底盘任务读取 `DJIChassisPowerGetState()` 返回的只读状态，将四轮预测功率分别求和，并把三项总量写入 `chassis_feedback_data.power_control`，随后沿现有 `chassis_feed` 数据流发布。

不直接把算法层的 `ChassisPowerOutput_s` 暴露给应用层，以免公共通信结构依赖算法内部的逐电机诊断布局。

## 异常处理

只有功率控制组已注册且功率模型状态为 `POWER_MODEL_STATUS_OK` 时才发布计算结果。未注册、输入无效或模型计算失败时，三个字段均写为 `0.0f`，避免发布上一周期的陈旧数据或非有限值。

## 验证

新增主机侧测试，构造已知的四轮功率状态，验证：

- 限流前预测总功率等于四轮限流前预测功率之和；
- 限流后预测总功率等于四轮限流后预测功率之和；
- 有效功率上限原样写入反馈结构体；
- 无效状态输出三个零值。

最后运行相关主机测试和固件编译，确认扩展后的上传结构体在单板与双板构建路径中均可编译。
