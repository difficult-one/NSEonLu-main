# Shoot — 发射控制

## 模块职责

Shoot 应用控制发射机构的三个电机（左摩擦轮、右摩擦轮、拨盘电机），根据 robot_cmd 发布的模式和参数，实现摩擦轮转速控制、拨弹（单发/三连发/连发）控制，以及弹舱盖开关。

---

## 设计思路

### 为什么发射需要独立应用？

发射机构涉及三种完全不同的控制需求：

1. **摩擦轮** — 恒定高速旋转，对速度闭环，转速决定弹丸初速
2. **拨盘** — 精确的角度控制（单发）或速度控制（连发），需要在不同闭环模式间切换
3. **弹舱盖** — 舵机控制，开/关两位状态（待实现）

三者协调工作才能完成"摩擦轮加速到目标转速 -> 拨弹送弹 -> 弹丸飞出"的完整发射流程。

### 摩擦轮与拨盘的协同

```
发射流程：
  1. 摩擦轮启动 -> 速度环控制到目标转速（需要约1-2秒加速）
  2. 拨盘动作   -> 根据模式选择单发/三连发/连发
  3. 摩擦轮维持 -> 持续旋转保持弹速

停止流程：
  1. 拨盘停止   -> LOAD_STOP 模式
  2. 摩擦轮减速 -> 参考值设为 0，速度环自然减速
```

### 拨弹模式设计

拨盘电机 (M2006) 通过减速比为 36 的减速器连接拨弹机构，一圈可装载 10 发弹丸。

- **单发 LOAD_1_BULLET** — 切换到角度环，total_angle 增加一发弹丸对应的角度（36度），然后进入休眠
- **三连发 LOAD_3_BULLET** — 类似单发，但增加 3 倍角度，休眠时间更长
- **连发 LOAD_BURSTFIRE** — 切换到速度环，以固定射频持续旋转
- **反转 LOAD_REVERSE** — 卡弹时反转拨盘（待实现）

---

## 核心数据结构

### 私有变量（shoot.c）

```c
// ---- 电机实例 ----
static DJIMotorInstance *friction_l, *friction_r;   // 左右摩擦轮
static DJIMotorInstance *loader;                     // 拨盘电机

// ---- 消息中心 ----
static Publisher_t *shoot_pub;                       // 发布 "shoot_feed"
static Subscriber_t *shoot_sub;                      // 订阅 "shoot_cmd"
static Shoot_Ctrl_Cmd_s shoot_cmd_recv;              // 接收的控制命令
static Shoot_Upload_Data_s shoot_feedback_data;      // 发送的反馈数据

// ---- 冷却计时 ----
static float hibernate_time = 0;   // 上次触发拨弹的时间
static float dead_time = 0;        // 不应期（防止重复触发）
```

### 发射参数（robot_def.h）

```c
#define ONE_BULLET_DELTA_ANGLE 36       // 一发弹丸拨盘转动的角度
#define REDUCTION_RATIO_LOADER 36.0f    // 2006 拨盘减速比
#define NUM_PER_CIRCLE 10               // 一圈装弹量
```

---

## 函数详解

### ShootInit() — 初始化

位于 `shoot.c:21`。

```c
void ShootInit()
{
    // 1. 摩擦轮配置模板
    Motor_Init_Config_s friction_config = {
        .can_init_config = {
            .can_handle = &hcan2,     // CAN2
        },
        .controller_param_init_config = {
            .speed_PID = {
                .Kp = 0, .Ki = 0, .Kd = 0,       // 当前 PID 参数为 0，待调参
                .MaxOut = 15000,
            },
            .current_PID = {
                .Kp = 0, .Ki = 0, .Kd = 0,       // 当前 PID 参数为 0，待调参
                .MaxOut = 15000,
            },
        },
        .controller_setting_init_config = {
            .outer_loop_type = SPEED_LOOP,         // 外环速度环
            .close_loop_type = SPEED_LOOP | CURRENT_LOOP,  // 速度+电流串级
        },
        .motor_type = M3508,
    };

    // 2. 左摩擦轮（tx_id=1，正常方向）
    friction_config.can_init_config.tx_id = 1;
    friction_l = DJIMotorInit(&friction_config);

    // 3. 右摩擦轮（tx_id=2，反转方向）
    friction_config.can_init_config.tx_id = 2;
    friction_config.controller_setting_init_config.motor_reverse_flag = MOTOR_DIRECTION_REVERSE;
    friction_r = DJIMotorInit(&friction_config);

    // 4. 拨盘电机配置
    Motor_Init_Config_s loader_config = {
        .can_init_config = {
            .can_handle = &hcan2, .tx_id = 3,
        },
        .controller_param_init_config = {
            .angle_PID = {
                .Kp = 0, .Ki = 0, .Kd = 0,
                .MaxOut = 200,      // 角度环输出较小，作为速度环输入
            },
            .speed_PID = {
                .Kp = 0, .Ki = 0, .Kd = 0,       // 待调参
                .MaxOut = 5000,
            },
            .current_PID = {
                .Kp = 0, .Ki = 0, .Kd = 0,       // 待调参
                .MaxOut = 5000,
            },
        },
        .controller_setting_init_config = {
            .outer_loop_type = SPEED_LOOP,         // 初始化为速度环（停住拨盘）
            .close_loop_type = CURRENT_LOOP | SPEED_LOOP,
        },
        .motor_type = M2006,                       // 2006 电机
    };
    loader = DJIMotorInit(&loader_config);

    // 5. 消息中心
    shoot_pub = PubRegister("shoot_feed", sizeof(Shoot_Upload_Data_s));
    shoot_sub = SubRegister("shoot_cmd", sizeof(Shoot_Ctrl_Cmd_s));
}
```

