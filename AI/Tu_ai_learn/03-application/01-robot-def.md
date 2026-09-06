# robot_def.h — 机器人配置文件

## 模块职责

robot_def.h 是全项目的"配置中心"，定义板型选择、物理参数、控制模式枚举和跨应用共享的通信数据结构。

---

## 设计思路

### 为什么需要集中配置？

机器人开发中存在大量"改一处影响多处"的参数：
- 轮距和轴距变了，底盘运动学解算就要改
- 云台编码器零位变了，cmd 和 gimbal 都要改
- 切换单板/双板模式，所有应用的通信方式都要变

把这些参数和类型集中到一个头文件中，**修改时只需改一个地方**，且通过条件编译实现不同配置的自动切换。

### 文件的三个职责

1. **硬件配置** — 板型宏定义、物理参数（轮距、减速比等）
2. **状态枚举** — 机器人状态、各应用工作模式
3. **通信数据结构** — 应用间传递的控制命令和反馈数据

---

## 核心数据结构

### 1. 板型定义

```c
// robot_def.h:20-22
#define ONE_BOARD       // 单板控制整车（默认）
// #define CHASSIS_BOARD // 底盘板（双板模式）
// #define GIMBAL_BOARD  // 云台板（双板模式）
```

编译时只允许存在一个定义，冲突检测：

```c
// robot_def.h:51-55
#if (defined(ONE_BOARD) && defined(CHASSIS_BOARD)) || \
    (defined(ONE_BOARD) && defined(GIMBAL_BOARD)) ||  \
    (defined(CHASSIS_BOARD) && defined(GIMBAL_BOARD))
#error Conflict board definition! You can only define one board type.
#endif
```

### 2. 物理参数

```c
// 云台参数
#define YAW_CHASSIS_ALIGN_ECD 2711   // yaw 对齐底盘时的编码器值
#define YAW_ECD_GREATER_THAN_4096 0  // 对齐值是否 > 4096
#define PITCH_HORIZON_ECD 3412       // pitch 水平时的编码器值
#define PITCH_MAX_ANGLE 0            // pitch 最大角度限制
#define PITCH_MIN_ANGLE 0            // pitch 最小角度限制

// 发射参数
#define ONE_BULLET_DELTA_ANGLE 36     // 拨盘发一发弹丸转过的角度
#define REDUCTION_RATIO_LOADER 36.0f  // 拨盘电机减速比（2006=36, 3508=19）
#define NUM_PER_CIRCLE 10             // 拨盘一圈装弹量

// 底盘参数（单位: mm）
#define WHEEL_BASE 350               // 纵向轴距
#define TRACK_WIDTH 300              // 横向轮距
#define CENTER_GIMBAL_OFFSET_X 0     // 云台相对底盘中心的 x 偏移
#define CENTER_GIMBAL_OFFSET_Y 0     // 云台相对底盘中心的 y 偏移
#define RADIUS_WHEEL 60              // 轮子半径
#define REDUCTION_RATIO_WHEEL 19.0f  // 轮毂电机减速比

// 陀螺仪方向映射
#define GYRO2GIMBAL_DIR_YAW 1        // 1=同向, -1=反向
#define GYRO2GIMBAL_DIR_PITCH 1
#define GYRO2GIMBAL_DIR_ROLL 1
```

**为什么这些参数放在 robot_def.h 而不是各应用中？** 因为多个应用可能使用同一个参数。例如 `YAW_CHASSIS_ALIGN_ECD` 在 robot_cmd 中计算偏转角，在 gimbal 中也可能用到。

### 3. 状态枚举

```c
// 机器人整体状态
typedef enum {
    ROBOT_STOP = 0,   // 急停
    ROBOT_READY,      // 正常运行
} Robot_Status_e;

// 应用状态
typedef enum {
    APP_OFFLINE = 0,  // 离线
    APP_ONLINE,       // 在线
    APP_ERROR,        // 异常
} App_Status_e;

// 底盘模式
typedef enum {
    CHASSIS_ZERO_FORCE = 0,    // 零力（断电）
    CHASSIS_ROTATE,            // 小陀螺自旋
    CHASSIS_NO_FOLLOW,         // 不跟随云台
    CHASSIS_FOLLOW_GIMBAL_YAW, // 跟随云台偏航角
} chassis_mode_e;

// 云台模式
typedef enum {
    GIMBAL_ZERO_FORCE = 0, // 零力
    GIMBAL_FREE_MODE,      // 自由模式（编码器反馈）
    GIMBAL_GYRO_MODE,      // 陀螺仪模式（IMU 反馈）
} gimbal_mode_e;

// 发射相关模式
typedef enum {
    SHOOT_OFF = 0, SHOOT_ON,
} shoot_mode_e;

typedef enum {
    FRICTION_OFF = 0, FRICTION_ON,
} friction_mode_e;

typedef enum {
    LID_OPEN = 0, LID_CLOSE,
} lid_mode_e;

typedef enum {
    LOAD_STOP = 0,    // 停止
    LOAD_REVERSE,     // 反转
    LOAD_1_BULLET,    // 单发
    LOAD_3_BULLET,    // 三连发
    LOAD_BURSTFIRE,   // 连发
} loader_mode_e;
```

### 4. 通信数据结构（#pragma pack(1)）

所有用于跨应用/跨板通信的结构体必须取消字节对齐：

