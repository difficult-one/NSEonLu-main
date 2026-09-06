# 消息中心 -- 发布-订阅通信机制

## 模块职责

消息中心（message_center）为 APP 层应用之间提供松耦合的发布-订阅（Pub-Sub）通信机制，替代全局变量实现数据共享。

## 设计思路

### 为什么需要消息中心

在嵌入式开发中，模块间共享数据最简单的方式是全局变量，但全局变量存在以下问题：

- 应用之间产生隐含的依赖关系（耦合）
- 无法追踪数据的读写来源
- 命名冲突风险高
- 难以扩展为双板通信

消息中心通过"话题名"实现松耦合：发布者只需知道话题名，不需要知道谁在订阅；订阅者同理。应用之间完全平行，不存在包含关系。

### 为什么说是"伪"Pub-Sub

消息中心的实现是"伪"发布-订阅：它只做了通信隔离，没有做进程间同步、消息队列深度管理等完整功能。队列深度固定为 1（`QUEUE_SIZE = 1`），即只保留最新一条消息。这符合嵌入式实时控制的场景：我们通常只关心最新状态，不需要历史消息。

## 核心数据结构

### Publisher_t -- 发布者

```c
// message_center.h
typedef struct ent {
    char topic_name[MAX_TOPIC_NAME_LEN + 1]; // 话题名称，最多32字符+结束符
    uint8_t data_len;                        // 该话题的数据长度
    Subscriber_t *first_subs;                // 指向第一个订阅了该话题的订阅者
    struct ent *next_topic_node;             // 指向下一个 Publisher（链表）
    uint8_t pub_registered_flag;             // 是否已注册标志
} Publisher_t;
```

- `topic_name`：话题的唯一标识，相同话题名对应同一个 Publisher 节点
- `first_subs`：通过链表连接所有订阅了此话题的订阅者
- `next_topic_node`：所有 Publisher 组成一条链表，头节点是 `message_center`

### Subscriber_t -- 订阅者

```c
// message_center.h
typedef struct mqt {
    void *queue[QUEUE_SIZE]; // FIFO 队列，保存数据指针
    uint8_t data_len;        // 每条消息的长度
    uint8_t front_idx;       // 队头索引
    uint8_t back_idx;        // 队尾索引
    uint8_t temp_size;       // 当前队列中的消息数
    struct mqt *next_subs_queue; // 指向下一个订阅了同一话题的订阅者
} Subscriber_t;
```

- `queue[QUEUE_SIZE]`：数组模拟的 FIFO 队列，`QUEUE_SIZE = 1`，即只保存最新一条消息
- 每个 `queue[i]` 在注册时通过 `malloc(data_len)` 分配空间，这样可以兼容不同长度的数据

### message_center -- 哑头节点

```c
// message_center.c
static Publisher_t message_center = {
    .topic_name = "Message_Manager",
    .first_subs = NULL,
    .next_topic_node = NULL
};
```

这是一个"哑头节点"（dummy head），简化链表操作，不需要特殊处理链表头部。

## 函数详解

### PubRegister() -- 注册发布者

```c
Publisher_t *PubRegister(char *name, uint8_t data_len)
```

**逻辑**：
1. 检查话题名长度是否超限（`CheckName`）
2. 遍历 Publisher 链表，查找是否已存在同名话题
   - 若存在：校验 `data_len` 是否一致（`CheckLen`），设置 `pub_registered_flag = 1`，返回已有节点
   - 若不存在：在链表尾部 `malloc` 新节点，初始化并返回

**关键设计**：同名话题只创建一次 Publisher 节点，多个发布者可以共享同一节点。`data_len` 校验确保发布和订阅的数据长度一致。

### SubRegister() -- 注册订阅者

```c
Subscriber_t *SubRegister(char *name, uint8_t data_len)
```

**逻辑**：
1. 调用 `PubRegister(name, data_len)` 查找或创建对应话题的 Publisher
2. `malloc` 新的 Subscriber 实例，`memset` 清零
3. 为队列中的每个元素 `malloc(data_len)` 分配数据空间
4. 将新 Subscriber 接入 Publisher 的订阅者链表尾部

**关键设计**：先调用 `PubRegister` 确保话题存在，再创建订阅者并链接。这意味着即使订阅者先注册，话题也会被自动创建。

### PubPushMessage() -- 发布消息

```c
uint8_t PubPushMessage(Publisher_t *pub, void *data_ptr)
```

**逻辑**：
1. 从 `pub->first_subs` 开始，遍历订阅者链表
2. 对每个订阅者：
   - 若队列已满（`temp_size == QUEUE_SIZE`）：先弹出最老的数据（`front_idx` 前移）
   - 将数据 `memcpy` 到 `queue[back_idx]`
   - 更新 `back_idx` 和 `temp_size`
3. 返回成功推送的订阅者数量

**队列溢出处理**：当 `QUEUE_SIZE = 1` 时，旧消息直接被新消息覆盖，确保订阅者始终能获取最新数据。

### SubGetMessage() -- 获取消息

```c
uint8_t SubGetMessage(Subscriber_t *sub, void *data_ptr)
```

**逻辑**：
1. 若 `temp_size == 0`（队列为空），返回 0
2. 从 `queue[front_idx]` 拷贝数据到 `data_ptr`
3. 更新 `front_idx` 和 `temp_size`
4. 返回 1 表示成功获取

**使用方式**：在控制循环中调用，返回 0 表示没有新数据。

## 调用链

### 发布-订阅的完整数据流

```
发布端 (APP: robot_cmd)
  ChassisInit():
    chassis_pub = PubRegister("chassis_speed", sizeof(ChassisSpeed_t))  // 注册

  ChassisTask():
    PubPushMessage(chassis_pub, &chassis_data)  // 每个控制周期发布

订阅端 (APP: chassis)
  ChassisInit():
    cmd_sub = SubRegister("chassis_cmd", sizeof(ChassisCmd_t))  // 订阅

  ChassisTask():
    SubGetMessage(cmd_sub, &cmd)  // 每个控制周期读取
    if (msg_received) { /* 使用 cmd */ }
```

### 内部数据结构关系

```
message_center (哑头)
  |
  +--> Publisher("chassis_speed") --first_subs--> Subscriber_A --next--> Subscriber_B
  |
  +--> Publisher("gimbal_angle")  --first_subs--> Subscriber_C
```

## 注意事项

1. **话题名长度限制** -- 最大 32 字符（`MAX_TOPIC_NAME_LEN`），超长会触发 `LOGERROR` 并进入死循环。这在初始化阶段就会暴露。

2. **数据长度必须一致** -- 同一话题的发布者和订阅者传入的 `data_len` 必须相同，否则触发 `CheckLen` 报错。这是为了防止内存越界。

3. **队列深度为 1** -- `QUEUE_SIZE = 1` 意味着只保留最新消息。如果发布频率高于订阅读取频率，旧数据会被丢弃。在实时控制场景下这是期望行为。

4. **使用 malloc** -- Publisher 和 Subscriber 实例均通过 `malloc` 动态分配。在 FreeRTOS 环境下应考虑使用 `pvPortMalloc` 以保证线程安全，当前实现直接使用了标准库 `malloc`。

5. **最大话题数量** -- `MAX_TOPIC_COUNT = 12`，但当前实现未做硬性数量限制检查，仅依赖 `malloc` 的返回值。如果话题过多可能导致内存不足。

6. **不是真正的消息队列** -- 当前实现不提供阻塞等待、消息过滤等高级功能。如需这些特性，应使用 FreeRTOS 的 Queue 或 EventGroup。