**要点**：
- 摩擦轮和拨盘的 PID 参数全部为 0 — 这是待调参状态，实际使用前必须填入合适的值
- 拨盘初始化为速度环模式，参考值默认为 0，防止上电时拨盘乱转
- 右摩擦轮设置 `MOTOR_DIRECTION_REVERSE`，因为两个摩擦轮需要反向旋转才能将弹丸加速射出

### ShootTask() — 核心任务

位于 `shoot.c:108`。

```c
void ShootTask()
{
    // 1. 获取控制命令
    SubGetMessage(shoot_sub, &shoot_cmd_recv);

    // 2. 急停处理：SHOOT_OFF 直接停止所有电机
    if (shoot_cmd_recv.shoot_mode == SHOOT_OFF) {
        DJIMotorStop(friction_l);
        DJIMotorStop(friction_r);
        DJIMotorStop(loader);
    } else {
        DJIMotorEnable(friction_l);
        DJIMotorEnable(friction_r);
        DJIMotorEnable(loader);
    }

    // 3. 拨盘模式处理
    switch (shoot_cmd_recv.load_mode) {
    case LOAD_STOP:
        DJIMotorOuterLoop(loader, SPEED_LOOP);    // 切换到速度环
        DJIMotorSetRef(loader, 0);                 // 参考值=0，最快停止
        break;

    case LOAD_1_BULLET:
        DJIMotorOuterLoop(loader, ANGLE_LOOP);    // 切换到角度环
        DJIMotorSetRef(loader,
            loader->measure.total_angle + ONE_BULLET_DELTA_ANGLE);  // 增加36度
        hibernate_time = DWT_GetTimeline_ms();     // 记录触发时间
        dead_time = 150;                           // 150ms 不应期
        break;

    case LOAD_3_BULLET:
        DJIMotorOuterLoop(loader, ANGLE_LOOP);
        DJIMotorSetRef(loader,
            loader->measure.total_angle + 3 * ONE_BULLET_DELTA_ANGLE);  // 增加108度
        hibernate_time = DWT_GetTimeline_ms();
        dead_time = 300;                           // 300ms 不应期
        break;

    case LOAD_BURSTFIRE:
        DJIMotorOuterLoop(loader, SPEED_LOOP);    // 切换到速度环
        // 射频换算：发/秒 -> 角速度 (度/秒)
        // shoot_rate * 360(一圈360度) * REDUCTION_RATIO_LOADER(减速比) / NUM_PER_CIRCLE(一圈发数)
        DJIMotorSetRef(loader,
            shoot_cmd_recv.shoot_rate * 360 * REDUCTION_RATIO_LOADER / NUM_PER_CIRCLE);
        break;

    case LOAD_REVERSE:
        DJIMotorOuterLoop(loader, SPEED_LOOP);
        // 反转逻辑待实现
        break;

    default:
        while (1);  // 未知模式，死循环报警（检查内存越界）
    }

    // 4. 摩擦轮控制
    if (shoot_cmd_recv.friction_mode == FRICTION_ON) {
        switch (shoot_cmd_recv.bullet_speed) {
        case SMALL_AMU_15:
            DJIMotorSetRef(friction_l, 0);  // 待实测后填入
            DJIMotorSetRef(friction_r, 0);
            break;
        case SMALL_AMU_18:
            DJIMotorSetRef(friction_l, 0);
            DJIMotorSetRef(friction_r, 0);
            break;
        case SMALL_AMU_30:
            DJIMotorSetRef(friction_l, 0);
            DJIMotorSetRef(friction_r, 0);
            break;
        default:
            // 调试默认值
            DJIMotorSetRef(friction_l, 30000);
            DJIMotorSetRef(friction_r, 30000);
            break;
        }
    } else {
        DJIMotorSetRef(friction_l, 0);   // 关闭摩擦轮
        DJIMotorSetRef(friction_r, 0);
    }

    // 5. 弹舱盖控制（待实现）
    // ...

    // 6. 发布反馈
    PubPushMessage(shoot_pub, (void *)&shoot_feedback_data);
}
```

