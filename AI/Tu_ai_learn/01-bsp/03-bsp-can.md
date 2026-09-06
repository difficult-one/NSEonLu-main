# BSP CAN — CAN 通信封装

## 模块职责

对 STM32 BxCAN 外设进行面向多实例的封装，实现自动过滤器配置、FIFO 中断回调分发和实例注册制管理，是整个框架中最核心的 BSP 模块。

---

## 设计思路

### 为什么 CAN 封装这么复杂？

与 GPIO/UART 等点对点通信不同，CAN 总线是多主机广播式的：

1. **一条总线上挂多个设备** — 需要过滤器决定"我收谁的消息"
2. **多个 Module 共享同一 CAN 外设** — 需要注册制管理多个实例
3. **中断回调需要精确分发** — 不同 ID 的消息要送给不同的 Module 处理

### 核心设计决策

- **IDLIST 过滤模式** — 只接收我们关心的 ID，其他报文在硬件层面被丢弃
- **FIFO 均衡** — 奇数 ID 分配 FIFO0，偶数 ID 分配 FIFO1，分散中断负载
- **首次注册触发服务初始化** — 没有模块用 CAN 就不启动总线

---

## 核心数据结构

### 常量定义

```c
// basic_framework/bsp/can/bsp_can.h:8-10
#define CAN_MX_REGISTER_CNT 16     // 最大 CAN 实例数，取决于总线负载
#define MX_CAN_FILTER_CNT (2 * 14) // 最多 28 个过滤器（F407 的 BxCAN 限制）
#define DEVICE_CAN_CNT 2           // F407IG 有 CAN1 和 CAN2
```

### CANInstance（packed）

```c
// basic_framework/bsp/can/bsp_can.h:15-28
#pragma pack(1)
typedef struct _
{
    CAN_HandleTypeDef *can_handle;          // CAN 句柄，指向 CubeMX 生成的 hcan1/hcan2
    CAN_TxHeaderTypeDef txconf;             // 发送报文配置（StdId, IDE, RTR, DLC）
    uint32_t tx_id;                         // 发送 ID
    uint32_t tx_mailbox;                    // 实际填入的发送邮箱号
    uint8_t tx_buff[8];                     // 发送缓冲区，发送前写入数据
    uint8_t rx_buff[8];                     // 接收缓冲区，回调时已填充数据
    uint32_t rx_id;                         // 接收 ID，用于过滤器匹配和回调分发
    uint8_t rx_len;                         // 接收数据长度（0-8）
    void (*can_module_callback)(struct _ *); // Module 层注册的回调函数
    void *id;                               // Parent Pointer，指向拥有此实例的 Module 结构体
} CANInstance;
#pragma pack()
```

逐字段说明：

| 字段 | 作用 | 备注 |
|------|------|------|
| `can_handle` | 区分 CAN1/CAN2 | 回调分发时用于匹配 |
| `txconf` | HAL 发送所需的报文头 | 初始化时配置好，发送时直接用 |
| `tx_id` | 本实例的发送 ID | 源码注释"好像没用" |
| `tx_mailbox` | HAL 填入的邮箱号 | 调试用，一般不关心 |
| `tx_buff[8]` | 发送数据缓冲 | Module 层发送前写入 |
| `rx_buff[8]` | 接收数据缓冲 | BSP 中断中填充，Module 回调中读取 |
| `rx_id` | 本实例要接收的 ID | 同时用于过滤器和回调分发 |
| `rx_len` | 接收数据实际长度 | DLC 字段值 |
| `can_module_callback` | Module 层解码函数 | 中断中调用 |
| `id` | Parent Pointer | 回调中通过此指针获取 Module 实例 |

### CAN_Init_Config_s

```c
// basic_framework/bsp/can/bsp_can.h:32-39
typedef struct
{
    CAN_HandleTypeDef *can_handle;              // CAN 句柄
    uint32_t tx_id;                             // 发送 ID
    uint32_t rx_id;                             // 接收 ID
    void (*can_module_callback)(CANInstance *); // 接收回调
    void *id;                                   // Parent Pointer
} CAN_Init_Config_s;
```

