(function (root) {
  'use strict';
  const C = root.WaveformCore;
  const number = new Intl.NumberFormat('zh-CN', { maximumSignificantDigits: 7, useGrouping: false });
  const format = value => number.format(value);

  function ticks(minimum, maximum, count) {
    const raw = (maximum - minimum) / count;
    const unit = 10 ** Math.floor(Math.log10(raw));
    const step = [1, 2, 5, 10].find(v => v * unit >= raw) * unit;
    const result = [];
    for (let value = Math.ceil(minimum / step) * step; value <= maximum && result.length < 30; value += step) result.push(value);
    return result;
  }

  class WaveformPlot {
    constructor(canvas, overlay, tooltip, empty, onRange) {
      this.canvas = canvas; this.overlay = overlay; this.tooltip = tooltip; this.empty = empty;
      this.ctx = canvas.getContext('2d'); this.hoverCtx = overlay.getContext('2d');
      this.onRange = onRange; this.series = []; this.x = null; this.label = '采样序号';
      this.fullRange = [0, 1]; this.range = [0, 1]; this.pending = false;
      new ResizeObserver(() => this.schedule()).observe(canvas.parentElement);
      canvas.addEventListener('wheel', event => this.wheel(event), { passive: false });
      canvas.addEventListener('pointerdown', event => {
        if (!this.x || event.button !== 0) return;
        this.drag = { clientX: event.clientX, range: [...this.range] };
        canvas.setPointerCapture(event.pointerId); canvas.classList.add('dragging'); this.hideHover();
      });
      canvas.addEventListener('pointermove', event => {
        if (this.drag) {
          const shift = (event.clientX - this.drag.clientX) / this.bounds.width * (this.drag.range[1] - this.drag.range[0]);
          this.setRange([this.drag.range[0] - shift, this.drag.range[1] - shift]);
        } else this.hover(event);
      });
      const endDrag = () => { this.drag = null; canvas.classList.remove('dragging'); };
      canvas.addEventListener('pointerup', endDrag);
      canvas.addEventListener('pointercancel', endDrag);
      canvas.addEventListener('lostpointercapture', endDrag);
      canvas.addEventListener('pointerleave', event => {
        if (!this.drag && !tooltip.contains(event.relatedTarget)) this.hideHover();
      });
      tooltip.addEventListener('pointerleave', event => {
        if (event.relatedTarget !== canvas) this.hideHover();
      });
      canvas.addEventListener('keydown', event => {
        if (!this.x) return;
        const span = this.range[1] - this.range[0];
        if (event.key === 'Home') this.reset();
        else if (event.key === '+' || event.key === '=') this.zoom(0.75, 0.5);
        else if (event.key === '-') this.zoom(1.25, 0.5);
        else if (event.key === 'ArrowLeft' || event.key === 'ArrowRight') {
          const shift = (event.key === 'ArrowLeft' ? -1 : 1) * span * 0.1;
          this.setRange([this.range[0] + shift, this.range[1] + shift]);
        } else return;
        event.preventDefault();
      });
      this.schedule();
    }

    setData(x, label, series, showPoints, reset = false) {
      if (x !== this.x) {
        let minimum = Infinity, maximum = -Infinity;
        for (const value of x) if (Number.isFinite(value)) { minimum = Math.min(minimum, value); maximum = Math.max(maximum, value); }
        if (minimum === maximum) { minimum -= 1; maximum += 1; }
        this.fullRange = [minimum, maximum]; reset = true;
      }
      this.x = x; this.label = label; this.series = series; this.showPoints = showPoints;
      if (reset) this.range = [...this.fullRange];
      this.hideHover(); this.schedule();
    }

    reset() { this.range = [...this.fullRange]; this.hideHover(); this.schedule(); }

    setRange(range) {
      const span = range[1] - range[0], full = this.fullRange;
      if (span >= full[1] - full[0]) this.range = [...full];
      else {
        const left = Math.max(full[0], Math.min(full[1] - span, range[0]));
        this.range = [left, left + span];
      }
      this.hideHover(); this.schedule();
    }

    zoom(factor, ratio) {
      const currentSpan = this.range[1] - this.range[0], fullSpan = this.fullRange[1] - this.fullRange[0];
      const span = Math.max(fullSpan / 1e6, Math.min(fullSpan, currentSpan * factor));
      const anchor = this.range[0] + currentSpan * ratio;
      this.setRange([anchor - span * ratio, anchor + span * (1 - ratio)]);
    }

    wheel(event) {
      if (!this.x || !this.bounds) return;
      const point = this.position(event), b = this.bounds;
      if (point.x < b.left || point.x > b.left + b.width || point.y < b.top || point.y > b.top + b.height) return;
      event.preventDefault();
      const delta = event.deltaY * (event.deltaMode === 1 ? 16 : event.deltaMode === 2 ? b.height : 1);
      this.zoom(Math.exp(delta * 0.0015), (point.x - b.left) / b.width);
    }

    schedule() {
      if (this.pending) return;
      this.pending = true;
      requestAnimationFrame(() => { this.pending = false; this.draw(); });
    }

    draw() {
      const rect = this.canvas.getBoundingClientRect(), width = rect.width, height = rect.height;
      if (!width || !height) return;
      this.dpr = root.devicePixelRatio || 1;
      for (const canvas of [this.canvas, this.overlay]) {
        canvas.width = Math.round(width * this.dpr); canvas.height = Math.round(height * this.dpr);
      }
      const ctx = this.ctx;
      ctx.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);
      ctx.clearRect(0, 0, width, height); ctx.fillStyle = '#fff'; ctx.fillRect(0, 0, width, height);
      this.bounds = { left: 72, top: 24, width: Math.max(10, width - 96), height: Math.max(10, height - 82) };
      this.paths = [];
      let minimum = Infinity, maximum = -Infinity;
      const include = value => { minimum = Math.min(minimum, value); maximum = Math.max(maximum, value); };
      for (const series of this.series) {
        for (const [start, end] of series.segments) {
          const indices = C.decimateSegment(this.x, series.y, start, end, this.range, this.bounds.width);
          if (!indices.length) continue;
          const x = indices.map(i => this.x[i]), y = indices.map(i => series.y[i]), m = C.pchipSlopes(x, y);
          x.forEach((value, i) => { if (value >= this.range[0] && value <= this.range[1]) include(y[i]); });
          const left = Math.max(this.range[0], x[0]), right = Math.min(this.range[1], x.at(-1));
          include(C.evaluatePchip(x, y, m, left)); include(C.evaluatePchip(x, y, m, right));
          this.paths.push({ x, y, m, series });
        }
      }
      const hasVisible = Number.isFinite(minimum);
      if (!hasVisible) { minimum = 0; maximum = 1; }
      const padding = minimum === maximum ? Math.max(1, Math.abs(minimum) * 0.05) : (maximum - minimum) * 0.1;
      this.yRange = [minimum - padding, maximum + padding];
      this.drawAxes();
      const b = this.bounds;
      ctx.save(); ctx.beginPath(); ctx.rect(b.left, b.top, b.width, b.height); ctx.clip();
      for (const path of this.paths) {
        const { x, y, m, series } = path;
        ctx.strokeStyle = series.color; ctx.fillStyle = series.color; ctx.lineWidth = 1.8; ctx.lineJoin = 'round';
        ctx.beginPath(); ctx.moveTo(this.toX(x[0]), this.toY(y[0]));
        for (let i = 0; i < x.length - 1; i++) {
          const h = (x[i + 1] - x[i]) / 3;
          if (x.length === 2) ctx.lineTo(this.toX(x[i + 1]), this.toY(y[i + 1]));
          else ctx.bezierCurveTo(this.toX(x[i] + h), this.toY(y[i] + m[i] * h), this.toX(x[i + 1] - h), this.toY(y[i + 1] - m[i + 1] * h), this.toX(x[i + 1]), this.toY(y[i + 1]));
        }
        ctx.stroke();
        if (this.showPoints || x.length === 1) {
          ctx.beginPath();
          x.forEach((value, i) => { ctx.moveTo(this.toX(value) + 2.5, this.toY(y[i])); ctx.arc(this.toX(value), this.toY(y[i]), 2.5, 0, Math.PI * 2); });
          ctx.fill();
        }
      }
      ctx.restore();
      if (this.x) {
        this.empty.hidden = this.series.length > 0 && hasVisible;
        if (!this.empty.hidden) {
          const strong = document.createElement('strong');
          strong.textContent = this.series.length ? '当前范围没有有效采样值' : '在左侧勾选字段，显示对应波形';
          this.empty.replaceChildren(strong);
        }
        this.onRange(`${this.label}  ${format(this.range[0])} – ${format(this.range[1])}`);
      }
      this.canvas.setAttribute('aria-label', `波形图：${this.series.map(s => s.name).join('、') || '未选择字段'}，横轴 ${this.label}`);
      this.hideHover();
    }

    drawAxes() {
      const ctx = this.ctx, b = this.bounds;
      ctx.font = '10px "Segoe UI", "Microsoft YaHei", sans-serif'; ctx.lineWidth = 1;
      ctx.fillStyle = '#96a2b5'; ctx.textAlign = 'right'; ctx.textBaseline = 'middle';
      for (const value of ticks(this.yRange[0], this.yRange[1], 5)) {
        const y = this.toY(value);
        ctx.strokeStyle = '#eef2f8'; ctx.beginPath(); ctx.moveTo(b.left, y); ctx.lineTo(b.left + b.width, y); ctx.stroke();
        ctx.fillText(format(value), b.left - 12, y);
      }
      ctx.textAlign = 'center'; ctx.textBaseline = 'top';
      for (const value of ticks(this.range[0], this.range[1], Math.max(2, Math.floor(b.width / 110)))) {
        const x = this.toX(value);
        ctx.strokeStyle = '#f0f3f8'; ctx.beginPath(); ctx.moveTo(x, b.top); ctx.lineTo(x, b.top + b.height); ctx.stroke();
        ctx.fillText(format(value), x, b.top + b.height + 12);
      }
      ctx.strokeStyle = '#dde5f0'; ctx.beginPath(); ctx.moveTo(b.left, b.top); ctx.lineTo(b.left, b.top + b.height); ctx.lineTo(b.left + b.width, b.top + b.height); ctx.stroke();
      ctx.fillStyle = '#a3aec0'; ctx.textAlign = 'left'; ctx.fillText('数值 · 原始单位', b.left, 3);
      ctx.textAlign = 'center'; ctx.fillText(this.label, b.left + b.width / 2, b.top + b.height + 35);
    }

    toX(value) { return this.bounds.left + (value - this.range[0]) / (this.range[1] - this.range[0]) * this.bounds.width; }
    toY(value) { return this.bounds.top + (this.yRange[1] - value) / (this.yRange[1] - this.yRange[0]) * this.bounds.height; }
    position(event) { const rect = this.canvas.getBoundingClientRect(); return { x: event.clientX - rect.left, y: event.clientY - rect.top }; }

    hideHover() {
      this.tooltip.hidden = true;
      this.hoverCtx.setTransform(1, 0, 0, 1, 0, 0); this.hoverCtx.clearRect(0, 0, this.overlay.width, this.overlay.height);
    }

    hover(event) {
      if (!this.x || !this.bounds || !this.series.length) return;
      const p = this.position(event), b = this.bounds;
      if (p.x < b.left || p.x > b.left + b.width || p.y < b.top || p.y > b.top + b.height) { this.hideHover(); return; }
      this.hideHover();
      const target = this.range[0] + (p.x - b.left) / b.width * (this.range[1] - this.range[0]);
      const ctx = this.hoverCtx;
      ctx.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);
      ctx.strokeStyle = '#aebcd3'; ctx.setLineDash([3, 4]); ctx.beginPath(); ctx.moveTo(p.x, b.top); ctx.lineTo(p.x, b.top + b.height); ctx.stroke(); ctx.setLineDash([]);
      const title = document.createElement('div'); title.className = 'tooltip-title'; title.textContent = '最近采样点 · 原始单位';
      this.tooltip.replaceChildren(title);
      let rows = 0;
      for (const series of this.series) {
        let closest = -1, distance = Infinity;
        for (const [start, end] of series.segments) {
          if (this.x[end - 1] < this.range[0] || this.x[start] > this.range[1]) continue;
          const i = C.nearestInSegment(this.x, start, end, target), d = Math.abs(this.x[i] - target);
          if (this.x[i] >= this.range[0] && this.x[i] <= this.range[1] && d < distance) { closest = i; distance = d; }
        }
        if (closest < 0) continue;
        rows++;
        const row = document.createElement('div'); row.className = 'tooltip-row';
        const field = document.createElement('div'); field.className = 'tooltip-field'; field.style.color = series.color; field.textContent = series.name;
        const value = document.createElement('div'); value.className = 'tooltip-value'; value.textContent = `原始值 ${String(series.raw[closest])}${series.filtered ? `  ·  滤波值 ${String(series.y[closest])}` : ''}`;
        const coordinate = document.createElement('div'); coordinate.className = 'tooltip-coordinate'; coordinate.textContent = `${this.label} ${String(this.x[closest])} · 第 ${series.lines[closest]} 行`;
        row.append(field, value, coordinate); this.tooltip.append(row);
        ctx.fillStyle = series.color; ctx.beginPath(); ctx.arc(this.toX(this.x[closest]), this.toY(series.y[closest]), 3.5, 0, Math.PI * 2); ctx.fill();
      }
      if (!rows) { this.hideHover(); return; }
      this.tooltip.hidden = false;
      const width = this.canvas.clientWidth, height = this.canvas.clientHeight;
      const left = p.x + 18 + this.tooltip.offsetWidth <= width ? p.x + 18 : p.x - this.tooltip.offsetWidth - 18;
      this.tooltip.style.left = `${Math.max(4, left)}px`;
      this.tooltip.style.top = `${Math.max(4, Math.min(height - this.tooltip.offsetHeight - 4, p.y - 15))}px`;
    }
  }
  root.WaveformPlot = WaveformPlot;
})(globalThis);
