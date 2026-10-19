# bsp_log

<p align='right'>neozng1@hnu.edu.cn</p>

## 使用说明

bsp_log支持SEGGER RTT和C板USART1两种日志出口，具体由`Inc/board_config.h`中的`UART1_USE`选择。

## C板USART1 + DAPLink Serial Monitor

`UART1_USE`有三个模式：

- `UART1_USE_REFEREE`（`0`）：USART1给裁判系统，日志只走RTT；串口保持`115200 8N1`。
- `UART1_USE_LOG`（`1`）：USART1给日志，日志通过TX DMA发送到DAPLink；裁判/UI不初始化，串口为`115200 8N1`。
- `UART1_USE_LOG_RTT`（`2`）：日志同时发送到USART1和RTT。

默认值是`UART1_USE_LOG`。也可以在GNU Make中覆盖：

```text
mingw32-make UART1_USE=1
```

硬件连接：C板`USART1_TX`（MCU `PA9`）接DAPLink UART RX，并连接GND；单向观察日志不需要连接USART1_RX。VSCode **Serial Monitor**选择DAPLink枚举出的串口，设置为`115200 baud`、`8 data bits`、`no parity`、`1 stop bit`、`no flow control`。

日志发送使用固定大小的非阻塞DMA队列，队列空间不足时整条消息丢弃，不会阻塞电机控制任务。UART日志会移除格式化日志中的ANSI颜色控制序列；原始`BSPLogWrite*`数据保持字节不变。

当前根`Makefile`保留了`DISABLE_LOG_SYSTEM`定义，因此`LOGINFO`、`LOGWARNING`和`LOGERROR`宏默认关闭；`PrintLog`、`BSPLogWrite*`以及结构化底盘功率CSV仍会按上述出口发送。需要分级日志时请移除该构建定义。

切回实车裁判模式时，将`UART1_USE`改为`UART1_USE_REFEREE`（或构建时传`UART1_USE=0`），重新下载固件并恢复裁判系统接线；日志会继续通过RTT输出。

推荐使用`bsp_log.h`中提供了三级日志：

```c
#define LOGINFO(format,...)
#define LOGWARNING(format,...)
#define LOGERROR(format,...)
```

分别用于输出不同等级的日志。注意RTT不支持直接使用`%f`进行浮点格式化,要使用`void Float2Str(char *str, float va);`转化成字符串之后再发送。

## 代码放置约定

- 普通分级日志继续使用`bsp_log.h`中的`LOG`、`LOGINFO`、`LOGWARNING`和`LOGERROR`。
- 结构化RTT数据（CSV表头、字段格式、输出缓冲区）统一放在`modules/telemetry/rtt_telemetry.c/.h`。
- 业务模块只调用上述接口，不直接调用`SEGGER_RTT_*`。

**若想启用RTT，必须通过`launch.json`的`debug-jlink`启动调试（不论使用什么调试器）。** 按照`VSCode+Ozone环境配置`完成配置之后的cmsis dap和daplink是可以支持Jlink全家桶的。

另外，若你使用的是cmsis-dap和daplink，**请在 *jlink* 调试任务启动之后再打开`log`任务。**（均在项目文件夹下的.vsocde/task.json中，有注释自行查看）。否则可能出线RTT viewer无法连接客户端的情况。

在ozone中查看log输出，直接打开console调试任务台和terminal调试中断便可看到调试输出。

> 由于ozone版本的原因，可能出现日志不换行或没有颜色。

## 自定义输出

你也可以自定义输出格式，详见Segger RTT的文档。

```c
int PrintLog(const char *fmt, ...);
void Float2Str(char *str, float va); // 输出浮点需要先用此函数进行转换
```

调用第一个函数，可以通过jlink或dap-link向调试器连接的上位机发送信息，格式和printf相同，示例如下：

```c
PrintLog("Hello World!\n");
PrintLog("Motor %d met some problem, error code %d!\n",3,1);
```

第二个函数可以将浮点类型转换成字符串以方便发送：

```c
float current_feedback=114.514;
char* str_buff[64];
Float2Str(str_buff,current_feedback);
PrintLog("Motor %d met some problem, error code %d!\n",3,1);
```
