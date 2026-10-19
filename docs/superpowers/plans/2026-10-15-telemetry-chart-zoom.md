# 放大波形 Ctrl 缩放实施计划

用户已确认设计 `docs/superpowers/specs/2026-10-15-telemetry-chart-zoom-design.md`，在当前会话直接实现。

**架构：** app.js 保存相对显示窗口末端的时间范围，charts.js 使用此范围绘图和悬停；普通卡片维持 60 秒。无新增运行依赖或固件变动。

- [x] 在 tests/zoom-check.js 中使用暂停的示例数据验证 Ctrl 滚轮事件被阻止且显示时间范围缩小；先在旧实现确认失败。测试还覆盖鼠标锚点、上下限、Ctrl + 加减号/0、普通滚轮、关闭后浏览器默认行为、实时跟随、暂停和悬停。
- [x] charts.js 的 drawChart 增加可选 `range`（相对 now 的毫秒起止值，默认 [-60000, 0]），令横轴、可见采样、悬停映射共享范围；缩放后纵轴按可见数据适配。坐标与悬停均相对 snapshot.at 标记时间。
- [x] app.js 增加弹窗独立范围、鼠标锚点缩放与 100～60000 ms 限制；在弹窗监听非 passive 的 Ctrl 滚轮和 Ctrl 加减号/0，阻止页面缩放并即时重绘当前快照，不恢复暂停。
- [x] index.html/styles.css 增加恢复全图按钮、范围提示和快捷键说明；README.md 记录使用方式。
- [x] 在隔离 Edge 中运行缩放与现有图表/串口 QA，检查页面比例、尺寸不变与悬停读数；运行相应开发测试并审查限定 diff。
- [x] 按 requesting-code-review 技能处理独立只读审查反馈，完成最终 diff 自审，仅提交本任务文件并报告提交号。
