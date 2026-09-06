# BSP IIC — I2C 通信封装

## 模块职责

对 STM32 HAL I2C 接口进行实例化封装，支持阻塞/中断/DMA 三种传输模式以及序列传输（HOLD_ON）和寄存器读写操作。

---

## 设计思路

### 为什么 I2C 需要多种传输模式？

- **阻塞模式** — 简单可靠，适合初始化阶段和低频访问（如 IMU 上电配置）
- **IT 模式** — 中断驱动，适合不需要 DMA 通道的场景
- **DMA 模式** — 最高效，适合大数据量传输，但占用 DMA 通道

### 为什么需要序列传输（Seq）？

I2C 协议中，一次完整通信包含 START + 地址 + 数据 + STOP。序列传输允许在一次通信中发送多段数据而不释放总线：

- `IIC_SEQ_RELEASE` — 传输完成后发送 STOP，释放总线
- `IIC_SEQ_HOLDON` — 传输完成后不发送 STOP，继续保持总线占有权

**使用场景**：连续读写多个寄存器时，HOLD_ON 避免了每次都重新发起 START 的开销。

### 地址左移 1 位

I2C 设备地址是 7 位，HAL 库要求传入 8 位地址（7 位地址 + 1 位 R/W）。框架在 `IICRegister()` 中自动将用户传入的 7 位地址左移 1 位，简化上层使用。

---

## 核心数据结构

### IIC_Work_Mode_e 枚举

```c
// basic_framework/bsp/iic/bsp_iic.h:9-14
typedef enum
{
    IIC_BLOCK_MODE = 0,  // 阻塞模式
    IIC_IT_MODE,         // 中断模式
    IIC_DMA_MODE,        // DMA 模式
} IIC_Work_Mode_e;
```

### IIC_Mem_Mode_e 枚举

```c
// basic_framework/bsp/iic/bsp_iic.h:17-22
typedef enum
{
    IIC_READ_MEM = 0,   // 读取从机寄存器/内存
    IIC_WRITE_MEM,      // 写入从机寄存器/内存
} IIC_Mem_Mode_e;
```

### IIC_Seq_Mode_e 枚举

```c
// basic_framework/bsp/iic/bsp_iic.h:25-30
typedef enum
{
    IIC_SEQ_RELEASE,    // 传输完成后释放总线（默认）
    IIC_SEQ_HOLDON,     // 保持总线占有权（仅 IT/DMA 模式支持）
} IIC_Seq_Mode_e;
```

### IICInstance

```c
// basic_framework/bsp/iic/bsp_iic.h:33-44
typedef struct iic_temp_s
{
    I2C_HandleTypeDef *handle;      // I2C 句柄
    uint8_t dev_address;            // 设备地址（已左移 1 位，包含 R/W 位）
    IIC_Work_Mode_e work_mode;      // 当前工作模式
    uint8_t *rx_buffer;             // 接收缓冲区指针（IT/DMA 接收时设置）
    uint8_t rx_len;                 // 接收长度
    void (*callback)(struct iic_temp_s *); // 接收完成回调
    void *id;                       // Parent Pointer
} IICInstance;
```

逐字段说明：

| 字段 | 作用 | 备注 |
|------|------|------|
| `handle` | I2C 句柄 | 区分 I2C1/I2C2/I2C3 |
| `dev_address` | 设备地址 | 已左移 1 位，最低位为 R/W 标志 |
| `work_mode` | 传输模式 | 可通过 `IICSetMode()` 动态切换 |
| `rx_buffer` | 接收缓冲区 | 在 `IICReceive()` 中设置，回调中使用 |
| `rx_len` | 接收长度 | 在 `IICReceive()` 中设置 |
| `callback` | 接收完成回调 | 仅 IT/DMA 模式下有用 |
| `id` | Parent Pointer | 指向拥有此实例的 Module 结构体 |

### IIC_Init_Config_s

```c
// basic_framework/bsp/iic/bsp_iic.h:47-54
typedef struct
{
    I2C_HandleTypeDef *handle;
    uint8_t dev_address;             // 7 位地址，不需要左移
    IIC_Work_Mode_e work_mode;
    void (*callback)(IICInstance *);
    void *id;
} IIC_Init_Config_s;
```

---

## 函数详解

### IICRegister()

