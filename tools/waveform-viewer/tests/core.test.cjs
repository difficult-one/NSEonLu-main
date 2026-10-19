const test = require('node:test');
const assert = require('node:assert/strict');
const C = require('../core.js');

const values = column => Array.from(column.values);
const close = (actual, expected) => assert.ok(Math.abs(actual - expected) < 1e-9, `${actual} != ${expected}`);

test('CSV handles BOM, CRLF, quoted names, escaped quotes and repeated headers', () => {
  const data = C.parseCsv('\ufeff"tick","power,raw","say""hi"\r\n0,10,1\r\n\r\n"tick","power,raw","say""hi"\r\n10,20,2\r\n');
  assert.deepEqual(data.columns.map(c => c.name), ['tick', 'power,raw', 'say"hi']);
  assert.deepEqual(values(data.columns[1]), [10, 20]);
  assert.deepEqual(Array.from(data.lines), [2, 5]);
  assert.equal(data.repeatedHeaders, 1);
  assert.equal(C.defaultXAxis(data), 0);
});

test('quoted multiline headers preserve the physical line numbers of bad rows', () => {
  const data = C.parseCsv('tick,"power\nraw"\n0,1\n10,2,extra\n20,3');
  assert.deepEqual(data.badLines, [4]);
  assert.deepEqual(Array.from(data.lines), [3, 5]);
});

test('CSV does not turn empty cells, NaN, Infinity, hexadecimal or text into zero', () => {
  const data = C.parseCsv('tick,v,note\n0,,hello\n1,NaN,world\n2,Infinity,x\n3,0x10,y\n4,-1.25e2,z\n5,.5,q');
  assert.deepEqual(values(data.columns[1]), [NaN, NaN, NaN, NaN, -125, 0.5]);
  assert.equal(data.columns[2].numeric, false);
  assert.equal(data.columns[1].numeric, true);
});

test('bad-width and malformed quoted records are skipped with their line numbers', () => {
  const data = C.parseCsv('tick,v\n0,1\n1,2,3\n2,"4"oops\n3,5\n4,6"x\n');
  assert.deepEqual(data.badLines, [3, 4, 6]);
  assert.deepEqual(values(data.columns[1]), [1, 5]);
});

test('rejects an unterminated quote rather than silently losing the rest of the file', () => {
  assert.throws(() => C.parseCsv('tick,v\n0,1\n1,"bad\n2,3'), /引号/);
});

test('rejects empty, duplicate or missing headers and data without numeric samples', () => {
  for (const text of ['', '\n \n', 'tick,\n0,1', 'tick,tick\n0,1', 'tick,v\n', 'note\nhello']) {
    assert.throws(() => C.parseCsv(text));
  }
});

test('time-column preference is tick, tick_ms, then sample index', () => {
  assert.equal(C.defaultXAxis(C.parseCsv('v,tick_ms,tick\n1,2,3')), 2);
  assert.equal(C.defaultXAxis(C.parseCsv('v,tick_ms\n1,2')), 1);
  assert.equal(C.defaultXAxis(C.parseCsv('v\n1\n2')), -1);
  assert.equal(C.defaultXAxis(C.parseCsv('tick,v\nunknown,2')), -1);
});

test('accepts 17-column telemetry, 26-column calibration and capture-export fields', () => {
  const standard = 'tick,nominal_budget_cw,target_power_cw,corrected_budget_cw,effective_limit_cw,p_unlimited_cw,p_limited_cw,p_meter_cw,p_filtered_cw,error_cw,correction_cw,min_scale_pm,limited_mask,outer_mode,meter_online,meter_age_ms,motor_enabled_mask';
  const calibration = standard + ',i_lf_cmd,i_rf_cmd,i_lb_cmd,i_rb_cmd,n_lf_rpm,n_rf_rpm,n_lb_rpm,n_rb_rpm,meter_tick_ms';
  for (const [header, count] of [[standard, 17], [calibration, 26], ['capture_id,sequence,tick_ms,measured_w,state', 5]]) {
    const data = C.parseCsv(header + '\n' + Array(count).fill('12').join(','));
    assert.equal(data.columns.length, count);
    assert.equal(data.length, 1);
  }
});

test('segments split at missing values, duplicate x and backwards time', () => {
  assert.deepEqual(C.splitSegments([0, 1, 2, 2, 3, 0, 1, 2, NaN, 4], [1, 2, NaN, 4, 5, 6, 7, 8, 9, 10]), [[0, 2], [3, 5], [5, 8], [9, 10]]);
});

test('disabled filtering preserves every original value including gaps', () => {
  const raw = new Float64Array([0, 9, NaN, 4, 8]);
  assert.deepEqual(C.prepareSeries([0, 1, 2, 3, 4], raw, { enabled: false, window: 5 }), raw);
  assert.deepEqual(Array.from(raw), [0, 9, NaN, 4, 8]);
});

test('centered average uses clipped edges and does not filter across missing values', () => {
  const y = C.prepareSeries([0, 1, 2, 3, 4, 5, 6], [0, 3, 6, NaN, 20, 40, 60], { enabled: true, window: 3 });
  assert.deepEqual(Array.from(y), [1.5, 3, 4.5, NaN, 30, 40, 50]);
});

test('filtering restarts when time resets or an x value is missing', () => {
  const y = C.prepareSeries([0, 1, 0, 1, NaN, 3], [0, 10, 100, 200, 999, 30], { enabled: true, window: 3 });
  assert.deepEqual(Array.from(y), [5, 5, 150, 150, NaN, 30]);
});

