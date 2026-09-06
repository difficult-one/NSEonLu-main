# Message Center — 链表 + 循环数组的发布订阅机制

> 解释 `APP层应用编写指引.md` 中的这句话：
> "为了节省空间，数据结构上采用了链表+循环数组模拟队列的方式。C 没有哈希表，因此让发布者保存所有订阅者的地址（实际上只保存首地址，然后通过链表访问所有订阅者）。"

---

## 1. 整体数据结构

```
message_center (哑头节点, 方便链表操作)
│
└─ Publisher_t* next_topic_node → Publisher_t "gimbal_cmd"
│   ├─ topic_name: "gimbal_cmd"
│   ├─ data_len:  sizeof(Gimbal_Ctrl_Cmd_s)
│   ├─ first_subs → Subscriber_t ← ★ 第一个订阅者
│   │   ├─ queue[1]  ← 循环数组 (模拟队列)
│   │   │   └─ queue[0] → malloc(sizeof(Gimbal_Ctrl_Cmd_s))  ← 实际数据存储
│   │   ├─ front_idx, back_idx, temp_size  ← 循环队列指针
│   │   └─ next_subs_queue → Subscriber_t ← ★ 第二个订阅者
│   │       ├─ queue[1]
│   │       └─ next_subs_queue → NULL      ← 链尾
│   │
│   └─ next_topic_node → Publisher_t "chassis_cmd"
│       ├─ topic_name: "chassis_cmd"
│       ├─ first_subs → Subscriber_t
│       │   └─ next_subs_queue → NULL      ← 只有一个订阅者
│       └─ next_topic_node → NULL          ← 链尾
│
└─ ...
```

**两层链表嵌套**：
- 外层：Publisher 链表（按话题名串起来）
- 内层：每个 Publisher 挂着一个 Subscriber 链表（订阅了该话题的所有订阅者）

---

## 2. "C 没有哈希表，因此让发布者保存所有订阅者的地址"

### 如果有哈希表

```
话题名 "gimbal_cmd" ──→ hash("gimbal_cmd") ──→ 哈希表[3] ──→ 订阅者列表
话题名 "chassis_cmd" ──→ hash("chassis_cmd") ──→ 哈希表[7] ──→ 订阅者列表
```

直接通过话题名 O(1) 找到订阅者，但需要额外的哈希表数据结构。在嵌入式资源受限环境下，哈希表的实现会消耗更多 RAM。

### 本项目的做法

**Publisher 本身就是一个节点**，它持有 `first_subs` 指针直接指向第一个订阅者：

```c
typedef struct ent {
    char topic_name[MAX_TOPIC_NAME_LEN + 1];   // 话题名
    uint8_t data_len;                           // 数据长度
    Subscriber_t *first_subs;                   // ★ 只保存链表头指针 ★
    struct ent *next_topic_node;                // 指向下一个 Publisher
} Publisher_t;
```

- `first_subs` 指向第一个订阅者
- 每个订阅者通过 `next_subs_queue` 连接成链表
- 发布者只需要持有**首地址**，就能访问到所有订阅者

**发布时遍历**（`message_center.c:96-117`）：

```c
uint8_t PubPushMessage(Publisher_t *pub, void *data_ptr)
{
    Subscriber_t *iter = pub->first_subs;  // ① 从头节点开始

    while (iter)                            // ② 遍历链表直到 NULL
    {
        // ③ 给当前订阅者的队列里塞数据
        memcpy(iter->queue[iter->back_idx], data_ptr, pub->data_len);
        iter->back_idx = (iter->back_idx + 1) % QUEUE_SIZE;  // 循环
        iter->temp_size++;

        iter = iter->next_subs_queue;       // ④ 移动下一个订阅者
    }
}
```

---

## 3. "链表 + 循环数组模拟队列"

### 链表 — 订阅者链表

```c
typedef struct mqt {
    void *queue[QUEUE_SIZE];           // 循环数组（当前 QUEUE_SIZE = 1）
    uint8_t data_len;
    uint8_t front_idx;                 // 队头索引
    uint8_t back_idx;                  // 队尾索引
    uint8_t temp_size;                 // 当前队列中元素个数
    struct mqt *next_subs_queue;       // ★ 链表指针：指向下一个订阅者
} Subscriber_t;
```

每个订阅者是一个链表节点。同一话题的所有订阅者通过 `next_subs_queue` 串联。

### 循环数组 — 每个订阅者的消息队列

```c
void *queue[QUEUE_SIZE];  // 指针数组，当前 SIZE = 1
```

这个数组用 `front_idx`（队头）和 `back_idx`（队尾）模拟 FIFO 队列。

**入队**（发布者推送数据时）：

```
入队前: queue = [ 旧数据 ]
        front_idx = 0, back_idx = 1, temp_size = 1  ← 满了

入队步骤:
① front_idx = (0 + 1) % 1 = 0  ← 队头前移（抛弃旧数据）
② temp_size--  (变为 0)
③ memcpy(queue[0], 新数据, data_len)  ← 覆盖
④ back_idx = (1 + 1) % 1 = 0  ← 队尾前移
⑤ temp_size++  (变为 1)
```