Module 层填充此结构体后传入 `CANRegister()`。

---

## 函数详解

### CANAddFilter()

```c
// basic_framework/bsp/can/bsp_can.c:29-43
static void CANAddFilter(CANInstance *_instance)
{
    CAN_FilterTypeDef can_filter_conf;
    static uint8_t can1_filter_idx = 0, can2_filter_idx = 14;

    can_filter_conf.FilterMode = CAN_FILTERMODE_IDLIST;           // ID 列表模式
    can_filter_conf.FilterScale = CAN_FILTERSCALE_16BIT;          // 16 位尺度
    can_filter_conf.FilterFIFOAssignment = (_instance->tx_id & 1) // 奇数 FIFO0，偶数 FIFO1
        ? CAN_RX_FIFO0 : CAN_RX_FIFO1;
    can_filter_conf.SlaveStartFilterBank = 14;                    // CAN2 从 bank 14 开始
    can_filter_conf.FilterIdLow = _instance->rx_id << 5;          // rx_id 左移 5 位
    can_filter_conf.FilterBank = _instance->can_handle == &hcan1  // CAN1: 0-13, CAN2: 14-27
        ? (can1_filter_idx++) : (can2_filter_idx++);
    can_filter_conf.FilterActivation = CAN_FILTER_ENABLE;

    HAL_CAN_ConfigFilter(_instance->can_handle, &can_filter_conf);
}
```

关键点详解：

1. **IDLIST 模式** — 只有 ID 匹配的报文才接收，其他全部丢弃。比 MASK 模式更精确
2. **16BIT 尺度** — 标准帧 ID 只有 11 位，16 位足够。一个过滤器可以配两个 ID（高 16 位 + 低 16 位），但本框架每个过滤器只配一个
3. **FIFO 均衡** — 用 `tx_id & 1` 决定分配到 FIFO0 还是 FIFO1，分散中断压力
4. **SlaveStartFilterBank = 14** — F407 的 BxCAN 中，CAN2 是 CAN1 的从机。Bank 0-13 给 CAN1，Bank 14-27 给 CAN2
5. **rx_id << 5** — 过滤器寄存器格式要求：标准 ID 占 [15:5] 位，低 5 位为 RTR/IDE 等标志位。因此需要左移 5 位

### CANServiceInit()

```c
// basic_framework/bsp/can/bsp_can.c:51-59
static void CANServiceInit()
{
    HAL_CAN_Start(&hcan1);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO1_MSG_PENDING);
    HAL_CAN_Start(&hcan2);
    HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING);
    HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO1_MSG_PENDING);
}
```

- **功能**：启动 CAN1 和 CAN2，开启两个 FIFO 的消息挂起中断
- **调用时机**：仅在第一个 CAN 实例注册时调用
- **注意**：直接启动了 hcan1 和 hcan2，如果板子只有一个 CAN，需要修改此处

### CANRegister()

```c
// basic_framework/bsp/can/bsp_can.c:63-102
CANInstance *CANRegister(CAN_Init_Config_s *config)
{
    if (!idx) {                     // 首次注册
        CANServiceInit();
        LOGINFO("[bsp_can] CAN Service Init");
    }
    if (idx >= CAN_MX_REGISTER_CNT) // 超过最大实例数
        while (1) LOGERROR("...");

    for (size_t i = 0; i < idx; i++) {  // 检查 ID 冲突
        if (can_instance[i]->rx_id == config->rx_id
            && can_instance[i]->can_handle == config->can_handle)
            while (1) LOGERROR("...");
    }

    CANInstance *instance = (CANInstance *)malloc(sizeof(CANInstance));
    memset(instance, 0, sizeof(CANInstance));

    // 配置发送报文头
    instance->txconf.StdId = config->tx_id;  // 标准 ID
    instance->txconf.IDE = CAN_ID_STD;       // 使用标准帧（11 位 ID）
    instance->txconf.RTR = CAN_RTR_DATA;     // 数据帧（非远程帧）
    instance->txconf.DLC = 0x08;             // 默认数据长度 8 字节

    // 配置实例字段
    instance->can_handle = config->can_handle;
    instance->tx_id = config->tx_id;
    instance->rx_id = config->rx_id;
    instance->can_module_callback = config->can_module_callback;
    instance->id = config->id;

    CANAddFilter(instance);                  // 添加过滤器
    can_instance[idx++] = instance;          // 保存到实例数组

    return instance;
}
```

