(function () {
  'use strict';
  const C = window.WaveformCore;
  const $ = id => document.getElementById(id);
  const colors = ['#386fec', '#13a28b', '#d78b26', '#8e64d7', '#d15b77', '#41a4c6', '#7b922e', '#986350'];
  let data = null, x = null, xIndex = -1, settings = [], sourceName = '';
  const plot = new window.WaveformPlot($('waveform-chart'), $('chart-overlay'), $('tooltip'), $('chart-empty'), text => { $('visible-range').textContent = text; });
  const element = (tag, className, text) => {
    const node = document.createElement(tag); if (className) node.className = className; if (text !== undefined) node.textContent = text; return node;
  };

  function updatePlot(reset = false) {
    if (!data) return;
    const series = [];
    $('legend').replaceChildren();
    data.columns.forEach((column, index) => {
      const setting = settings[index];
      if (!setting.selected || !column.numeric) return;
      if (!setting.y || setting.lastX !== x) {
        setting.y = C.prepareSeries(x, column.values, setting);
        setting.segments = C.splitSegments(x, setting.y); setting.lastX = x;
      }
      const color = colors[index % colors.length];
      series.push({ name: column.name, color, raw: column.values, y: setting.y, segments: setting.segments, filtered: setting.enabled, lines: data.lines });
      const item = element('span', 'legend-item');
      const line = element('span', 'legend-line'); line.style.background = color;
      item.append(line, element('span', '', column.name));
      if (setting.enabled) item.append(element('span', 'legend-mode', `均值 ${setting.window} 点`));
      $('legend').append(item);
    });
    if (!series.length) $('legend').append(element('span', 'legend-placeholder', '勾选字段以显示曲线'));
    const label = xIndex === -1 ? '采样序号' : data.columns[xIndex].name + (['tick', 'tick_ms'].includes(data.columns[xIndex].name) ? ' (ms)' : '');
    plot.setData(x, label, series, $('show-points').checked, reset);
  }

  function renderFields() {
    $('field-list').replaceChildren();
    data.columns.forEach((column, index) => {
      const row = element('div', 'field-row'), setting = settings[index];
      const label = element('label', 'field-label'), checkbox = element('input');
      checkbox.type = 'checkbox'; checkbox.checked = setting.selected; checkbox.disabled = !column.numeric;
      checkbox.setAttribute('aria-label', `显示 ${column.name}`);
      const swatch = element('span', 'field-swatch'); swatch.style.background = colors[index % colors.length];
      label.append(checkbox, element('span', 'field-name', column.name), swatch); row.append(label);
      const badges = element('div', 'field-badges');
      if (column.name === 'p_filtered_cw') badges.append(element('span', 'field-badge existing', '已有滤波'));
      if (column.name.endsWith('_cw')) badges.append(element('span', 'field-badge', '0.01 W'));
      if (!column.numeric) badges.append(element('span', 'field-badge', '无有效数值'));
      if (badges.childNodes.length) row.append(badges);
      if (column.numeric) {
        const controls = element('div', 'field-settings');
        const filterLabel = element('label', 'filter-toggle'), filter = element('input');
        filter.type = 'checkbox'; filter.checked = false; filter.setAttribute('aria-label', `对 ${column.name} 额外滤波`);
        filterLabel.append(filter, document.createTextNode('额外滤波'));
        const windowLabel = element('label', 'window-control'), windowInput = element('input');
        windowInput.type = 'number'; windowInput.min = '3'; windowInput.max = '101'; windowInput.step = '2'; windowInput.value = '5'; windowInput.disabled = true;
        windowInput.setAttribute('aria-label', `${column.name} 滤波窗口`);
        windowLabel.append(document.createTextNode('窗口'), windowInput, document.createTextNode('点'));
        filter.addEventListener('change', () => { setting.enabled = filter.checked; windowInput.disabled = !filter.checked; setting.y = null; updatePlot(); });
        windowInput.addEventListener('change', () => {
          const value = Number(windowInput.value);
          if (!Number.isInteger(value) || value < 3 || value > 101 || value % 2 !== 1) {
            windowInput.setCustomValidity('请选择 3～101 的奇数窗口。'); windowInput.reportValidity();
            windowInput.value = String(setting.window); windowInput.setCustomValidity(''); return;
          }
          setting.window = value; setting.y = null; updatePlot();
        });
        controls.append(filterLabel, windowLabel); row.append(controls);
      }
      checkbox.addEventListener('change', () => { setting.selected = checkbox.checked; updatePlot(); });
      $('field-list').append(row);
    });
    $('field-count').textContent = data.columns.length;
  }

  function changeAxis() {
    xIndex = Number($('x-axis').value);
    x = xIndex === -1 ? Float64Array.from({ length: data.length }, (_, i) => i) : data.columns[xIndex].values;
    updatePlot(true);
  }

  async function importData(read, name) {
    for (const id of ['parse-button', 'sample-button', 'file-input']) $(id).disabled = true;
    $('parse-button').textContent = '正在读取…'; $('import-error').hidden = true;
    $('csv-input').setAttribute('aria-busy', 'true');
    // Let the busy state paint before parsing a large paste.
    await new Promise(resolve => setTimeout(resolve, 0));
    try {
      const parsed = C.parseCsv(await read());
      data = parsed; sourceName = name;
      settings = data.columns.map(() => ({ selected: false, enabled: false, window: 5, y: null }));
      xIndex = C.defaultXAxis(data);
      const first = data.columns.findIndex((c, i) => c.numeric && i !== xIndex);
      if (first >= 0) settings[first].selected = true;
      else settings[data.columns.findIndex(c => c.numeric)].selected = true;
      $('x-axis').replaceChildren();
      const indexOption = element('option', '', '采样序号'); indexOption.value = '-1'; $('x-axis').append(indexOption);
      data.columns.forEach((column, i) => {
        if (!column.numeric) return;
        const option = element('option', '', column.name); option.value = i; $('x-axis').append(option);
      });
      $('x-axis').value = String(xIndex); $('x-axis').disabled = false; $('reset-view').disabled = false;
      $('data-status').textContent = `${sourceName} · ${data.length.toLocaleString('zh-CN')} 行 · ${data.columns.length} 个字段`;
      const notices = [];
      if (data.badLines.length) notices.push(`跳过 ${data.badLines.length} 个坏行：${data.badLines.slice(0, 12).map(n => `第 ${n} 行`).join('、')}${data.badLines.length > 12 ? ' 等' : ''}。`);
      if (data.invalidCells) notices.push(`${data.invalidCells} 个空值或非数值单元格按断点处理。`);
      if (data.repeatedHeaders) notices.push(`忽略 ${data.repeatedHeaders} 个重复表头。`);
      $('import-notice').textContent = notices.join('\n'); $('import-notice').hidden = !notices.length;
      renderFields(); changeAxis();
      $('paste-section').open = false;
    } catch (error) {
      $('import-error').textContent = error.message; $('import-error').hidden = false;
    } finally {
      for (const id of ['parse-button', 'sample-button', 'file-input']) $(id).disabled = false;
      const arrow = element('span', '', '↗'); arrow.setAttribute('aria-hidden', 'true');
      $('parse-button').replaceChildren(document.createTextNode('读取并绘图 '), arrow);
      $('csv-input').setAttribute('aria-busy', 'false');
    }
  }

  $('parse-button').addEventListener('click', () => importData(() => $('csv-input').value, '粘贴数据'));
  $('file-input').addEventListener('change', event => {
    const file = event.target.files[0];
    if (file) importData(() => file.text(), file.name);
    event.target.value = '';
  });
  $('sample-button').addEventListener('click', () => {
    const sample = 'tick,target_power_cw,p_meter_cw,p_filtered_cw\n0,4000,1800,1800\n10,4000,2300,1950\n20,4000,2800,2205\n30,4000,3400,2564\n40,4000,3800,2935\n50,4000,4300,3345\n60,4000,3950,3527\n70,4000,4450,3804\n80,4000,4100,3893\n90,4000,3850,3880\n100,4000,4350,4021\n110,4000,4150,4060\n120,4000,3800,3982\n130,4000,4300,4077\n140,4000,3950,4039\n150,4000,4400,4147\n160,4000,4050,4118\n170,4000,3700,3993\n180,4000,4200,4055\n190,4000,4000,4039\n200,4000,4050,4042';
    $('csv-input').value = sample;
    importData(() => sample, '示例数据');
  });
  $('x-axis').addEventListener('change', changeAxis);
  $('show-points').addEventListener('change', () => updatePlot());
  $('reset-view').addEventListener('click', () => plot.reset());
})();