```c
// basic_framework/bsp/iic/bsp_iic.c:8-26
IICInstance *IICRegister(IIC_Init_Config_s *conf)
{
    if (idx >= MX_IIC_SLAVE_CNT)  // 最多 8 个从机
        while (1);

    // Bug: 连续两次 malloc，第一次分配的内存泄漏
    IICInstance *instance = (IICInstance *)malloc(sizeof(IICInstance));
    instance = (IICInstance *)malloc(sizeof(IICInstance));  // 覆盖了第一次的指针
    memset(instance, 0, sizeof(IICInstance));

    instance->dev_address = conf->dev_address << 1;  // 地址左移 1 位
    instance->callback = conf->callback;
    instance->work_mode = conf->work_mode;
    instance->handle = conf->handle;
    instance->id = conf->id;

    iic_instance[idx++] = instance;
    return instance;
}
```

**Bug：双重 malloc** — 第 14-15 行连续调用了两次 `malloc()`，第一次分配的内存被第二次的返回值覆盖，导致内存泄漏。第一次 `malloc` 完全多余，应删除。

**地址左移** — `conf->dev_address << 1` 将用户传入的 7 位地址转为 HAL 要求的 8 位格式。例如 BMI088 的加速度计地址 0x18，左移后变为 0x30。

### IICSetMode()

```c
// basic_framework/bsp/iic/bsp_iic.c:28-34
void IICSetMode(IICInstance *iic, IIC_Work_Mode_e mode)
{
    if (iic->work_mode != mode)
    {
        iic->work_mode = mode;
    }
}
```

- **功能**：动态切换工作模式
- **HAL 自带重入保护** — 注释说明不需要手动等待传输完成

### IICTransmit()

```c
// basic_framework/bsp/iic/bsp_iic.c:36-67
void IICTransmit(IICInstance *iic, uint8_t *data, uint16_t size, IIC_Seq_Mode_e seq_mode)
{
    // ... 模式检查 ...
    switch (iic->work_mode)
    {
    case IIC_BLOCK_MODE:
        // 阻塞模式不支持 HOLD_ON
        HAL_I2C_Master_Transmit(iic->handle, iic->dev_address, data, size, 100);
        break;
    case IIC_IT_MODE:
        if (seq_mode == IIC_SEQ_RELEASE)
            HAL_I2C_Master_Seq_Transmit_IT(iic->handle, iic->dev_address,
                data, size, I2C_OTHER_AND_LAST_FRAME);
        else  // HOLDON
            HAL_I2C_Master_Seq_Transmit_IT(iic->handle, iic->dev_address,
                data, size, I2C_OTHER_FRAME);
        break;
    case IIC_DMA_MODE:
        // 与 IT 类似，使用 DMA 版本的 Seq 函数
        break;
    }
}
```

- **阻塞模式不支持 HOLD_ON** — 阻塞模式下函数调用返回时传输已完成，总线已释放，无法保持占有权
- **Seq 函数的帧标志** — `I2C_OTHER_AND_LAST_FRAME` 表示最后一帧（发 STOP），`I2C_OTHER_FRAME` 表示中间帧（不发 STOP）
- **超时 100ms** — 阻塞模式的硬编码超时

### IICReceive()

```c
// basic_framework/bsp/iic/bsp_iic.c:69-104
void IICReceive(IICInstance *iic, uint8_t *data, uint16_t size, IIC_Seq_Mode_e seq_mode)
{
    // 保存接收缓冲区地址和长度，供回调使用
    iic->rx_buffer = data;
    iic->rx_len = size;

    switch (iic->work_mode)
    {
    case IIC_BLOCK_MODE:
        HAL_I2C_Master_Receive(iic->handle, iic->dev_address, data, size, 100);
        break;
    case IIC_IT_MODE:
        // ... Seq Receive IT ...
        break;
    case IIC_DMA_MODE:
        // ... Seq Receive DMA ...
        break;
    }
}
```

- **关键**：在发起接收前保存了 `rx_buffer` 和 `rx_len`，这样在 IT/DMA 完成回调中可以通过实例访问接收数据
- **阻塞模式直接返回数据**，IT/DMA 模式通过回调通知

### IICAccessMem()

```c
// basic_framework/bsp/iic/bsp_iic.c:106-122
void IICAccessMem(IICInstance *iic, uint16_t mem_addr, uint8_t *data,
                   uint16_t size, IIC_Mem_Mode_e mem_mode, uint8_t mem8bit_flag)
{
    uint16_t bit_flag = mem8bit_flag ? I2C_MEMADD_SIZE_8BIT : I2C_MEMADD_SIZE_16BIT;
    if (mem_mode == IIC_WRITE_MEM)
        HAL_I2C_Mem_Write(iic->handle, iic->dev_address, mem_addr, bit_flag, data, size, 100);
    else if (mem_mode == IIC_READ_MEM)
        HAL_I2C_Mem_Read(iic->handle, iic->dev_address, mem_addr, bit_flag, data, size, 100);
}
```