因为是 `QUEUE_SIZE = 1`（只有 1 个槽），所以始终**只保留最新的一条消息**，旧的直接被覆盖。这是一个设计选择——机器人控制中，旧的指令没意义，只关心最新。

**出队**（订阅者取数据时）：

```c
memcpy(data_ptr, queue[front_idx], data_len);    // 读出
front_idx = (front_idx + 1) % QUEUE_SIZE;         // 队头前移（循环）
temp_size--;                                      // 计数减 1
```

---

## 4. 哑头节点（Dumb Head）技巧

```c
static Publisher_t message_center = {
    .topic_name = "Message_Manager",
    .first_subs = NULL,
    .next_topic_node = NULL       // ① 这个是链表真正的头
};
```

这个 `message_center` 本身不发布任何数据，只充当**链表头的前一个节点**。好处是不需要处理"链表为空时插入第一个节点"的特殊情况。`PubRegister` 中：

```c
Publisher_t *node = &message_center;
while (node->next_topic_node)       // 遍历已有的话题
    node = node->next_topic_node;

// 走到链尾，直接接上
node->next_topic_node = malloc(sizeof(Publisher_t));
// 不需要判断 "如果 message_center->next_topic_node 是 NULL 怎么办"
```

---

## 5. 注册流程：如何从"话题名"找到"订阅者"

### 订阅者注册

```c
Subscriber_t *SubRegister(char *name, uint8_t data_len)
{
    // ① 先找到（或创建）对应话题的 Publisher
    Publisher_t *pub = PubRegister(name, data_len);

    // ② 创建一个新的 Subscriber
    Subscriber_t *ret = malloc(sizeof(Subscriber_t));
    memset(ret, 0, sizeof(Subscriber_t));
    ret->data_len = data_len;
    for (int i = 0; i < QUEUE_SIZE; i++)
        ret->queue[i] = malloc(data_len);  // 为每个队列槽分配空间

    // ③ 如果这是该话题的第一个订阅者
    if (pub->first_subs == NULL) {
        pub->first_subs = ret;             // 直接挂到 Publisher 上
        return ret;
    }

    // ④ 否则遍历到订阅者链表末尾，把新订阅者接上去
    Subscriber_t *sub = pub->first_subs;
    while (sub->next_subs_queue)
        sub = sub->next_subs_queue;
    sub->next_subs_queue = ret;
    return ret;
}
```

### 发布者注册

```c
Publisher_t *PubRegister(char *name, uint8_t data_len)
{
    Publisher_t *node = &message_center;       // 从哑头开始
    while (node->next_topic_node) {            // 遍历已有话题
        node = node->next_topic_node;
        if (strcmp(node->topic_name, name) == 0)  // 已存在 → 直接返回
            return node;
    }
    // 不存在 → 在链尾新建
    node->next_topic_node = malloc(sizeof(Publisher_t));
    strcpy(node->next_topic_node->topic_name, name);
    node->next_topic_node->data_len = data_len;
    return node->next_topic_node;
}
```

---

## 6. 完整数据流

```
Application A (发布者)                        Application B (订阅者)
══════════════════                          ══════════════════

① PubRegister("gimbal_cmd", 20)             ③ SubRegister("gimbal_cmd", 20)
   → 在 Publisher 链表中查找                    → 先调 PubRegister 找到 Publisher
   → 不存在 → 新建 Publisher 节点               → 创建 Subscriber
                                               → 加入订阅者链表

④ while(1) {
      PubPushMessage(pub, &gimbal_cmd_send); ⑤ while(1) {
        → 遍历 first_subs 链表                    SubGetMessage(sub, &gimbal_cmd_recv);
        → 每个 subscriber 的 queue[0] 写入新数据    → 从 queue[0] 读出最新数据
  }                                            }
```

---

## 7. 为什么这样设计

| 设计决策 | 原因 |
|---------|------|
| **不用哈希表** | 嵌入式 RAM 紧张；话题数量最多 12 个，O(n) 遍历足够快 |
| **发布者保存订阅者首地址** | 发布时直接遍历链表，不需要反查；节省了全局索引结构 |
| **QUEUE_SIZE = 1** | 机器人控制只需最新指令，不需要缓存历史数据 |
| **malloc 每个队列槽** | 不同话题的数据长度不同，动态分配兼容任意类型 |
| **哑头节点** | 简化链表操作，避免空链表边界条件 |

---

## 8. 内存模型对比

```
哈希表方案：                            本项目方案：
┌─────────────────┐                   message_center → Publisher → Publisher
│ hash_table[12]  │  ← 固定数组         ^                ^
│ [0] → NULL      │                   /                /
│ [1] → "gimbal"  │                  Subscriber链表    Subscriber链表
│ [2] → NULL      │                  (发布者内嵌指针)   (发布者内嵌指针)
│ ...             │
└─────────────────┘
  额外 RAM: 12 个指针

                  本项目：Publisher 链表本身就是话题索引
                  Subscriber 链表本身就是订阅者索引
                  零额外开销，利用结构体的内部指针串联
```
