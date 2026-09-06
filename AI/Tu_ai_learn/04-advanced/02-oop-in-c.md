# C 语言面向对象设计模式深入

## 模块职责

本篇从框架的实际代码出发，详解如何用纯 C 语言实现面向对象的核心特性——封装、多态、组合，以及框架中广泛使用的 parent pointer 模式和实例注册机制。

---

## 设计思路

### 为什么不用 C++？

RoboMaster 嵌入式开发普遍使用 C 语言，原因包括：
1. STM32 HAL 库和 CubeMX 生成代码均为 C
2. C++ 的异常、RTTI 等特性在裸机/RTOS 环境下开销不可控
3. 团队成员对 C 的掌握程度更一致

但 C 语言缺乏类、继承、多态等语言级支持。本框架通过设计模式弥补这一差距，在不引入 C++ 的前提下实现清晰的架构分层。

### 核心模式对照

| OOP 概念 | C 语言实现 | 框架中的例子 |
|----------|-----------|-------------|
| 类 | 结构体 | `DJIMotorInstance`, `CANInstance` |
| 构造函数 | `XXXRegister()` / `XXXInit()` | `DJIMotorInit()`, `CANRegister()` |
| 成员方法 | 函数指针 / 外部函数 | `can_module_callback`, `DJIMotorSetRef()` |
| 多态 | 函数指针 + 遍历分发 | `CANFIFOxCallback()` |
| 继承 | 结构体嵌套 / void* 指针 | `CANCommInstance` 内嵌 `CANInstance*` |
| this 指针 | 显式 self 参数 | `DJIMotorSetRef(motor, ref)` |
| 访问控制 | static / 头文件声明 | `.c` 中 `static` 为 private，`.h` 声明为 public |

---

## 核心数据结构

### 模式一：结构体即"类"

以 `DJIMotorInstance` 为例，它等价于 C++ 中的：

```cpp
// C++ 等价伪代码
class DJIMotor {
public:
    DJI_Motor_Measure_s measure;          // 量测值（public，因为 APP 层需要读取）
    Motor_Control_Setting_s motor_settings;  // 控制设置
    Motor_Controller_s motor_controller;     // 控制器（包含 PID）
    CANInstance *motor_can_instance;         // 组合：拥有一个 CAN 实例
    Motor_Type_e motor_type;
    Motor_Working_Type_e stop_flag;
    DaemonInstance *daemon;

    // 方法
    void SetRef(float ref);
    void Stop();
    void Enable();
    void ChangeFeed(Closeloop_Type_e loop, Feedback_Source_e type);
    void ChangeOuterLoop(Closeloop_Type_e outer_loop);
};
```

C 语言中的实现：

```c
// dji_motor.h — 结构体定义（相当于 class 的数据成员）
typedef struct {
    DJI_Motor_Measure_s measure;
    Motor_Control_Setting_s motor_settings;
    Motor_Controller_s motor_controller;
    CANInstance *motor_can_instance;     // 组合关系
    uint8_t sender_group;
    uint8_t message_num;
    Motor_Type_e motor_type;
    Motor_Working_Type_e stop_flag;
    DaemonInstance *daemon;
    uint32_t feed_cnt;
    float dt;
} DJIMotorInstance;

// dji_motor.h — 函数声明（相当于 class 的 public 方法）
void DJIMotorSetRef(DJIMotorInstance *motor, float ref);  // self 参数在首位
void DJIMotorStop(DJIMotorInstance *motor);
void DJIMotorEnable(DJIMotorInstance *motor);
```

**对比 C++**：C 语言的"成员函数"多了一个显式的 `motor` 参数，等价于 C++ 中隐式的 `this` 指针。

### 模式二：Parent Pointer 模式

这是框架中最重要的 OOP 模式，解决了"底层代码如何回调上层代码"的问题。

```c
// bsp_can.h — CANInstance 中的 void* id 字段
typedef struct _ {
    CAN_HandleTypeDef *can_handle;
    // ...
    void (*can_module_callback)(struct _ *);  // 回调函数指针
    void *id;                                 // parent pointer: 指向拥有此实例的上层对象
} CANInstance;
```

**调用关系**：

