const test = require('node:test');
const assert = require('node:assert/strict');
let P = {};
try { P = require('../protocol.js'); } catch (error) { if (error.code !== 'MODULE_NOT_FOUND') throw error; }

function sample(overrides = {}) {
  return Object.fromEntries(P.FIELDS.map(name => [name, 0]).concat(Object.entries({version: 1, tick: 1000, seq: 1, chassis_valid: 1, chassis_tick_ms: 1000, rc_online: 1, power_valid: 1, model_tick_ms: 1000, meter_online: 1, referee_online: 1, buffer_estimate_valid: 1, cap_online: 1, cap_input_online: 1, motor_online_mask: 15, motor_enabled_mask: 15, ...overrides})));
}
function line(values) { return P.FIELDS.map(key => values[key]).join(','); }

test('exports the complete version 1 schema', () => {
  assert.equal(typeof P.StreamParser, 'function');
  assert.equal(P.FIELDS.length, 72);
  assert.equal(new Set(P.FIELDS).size, 72);
  assert.equal(P.FIELDS[71], 'cap_input_age_ms');
});
test('streams arbitrary bytes, split CRLF, repeated headers and multiple rows', () => {
  const rows = [], parser = new P.StreamParser({onSample: value => rows.push(value)});
  const csv = P.HEADER + '\r\n' + line(sample({p_meter_cw: -1234})) + '\r\n' + P.HEADER + '\r\n' + line(sample({seq: 2}))+ '\r\n';
  const bytes = new TextEncoder().encode(csv);
  for (let i = 0; i < bytes.length; i += 7) parser.push(bytes.slice(i, i + 7));
  assert.equal(rows.length, 2);
  assert.equal(rows[0].p_meter_cw, -1234);
  assert.equal(parser.stats.headers, 2);
  assert.equal(parser.stats.bad, 0);
});
test('discards malformed / unsupported / oversized rows and recovers on next newline', () => {
  const rows = [], parser = new P.StreamParser({onSample: value => rows.push(value)});
  parser.push(P.HEADER + '\n');
  parser.push(line(sample({seq: -1})) + '\n');
  parser.push(line(sample({p_meter_cw: 'NaN'})) + '\n');
  parser.push(line(sample({tick: 4294967296})) + '\n');
  parser.push(line(sample({version: 2})) + '\n');
  parser.push('0'.repeat(10000));
  assert.ok(parser.pendingLength <= 2048);
  parser.push('\n' + line(sample()) + '\n');
  assert.equal(rows.length, 1);
  assert.equal(parser.stats.bad, 4);
  assert.equal(parser.stats.unsupported, 1);
  assert.equal(parser.stats.oversized, 1);
});
test('requires exact header but can recover from a later repeated firmware header', () => {
  const rows = [], parser = new P.StreamParser({onSample: value => rows.push(value)});
  parser.push(line(sample()) + '\nversion,seq,tick\n' + line(sample()) + '\n');
  assert.equal(rows.length, 0);
  parser.push(P.HEADER + '\n' + line(sample()) + '\n');
  assert.equal(rows.length, 1);
});
test('converts physical units while masking offline and stale sources', () => {
  const source = sample({p_meter_cw: 1234, buffer_j: 35, buffer_lower_cj: 2150, cap_energy_pm: 725, vx_cmd_milli: -1234, i_lf_fb: 1000});
  assert.equal(P.displayValue(source, 'p_meter_cw'), 12.34);
  assert.equal(P.displayValue(source, 'buffer_lower_cj'), 21.5);
  assert.equal(P.displayValue(source, 'cap_energy_pm'), 72.5);
  assert.equal(P.displayValue(source, 'vx_cmd_milli'), -1.234);
  assert.equal(P.displayValue(source, 'i_lf_fb'), 1000);
  assert.equal(P.displayValue({...source, meter_online: 0}, 'p_meter_cw'), null);
  assert.equal(P.displayValue({...source, meter_age_ms: 4294967295}, 'p_meter_cw'), null);
  assert.equal(P.displayValue({...source, power_valid: 0}, 'p_limited_cw'), null);
  assert.equal(P.displayValue({...source, model_tick_ms: 400}, 'target_lf_rpm'), null);
  assert.equal(P.displayValue({...source, buffer_estimate_valid: 0}, 'buffer_lower_cj'), null);
  assert.equal(P.displayValue({...source, buffer_age_ms: 4294967295}, 'buffer_j'), null);
  assert.equal(P.displayValue({...source, cap_input_age_ms: 21}, 'cap_input_cw'), null);
  assert.equal(P.normalizeSample({...source, motor_online_mask: 14}).motors[0].rpm, null);
});
test('a negative signed capacitor measurement does not discard unrelated telemetry', () => {
  const rows = [], parser = new P.StreamParser({onSample: row => rows.push(row)});
  parser.push(P.HEADER + '\n' + line(sample({cap_energy_pm: -10, p_meter_cw: 1234})) + '\n');
  assert.equal(rows.length, 1);
  assert.equal(P.displayValue(rows[0], 'cap_energy_pm'), null);
  assert.equal(P.displayValue(rows[0], 'p_meter_cw'), 12.34);
});
test('live motor feedback remains visible when the power model has an invalid budget', () => {
  const row = sample({power_valid: 0, rpm_lf: 1234, target_lf_rpm: 1500, i_lf_cmd: 0, i_lf_fb: 789, p_unlimited_cw: 5000, p_limited_cw: 4000, effective_limit_cw: 0});
  const live = P.normalizeSample(row);
  assert.equal(live.valid.model, false);
  assert.equal(live.motors[0].online, true);
  assert.equal(live.motors[0].enabled, true);
  assert.equal(live.motors[0].rpm, 1234);
  assert.equal(live.motors[0].target, 1500);
  assert.equal(live.motors[0].currentCommand, 0);
  assert.equal(live.motors[0].currentFeedback, 789);
  assert.equal(live.values.rpm_lf, 1234);
  for (const key of ['p_unlimited_cw', 'p_limited_cw', 'effective_limit_cw']) assert.equal(live.values[key], null);
  const stale = P.normalizeSample({...row, model_tick_ms: 499});
  assert.equal(stale.motors[0].online, false);
  assert.equal(stale.motors[0].enabled, false);
  for (const key of ['rpm_lf', 'target_lf_rpm', 'i_lf_cmd', 'i_lf_fb']) assert.equal(stale.values[key], null);
});
test('rollover ages stay valid while reboot, gaps and stale reception create segments', () => {
  const store = new P.TelemetryStore();
  store.add(sample({tick: 4294967280, chassis_tick_ms: 4294967270}), 0);
  assert.equal(P.normalizeSample(store.latest).valid.chassis, true);
  store.add(sample({seq: 2, tick: 4}), 20);
  assert.equal(store.latest.segment, 0);
  store.add(sample({seq: 5, tick: 64}), 80);
  assert.equal(store.stats.missing, 2);
  assert.equal(store.latest.segment, 1);
  store.add(sample({seq: 1, tick: 20}), 100);
  assert.equal(store.stats.resets, 1);
  assert.equal(store.latest.segment, 2);
  store.add(sample({seq: 2, tick: 40}), 800);
  assert.equal(store.latest.segment, 3);
  assert.equal(store.isStale(1301), true);
});
test('keeps more than a minute available for paused history navigation', () => {
  const store = new P.TelemetryStore();
  store.add(sample({seq: 1, tick: 0}), 0);
  store.add(sample({seq: 2, tick: 120000}), 120000);
  const view = new P.DisplayState(store);view.pause(120010);
  assert.equal(view.snapshot(180000).samples.length, 2);
  assert.equal(view.snapshot(180000).samples[0].arrivalMs, 0);
});
test('bounds ten-minute history by count and arrival time and exports only retained raw CSV', () => {
  const store = new P.TelemetryStore();
  for (let i = 0; i < 32000; i++) store.add({...sample({seq: i, tick: i * 20}), rawLine: line(sample({seq: i, tick: i * 20}))}, i * 20);
  assert.equal(store.samples.length, 30000);
  assert.equal(store.exportCSV().split('\r\n').filter(Boolean).length, 30001);
  store.add(sample({seq: 32000, tick: 1400000}), 1400000);
  assert.equal(store.samples.length, 1);
  assert.ok(store.exportCSV().startsWith(P.HEADER + '\r\n'));
});
test('paused presentation freezes samples and time while reception and retention continue', () => {
  const store = new P.TelemetryStore(), view = new P.DisplayState(store);
  store.add(sample({seq: 1}), 100);
  view.pause(110);
  store.add(sample({seq: 2}), 120);
  assert.equal(store.latest.seq, 2);
  assert.equal(view.snapshot(1000).latest.seq, 1);
  assert.equal(view.snapshot(1000).at, 110);
  assert.equal(view.snapshot(1000).samples.length, 1);
  store.add(sample({seq: 3}), 1000000);
  assert.equal(store.samples.length, 1);
  assert.equal(view.snapshot(1000000).samples[0].seq, 1);
  view.resume();
  assert.equal(view.snapshot(1000).latest.seq, 3);
  assert.equal(view.snapshot(1000).at, 1000);
});
test('presentation keeps a valid received measurement between rows and masks a stale board', () => {
  const store = new P.TelemetryStore();
  store.add(sample({p_meter_cw: 1234, meter_age_ms: 10}), 100);
  assert.equal(P.normalizeForDisplay(store.latest, 115).values.p_meter_cw, 12.34);
  assert.equal(P.normalizeForDisplay(store.latest, 601), null);
});