- **首次注册触发** — `!idx` 判断是否为第一个注册的实例
- **ID 冲突检测** — 同一 CAN 总线上不能有两个相同 rx_id 的实例
- **malloc + memset** — 动态分配内存，memset 确保初始值为 0
- **DLC 默认 8** — 可通过 `CANSetDLC()` 修改

### CANTransmit()

```c
// basic_framework/bsp/can/bsp_can.c:106-129
uint8_t CANTransmit(CANInstance *_instance, float timeout)
{
    static uint32_t busy_count;
    static volatile float wait_time __attribute__((unused));
    float dwt_start = DWT_GetTimeline_ms();

    while (HAL_CAN_GetTxMailboxesFreeLevel(_instance->can_handle) == 0) // 等待邮箱空闲
    {
        if (DWT_GetTimeline_ms() - dwt_start > timeout) // 超时检测
        {
            LOGWARNING("[bsp_can] CAN MAILbox full! ...");
            busy_count++;
            return 0;
        }
    }
    wait_time = DWT_GetTimeline_ms() - dwt_start;

    if (HAL_CAN_AddTxMessage(_instance->can_handle, &_instance->txconf,
                              _instance->tx_buff, &_instance->tx_mailbox))
    {
        LOGWARNING("[bsp_can] CAN bus BUS! cnt:%d", busy_count);
        busy_count++;
        return 0;
    }
    return 1; // 发送成功
}
```

- **超时等待** — 用 DWT 计时而非 HAL_Delay，不受中断影响
- **三级邮箱** — STM32 BxCAN 有 3 个发送邮箱，`GetTxMailboxesFreeLevel()` 返回空闲数
- **返回值** — 1 成功，0 失败（超时或总线错误）
- **注意**：超时时间不应超过调用此函数的任务周期，否则任务会阻塞

### CANSetDLC()

```c
// basic_framework/bsp/can/bsp_can.c:131-138
void CANSetDLC(CANInstance *_instance, uint8_t length)
{
    if (length > 8 || length == 0)  // 安全检查
        while (1) LOGERROR("...");
    _instance->txconf.DLC = length;
}
```

- **DLC** — Data Length Code，CAN 数据帧的有效载荷长度（1-8 字节）
- **默认 8** — 注册时设为 8，大部分电机控制帧都是 8 字节

### CANFIFOxCallback()

```c
// basic_framework/bsp/can/bsp_can.c:149-170
static void CANFIFOxCallback(CAN_HandleTypeDef *_hcan, uint32_t fifox)
{
    static CAN_RxHeaderTypeDef rxconf;
    uint8_t can_rx_buff[8];

    while (HAL_CAN_GetRxFifoFillLevel(_hcan, fifox))  // FIFO 不为空
    {
        HAL_CAN_GetRxMessage(_hcan, fifox, &rxconf, can_rx_buff); // 读一帧

        for (size_t i = 0; i < idx; ++i) {  // 遍历所有实例
            if (_hcan == can_instance[i]->can_handle       // 匹配 CAN 总线
                && rxconf.StdId == can_instance[i]->rx_id)  // 匹配接收 ID
            {
                if (can_instance[i]->can_module_callback != NULL) {
                    can_instance[i]->rx_len = rxconf.DLC;              // 保存长度
                    memcpy(can_instance[i]->rx_buff, can_rx_buff, rxconf.DLC); // 拷贝数据
                    can_instance[i]->can_module_callback(can_instance[i]);     // 触发回调
                }
                return;  // 找到匹配就退出
            }
        }
    }
}
```