```
BSP 层 (CANInstance)                Module 层 (DJIMotorInstance)
    |                                      |
    | can_module_callback() 被中断触发      |
    |                                      |
    | DecodeDJIMotor(_instance)  <---------+  回调函数
    |   |                                  |
    |   +-- motor = (DJIMotorInstance*)    |
    |         _instance->id   -----------+  通过 id 获取 parent
    |   |                                |
    |   +-- motor->measure.ecd = ...     |  访问 parent 的成员
```

具体代码（dji_motor.c:122-148）：

```c
static void DecodeDJIMotor(CANInstance *_instance)
{
    // 通过 parent pointer 获取电机实例
    DJIMotorInstance *motor = (DJIMotorInstance *)_instance->id;

    // 访问电机实例的成员
    DJI_Motor_Measure_s *measure = &motor->measure;
    measure->ecd = ((uint16_t)rxbuff[0]) << 8 | rxbuff[1];
    // ...
}
```

**注册时设置 parent pointer**（dji_motor.c:182-184）：

```c
// 在 DJIMotorInit() 中：
config->can_init_config.can_module_callback = DecodeDJIMotor;  // 设置回调
config->can_init_config.id = instance;                         // 设置 parent pointer
instance->motor_can_instance = CANRegister(&config->can_init_config);
```

**同样的模式在 CANComm 中**（can_comm.c:93-94）：

```c
// CANCommInit() 中：
comm_config->can_config.id = ins;                              // CANCommInstance* 作为 parent
comm_config->can_config.can_module_callback = CANCommRxCallback;
ins->can_ins = CANRegister(&comm_config->can_config);
```

**回调中通过 id 获取 parent**（can_comm.c:28）：

```c
static void CANCommRxCallback(CANInstance *_instance)
{
    CANCommInstance *comm = (CANCommInstance *)_instance->id;  // 获取 parent
    // ...
}
```

### 模式三：实例注册 + 回调分发

多个同类实例的回调分发是嵌入式开发中的经典问题。框架的解决方案是：

1. BSP 层维护实例指针数组
2. 中断发生时遍历数组，匹配硬件标识（CAN 句柄 + rx_id）
3. 匹配成功后调用该实例的回调函数

```c
// bsp_can.c — 实例指针数组
static CANInstance *can_instance[CAN_MX_REGISTER_CNT] = {NULL};
static uint8_t idx;  // 实例计数

// bsp_can.c:149-169 — 中断回调分发
static void CANFIFOxCallback(CAN_HandleTypeDef *_hcan, uint32_t fifox)
{
    static CAN_RxHeaderTypeDef rxconf;
    uint8_t can_rx_buff[8];
    while (HAL_CAN_GetRxMessage(_hcan, fifox, &rxconf, can_rx_buff)) {
        for (size_t i = 0; i < idx; ++i) {
            // 匹配 CAN 句柄和接收 ID
            if (_hcan == can_instance[i]->can_handle
                && rxconf.StdId == can_instance[i]->rx_id) {
                if (can_instance[i]->can_module_callback != NULL) {
                    // 拷贝数据到实例的 rx_buff
                    memcpy(can_instance[i]->rx_buff, can_rx_buff, rxconf.DLC);
                    // 调用实例的回调（多态）
                    can_instance[i]->can_module_callback(can_instance[i]);
                }
                return;
            }
        }
    }
}
```

**这就是 C 语言中的多态**：相同的调用形式 `can_module_callback(can_instance[i])`，不同的实例执行不同的回调函数（DecodeDJIMotor、CANCommRxCallback 等）。

### 模式四：消息中心——发布-订阅

消息中心用链表实现发布-订阅模式，将"谁发布"和"谁订阅"解耦。

```c
// message_center.h — 发布者（话题节点）
typedef struct ent {
    char topic_name[MAX_TOPIC_NAME_LEN + 1];  // 话题名（相当于 channel 名）
    uint8_t data_len;                          // 数据长度
    Subscriber_t *first_subs;                  // 指向第一个订阅者（链表头）
    struct ent *next_topic_node;               // 下一个话题节点
    uint8_t pub_registered_flag;
} Publisher_t;

// message_center.h — 订阅者
typedef struct mqt {
    void *queue[QUEUE_SIZE];    // FIFO 消息队列
    uint8_t data_len;
    uint8_t front_idx, back_idx, temp_size;
    struct mqt *next_subs_queue; // 下一个订阅同一话题的订阅者
} Subscriber_t;
```

