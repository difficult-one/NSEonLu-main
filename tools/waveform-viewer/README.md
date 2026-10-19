# 本地平滑波形工具

用 Edge、Chrome 或 Firefox 打开本目录的 `index.html`。无需安装依赖、启动服务器或联网；数据只在浏览器内存中处理，刷新页面后清空。

复制给其他电脑时，请复制整个 `waveform-viewer` 目录，保持 HTML、CSS 和 JavaScript 文件在一起。

## 使用

1. 点击“打开数据文件”选择 CSV/TXT 日志，或展开“或粘贴 CSV 数据”，粘贴内容并点击“读取并绘图”。“先用示例数据试一试”可直接加载示例，目录内也提供 `example.csv`。
2. 勾选左侧字段，同时查看不同颜色的波形。默认选择第一个非横轴的数值字段。
3. 按需给某个字段开启“额外滤波”，调整窗口点数。每个字段独立设置，默认全部关闭。
4. 在图内滚轮缩放、拖动平移、悬停查看原始采样值。字段较多时，将鼠标移进悬停框并滚动查看。点击“恢复全图”复位；“显示采样点”显示点标记。
5. 可在“横轴”中选择数值字段或采样序号。图获得键盘焦点后，`+` / `-` 缩放，左右方向键平移，`Home` 恢复全图。

## 平滑与滤波的区别

- **平滑连线**使用 PCHIP 形状保持三次插值，连接采样值而不修改原始数据，不会制造超过相邻端点的插值峰值。只有两个点时使用直线，单点使用标记。
- **额外滤波**使用居中移动平均，窗口为 3～101 的奇数，默认 5 点。窗口按采样点数计算，并非毫秒；窗口越大，噪声越少，短促峰值也可能变小。片段边缘按可用点数平均。
- 调整窗口或重新开启滤波时，始终从导入的原始数据计算，不累计上一次滤波结果。悬停同时显示原始值和软件滤波值。
- 项目的 `p_filtered_cw` 标注“已有滤波”，默认直接使用设备提供的结果；只有手动勾选才会再滤波。其他字段是否已经滤波需根据数据来源判断。
- `_cw` 数据保留百分之一瓦的原始单位，例如 `4000` 表示 `40 W`；软件不会自动改成 `40`。

## 数据格式

输入为 UTF-8 编码、逗号分隔的文本，第一条非空记录为表头，字段名必须非空且唯一。例如：

```csv
tick,p_meter_cw,p_filtered_cw
0,2400,2350
10,2600,2400
20,2500,2420
```

支持 BOM、LF/CRLF 换行、带引号字段、转义引号、科学计数法及重复表头。TXT 是扩展名不同的 CSV 文本，不是任意格式的日志。没有数值的字段显示为不可选。

横轴优先使用 `tick`，其次为 `tick_ms`，均标为毫秒；没有这些数值字段时使用从 0 开始的采样序号。不会自动排序：横轴重复、倒退或无效，或纵轴有空值、NaN、Infinity、非数值时，波形和滤波都在该处断开。

列数错误或引号用法错误的记录会跳过，提示包含坏行数量和前 12 个位置；未闭合引号会拒绝整次导入。空文件、重复表头字段等导入失败时，保留之前成功导入的数据。

可以读取项目的标准 17 列遥测、26 列标定遥测和 `decode-chassis-power-capture.ps1` 导出的 CSV。原始二进制采集帧需先使用该脚本解码。

面向十万行以内日志设计。密集视图按屏幕列保留首尾和峰谷来简化绘制，放大后恢复细节；仅显示被简化，滤波和悬停读取完整采样数据。

## 开发验证

应用运行不需要 Node.js。开发时可用 Node.js 18 或更新版本，从仓库根目录运行：

```powershell
node --test tools/waveform-viewer/tests/core.test.cjs
```

浏览器操作检查使用开发工具 Playwright CLI（不属于应用运行依赖）。以仓库根目录为工作目录，临时配置 `waveform-browser.json`：

```json
{
  "browser": { "launchOptions": { "channel": "msedge", "headless": true } },
  "allowUnrestrictedFileAccess": true,
  "outputDir": "output/playwright"
}
```

```powershell
npx --yes --package @playwright/cli playwright-cli -s=waveform open file:///D:/nsezxb/NSEonLu/tools/waveform-viewer/index.html --config waveform-browser.json
npx --yes --package @playwright/cli playwright-cli -s=waveform run-code --filename tools/waveform-viewer/tests/browser-check.js
npx --yes --package @playwright/cli playwright-cli -s=waveform run-code --filename tools/waveform-viewer/tests/tooltip-check.js
npx --yes --package @playwright/cli playwright-cli -s=waveform close
```

其他目录请替换本地文件 URL。检查脚本将测试会话设为离线，再执行导入、字段开关、滤波、悬停、缩放、平移、横轴切换和导入错误处理。