- **双重匹配** — 必须同时匹配 `can_handle`（哪条 CAN 总线）和 `rx_id`（哪个设备）
- **while 循环** — FIFO 中可能积压多帧数据（其他中断占用时），一次全部读出
- **return 而非 break** — 找到匹配的实例后立即返回，因为一个 rx_id 只对应一个实例
- **static rxconf** — 避免反复分配栈空间，但要注意非重入安全

### HAL 回调重载

```c
// basic_framework/bsp/can/bsp_can.c:184-197
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CANFIFOxCallback(hcan, CAN_RX_FIFO0);
}

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CANFIFOxCallback(hcan, CAN_RX_FIFO1);
}
```

- HAL 声明这两个回调为 `__weak`，BSP 层重载它们
- FIFO0 和 FIFO1 的回调统一进入 `CANFIFOxCallback()`，通过 `fifox` 参数区分

---

## 调用链

### 发送路径

```
Module 层（如 DJIMotor）
  |-- 填充 instance->tx_buff[0..7]
  +-- CANTransmit(instance, timeout)
        +-- DWT_GetTimeline_ms()          // 超时计时起点
        +-- HAL_CAN_GetTxMailboxesFreeLevel() // 查邮箱空闲
        +-- HAL_CAN_AddTxMessage()            // 将报文投入邮箱
              +-- BxCAN 硬件自动发送到 CAN 总线
```

### 接收路径

```
CAN 总线上的报文
  +-- BxCAN 硬件根据过滤器接收
  +-- 填入 FIFO0 或 FIFO1
  +-- 触发 FIFO 消息挂起中断
  +-- HAL 中断处理 -> HAL_CAN_RxFifo0MsgPendingCallback()
        +-- CANFIFOxCallback(hcan, CAN_RX_FIFO0)
              +-- HAL_CAN_GetRxFifoFillLevel()  // FIFO 有数据？
              +-- HAL_CAN_GetRxMessage()          // 读一帧
              +-- 遍历 can_instance[] 匹配
              +-- memcpy rx_buff
              +-- can_module_callback(instance)    // 回调到 Module 层
                    +-- DJIMotorDecode()
                          +-- instance->id 获取 Motor 实例
                          +-- 解析反馈数据
```

### 注册路径

```
Module 层
  +-- CANRegister(&config)
        +-- 首次? CANServiceInit() -> HAL_CAN_Start + ActivateNotification
        +-- malloc + memset
        +-- 配置 txconf（StdId, IDE, RTR, DLC）
        +-- CANAddFilter() -> HAL_CAN_ConfigFilter
        +-- can_instance[idx++] = instance
        +-- return instance
```

---

## 注意事项

1. **CAN2 是 CAN1 的从机** — 在 STM32 BxCAN 架构中，CAN2 必须通过 CAN1 的过滤器。`SlaveStartFilterBank = 14` 指定了分界点。
2. **rx_id << 5 的原因** — 过滤器寄存器中标准 ID 占 bit[15:5]，低 5 位是 IDE/RTR 标志。这是硬件寄存器格式决定的。
3. **FIFO 均衡策略** — 按 `tx_id & 1` 分配 FIFO，这是粗略的负载均衡。理想情况应按实际流量分配。
4. **回调在中断上下文中执行** — `can_module_callback()` 在 CAN 中断中调用，回调函数必须快速返回，不能做复杂计算或阻塞操作。
5. **ID 冲突检测** — 同一 CAN 总线上两个实例的 rx_id 不能相同，否则回调只执行第一个匹配的。
6. **过滤器的数量限制** — F407 的 BxCAN 共 28 个过滤器（CAN1: 0-13, CAN2: 14-27），每注册一个实例消耗一个过滤器。
7. **单 CAN 板子的适配** — 如果只有 CAN1，需要把 `CANServiceInit()` 中的 hcan2 改为 hcan1，并调整 `DEVICE_CAN_CNT`。
8. **static rxconf 的风险** — `CANFIFOxCallback()` 中的 `rxconf` 是 static 的，如果回调被中断嵌套重入会导致数据错乱。但 CAN 中断优先级通常配置相同，实际不会发生。