**数据结构关系**：

```
message_center (哑头节点)
    |
    +-- Publisher_t("gimbal_cmd")
    |     |
    |     +-- Subscriber_t (gimbal.c 订阅)
    |
    +-- Publisher_t("chassis_cmd")
    |     |
    |     +-- Subscriber_t (chassis.c 订阅)
    |
    +-- Publisher_t("shoot_cmd")
          |
          +-- Subscriber_t (shoot.c 订阅)

每个 Publisher 持有一个 Subscriber 链表
发布时遍历链表，将数据 memcpy 到每个订阅者的队列
```

---

## 函数详解

### XXXRegister() — 统一的"构造函数"

所有模块的初始化接口统一命名为 `XXXRegister()` 或 `XXXInit()`，本质是构造函数：

```c
// CANRegister — BSP 层的构造函数 (bsp_can.c:63)
CANInstance *CANRegister(CAN_Init_Config_s *config)
{
    CANInstance *instance = (CANInstance *)malloc(sizeof(CANInstance));  // 分配内存
    memset(instance, 0, sizeof(CANInstance));                           // 清零（类似 C++ 默认构造）

    // 初始化成员
    instance->txconf.StdId = config->tx_id;
    instance->can_handle = config->can_handle;
    instance->can_module_callback = config->can_module_callback;
    instance->id = config->id;

    // 注册到实例数组
    CANAddFilter(instance);
    can_instance[idx++] = instance;

    return instance;  // 返回实例指针
}

// DJIMotorInit — Module 层的构造函数 (dji_motor.c:159)
DJIMotorInstance *DJIMotorInit(Motor_Init_Config_s *config)
{
    DJIMotorInstance *instance = (DJIMotorInstance *)malloc(sizeof(DJIMotorInstance));
    memset(instance, 0, sizeof(DJIMotorInstance));

    // 初始化成员
    instance->motor_type = config->motor_type;
    instance->motor_settings = config->controller_setting_init_config;
    PIDInit(&instance->motor_controller.current_PID, &config->...current_PID);
    PIDInit(&instance->motor_controller.speed_PID, &config->...speed_PID);
    PIDInit(&instance->motor_controller.angle_PID, &config->...angle_PID);

    // 组合：注册底层 BSP 实例
    config->can_init_config.can_module_callback = DecodeDJIMotor;
    config->can_init_config.id = instance;  // 设置 parent pointer
    instance->motor_can_instance = CANRegister(&config->can_init_config);

    // 注册到模块的实例数组
    dji_motor_instance[idx++] = instance;
    return instance;
}
```

**对比 C++**：

```cpp
// C++ 等价写法
class DJIMotor {
    CANInstance *motor_can_instance;
public:
    DJIMotor(Motor_Init_Config_s &config) {  // 构造函数
        motor_type = config.motor_type;
        motor_can_instance = new CANInstance(config.can_config);  // 组合
    }
};
```

C 语言版本需要手动 malloc、memset、设置函数指针和 parent pointer，但效果相同。

### PubRegister() / SubRegister() — 消息中心注册

```c
// message_center.c:32 — 发布者注册
Publisher_t *PubRegister(char *name, uint8_t data_len)
{
    // 遍历话题链表
    Publisher_t *node = &message_center;
    while (node->next_topic_node) {
        node = node->next_topic_node;
        if (strcmp(node->topic_name, name) == 0) {
            // 话题已存在，检查数据长度一致
            CheckLen(data_len, node->data_len);
            return node;  // 直接返回已有节点
        }
    }
    // 话题不存在，创建新节点
    node->next_topic_node = (Publisher_t *)malloc(sizeof(Publisher_t));
    memset(node->next_topic_node, 0, sizeof(Publisher_t));
    node->next_topic_node->data_len = data_len;
    strcpy(node->next_topic_node->topic_name, name);
    return node->next_topic_node;
}

// message_center.c:55 — 订阅者注册
Subscriber_t *SubRegister(char *name, uint8_t data_len)
{
    Publisher_t *pub = PubRegister(name, data_len);  // 确保话题存在
    Subscriber_t *ret = (Subscriber_t *)malloc(sizeof(Subscriber_t));
    memset(ret, 0, sizeof(Subscriber_t));
    ret->data_len = data_len;
    for (size_t i = 0; i < QUEUE_SIZE; ++i)
        ret->queue[i] = malloc(data_len);  // 为队列元素分配空间

    // 加入订阅者链表
    if (pub->first_subs == NULL)
        pub->first_subs = ret;
    else {
        Subscriber_t *sub = pub->first_subs;
        while (sub->next_subs_queue) sub = sub->next_subs_queue;
        sub->next_subs_queue = ret;
    }
    return ret;
}
```

