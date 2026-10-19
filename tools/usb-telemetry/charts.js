(function (root) {
  'use strict';
  const hoverStates = new WeakMap();
  function hoverState(canvas) {
    if (hoverStates.has(canvas)) return hoverStates.get(canvas);
    const layer = document.createElement('div');layer.className = 'chart-hover';layer.hidden = true;
    const guide = document.createElement('div');guide.className = 'chart-crosshair';
    const tooltip = document.createElement('div');tooltip.className = 'chart-tooltip';tooltip.setAttribute('role', 'tooltip');
    const heading = document.createElement('strong'), rows = document.createElement('div');rows.className = 'chart-tooltip-values';
    tooltip.append(heading, rows);layer.append(guide, tooltip);canvas.parentElement.append(layer);
    const state = {layer, guide, tooltip, heading, rows, pointer: null, traces: null};
    canvas.addEventListener('pointermove', event => {
      state.bounds = canvas.getBoundingClientRect();
      state.pointer = {clientX: event.clientX, clientY: event.clientY};
      renderHover(state);
    });
    canvas.addEventListener('pointerleave', () => {state.pointer = null;layer.hidden = true;});
    hoverStates.set(canvas, state);
    return state;
  }
  function renderHover(state) {
    const {samples, visible, start, end, now, pad, plotWidth, plotHeight} = state;
    const pointer = state.pointer && {x: state.pointer.clientX - state.bounds.left, y: state.pointer.clientY - state.bounds.top};
    const hide = () => {state.layer.hidden = true;};
    if (!pointer || !visible || !state.traces.length || pointer.x < pad.left || pointer.x > pad.left + plotWidth || pointer.y < pad.top || pointer.y > pad.top + plotHeight || !visible.length) {hide();return;}
    const time = start + (pointer.x - pad.left) / plotWidth * (end - start);
    let left = 0, right = samples.length;
    while (left < right) {const middle = (left + right) >>> 1;if (samples[middle].arrivalMs < time) left = middle + 1;else right = middle;}
    const before = samples[left - 1], after = samples[left];
    if (before && after && before.segment !== after.segment && time > before.arrivalMs && time < after.arrivalMs) {hide();return;}
    const sample = !before ? after : !after || time - before.arrivalMs <= after.arrivalMs - time ? before : after;
    if (!sample || sample.arrivalMs < start || sample.arrivalMs > end || Math.abs(time - sample.arrivalMs) > 500) {hide();return;}
    const pointX = pad.left + (sample.arrivalMs - start) / (end - start) * plotWidth;
    state.layer.hidden = false;
    state.guide.style.left = `${pointX}px`;state.guide.style.top = `${pad.top}px`;state.guide.style.height = `${plotHeight}px`;
    const seconds = Math.max(0, (now - sample.arrivalMs) / 1000);
    state.heading.textContent = `距窗口末端 ${seconds.toFixed(2)} s · tick ${sample.tick} ms`;
    state.traces.forEach((trace, index) => {
      const value = sample.normalized.values[trace.key];
      state.rows.children[index].querySelector('b').textContent = Number.isFinite(value) ? `${value.toFixed(trace.digits ?? (state.unit === 'rpm' ? 0 : 2))} ${trace.unit || state.unit}` : '—';
    });
    const tooltipWidth = state.tooltip.offsetWidth, tooltipHeight = state.tooltip.offsetHeight;
    state.tooltip.style.left = `${Math.max(0, Math.min(state.width - tooltipWidth, pointX + (pointX + tooltipWidth + 14 > state.width ? -tooltipWidth - 14 : 14)))}px`;
    state.tooltip.style.top = `${Math.max(0, Math.min(state.height - tooltipHeight, pointer.y - 20))}px`;
  }
  function valueRange(samples, traces, fullRange) {
    let low = fullRange ? 0 : Infinity, high = fullRange ? 1 : -Infinity;
    for (const sample of samples) for (const trace of traces) {
      const value = sample.normalized.values[trace.key];
      if (Number.isFinite(value)) {low = Math.min(low, value);high = Math.max(high, value);}
    }
    if (!Number.isFinite(low)) {low = 0;high = 1;}
    const spread = high - low || Math.max(1, Math.abs(high) * .1);
    if (!fullRange || low < 0) low -= spread * .09;
    high += spread * .12;
    return {low, high, digits: Math.max(0, Math.min(4, 1 - Math.floor(Math.log10((high - low) / 4))))};
  }
  function getPlotBounds(canvas) {
    const state = hoverStates.get(canvas);
    return state ? {left: state.pad.left, top: state.pad.top, width: state.plotWidth, height: state.plotHeight} : null;
  }
  function drawChart(canvas, samples, traces, {now, unit = '', rightUnit = '', range = [-60000, 0]} = {}) {
    const bounds = canvas.getBoundingClientRect();
    const width = bounds.width, height = bounds.height;
    if (!width || !height) return;
    const ratio = Math.min(window.devicePixelRatio || 1, 2);
    if (canvas.width !== Math.round(width * ratio) || canvas.height !== Math.round(height * ratio)) {canvas.width = Math.round(width * ratio); canvas.height = Math.round(height * ratio);}
    const ctx = canvas.getContext('2d');
    ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
    ctx.clearRect(0, 0, width, height);
    const leftTraces = traces.filter(trace => trace.axis !== 'right'), rightTraces = traces.filter(trace => trace.axis === 'right');
    const pad = {left: 42, right: rightTraces.length ? 66 : 12, top: 15, bottom: 25}, plotWidth = width - pad.left - pad.right, plotHeight = height - pad.top - pad.bottom;
    const start = now + range[0], end = now + range[1], duration = end - start, fullRange = duration === 60000;
    const visible = samples.filter(sample => sample.arrivalMs >= start && sample.arrivalMs <= end);
    const leftScale = valueRange(visible, leftTraces, fullRange), rightScale = valueRange(visible, rightTraces, fullRange);
    const y = (value, trace) => {const scale = trace.axis === 'right' ? rightScale : leftScale;return pad.top + plotHeight * (1 - (value - scale.low) / (scale.high - scale.low));};
    const x = time => pad.left + (time - start) / duration * plotWidth;
    ctx.font = '9px "Segoe UI", sans-serif';
    ctx.lineWidth = 1;
    for (let i = 0; i <= 4; i++) {
      const lineY = pad.top + i / 4 * plotHeight;
      ctx.strokeStyle = '#edf1f6';
      ctx.beginPath();ctx.moveTo(pad.left, lineY);ctx.lineTo(width - pad.right, lineY);ctx.stroke();
      ctx.fillStyle = '#a4b0c1';ctx.textAlign = 'right';ctx.textBaseline = 'middle';
      const value = leftScale.high - (leftScale.high - leftScale.low) * i / 4;
      if (leftTraces.length) ctx.fillText(value.toFixed(leftScale.digits), pad.left - 8, lineY);
      if (rightTraces.length) {
        ctx.textAlign = 'left';ctx.fillStyle = '#c57279';
        ctx.fillText((rightScale.high - (rightScale.high - rightScale.low) * i / 4).toFixed(rightScale.digits), width - pad.right + 8, lineY);
      }
    }
    ctx.textBaseline = 'top';ctx.textAlign = 'left';ctx.fillStyle = '#a4b0c1';
    if (leftTraces.length) ctx.fillText(unit, pad.left, 0);
    if (rightTraces.length) {ctx.fillStyle = '#c57279';ctx.fillText(rightUnit, width - pad.right + 8, 0);}
    for (let i = 0; i <= 4; i++) {
      const lineX = pad.left + i / 4 * plotWidth;
      ctx.strokeStyle = '#f3f5f9';ctx.beginPath();ctx.moveTo(lineX, pad.top);ctx.lineTo(lineX, height - pad.bottom);ctx.stroke();
      const seconds = (start + i / 4 * duration - now) / 1000;
      const label = Math.abs(seconds) < .0005 ? '末端' : `−${Number(Math.abs(seconds).toFixed(duration < 1000 ? 3 : duration < 10000 ? 2 : 1))}s`;
      ctx.fillStyle = '#a4b0c1';ctx.textAlign = i === 0 ? 'left' : i === 4 ? 'right' : 'center';ctx.textBaseline = 'bottom';ctx.fillText(label, lineX, height - 3);
    }
    ctx.save();ctx.beginPath();ctx.rect(pad.left, pad.top, plotWidth, plotHeight);ctx.clip();
    for (const trace of traces) {
      ctx.beginPath();ctx.strokeStyle = trace.color;ctx.lineWidth = trace.width || (trace.dash ? 1.2 : 1.6);ctx.setLineDash(trace.dash ? trace.dashPattern || [4, 4] : []);
      let previous = null;
      for (const sample of visible) {
        const value = sample.normalized.values[trace.key];
        if (value === null || !Number.isFinite(value)) {previous = null; continue;}
        const pointX = x(sample.arrivalMs), pointY = y(value, trace);
        if (!previous || previous.segment !== sample.segment || sample.arrivalMs - previous.arrivalMs > 500) ctx.moveTo(pointX, pointY);
        else ctx.lineTo(pointX, pointY);
        previous = sample;
      }
      ctx.stroke();
      if (previous && end - previous.arrivalMs <= 500) {
        ctx.setLineDash([]);ctx.beginPath();ctx.arc(x(previous.arrivalMs), y(previous.normalized.values[trace.key], trace), 2.2, 0, Math.PI * 2);ctx.fillStyle = trace.color;ctx.fill();
      }
    }
    ctx.restore();ctx.setLineDash([]);
    const units = [leftTraces.length ? unit : '', rightTraces.length ? rightUnit : ''].filter(Boolean).join(' / ') || unit;
    canvas.setAttribute('aria-label', `${units} 趋势，显示 ${Number((duration / 1000).toFixed(3))} 秒，${visible.length} 个样本，${traces.length} 条曲线。`);
    const state = hoverState(canvas);
    if (state.traces !== traces) {
      state.rows.replaceChildren(...traces.map(trace => {
        const row = document.createElement('div'), label = document.createElement('span'), dot = document.createElement('i'), value = document.createElement('b');
        dot.style.background = trace.color;label.append(dot, document.createTextNode(trace.name));row.append(label, value);return row;
      }));
    }
    Object.assign(state, {samples, visible, traces, start, end, now, pad, plotWidth, plotHeight, width, height, unit, bounds});
    renderHover(state);
  }
  root.TelemetryCharts = {drawChart, getPlotBounds};
})(globalThis);