### 拨弹射频换算详解

连发模式下，将"每秒发射 N 发"换算为电机角速度：

```
射频换算公式：
  motor_speed = shoot_rate * 360 * REDUCTION_RATIO_LOADER / NUM_PER_CIRCLE

代入实际值（shoot_rate=8, REDUCTION_RATIO_LOADER=36, NUM_PER_CIRCLE=10）：
  motor_speed = 8 * 360 * 36 / 10 = 10368 度/秒

换算为 RPM：
  10368 / 360 * 60 = 1728 RPM

这表示拨盘需要以 1728 RPM 的速度旋转才能达到 8发/秒 的射频。
```

---

## 调用链

### 初始化调用链

```
RobotInit()                                     // robot.c:23
  |
  +-- ShootInit()                               // shoot.c:21
        |
        +-- DJIMotorInit(friction_l)            // 左摩擦轮 M3508
        |     +-- CANRegister() + DaemonRegister()
        +-- DJIMotorInit(friction_r)            // 右摩擦轮 M3508（反转）
        +-- DJIMotorInit(loader)                // 拨盘 M2006
        +-- PubRegister("shoot_feed")
        +-- SubRegister("shoot_cmd")
```

### 运行时控制链

```
RobotCMDTask() 发布 "shoot_cmd"
    |
    v
ShootTask()                                     // shoot.c:108
    |
    +-- SubGetMessage("shoot_cmd")               // 获取命令
    |
    +-- 急停判断 (SHOOT_OFF -> DJIMotorStop)
    |
    +-- 拨盘模式处理
    |     +-- LOAD_STOP:    速度环，ref=0
    |     +-- LOAD_1_BULLET: 角度环，ref+=36度
    |     +-- LOAD_3_BULLET: 角度环，ref+=108度
    |     +-- LOAD_BURSTFIRE: 速度环，ref=射频换算值
    |
    +-- 摩擦轮控制
    |     +-- FRICTION_ON:  根据弹速设ref
    |     +-- FRICTION_OFF: ref=0
    |
    +-- PubPushMessage("shoot_feed")

--- 电机 PID 计算路径 ---

MotorControlTask() (1kHz)                        // motor_task.c
  |
  +-- DJIMotorControl()                          // 速度环/角度环 PID + CAN 发送
        |
        +-- 对拨盘：角度环 PID -> 速度环 PID -> 电流环 PID（若启用）
        +-- 对摩擦轮：速度环 PID -> 电流环 PID
```

---

## 注意事项

1. **PID 参数全部为 0，需调参后才能使用** — 当前代码中摩擦轮和拨盘的 Kp/Ki/Kd 均为 0，意味着 PID 输出恒为 0，电机不会转动。实际部署前必须根据电机型号和负载进行调参。

2. **单发/三连发的休眠机制被注释掉** — `hibernate_time + dead_time > DWT_GetTimeline_ms()` 的判断被注释，当前单发模式下如果持续按住触发键，会不断累加角度导致连续发射。取消注释后可实现"按一次发一发"的效果。

3. **摩擦轮参考值 30000 超出 M3508 电流限制** — 默认分支中设定 `DJIMotorSetRef(friction_l, 30000)`，但 M3508 的最大电流约为 16384。由于 PID 有 MaxOut 限制，实际不会超限，但这个参考值本身不合理，应替换为实测的转速值。

4. **弹速对应转速待实测** — `SMALL_AMU_15/18/30` 三个分支的摩擦轮参考值都是 0，需要实测后填入。弹速与摩擦轮转速的关系取决于摩擦轮直径、间距、弹丸材质等因素。

5. **弹舱盖控制未实现** — `lid_mode` 的处理分支为空，需要舵机模块支持。

6. **卡弹检测未实现** — `LOAD_REVERSE` 分支为空。卡弹检测可通过比较裁判系统剩余热量变化和拨盘电机电流来判断：如果拨盘消耗了电流但热量没有增加，说明卡弹。

7. **摩擦轮需要预热时间** — 从启动到达到目标转速需要 1-2 秒。当前代码中没有"摩擦轮未就绪时禁止拨弹"的保护逻辑，可能导致弹速不足。建议增加摩擦轮速度反馈检查。

8. **`default: while(1);` 是硬保护** — 如果出现未知的 `loader_mode_e` 值（通常由内存越界或指针错误导致），程序会死循环。这是嵌入式开发中常见的保护策略，比继续运行更安全。