---

## 调用链

### 从 APP 到硬件的完整 OOP 链路

```
APP 层 (chassis.c)
  |
  +-- DJIMotorSetRef(motor_lf, vt_lf)           // 调用"成员函数"
        |     motor_lf->motor_controller.pid_ref = ref;  // 设置实例数据
        |
  Module 层 (dji_motor.c)
  |
  +-- DJIMotorControl()                          // 遍历所有实例，计算 PID
        |     motor = dji_motor_instance[i];     // 从实例数组获取
        |     PIDCalculate(&motor->motor_controller.speed_PID, ...)
        |     sender_assignment[group].tx_buff[...] = set;
        |
  BSP 层 (bsp_can.c)
  |
  +-- CANTransmit(&sender_assignment[i], 1)
        |     HAL_CAN_AddTxMessage(...)          // 最终调用 HAL
        |
  硬件
  +-- CAN 总线发送电信号

--- 反向：从硬件到 APP ---

硬件中断
  |
  +-- HAL_CAN_RxFifo0MsgPendingCallback()        // HAL 中断回调
        |
  BSP 层 (bsp_can.c)
  +-- CANFIFOxCallback()
        |     遍历 can_instance[] 匹配
        |     can_instance[i]->can_module_callback(can_instance[i])  // 多态分发
        |
  Module 层 (dji_motor.c)
  +-- DecodeDJIMotor(_instance)                   // DJI 电机解码
        |     motor = (DJIMotorInstance*)_instance->id;  // parent pointer
        |     motor->measure.ecd = ...            // 写入 parent 的数据
        |
  APP 层 (chassis.c)
  +-- 下次 ChassisTask() 读取 motor->measure      // 使用解码后的数据
```

---

## 注意事项

1. **parent pointer 类型安全** — `void *id` 没有类型检查，如果强转错误类型会直接导致数据覆写或 HardFault。注册时必须确保 `id` 指向的类型与回调中的强转类型一致。

2. **常见错误：直接将 CANInstance 强转为 module 实例** — 正确做法是通过 `instance->id` 间接获取 parent，而不是直接强转 `CANInstance` 本身。`CANInstance` 是"子对象"，`DJIMotorInstance` 是"父对象"，两者在内存中不是同一块区域。

3. **实例指针数组是 C 没有 STL 容器的替代方案** — 使用固定大小的数组（如 `can_instance[16]`、`dji_motor_instance[12]`），注册时检查是否越界。优点是无动态分配开销，缺点是数量受限于编译时常量。

4. **malloc 的使用** — 框架在实例注册时使用 `malloc` 动态分配内存，但只在初始化阶段（RTOS 启动前）调用，运行时不释放。这在嵌入式系统中是可接受的，因为实例的生命周期与程序相同。如果需要运行时创建/销毁实例，应考虑内存碎片问题。

5. **回调函数是 static 的** — `DecodeDJIMotor`、`CANCommRxCallback` 等回调函数声明为 `static`，外部无法直接调用。它们只能通过函数指针（回调机制）被间接调用，这相当于 C++ 中的 private 方法。

6. **配置结构体 = 构造函数参数** — `Motor_Init_Config_s`、`CAN_Init_Config_s` 等配置结构体相当于 C++ 构造函数的参数列表。使用指定初始化器（designated initializer）可以只设置需要的字段，其余为 0。

7. **与 C++ 的根本差异** — C 语言的这些模式是约定（convention），不是语言强制的。C++ 的 private/public 有编译器检查，C 的 static 只有文件作用域保护。团队必须遵守编码规范，否则 OOP 的优势无法体现。
