(function (root) {
  'use strict';
  // Stream CSV records so a large file does not require a second full row matrix.
  function readRecords(text, consume) {
    let fields = [], cell = '', quoted = false, closed = false, malformed = false;
    let line = 1, recordLine = 1;
    const finishCell = () => { fields.push(cell); cell = ''; closed = false; };
    const finishRecord = () => {
      finishCell(); consume(fields, recordLine, malformed);
      fields = []; malformed = false;
    };
    for (let i = 0; i < text.length; i++) {
      const c = text[i];
      if (quoted) {
        if (c === '"') {
          if (text[i + 1] === '"') { cell += '"'; i++; }
          else { quoted = false; closed = true; }
        } else {
          cell += c;
          if (c === '\n' || (c === '\r' && text[i + 1] !== '\n')) line++;
        }
      } else if (c === ',') finishCell();
      else if (c === '\n' || c === '\r') {
        finishRecord();
        if (c === '\r' && text[i + 1] === '\n') i++;
        line++; recordLine = line;
      } else if (c === '"') {
        if (cell.trim() || closed) { malformed = true; cell += c; }
        else { quoted = true; cell = ''; }
      } else if (closed) {
        if (!/\s/.test(c)) { malformed = true; cell += c; }
      } else cell += c;
    }
    if (quoted) throw new Error(`第 ${recordLine} 行的引号没有闭合，请检查 CSV。`);
    if (fields.length || cell.trim() || closed || malformed) finishRecord();
  }

  function parseCsv(input) {
    const text = String(input).replace(/^\ufeff/, '');
    let columns = null, repeatedHeaders = 0, invalidCells = 0;
    const lines = [], badLines = [];
    const number = /^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?$/;
    readRecords(text, (cells, line, malformed) => {
      if (cells.length === 1 && !cells[0].trim() && !malformed) return;
      if (!columns) {
        const names = cells.map(c => c.trim());
        if (malformed || names.some(n => !n) || new Set(names).size !== names.length) {
          throw new Error('表头必须包含非空且不重复的字段名，并正确使用引号。');
        }
        columns = names.map(name => ({ name, values: [], numeric: false }));
        return;
      }
      if (malformed || cells.length !== columns.length) { badLines.push(line); return; }
      if (cells.every((c, i) => c.trim() === columns[i].name)) { repeatedHeaders++; return; }
      lines.push(line);
      cells.forEach((cell, i) => {
        const token = cell.trim();
        const v = number.test(token) ? Number(token) : NaN;
        if (Number.isFinite(v)) columns[i].numeric = true;
        else invalidCells++;
        columns[i].values.push(Number.isFinite(v) ? v : NaN);
      });
    });
    if (!columns) throw new Error('数据为空，请导入带字段名的 CSV 数据。');
    if (!lines.length) throw new Error('没有有效数据行，请检查表头之后的数据。');
    if (!columns.some(c => c.numeric)) throw new Error('没有可绘制的数值字段。');
    columns.forEach(c => { c.values = Float64Array.from(c.values); });
    return { columns, length: lines.length, lines: Uint32Array.from(lines), badLines, repeatedHeaders, invalidCells };
  }

  function defaultXAxis(data) {
    for (const name of ['tick', 'tick_ms']) {
      const index = data.columns.findIndex(c => c.name === name && c.numeric);
      if (index !== -1) return index;
    }
    return -1;
  }

  // Half-open index ranges. Each contains only finite points and increasing x.
  function splitSegments(x, y) {
    const segments = [];
    let start = -1;
    for (let i = 0; i < x.length; i++) {
      if (!Number.isFinite(x[i]) || !Number.isFinite(y[i])) {
        if (start !== -1) segments.push([start, i]);
        start = -1;
      } else {
        if (start !== -1 && x[i] <= x[i - 1]) { segments.push([start, i]); start = -1; }
        if (start === -1) start = i;
      }
    }
    if (start !== -1) segments.push([start, x.length]);
    return segments;
  }

  function prepareSeries(x, raw, options = {}) {
    if (!options.enabled) return raw;
    const window = options.window ?? 5;
    if (!Number.isInteger(window) || window < 3 || window > 101 || window % 2 !== 1) {
      throw new Error('滤波窗口须为 3～101 的奇数。');
    }
    const result = new Float64Array(raw.length).fill(NaN);
    const radius = (window - 1) / 2;
    for (const [start, end] of splitSegments(x, raw)) {
      // Rolling sum is O(n), independent of the selected window size.
      let left = start, right = start, sum = 0;
      for (let i = start; i < end; i++) {
        const nextLeft = Math.max(start, i - radius), nextRight = Math.min(end, i + radius + 1);
        while (right < nextRight) sum += raw[right++];
        while (left < nextLeft) sum -= raw[left++];
        result[i] = sum / (right - left);
      }
    }
    return result;
  }

  function pchipSlopes(x, y) {
    const n = x.length, slopes = new Float64Array(n);
    if (!n || n !== y.length) throw new Error('插值数据长度不一致。');
    if (n === 1) return slopes;
    const h = new Float64Array(n - 1), d = new Float64Array(n - 1);
    for (let i = 0; i < n - 1; i++) {
      h[i] = x[i + 1] - x[i];
      if (!(h[i] > 0)) throw new Error('插值横轴必须严格递增。');
      d[i] = (y[i + 1] - y[i]) / h[i];
    }
    if (n === 2) { slopes.fill(d[0]); return slopes; }
    for (let i = 1; i < n - 1; i++) {
      if (!d[i - 1] || !d[i] || Math.sign(d[i - 1]) !== Math.sign(d[i])) slopes[i] = 0;
      else {
        const w1 = 2 * h[i] + h[i - 1], w2 = h[i] + 2 * h[i - 1];
        slopes[i] = (w1 + w2) / (w1 / d[i - 1] + w2 / d[i]);
      }
    }
    const edge = (h0, h1, d0, d1) => {
      const m = ((2 * h0 + h1) * d0 - h0 * d1) / (h0 + h1);
      if (Math.sign(m) !== Math.sign(d0)) return 0;
      if (Math.sign(d0) !== Math.sign(d1) && Math.abs(m) > 3 * Math.abs(d0)) return 3 * d0;
      return m;
    };
    slopes[0] = edge(h[0], h[1], d[0], d[1]);
    slopes[n - 1] = edge(h[n - 2], h[n - 3], d[n - 2], d[n - 3]);
    return slopes;
  }

  function lowerBound(x, start, end, target) {
    let low = start, high = end;
    while (low < high) {
      const mid = Math.floor((low + high) / 2);
      if (x[mid] < target) low = mid + 1;
      else high = mid;
    }
    return low;
  }

  function evaluatePchip(x, y, slopes, value) {
    if (x.length === 1 || value <= x[0]) return y[0];
    if (value >= x[x.length - 1]) return y[y.length - 1];
    const i = lowerBound(x, 0, x.length, value) - 1;
    const h = x[i + 1] - x[i], t = (value - x[i]) / h, t2 = t * t, t3 = t2 * t;
    return (2 * t3 - 3 * t2 + 1) * y[i] + (t3 - 2 * t2 + t) * h * slopes[i]
      + (-2 * t3 + 3 * t2) * y[i + 1] + (t3 - t2) * h * slopes[i + 1];
  }

  function nearestInSegment(x, start, end, target) {
    const i = lowerBound(x, start, end, target);
    if (i === start) return start;
    if (i === end) return end - 1;
    return target - x[i - 1] <= x[i] - target ? i - 1 : i;
  }

  function decimateSegment(x, y, start, end, range, pixels) {
    if (end <= start || x[end - 1] < range[0] || x[start] > range[1]) return [];
    // PCHIP needs a neighbour beyond both interval endpoints to keep boundary
    // derivatives unchanged when a sparse curve is clipped by the viewport.
    const first = Math.max(start, lowerBound(x, start, end, range[0]) - 2);
    const last = Math.min(end, lowerBound(x, start, end, range[1]) + 2);
    const width = Math.max(1, Math.floor(pixels));
    if (last - first <= width * 4) return Array.from({ length: last - first }, (_, i) => first + i);
    const out = [], span = range[1] - range[0] || 1;
    let bucket = null, indices = [];
    const flush = () => {
      if (!indices.length) return;
      let minimum = indices[0], maximum = indices[0];
      for (const i of indices) {
        if (y[i] < y[minimum]) minimum = i;
        if (y[i] > y[maximum]) maximum = i;
      }
      const chosen = [...new Set([indices[0], minimum, maximum, indices.at(-1)])].sort((a, b) => a - b);
      out.push(...chosen); indices = [];
    };
    for (let i = first; i < last; i++) {
      const nextBucket = Math.max(0, Math.min(width - 1, Math.floor((x[i] - range[0]) / span * width)));
      if (bucket !== nextBucket) { flush(); bucket = nextBucket; }
      indices.push(i);
    }
    flush();
    return out;
  }

  const api = { parseCsv, defaultXAxis, splitSegments, prepareSeries, pchipSlopes, evaluatePchip, nearestInSegment, decimateSegment };
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.WaveformCore = api;
})(globalThis);