```c
#pragma pack(1)

// cmd -> chassis 的控制命令
typedef struct {
    float vx;                    // 前进速度
    float vy;                    // 横移速度
    float wz;                    // 旋转速度
    float offset_angle;          // 底盘与归中位置的夹角
    chassis_mode_e chassis_mode; // 底盘模式
    int chassis_speed_buff;      // 速度增益
} Chassis_Ctrl_Cmd_s;

// cmd -> gimbal 的控制命令
typedef struct {
    float yaw;                       // yaw 角度参考
    float pitch;                     // pitch 角度参考
    float chassis_rotate_wz;        // 底盘旋转角速度
    gimbal_mode_e gimbal_mode;      // 云台模式
} Gimbal_Ctrl_Cmd_s;

// cmd -> shoot 的控制命令
typedef struct {
    shoot_mode_e shoot_mode;
    loader_mode_e load_mode;
    lid_mode_e lid_mode;
    friction_mode_e friction_mode;
    Bullet_Speed_e bullet_speed;
    uint8_t rest_heat;               // 剩余热量
    float shoot_rate;                // 射频（发/秒）
} Shoot_Ctrl_Cmd_s;

// chassis -> cmd 的反馈数据
typedef struct {
    uint8_t rest_heat;
    Bullet_Speed_e bullet_speed;
    Enemy_Color_e enemy_color;
} Chassis_Upload_Data_s;

// gimbal -> cmd 的反馈数据
typedef struct {
    attitude_t gimbal_imu_data;                  // IMU 姿态
    uint16_t yaw_motor_single_round_angle;       // yaw 单圈角度
} Gimbal_Upload_Data_s;

#pragma pack()
```

**为什么必须 `#pragma pack(1)`？** 这些结构体可能通过 CAN 总线传输。如果编译器做了默认的字节对齐（如 4 字节对齐），结构体中会出现空字节，导致发送/接收双方数据布局不一致，解包出错。

---

## 函数详解

robot_def.h 是纯配置文件，不包含函数。它的"逻辑"体现在宏定义和条件编译上。

### 条件编译 #ifdef 的三种场景

**场景一：robot.c 中控制应用初始化**

```c
// robot.c:12-19
#if defined(ONE_BOARD) || defined(GIMBAL_BOARD)
#include "gimbal.h"
#include "shoot.h"
#include "robot_cmd.h"
#endif

#if defined(ONE_BOARD) || defined(CHASSIS_BOARD)
#include "chassis.h"
#endif
```

- 单板模式：初始化所有应用
- 云台板：初始化 gimbal + shoot + robot_cmd
- 底盘板：只初始化 chassis

**场景二：通信数据结构中切换字段**

```c
// robot_def.h:182-184
typedef struct {
#if defined(CHASSIS_BOARD) || defined(GIMBAL_BOARD)
    // attitude_t chassis_imu_data;  // 双板时回传 IMU
#endif
    uint8_t rest_heat;
    // ...
} Chassis_Upload_Data_s;
```

**场景三：应用内切换通信方式**

```c
// robot_cmd.c:102-117
#ifdef ONE_BOARD
    chassis_cmd_pub = PubRegister("chassis_cmd", ...);   // 消息中心
#endif
#ifdef GIMBAL_BOARD
    cmd_can_comm = CANCommInit(&comm_conf);              // CAN 通信
#endif
```

---

## 调用链

robot_def.h 被几乎所有应用和模块包含，影响范围：

```
robot_def.h
  |
  +-- robot.c          // 条件编译选择初始化哪些应用
  +-- robot_cmd.c      // 使用控制命令结构和模式枚举
  +-- gimbal.c         // 使用云台参数和模式枚举
  +-- chassis.c        // 使用底盘参数和模式枚举
  +-- shoot.c          // 使用发射参数和模式枚举
  +-- can_comm.c       // 使用通信数据结构的大小
  |
  +-- main.c           // 不直接包含，但通过 robot.h 间接依赖
```

参数的使用示例（底盘运动学解算中的宏计算）：

```
robot_def.h 定义:
  WHEEL_BASE = 350, TRACK_WIDTH = 300

chassis.c 使用:
  HALF_WHEEL_BASE = WHEEL_BASE / 2.0f    = 175.0
  HALF_TRACK_WIDTH = TRACK_WIDTH / 2.0f   = 150.0
  LF_CENTER = (HALF_TRACK_WIDTH + ... + HALF_WHEEL_BASE - ...) * DEGREE_2_RAD
```

---

## 注意事项

1. **修改板型定义后必须全部重新编译** — `#define` 的改变影响条件编译，增量编译可能导致不一致。

2. **板型定义冲突会导致编译错误** — 代码中有 `#error` 检查，同时定义两个板型会直接报错。

3. **物理参数的单位约定** — 距离用 mm，角度用度，速度用 mm/s 或 deg/s。浮点数必须以 `.0` 或 `f` 结尾，否则默认为 double 可能导致隐式转换。

4. **`#pragma pack(1)` 的作用域** — 从 `#pragma pack(1)` 到 `#pragma pack()` 之间的所有结构体都会被压缩。不要在通信结构体之外的区域使用 pack(1)，否则会增加非通信代码的内存访问开销。

5. **编码器零位参数因机器人而异** — `YAW_CHASSIS_ALIGN_ECD` 和 `PITCH_HORIZON_ECD` 是每台机器人机械组装后实测的值，换机器人必须重新标定。

6. **编译时 pragma message 提醒** — robot.c 开头有 `#pragma message "check if you have configured the parameters in robot_def.h..."` 提醒开发者检查配置。

7. **`_s` vs `_t` 后缀区分** — 复杂结构体用 `_s`（如 `Chassis_Ctrl_Cmd_s`），简单数据类型用 `_t`（如 `IMU_Data_t`）。这在 Module 层规范中有详细说明。
