# 遥测曲线选择实施计划

用户已确认按每张图分类列出全部曲线，见对应设计文档。本会话直接实施。

**架构：** app.js 管理每图完整曲线、已选 key 与两处选择区；charts.js 统一绘制已选曲线，按 axis/单位独立缩放 Wz 右轴。保留 60 秒滚动、暂停与 Ctrl 时间缩放，不改固件。

- [x] 在 tests/selection-check.js 写失败验收：转速含 9 个复选框；取消实际、只留目标/只留 Wz、清空与全选，普通/大图同步。用固定 Wz=-800000 验证显示 -800.000 指令值且不当作 rpm，过期时为—。
- [x] app.js 中保留完整 trace 定义并增加 selected Set，生成每图及弹窗的字段选择区。更改后统一过滤 traces、同步 checkbox、图例与空选提示，即时重绘冻结快照，保持 chartRange。
- [x] charts.js 为左右轴分别取有效范围和坐标，tooltip 使用 trace.unit/digits；目标 rpm 加粗虚线。公开实际绘图区边界供 app.js 的 Ctrl 滚轮与 QA 定位使用，避免双轴时锚点错位。
- [x] index.html/styles.css 为弹窗及卡片提供图形+选择区布局，窄屏上下排列并让字段区独立滚动；更新 README。
- [x] 浏览器运行 selection-check 与既有 chart/zoom/browser-check；固定值验证目标、Wz 单位与无效值，检查桌面/375px 截图；运行 Node 测试。
- [x] 独立只读审查并处理实际问题；限定路径检查差异，只提交本次网页、测试与文档。
