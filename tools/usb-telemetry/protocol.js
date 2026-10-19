(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.TelemetryProtocol = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {
  'use strict';
  const FIELDS = Object.freeze(('version,seq,tick,tx_drop_count,chassis_valid,chassis_tick_ms,chassis_mode,vx_cmd_milli,vy_cmd_milli,wz_cmd_milli,rc_online,rc_age_ms,rc_lx,rc_ly,rc_rx,rc_ry,rc_dial,rc_sw_left,rc_sw_right,mouse_x,mouse_y,mouse_left,mouse_right,keys,model_tick_ms,motor_online_mask,motor_enabled_mask,rpm_lf,rpm_rf,rpm_lb,rpm_rb,target_lf_rpm,target_rf_rpm,target_lb_rpm,target_rb_rpm,i_lf_cmd,i_rf_cmd,i_lb_cmd,i_rb_cmd,i_lf_fb,i_rf_fb,i_lb_fb,i_rb_fb,power_valid,p_unlimited_cw,p_limited_cw,effective_limit_cw,p_meter_cw,meter_online,meter_age_ms,nominal_budget_cw,target_power_cw,power_source,referee_online,referee_limit_cw,buffer_j,buffer_age_ms,buffer_control_enabled,buffer_estimate_valid,buffer_lower_cj,buffer_predicted_cj,buffer_recovering,cap_online,cap_output_enabled,cap_error,cap_energy_pm,cap_chassis_cw,cap_limit_cw,cap_age_ms,cap_input_online,cap_input_cw,cap_input_age_ms').split(','));
  const HEADER = FIELDS.join(',');
  const UINT_MAX = 4294967295;
  const HISTORY_MS = 600000, MAX_SAMPLES = 30000;
  const SIGNED = /^(vx_cmd|vy_cmd|wz_cmd|rc_lx|rc_ly|rc_rx|rc_ry|rc_dial|mouse_x|mouse_y|rpm_|target_.*_rpm|i_|p_|effective_limit_cw|nominal_budget_cw|target_power_cw|referee_limit_cw|buffer_lower_cj|buffer_predicted_cj|cap_chassis_cw|cap_limit_cw|cap_input_cw|cap_energy_pm)/;
  const FLAGS = new Set(('chassis_valid rc_online mouse_left mouse_right power_valid meter_online referee_online buffer_control_enabled buffer_estimate_valid buffer_recovering cap_online cap_output_enabled cap_input_online').split(' '));

  class StreamParser {
    constructor({onSample = () => {}, onError = () => {}, onControl = () => {}} = {}) {
      this.onSample = onSample;
      this.onError = onError;
      this.onControl = onControl;
      this.reset();
    }
    reset() {
      this.decoder = new TextDecoder();
      this.pending = '';
      this.dropping = false;
      this.hasHeader = false;
      this.stats = {samples: 0, headers: 0, bad: 0, unsupported: 0, oversized: 0};
    }
    get pendingLength() { return this.pending.length; }
    error(kind) { this.onError(kind); }
    push(chunk) {
      const text = typeof chunk === 'string' ? chunk : this.decoder.decode(chunk, {stream: true});
      for (const character of text) {
        if (character === '\n') {
          if (!this.dropping) this.parseLine(this.pending.replace(/\r$/, ''));
          this.pending = '';
          this.dropping = false;
        } else if (!this.dropping) {
          if (this.pending.length >= 2047) {
            this.pending = '';
            this.dropping = true;
            this.stats.oversized++;
            this.stats.bad++;
            this.error('oversized');
          } else this.pending += character;
        }
      }
    }
    parseLine(line) {
      if (!line) return;
      if (line.startsWith('@T1,') || line.startsWith('@A1,')) {this.onControl(line);return;}
      if (line === HEADER) {this.hasHeader = true; this.stats.headers++; return;}
      if (line.startsWith('version,')) this.hasHeader = false;
      if (!this.hasHeader) {this.stats.bad++; this.error('header'); return;}
      const cells = line.split(',');
      if (cells.length !== FIELDS.length) {this.stats.bad++; this.error('columns'); return;}
      const row = {};
      for (let i = 0; i < cells.length; i++) {
        const key = FIELDS[i], signed = SIGNED.test(key), value = Number(cells[i]);
        if (!/^-?\d+$/.test(cells[i]) || !Number.isInteger(value) || value < (signed ? -2147483648 : 0) || value > (signed ? 2147483647 : UINT_MAX) || (FLAGS.has(key) && value > 1)) {
          this.stats.bad++; this.error('value'); return;
        }
        row[key] = value;
      }
      if (row.version !== 1) {this.stats.unsupported++; this.error('version'); return;}
      if (row.motor_online_mask > 15 || row.motor_enabled_mask > 15 || row.chassis_mode > 3 || row.power_source > 1) {this.stats.bad++; this.error('enum'); return;}
      row.rawLine = line;
      this.stats.samples++;
      this.onSample(row);
    }
  }

  function age(now, then) { return Number.isInteger(then) ? (now - then) >>> 0 : UINT_MAX; }
  function fresh(value, max) { return Number.isInteger(value) && value >= 0 && value <= max; }
  function normalizeSample(raw) {
    const ages = {chassis: age(raw.tick, raw.chassis_tick_ms), remote: raw.rc_age_ms, model: age(raw.tick, raw.model_tick_ms), meter: raw.meter_age_ms, buffer: raw.buffer_age_ms, cap: raw.cap_age_ms, capInput: raw.cap_input_age_ms};
    const motorSnapshotFresh = fresh(ages.model, 500);
    const valid = {chassis: raw.chassis_valid === 1 && fresh(ages.chassis, 500), remote: raw.rc_online === 1 && fresh(ages.remote, 100), model: raw.power_valid === 1 && motorSnapshotFresh, meter: raw.meter_online === 1 && fresh(ages.meter, 20), referee: raw.referee_online === 1 && fresh(ages.buffer, 300), cap: raw.cap_online === 1 && fresh(ages.cap, 20), capInput: raw.cap_input_online === 1 && fresh(ages.capInput, 20)};
    valid.bufferEstimate = valid.chassis && raw.buffer_estimate_valid === 1;
    const values = {};
    const motorFields = /^(?:rpm_|target_.*_rpm$|i_)/;
    const remoteFields = /^(?:rc_lx|rc_ly|rc_rx|rc_ry|rc_dial|rc_sw_left|rc_sw_right|mouse_|keys$)/;
    const chassisFields = /^(?:chassis_mode|vx_cmd|vy_cmd|wz_cmd|nominal_budget_cw|target_power_cw|power_source|buffer_control_enabled|buffer_recovering)/;
    for (const key of FIELDS) {
      let ok = true;
      if (remoteFields.test(key)) ok = valid.remote;
      else if (chassisFields.test(key)) ok = valid.chassis;
      else if (motorFields.test(key)) ok = motorSnapshotFresh;
      else if (/^(p_unlimited_cw|p_limited_cw|effective_limit_cw)$/.test(key)) ok = valid.model;
      else if (key === 'p_meter_cw') ok = valid.meter;
      else if (key === 'buffer_j' || key === 'referee_limit_cw') ok = valid.referee;
      else if (key === 'buffer_lower_cj' || key === 'buffer_predicted_cj') ok = valid.bufferEstimate;
      else if (key === 'cap_input_cw') ok = valid.capInput;
      else if (/^cap_(energy_pm|chassis_cw|limit_cw|error|output_enabled)$/.test(key)) ok = valid.cap;
      let value = Number.isFinite(raw[key]) ? raw[key] : null;
      if (value !== null && key.endsWith('_cw')) value /= 100;
      if (value !== null && key.endsWith('_cj')) value /= 100;
      if (value !== null && key.endsWith('_milli')) value /= 1000;
      if (value !== null && key === 'cap_energy_pm') value = value >= 0 && value <= 1000 ? value / 10 : null;
      values[key] = ok ? value : null;
    }
    const motors = ['lf', 'rf', 'lb', 'rb'].map((key, index) => {
      const online = motorSnapshotFresh && Boolean(raw.motor_online_mask & (1 << index));
      for (const field of [`rpm_${key}`, `target_${key}_rpm`, `i_${key}_cmd`, `i_${key}_fb`]) if (!online) values[field] = null;
      return {key, online, enabled: motorSnapshotFresh && Boolean(raw.motor_enabled_mask & (1 << index)), rpm: values[`rpm_${key}`], target: values[`target_${key}_rpm`], currentCommand: values[`i_${key}_cmd`], currentFeedback: values[`i_${key}_fb`]};
    });
    return {raw, values, valid, ages, motors};
  }
  function displayValue(sample, field) { return normalizeSample(sample).values[field] ?? null; }
  function normalizeForDisplay(sample, nowMs) {
    return !sample || nowMs - sample.arrivalMs > 500 ? null : sample.normalized || normalizeSample(sample);
  }

  class TelemetryStore {
    constructor() { this.clear(); }
    clear() { this.samples = []; this.latest = null; this.stats = {missing: 0, resets: 0, segments: 0, received: 0}; }
    add(sample, arrivalMs) {
      let segment = this.latest ? this.latest.segment : 0;
      if (this.latest) {
        const sequence = (sample.seq - this.latest.seq) >>> 0, elapsed = (sample.tick - this.latest.tick) >>> 0;
        const reset = sequence > 0x80000000 || elapsed > 0x80000000;
        if (reset) this.stats.resets++;
        else if (sequence > 1) this.stats.missing += sequence - 1;
        if (reset || sequence !== 1 || elapsed > 100 || arrivalMs - this.latest.arrivalMs > 500) segment++;
      }
      const entry = {...sample, arrivalMs, segment};
      entry.normalized = normalizeSample(sample);
      this.samples.push(entry);
      this.latest = entry;
      this.stats.received++;
      this.stats.segments = segment;
      while (this.samples.length > MAX_SAMPLES || (this.samples.length > 1 && arrivalMs - this.samples[0].arrivalMs > HISTORY_MS)) this.samples.shift();
      return entry;
    }
    isStale(nowMs) { return !this.latest || nowMs - this.latest.arrivalMs > 500; }
    exportCSV() {return HEADER + '\r\n' + this.samples.map(sample => sample.rawLine || FIELDS.map(key => sample[key]).join(',')).join('\r\n') + (this.samples.length ? '\r\n' : '');}
  }
  class DisplayState {
    constructor(store) { this.store = store; this.frozen = null; }
    get paused() { return this.frozen !== null; }
    pause(now) { if (!this.paused) this.frozen = {samples: this.store.samples.slice(), latest: this.store.latest, at: now}; }
    resume() { this.frozen = null; }
    snapshot(now) { return this.frozen || {samples: this.store.samples, latest: this.store.latest, at: now}; }
  }
  return {FIELDS, HEADER, UINT_MAX, HISTORY_MS, MAX_SAMPLES, StreamParser, TelemetryStore, DisplayState, normalizeSample, normalizeForDisplay, displayValue};
});
