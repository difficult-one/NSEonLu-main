(function () {
  'use strict';
  const P = window.TelemetryProtocol;
  const $ = id => document.getElementById(id);
  const store = new P.TelemetryStore();
  const view = new P.DisplayState(store);
  let mode = 'idle', demoTimer = null, demoSequence = 0, lastRender = -Infinity, lastRendered = null, hadError = false;
  let tuningUI = null;
  let autotuneUI = null;
  const autotuneClient = new window.TelemetryAutotune.AutotuneClient({send: line => session.send(line)});
  const tuningClient = new window.TelemetryTuning.TuningClient({send: line => session.send(line)});
  const clock = () => performance.now();
  const setText = (id, text) => {if ($(id).textContent !== String(text)) $(id).textContent = text;};
  const format = (value, digits = 2) => value === null || value === undefined || !Number.isFinite(value) ? '—' : value.toFixed(digits);
  const ageText = value => value === null || value === undefined || value >= P.UINT_MAX ? '—' : String(Math.round(value));
  function message(text, error = false) {setText('message', text); $('message').classList.toggle('error', error);}
  const parser = new P.StreamParser({onSample(sample) {store.add(sample, clock());}, onControl: line => {if(!autotuneClient.receive(line)) tuningClient.receive(line);}, onError(kind) {
    if (kind === 'version') message('收到不支持的协议版本。请使用与此仪表盘匹配的 CSV v1 遥测固件。', true);
  }});
  const session = new window.TelemetrySerial.SerialSession({serial: navigator.serial, onChunk: chunk => parser.push(chunk), onState(state) {
    tuningUI?.setConnected(state === 'connected');
    autotuneUI?.setConnected(state === 'connected');
    if (state === 'connected') {hadError = false; message('串口已打开，等待 CSV v1 遥测。若持续无数据，请确认固件启用了 USB 遥测模式。');}
    if (state === 'idle' && mode === 'real' && !hadError) message(store.latest ? '串口已断开，最后采集的历史数据仍可导出。点击“连接 C 板”可重新采集。' : '未连接。请确认 C 板已连接，点击“连接 C 板”选择串口。');
    renderToolbar(clock());
  }, onError(error) {
    hadError = true;
    const hints = {NetworkError: '串口读取中断或设备被拔出，请检查 USB 连接后重连。', InvalidStateError: '串口可能正被其他程序占用，请关闭串口助手或调试器后重试。', SecurityError: '浏览器拒绝串口访问。请使用 Chrome / Edge，并允许此 localhost 页面访问串口。'};
    message(hints[error.name] || `无法连接或读取串口：${error.message}`, true);
  }});
  tuningUI = window.createTuningUI(tuningClient);
  autotuneUI = window.createAutotuneUI({client:autotuneClient,tuningClient,lockManual:value=>tuningUI.setLocked(value),getSamples:()=>store.samples});
  const workspaceNames=['monitor','tuning','autotune'];
  function showPanel(key) {
    for (const name of workspaceNames) {
      const selected = name === key;
      $(name+'-panel').hidden = !selected;
      $(name+'-tab').setAttribute('aria-selected',String(selected));
      $(name+'-tab').tabIndex = selected ? 0 : -1;
    }
    if (chartDialog.open) chartDialog.close();
    lastRendered = null;
  }
  for (const name of workspaceNames) {
    $(name+'-tab').addEventListener('click', () => showPanel(name));
    $(name+'-tab').addEventListener('keydown', event => {
      if (['ArrowLeft','ArrowRight','Home','End'].includes(event.key)) {
        event.preventDefault();
        const next = event.key==='Home' ? workspaceNames[0] : event.key==='End' ? workspaceNames.at(-1) : workspaceNames[(workspaceNames.indexOf(name)+(event.key==='ArrowRight'?1:workspaceNames.length-1))%workspaceNames.length];
        showPanel(next);$(next+'-tab').focus();
      }
    });
  }
  const colors = {lf: '#3478f6', rf: '#16a69b', lb: '#e3a234', rb: '#8a65d6'};
  const wheelNames = {lf: '左前 LF', rf: '右前 RF', lb: '左后 LB', rb: '右后 RB'};
  const powerTraces = [{key: 'p_unlimited_cw', name: '模型未限流', color: '#b7c1d1'}, {key: 'effective_limit_cw', name: '最终限制', color: '#8a65d6', dash: true}, {key: 'target_power_cw', name: '目标功率', color: '#e3a234', dash: true}, {key: 'p_limited_cw', name: '模型限流后', color: '#16a69b'}, {key: 'p_meter_cw', name: '功率计实测', color: '#3478f6'}];
  const energyTraces = [{key: 'buffer_j', name: '裁判原始值', color: '#16a69b'}, {key: 'buffer_lower_cj', name: '保守下限', color: '#e3a234'}, {key: 'buffer_predicted_cj', name: '预测能量', color: '#8a65d6', dash: true}];
  const rpmTraces = Object.keys(colors).flatMap(key => [{key: `rpm_${key}`, name: `${wheelNames[key]} · 实际`, color: colors[key]}, {key: `target_${key}_rpm`, name: `${wheelNames[key]} · 目标`, color: colors[key], dash: true, width: 2, dashPattern: [7, 5]}]);
  rpmTraces.push({key: 'wz_cmd_milli', name: '底盘旋转目标 Wz', color: '#df6f7b', axis: 'right', unit: '指令值', digits: 3, dash: true, width: 2});
  const charts = [{id: 'power-chart', title: '功率观测', unit: 'W', traces: powerTraces}, {id: 'energy-chart', title: '缓冲能量', unit: 'J', traces: energyTraces}, {id: 'rpm-chart', title: '四轮转速', unit: 'rpm', rightUnit: 'Wz 指令', traces: rpmTraces}];
  charts.forEach(chart => {
    chart.allTraces = chart.traces;chart.selected = new Set(chart.traces.map(trace => trace.key));
    chart.traces.forEach(trace => {trace.unit ??= chart.unit;trace.digits ??= chart.unit === 'rpm' ? 0 : 2;});
  });
  const chartDialog = $('chart-dialog');
  const expandedCanvas = $('expanded-chart');
  let expandedChart = null, chartOpener = null, chartRange = [-60000, 0], pan = null;
  function earliestStart(snapshot, duration) {return Math.min(-duration, (snapshot.samples[0]?.arrivalMs ?? snapshot.at) - snapshot.at);}
  function setChartStart(start, duration = chartRange[1] - chartRange[0]) {
    const snapshot = view.snapshot(clock());
    start = Math.max(earliestStart(snapshot, duration), Math.min(-duration, start));
    chartRange = [start, start + duration];renderExpandedChart(snapshot);
  }
  function stopPan() {
    const pointerId = pan?.pointerId;pan = null;expandedCanvas.classList.remove('panning');
    if (pointerId !== undefined && expandedCanvas.hasPointerCapture(pointerId)) expandedCanvas.releasePointerCapture(pointerId);
  }
  function pauseDisplay() {if (!view.paused) {const now = clock();view.pause(now);lastRendered = null;renderToolbar(now);}}
  function resumeDisplay() {
    stopPan();view.resume();chartRange = [-60000, 0];charts.forEach(chart => {chart.historyRange = null;});
    lastRendered = null;renderToolbar(clock());renderExpandedChart(view.snapshot(clock()));
  }
  function drawChartView(canvas, chart, snapshot, range) {
    window.TelemetryCharts.drawChart(canvas, snapshot.samples, chart.traces, {now: snapshot.at, unit: chart.unit, rightUnit: chart.rightUnit, range});
    const empty = canvas.parentElement.querySelector('[data-empty-chart]');
    empty.hidden = Boolean(snapshot.latest) && chart.traces.length > 0;
    const heading = empty.querySelector('strong');
    heading.dataset.originalText ??= heading.textContent;
    heading.textContent = chart.traces.length ? heading.dataset.originalText : '请选择要显示的数据';
    empty.querySelectorAll('span:not(.empty-wave)').forEach(span => {span.hidden = !chart.traces.length;});
  }
  function renderLegend(element, chart) {
    const expandButton = element.querySelector('.chart-expand-button');
    element.replaceChildren(...chart.traces.map(trace => {
      const label = document.createElement('span'), dot = document.createElement('i');
      dot.className = trace.dash ? 'line-dot' : 'color-dot';
      if (trace.dash) dot.style.borderColor = trace.color;else dot.style.background = trace.color;
      label.append(dot, document.createTextNode(trace.name));return label;
    }));
    if (expandButton) element.append(expandButton);
  }
  function selectTraces(chart) {
    chart.traces = chart.allTraces.filter(trace => chart.selected.has(trace.key));
    document.querySelectorAll(`[data-chart="${chart.id}"] input[data-trace-key]`).forEach(input => {input.checked = chart.selected.has(input.dataset.traceKey);});
    lastRendered = null;
    const snapshot = view.snapshot(clock());
    renderLegend(chart.legend, chart);
    drawChartView($(chart.id), chart, snapshot);
    if (expandedChart === chart) {renderLegend($('expanded-chart-legend'), chart);renderExpandedChart(snapshot);}
  }
  function createTracePicker(chart) {
    const picker = document.createElement('fieldset');picker.className = 'trace-picker';picker.dataset.chart = chart.id;picker.setAttribute('aria-label', `${chart.title}数据选择`);
    const legend = document.createElement('legend');legend.textContent = '显示数据';
    const actions = document.createElement('div');actions.className = 'trace-picker-actions';
    for (const [label, select] of [['全选', true], ['清空', false]]) {
      const button = document.createElement('button');button.type = 'button';button.textContent = label;
      button.addEventListener('click', () => {chart.selected = new Set(select ? chart.allTraces.map(trace => trace.key) : []);selectTraces(chart);});actions.append(button);
    }
    const options = document.createElement('div');options.className = 'trace-options';
    chart.allTraces.forEach(trace => {
      const label = document.createElement('label'), input = document.createElement('input'), line = document.createElement('i'), name = document.createElement('span'), unit = document.createElement('small');
      input.type = 'checkbox';input.dataset.traceKey = trace.key;input.checked = chart.selected.has(trace.key);
      line.className = 'trace-line';line.style.borderColor = trace.color;if (trace.dash) line.style.borderTopStyle = 'dashed';
      name.textContent = trace.name;unit.textContent = trace.unit;
      input.addEventListener('change', () => {if (input.checked) chart.selected.add(trace.key);else chart.selected.delete(trace.key);selectTraces(chart);});
      label.append(input, line, name, unit);options.append(label);
    });
    picker.addEventListener('click', event => event.stopPropagation());
    picker.append(legend, actions, options);return picker;
  }
  function renderExpandedChart(snapshot) {
    if (!expandedChart || !chartDialog.open) return;
    if (view.paused) expandedChart.historyRange = chartRange.slice();
    setText('expanded-chart-unit', [...new Set(expandedChart.traces.map(trace => trace.unit))].join(' / ') || expandedChart.unit);
    drawChartView(expandedCanvas, expandedChart, snapshot, chartRange);
    setText('expanded-chart-range', `显示 ${Number(((chartRange[1] - chartRange[0]) / 1000).toFixed(3))} s · Ctrl + 滚轮缩放`);
    $('reset-chart-button').disabled = chartRange[1] - chartRange[0] === 60000 && chartRange[1] === 0;
    const duration = chartRange[1] - chartRange[0], minimum = earliestStart(snapshot, duration), position = $('history-position');
    position.min = String(Math.floor(minimum));position.max = String(Math.ceil(-duration));position.value = String(chartRange[0]);
    position.disabled = !snapshot.latest || minimum >= -duration;
    setText('history-mode', view.paused ? '历史回看 · 已暂停' : '实时跟随');
    setText('history-readout', `末端前 ${(-chartRange[0] / 1000).toFixed(2)} s ～ ${(-chartRange[1] / 1000).toFixed(2)} s`);
    $('return-live-button').disabled = !store.latest || !view.paused && chartRange[0] === -60000 && chartRange[1] === 0;
  }
  function zoomChart(factor, fraction = .5) {
    stopPan();
    const duration = chartRange[1] - chartRange[0], next = Math.max(100, Math.min(60000, duration * factor));
    const anchor = chartRange[0] + fraction * duration;
    setChartStart(anchor - fraction * next, next);
  }
  function resetChartZoom() {stopPan();chartRange = [-60000, 0];renderExpandedChart(view.snapshot(clock()));}
  function openChart(chart, opener) {
    if (chartDialog.open) return;
    expandedChart = chart;chartOpener = opener;chartRange = view.paused && chart.historyRange ? chart.historyRange.slice() : [-60000, 0];
    setText('expanded-chart-title', chart.title);
    setText('expanded-chart-unit', chart.unit);
    renderLegend($('expanded-chart-legend'), chart);
    $('expanded-trace-picker').replaceChildren(createTracePicker(chart));
    chartDialog.showModal();lastRendered = null;renderExpandedChart(view.snapshot(clock()));
  }
  charts.forEach(chart => {
    const canvas = $(chart.id), card = canvas.closest('.chart-card') || canvas.closest('.motor-plot');
    chart.legend = card.querySelector('.chart-legend');renderLegend(chart.legend, chart);
    const layout = document.createElement('div');layout.className = 'chart-layout';
    const plot = canvas.parentElement;plot.before(layout);layout.append(plot, createTracePicker(chart));
    const button = document.createElement('button');button.type = 'button';button.className = 'chart-expand-button';button.textContent = '放大 ↗';
    button.setAttribute('aria-label', `放大${chart.title}`);button.setAttribute('aria-haspopup', 'dialog');button.setAttribute('aria-controls', 'chart-dialog');
    (card.querySelector('.section-heading') || card.querySelector('.chart-legend')).append(button);
    card.classList.add('expandable-chart');
    card.addEventListener('click', () => openChart(chart, button));
    canvas.tabIndex = 0;canvas.setAttribute('aria-description', '悬停查看采样值，点击或按 Enter 放大');
    canvas.addEventListener('keydown', event => {if (event.key === 'Enter' || event.key === ' ') {event.preventDefault();openChart(chart, canvas);}});
  });
  $('close-chart-button').addEventListener('click', () => chartDialog.close());
  $('reset-chart-button').addEventListener('click', resetChartZoom);
  $('return-live-button').addEventListener('click', resumeDisplay);
  $('history-position').addEventListener('pointerdown', pauseDisplay);
  $('history-position').addEventListener('input', event => {pauseDisplay();setChartStart(Number(event.currentTarget.value));});
  expandedCanvas.addEventListener('pointerdown', event => {
    if (event.button !== 0 || !store.latest) return;
    const rect = expandedCanvas.getBoundingClientRect(), bounds = window.TelemetryCharts.getPlotBounds(expandedCanvas);
    if (!bounds || event.clientX < rect.left + bounds.left || event.clientX > rect.left + bounds.left + bounds.width || event.clientY < rect.top + bounds.top || event.clientY > rect.top + bounds.top + bounds.height) return;
    event.preventDefault();
    pan = {pointerId: event.pointerId, x: event.clientX, width: bounds.width, range: chartRange.slice(), active: false};
    expandedCanvas.setPointerCapture(event.pointerId);
  });
  expandedCanvas.addEventListener('pointermove', event => {
    if (!pan || event.pointerId !== pan.pointerId) return;
    const delta = event.clientX - pan.x;
    if (!pan.active && Math.abs(delta) < 4) return;
    if (!pan.active) {pan.active = true;pauseDisplay();expandedCanvas.classList.add('panning');}
    const duration = pan.range[1] - pan.range[0];
    setChartStart(pan.range[0] - delta / pan.width * duration, duration);
  });
  for (const event of ['pointerup', 'pointercancel', 'lostpointercapture']) expandedCanvas.addEventListener(event, stopPan);
  chartDialog.addEventListener('wheel', event => {
    if (!chartDialog.open || !event.ctrlKey) return;
    event.preventDefault();
    if (event.target !== expandedCanvas || !event.deltaY) return;
    const rect = expandedCanvas.getBoundingClientRect(), bounds = window.TelemetryCharts.getPlotBounds(expandedCanvas), x = event.clientX - rect.left, y = event.clientY - rect.top;
    if (!bounds || x < bounds.left || x > bounds.left + bounds.width || y < bounds.top || y > bounds.top + bounds.height) return;
    const delta = event.deltaY * (event.deltaMode === 1 ? 16 : event.deltaMode === 2 ? rect.height : 1);
    zoomChart(Math.exp(Math.max(-1, Math.min(1, delta * .002))), (x - bounds.left) / bounds.width);
  }, {passive: false});
  chartDialog.addEventListener('keydown', event => {
    if (!chartDialog.open || !event.ctrlKey || event.altKey || !['+', '=', '-', '0'].includes(event.key)) return;
    event.preventDefault();
    if (event.key === '0') resetChartZoom();else zoomChart(event.key === '-' ? 1.25 : .8);
  });
  let backdropPress = false;
  const outsideDialog = event => {const rect = chartDialog.getBoundingClientRect();return event.clientX < rect.left || event.clientX > rect.right || event.clientY < rect.top || event.clientY > rect.bottom;};
  chartDialog.addEventListener('pointerdown', event => {backdropPress = outsideDialog(event);});
  chartDialog.addEventListener('click', event => {if (backdropPress && outsideDialog(event)) chartDialog.close();backdropPress = false;});
  chartDialog.addEventListener('close', () => {stopPan();expandedChart = null;lastRendered = null;if (chartOpener) chartOpener.focus({preventScroll: true});});
  const keys = ['W', 'S', 'D', 'A', 'SHIFT', 'CTRL', 'Q', 'E', 'R', 'F', 'G', 'Z', 'X', 'C', 'V', 'B'];
  keys.forEach(key => {const element = document.createElement('span'); element.textContent = key; $('keyboard').append(element);});
  const sources = [['remote', '遥控器', 'remote'], ['meter', '功率计', 'meter'], ['model', '电机功控模型', 'model'], ['referee', '裁判与缓冲', 'buffer'], ['cap', '超级电容', 'cap'], ['capInput', '电容输入反馈', 'capInput']];
  sources.forEach(([key, label]) => {
    const row = document.createElement('div'); row.id = `source-${key}`; row.className = 'source-row';
    const name = document.createElement('span'); name.className = 'source-name'; name.append(document.createElement('i'), document.createTextNode(label));
    const info = document.createElement('span'); info.className = 'source-info';
    const status = document.createElement('b'); status.textContent = '等待';
    const age = document.createElement('small'); age.textContent = '— ms';
    info.append(status, age); row.append(name, info); $('source-list').append(row);
  });
  Object.keys(wheelNames).forEach(key => {
    const row = document.createElement('tr');row.id = `motor-${key}`;
    row.innerHTML = `<td><span class="motor-name"><i class="color-dot" style="background:${colors[key]}"></i>${wheelNames[key]}</span><span class="motor-state">等待数据</span></td><td>—</td><td>—</td><td>—</td><td>—</td>`;
    $('motor-table-body').append(row);
  });

  function resetData() {stopPan();store.clear();parser.reset();view.resume();chartRange = [-60000, 0];charts.forEach(chart => {chart.historyRange = null;});lastRendered = null;hadError = false;}
  function stopDemo() {if (demoTimer !== null) {clearInterval(demoTimer); demoTimer = null;} renderToolbar(clock());}
  function makeDemoSample(sequence) {
    const t = sequence / 50, wave = Math.sin(t * .8), row = Object.fromEntries(P.FIELDS.map(key => [key, 0]));
    Object.assign(row, {version: 1, seq: sequence, tick: sequence * 20, chassis_valid: 1, chassis_tick_ms: sequence * 20, chassis_mode: 2, vx_cmd_milli: Math.round(Math.sin(t * .42) * 2400000), vy_cmd_milli: Math.round(Math.cos(t * .27) * 1300000), wz_cmd_milli: Math.round(Math.sin(t * .3) * 120000), rc_online: 1, rc_lx: Math.round(Math.sin(t * .42) * 480), rc_ly: Math.round(Math.cos(t * .27) * 300), rc_rx: Math.round(Math.sin(t * .3) * 240), rc_ry: Math.round(Math.cos(t * .7) * 400), rc_dial: Math.round(Math.sin(t * .2) * 500), rc_sw_left: 3, rc_sw_right: 1, mouse_x: Math.round(Math.sin(t) * 12), mouse_y: Math.round(Math.cos(t * .8) * 8), mouse_left: Math.floor(t / 2) % 2, keys: Math.floor(t / 3) % 2 ? 9 : 5, model_tick_ms: sequence * 20, motor_online_mask: 15, motor_enabled_mask: 15, power_valid: 1, p_unlimited_cw: Math.round(6700 + wave * 1800), p_limited_cw: Math.round(5400 + wave * 1400), effective_limit_cw: 8000, p_meter_cw: Math.round(5400 + wave * 1400 + Math.sin(t * 7) * 180), meter_online: 1, nominal_budget_cw: 8000, target_power_cw: 7000, power_source: 1, referee_online: 1, referee_limit_cw: 8000, buffer_j: Math.round(42 + Math.sin(t * .25) * 10), buffer_control_enabled: 1, buffer_estimate_valid: 1, buffer_lower_cj: Math.round(3700 + Math.sin(t * .25) * 900), buffer_predicted_cj: Math.round(4100 + Math.sin(t * .25 + .2) * 950), buffer_recovering: 0, cap_online: 1, cap_output_enabled: 1, cap_energy_pm: Math.round(720 + Math.sin(t * .08) * 80), cap_chassis_cw: Math.round(5400 + wave * 1400), cap_limit_cw: 12000, cap_input_online: 1, cap_input_cw: 8000});
    Object.keys(colors).forEach((key, i) => {const target = Math.sin(t * .5 + i * .55) * 2500 + Math.cos(t * .3) * 1200; row[`target_${key}_rpm`] = Math.round(target); row[`rpm_${key}`] = Math.round(target - Math.cos(t * .5 + i * .55) * 65); row[`i_${key}_cmd`] = Math.round(Math.cos(t * .5 + i * .55) * 2800); row[`i_${key}_fb`] = row[`i_${key}_cmd`] + Math.round(Math.sin(t * 3 + i) * 90);});
    return P.FIELDS.map(key => row[key]).join(',');
  }
  function startDemo() {
    if (session.state !== 'idle') return;
    stopDemo(); resetData(); mode = 'demo'; demoSequence = 0;
    parser.push(P.HEADER + '\r\n');
    // Synthetic history fills the visible window without affecting real sessions.
    const start = clock() - P.HISTORY_MS;
    let result;
    const demoParser = new P.StreamParser({onSample: sample => {result = sample;}});
    demoParser.push(P.HEADER + '\n');
    for (let index = 0; index < P.MAX_SAMPLES; index++) {
      demoParser.push(makeDemoSample(demoSequence++) + '\n');
      store.add(result, start + index * 20);
    }
    // Keep the generated history adjacent to the first live demo frame.
    const offset = clock() - (start + P.HISTORY_MS);
    store.samples.forEach(sample => {sample.arrivalMs += offset;});
    demoTimer = setInterval(() => {parser.push(makeDemoSample(demoSequence++) + '\r\n');}, 20);
    message('演示数据由本机生成，仅用于预览。连接真实 C 板时将自动结束演示并清空记录。');
    renderToolbar(clock());
  }
  $('connect-button').addEventListener('click', () => {
    if (session.state === 'connected') {session.disconnect(); return;}
    if (session.state !== 'idle') return;
    stopDemo();resetData();mode = 'real';
    // Keep requestPort within this user gesture; no await precedes connect().
    session.connect();
  });
  $('demo-button').addEventListener('click', () => {if (demoTimer !== null) stopDemo();else startDemo();});
  $('stop-demo-button').addEventListener('click', stopDemo);
  $('pause-button').addEventListener('click', () => {if (view.paused) resumeDisplay();else pauseDisplay();});
  $('export-button').addEventListener('click', () => {
    if (!store.samples.length) return;
    const url = URL.createObjectURL(new Blob([store.exportCSV()], {type: 'text/csv;charset=utf-8'}));
    const link = document.createElement('a');link.href = url;link.download = `c-board-${mode === 'demo' ? 'DEMO-' : ''}${new Date().toISOString().replace(/[:.]/g, '-')}.csv`;
    document.body.append(link);link.click();link.remove();setTimeout(() => URL.revokeObjectURL(url), 1000);
  });

  function renderToolbar(now) {
    const connected = session.state === 'connected', busy = session.state === 'connecting' || session.state === 'disconnecting', stale = store.isStale(now);
    $('connect-button').disabled = busy;
    $('connect-button').querySelector('span').textContent = session.state === 'connecting' ? '选择 / 打开中' : session.state === 'disconnecting' ? '正在断开' : connected ? '断开连接' : '连接 C 板';
    $('demo-button').disabled = session.state !== 'idle';
    setText('demo-button', demoTimer === null ? '演示数据' : '停止演示');
    $('demo-notice').hidden = mode !== 'demo';
    $('connection-dot').className = 'connection-dot' + (mode === 'demo' ? ' demo' : connected ? stale && store.latest ? ' stale' : ' connected' : '');
    setText('connection-status', mode === 'demo' ? demoTimer === null ? '演示已结束 · 保留模拟记录' : '演示模式 · 模拟数据' : connected ? stale && store.latest ? 'C 板已连接 · 数据陈旧' : 'C 板已连接' : busy ? '正在连接 C 板' : '尚未连接 C 板');
    setText('connection-detail', mode === 'demo' ? 'SIMULATION · 非真实设备数据' : 'USB CDC 虚拟串口 · 50 Hz 遥测');
    $('pause-button').disabled = !store.latest;
    setText('pause-button', view.paused ? '恢复显示' : '暂停显示');
    $('pause-button').classList.toggle('active', view.paused);
    $('pause-button').setAttribute('aria-pressed', String(view.paused));
    $('export-button').disabled = !store.samples.length;
    setText('sample-age', store.latest ? Math.max(0, Math.round(now - store.latest.arrivalMs)) : '—');
    setText('sample-count', store.samples.length);
    setText('missing-count', store.stats.missing);
    setText('bad-count', parser.stats.bad + parser.stats.unsupported);
    setText('tx-drop-count', store.latest ? store.latest.tx_drop_count : '—');
    const recent = store.samples.filter(sample => now - sample.arrivalMs <= 1000);
    const frequency = recent.length > 1 ? (recent.length - 1) * 1000 / (recent.at(-1).arrivalMs - recent[0].arrivalMs) : null;
    setText('rx-rate', !stale && frequency !== null && Number.isFinite(frequency) ? frequency.toFixed(1) : '—');
    setText('display-status', view.paused ? '显示已暂停 · 采集继续' : !store.latest ? '等待数据' : stale ? '超过 500 ms 无新数据' : '● 实时更新');
    $('display-status').className = 'display-status' + (view.paused || stale && store.latest ? ' warning' : store.latest ? ' live' : '');
  }
  function pill(id, label, state) {setText(id, label);$(id).className = `status-pill ${state}`;}
  function renderValues(snapshot) {
    const sample = snapshot.latest, stale = !sample || snapshot.at - sample.arrivalMs > 500;
    const lag = sample ? Math.max(0, Math.round(snapshot.at - sample.arrivalMs)) : 0;
    const normalized = P.normalizeForDisplay(sample, snapshot.at);
    const v = normalized ? normalized.values : {}, valid = normalized ? normalized.valid : {};
    const ages = normalized ? Object.fromEntries(Object.entries(normalized.ages).map(([key, age]) => [key, age === P.UINT_MAX ? P.UINT_MAX : Math.min(P.UINT_MAX, age + lag)])) : {};
    setText('metric-meter', format(v.p_meter_cw));
    setText('metric-limit', format(v.effective_limit_cw));
    setText('metric-buffer', format(v.buffer_j, 0));
    setText('metric-estimate', format(v.buffer_lower_cj));
    setText('metric-cap', format(v.cap_energy_pm));
    setText('metric-meter-detail', valid.meter ? `功率计样本 ${ageText(ages.meter)} ms` : sample ? '功率计离线或数据过期' : '等待功率计数据');
    setText('metric-limit-detail', `名义预算 ${format(v.nominal_budget_cw)} W`);
    setText('metric-buffer-detail', valid.referee ? `裁判功率上限 ${format(v.referee_limit_cw)} W` : sample ? '裁判离线或数据过期' : '等待裁判数据');
    setText('metric-estimate-detail', `预测能量 ${format(v.buffer_predicted_cj)} J`);
    setText('metric-cap-detail', valid.cap ? `电容输出${v.cap_output_enabled ? '已使能' : '已关闭'}` : sample ? '电容离线或数据过期' : '等待电容数据');
    $('cap-progress-value').style.width = `${Math.max(0, Math.min(100, v.cap_energy_pm || 0))}%`;
    pill('buffer-state', !normalized ? '等待数据' : !valid.bufferEstimate ? '估算无效' : v.buffer_recovering ? '恢复能量中' : v.buffer_control_enabled ? '缓冲控制已启用' : '缓冲控制未启用', valid.bufferEstimate ? v.buffer_recovering ? 'warning' : 'good' : 'neutral');
    pill('remote-status', !sample ? '等待数据' : valid.remote ? '在线' : '离线 / 过期', valid.remote ? 'good' : sample ? 'bad' : 'neutral');
    for (const [prefix, xkey, ykey] of [['left', 'rc_lx', 'rc_ly'], ['right', 'rc_rx', 'rc_ry']]) {
      const point = $(`${prefix}-stick-point`);
      point.style.left = `${50 + (valid.remote ? Math.max(-660, Math.min(660, v[xkey])) / 660 * 42 : 0)}%`;
      point.style.top = `${50 - (valid.remote ? Math.max(-660, Math.min(660, v[ykey])) / 660 * 42 : 0)}%`;
      point.style.opacity = valid.remote ? '1' : '.25';
    }
    for (const key of ['lx', 'ly', 'rx', 'ry', 'dial']) setText(`rc-${key}`, format(v[`rc_${key}`], 0));
    const switchNames = {1: '上档', 3: '中档', 2: '下档'};
    setText('rc-sw-left', switchNames[v.rc_sw_left] || '—');setText('rc-sw-right', switchNames[v.rc_sw_right] || '—');
    $('dial-indicator').style.left = `${50 + (valid.remote ? Math.max(-660, Math.min(660, v.rc_dial)) / 660 * 47 : 0)}%`;
    $('dial-indicator').style.opacity = valid.remote ? '1' : '.3';
    [...$('keyboard').children].forEach((element, i) => {const pressed = Boolean(valid.remote && (v.keys & (1 << i)));element.classList.toggle('pressed', pressed);element.setAttribute('aria-label', `${keys[i]} ${pressed ? '按下' : valid.remote ? '松开' : '无数据'}`);});
    setText('mouse-x', format(v.mouse_x, 0));setText('mouse-y', format(v.mouse_y, 0));
    for (const key of ['left', 'right']) {const pressed = Boolean(valid.remote && v[`mouse_${key}`]);$(`mouse-${key}`).classList.toggle('pressed', pressed);$(`mouse-${key}`).setAttribute('aria-label', `鼠标${key === 'left' ? '左' : '右'}键 ${pressed ? '按下' : valid.remote ? '松开' : '无数据'}`);}
    setText('rc-age', `遥控器样本年龄 ${ageText(ages.remote)} ms`);
    for (const [key, , ageKey] of sources) {
      const row = $(`source-${key}`);row.className = `source-row ${valid[key] && !(key === 'cap' && v.cap_error) ? 'good' : sample ? 'bad' : ''}`;
      row.querySelector('b').textContent = valid[key] ? key === 'cap' && v.cap_error ? `故障 ${v.cap_error}` : '在线' : sample ? stale ? '陈旧' : '离线 / 过期' : '等待';
      row.querySelector('small').textContent = `${ageText(ages[ageKey])} ms`;
    }
    setText('cap-output', v.cap_output_enabled === null || v.cap_output_enabled === undefined ? '—' : v.cap_output_enabled ? '已使能' : '已关闭');
    setText('cap-error', format(v.cap_error, 0));setText('cap-chassis', format(v.cap_chassis_cw));setText('cap-limit', format(v.cap_limit_cw));setText('cap-input', format(v.cap_input_cw));
    setText('power-source', v.power_source === 1 ? '超级电容' : v.power_source === 0 ? '直供' : '—');
    const modes = ['零力模式', '小陀螺', '不跟随云台', '跟随云台'];
    pill('chassis-mode', valid.chassis ? modes[v.chassis_mode] || '未知模式' : sample ? '底盘数据无效 / 过期' : '等待底盘数据', valid.chassis ? 'good' : 'neutral');
    setText('vx-command', format(v.vx_cmd_milli, 3));setText('vy-command', format(v.vy_cmd_milli, 3));setText('wz-command', format(v.wz_cmd_milli, 3));
    for (let index = 0; index < 4; index++) {
      const key = Object.keys(colors)[index], motor = normalized ? normalized.motors[index] : null, row = $(`motor-${key}`);
      const state = row.querySelector('.motor-state');state.textContent = motor && motor.online ? motor.enabled ? '在线 · 已使能' : '在线 · 未使能' : sample ? '离线 / 过期' : '等待数据';state.className = `motor-state${motor && motor.online ? ' good' : ''}`;
      const cells = row.querySelectorAll('td');
      for (const [cell, property] of [[1, 'target'], [2, 'rpm'], [3, 'currentCommand'], [4, 'currentFeedback']]) cells[cell].textContent = format(motor ? motor[property] : null, 0);
    }
  }
  function render(now) {
    if (now - lastRender >= 50) {
      lastRender = now;renderToolbar(now);
      autotuneUI.renderChart();
      const snapshot = view.snapshot(now);
      if (!view.paused || snapshot !== lastRendered) {
        renderValues(snapshot);
        if (!$('monitor-panel').hidden) charts.forEach(chart => drawChartView($(chart.id), chart, snapshot));
        renderExpandedChart(snapshot);
        lastRendered = snapshot;
      }
    }
    requestAnimationFrame(render);
  }
  window.addEventListener('resize', () => {stopPan();lastRendered = null;});
  window.addEventListener('pagehide', () => {autotuneUI.stop();stopDemo();session.disconnect();});
  window.telemetryApp = {store, parser, session, view, get mode() {return mode;}, get paused() {return view.paused;}};
  if (!('serial' in navigator)) message('此浏览器不支持 Web Serial。请用桌面版 Chrome / Edge 打开 localhost 页面；演示数据仍可使用。');
  renderToolbar(clock());requestAnimationFrame(render);
})();