test('window changes always derive from original values and never accumulate filtering', () => {
  const x = [0, 1, 2, 3, 4];
  const raw = new Float64Array([0, 0, 10, 0, 0]);
  assert.deepEqual(Array.from(C.prepareSeries(x, raw, { enabled: true, window: 5 })), [10 / 3, 2.5, 2, 2.5, 10 / 3]);
  assert.deepEqual(Array.from(C.prepareSeries(x, raw, { enabled: true, window: 3 })), [0, 10 / 3, 10 / 3, 10 / 3, 0]);
  assert.deepEqual(Array.from(raw), [0, 0, 10, 0, 0]);
  assert.deepEqual(Array.from(C.prepareSeries(x, raw, { enabled: false })), [0, 0, 10, 0, 0]);
});

test('filter retains constant signals and validates odd windows between 3 and 101', () => {
  assert.deepEqual(Array.from(C.prepareSeries([0, 1, 2], [7, 7, 7], { enabled: true, window: 101 })), [7, 7, 7]);
  for (const window of [0, 2, 4, 103, 3.5, NaN]) {
    assert.throws(() => C.prepareSeries([0], [7], { enabled: true, window }));
  }
});

test('PCHIP passes through samples on uneven time intervals and keeps a flat signal flat', () => {
  const x = [0, 1, 4, 10];
  const y = [2, 5, -1, 3];
  const slopes = C.pchipSlopes(x, y);
  x.forEach((v, i) => close(C.evaluatePchip(x, y, slopes, v), y[i]));
  close(C.evaluatePchip([0, 2, 5], [7, 7, 7], C.pchipSlopes([0, 2, 5], [7, 7, 7]), 3), 7);
});

test('PCHIP never overshoots endpoints or creates extra extrema', () => {
  for (const y of [[0, 1, 10, 11], [11, 10, 1, 0], [0, 10, -5, 0], [0, 0, 1, 1]]) {
    const x = [0, 1, 3, 7];
    const slopes = C.pchipSlopes(x, y);
    for (let i = 0; i < 3; i++) {
      for (let step = 0; step <= 100; step++) {
        const v = C.evaluatePchip(x, y, slopes, x[i] + (x[i + 1] - x[i]) * step / 100);
        assert.ok(v >= Math.min(y[i], y[i + 1]) - 1e-9 && v <= Math.max(y[i], y[i + 1]) + 1e-9);
      }
    }
  }
});

test('PCHIP has continuous first derivatives at sample points', () => {
  const x = [0, 1, 2, 4], y = [0, 2, 3, 6], m = C.pchipSlopes(x, y), e = 1e-5;
  for (let i = 1; i < 3; i++) {
    const left = (y[i] - C.evaluatePchip(x, y, m, x[i] - e)) / e;
    const right = (C.evaluatePchip(x, y, m, x[i] + e) - y[i]) / e;
    assert.ok(Math.abs(left - right) < 0.001);
  }
});

test('single points and pairs have defined constant and linear interpolants', () => {
  close(C.evaluatePchip([2], [8], C.pchipSlopes([2], [8]), 2), 8);
  close(C.evaluatePchip([0, 4], [2, 10], C.pchipSlopes([0, 4], [2, 10]), 1), 4);
  assert.throws(() => C.pchipSlopes([0, 0], [1, 2]));
});

test('decimation preserves a narrow peak and trough in original sample order', () => {
  const x = Float64Array.from({ length: 10000 }, (_, i) => i);
  const y = new Float64Array(10000);
  y[501] = 200; y[502] = -300;
  const indices = C.decimateSegment(x, y, 0, 10000, [0, 9999], 50);
  assert.ok(indices.includes(501) && indices.includes(502));
  assert.equal(indices[0], 0);
  assert.equal(indices.at(-1), 9999);
  assert.ok(indices.length <= 202);
  assert.ok(indices.every((v, i) => i === 0 || v > indices[i - 1]));
  assert.equal(C.nearestInSegment(x, 0, x.length, 501.2), 501);
});

test('zoomed decimation includes slope context for curves crossing the visible boundary', () => {
  assert.deepEqual(C.decimateSegment([0, 10, 20, 30], [1, 2, 3, 4], 0, 4, [12, 18], 100), [0, 1, 2, 3]);
  assert.equal(C.nearestInSegment([0, 10, 20], 0, 3, 14), 1);
});

test('zooming a sparse curve does not change its interpolated values', () => {
  const x = [0, 1, 2, 3], y = [0, 1, 0, 1];
  for (const range of [[0, 3], [1, 2], [1, 2.01], [1.25, 1.75]]) {
    const indices = C.decimateSegment(x, y, 0, x.length, range, 100);
    const xs = indices.map(i => x[i]), ys = indices.map(i => y[i]), slopes = C.pchipSlopes(xs, ys);
    close(C.evaluatePchip(xs, ys, slopes, 1.5), 0.5);
    close(C.evaluatePchip(xs, ys, slopes, 1.25), 0.84375);
  }
});

test('100,000-row import, filtering and display simplification retain all source data', () => {
  const rows = Array.from({ length: 100000 }, (_, i) => `${i * 10},${i === 54321 ? 9999 : 4}`);
  const data = C.parseCsv('tick,p_meter_cw\n' + rows.join('\n'));
  assert.equal(data.length, 100000);
  const x = data.columns[0].values, raw = data.columns[1].values;
  assert.equal(raw[54321], 9999);
  const filtered = C.prepareSeries(x, raw, { enabled: true, window: 5 });
  close(filtered[54321], 2003);
  assert.equal(raw[54321], 9999);
  const indices = C.decimateSegment(x, raw, 0, data.length, [0, 999990], 1000);
  assert.ok(indices.includes(54321));
  assert.ok(indices.length <= 4002);
});