- **功能**：读写从机内部寄存器/内存，仅支持阻塞模式
- **mem8bit_flag** — 寄存器地址宽度：1 表示 8 位，0 表示 16 位。不同传感器的寄存器地址宽度不同
- **典型用法**：传感器初始化时读写配置寄存器

```c
// 读取 BMI088 加速度计的 CHIP_ID 寄存器
uint8_t chip_id;
IICAccessMem(iic_inst, 0x00, &chip_id, 1, IIC_READ_MEM, 1);
```

### HAL_I2C_MasterRxCpltCallback()

```c
// basic_framework/bsp/iic/bsp_iic.c:129-141
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    for (uint8_t i = 0; i < idx; i++)
    {
        if (iic_instance[i]->handle == hi2c
            && hi2c->Devaddress == iic_instance[i]->dev_address)
        {
            if (iic_instance[i]->callback != NULL)
                iic_instance[i]->callback(iic_instance[i]);
            return;
        }
    }
}
```

- **匹配方式**：同时匹配 `handle`（哪个 I2C 外设）和 `Devaddress`（哪个从机设备）
- `hi2c->Devaddress` 是 HAL 在发起接收时自动保存的设备地址，与 `iic_instance[i]->dev_address`（左移后的 8 位地址）比较

### HAL_I2C_MemRxCpltCallback()

```c
// basic_framework/bsp/iic/bsp_iic.c:148-151
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    HAL_I2C_MasterRxCpltCallback(hi2c);  // 直接复用 Master 回调
}
```

- Mem 读取完成和 Master 接收完成的处理逻辑相同，直接复用

---

## 调用链

### 阻塞读取寄存器

```
Module 层
  +-- IICAccessMem(inst, 0x00, &data, 1, IIC_READ_MEM, 1)
        +-- HAL_I2C_Mem_Read(handle, dev_addr, mem_addr, 8BIT, &data, 1, 100)
              +-- 发送 START + 设备地址 + 寄存器地址
              +-- 重新 START + 设备地址(读) + 读取数据 + STOP
              +-- 阻塞等待完成，返回数据
```

### IT 模式接收

```
Module 层
  +-- IICReceive(inst, rx_buf, 6, IIC_SEQ_RELEASE)
        +-- 保存 rx_buffer 和 rx_len
        +-- HAL_I2C_Master_Seq_Receive_IT(handle, addr, buf, 6, LAST_FRAME)
              +-- 非阻塞，函数立即返回
              ... (硬件完成接收) ...
              +-- HAL_I2C_MasterRxCpltCallback(hi2c)
                    +-- 遍历匹配 handle + Devaddress
                    +-- callback(iic_instance)  // 回调到 Module 层
```

### 注册路径

```
Module 层
  +-- IICRegister(&conf)
        +-- malloc (x2, 有 Bug) + memset
        +-- dev_address = conf->dev_address << 1
        +-- iic_instance[idx++] = instance
        +-- return instance
```

---

## 注意事项

1. **双重 malloc Bug** — `IICRegister()` 中连续两次 `malloc()`，第一次分配的内存泄漏。应删除第一行 `malloc`。这个 Bug 不影响功能（第二次 malloc 的内存正常使用），但会造成内存泄漏。
2. **地址左移** — 用户传入 7 位地址（如 0x18），框架自动左移为 8 位（0x30）。注意头文件注释"不需要左移"，不要在传入时已经左移。
3. **阻塞模式不支持 HOLD_ON** — 尝试在阻塞模式下使用 `IIC_SEQ_HOLDON` 会导致 `while(1)` 死循环。
4. **IICAccessMem 仅阻塞** — 寄存器读写只支持阻塞模式，超时硬编码 100ms。如果传感器响应慢可能需要更长超时。
5. **通信结构体必须 pack(1)** — 用于 I2C 通信的结构体必须用 `#pragma pack(1)` 包裹，否则字节对齐会导致数据与协议不匹配。
6. **`while(1)` 错误处理** — 多处使用 `while(1)` 死循环作为错误处理，开发阶段方便调试，生产环境需改进。
7. **回调中的 Devaddress** — `hi2c->Devaddress` 是 HAL 在发起通信时保存的，回调时可以直接用来匹配。但注意这个值也是左移后的 8 位格式。
8. **HOLD_ON 的正确使用** — 最后一次传输必须使用 `IIC_SEQ_RELEASE`，否则总线占有权无法释放，后续通信会失败。
