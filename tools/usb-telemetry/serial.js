(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.TelemetrySerial = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {
  'use strict';
  class SerialSession {
    constructor({serial, onChunk = () => {}, onState = () => {}, onError = () => {}} = {}) {
      this.serial = serial;
      this.onChunk = onChunk;
      this.onState = onState;
      this.onError = onError;
      this.state = 'idle';
      this.port = null;
      this.reader = null;
      this.readTask = null;
      this.stopping = false;
      this.writer = null;
      this.writeTask = Promise.resolve();
      if (serial && serial.addEventListener) serial.addEventListener('disconnect', event => {
        if (this.port && (event.port === this.port || event.target === this.port)) this.disconnect();
      });
    }
    setState(state) { this.state = state; this.onState(state); }
    async connect() {
      if (this.state !== 'idle') return false;
      if (!this.serial || !this.serial.requestPort) {this.onError(new Error('此浏览器未提供 Web Serial，请用 Chrome / Edge 打开 localhost 页面。')); return false;}
      this.setState('connecting');
      this.stopping = false;
      try {
        // Called directly in the click handler's activation, before any asynchronous work.
        this.port = await this.serial.requestPort();
        await this.port.open({baudRate: 115200});
        await this.port.setSignals({dataTerminalReady: true});
        if (!this.port.readable) throw new Error('串口没有可读数据流，请关闭占用端口的软件后重试。');
        this.reader = this.port.readable.getReader();
        this.setState('connected');
        this.readTask = this.readLoop();
        return true;
      } catch (error) {
        await this.closePort();
        this.setState('idle');
        if (error.name !== 'NotFoundError') this.onError(error);
        return false;
      }
    }
    async readLoop() {
      try {
        while (!this.stopping) {
          const {value, done} = await this.reader.read();
          if (done) break;
          if (value && !this.stopping) this.onChunk(value);
        }
      } catch (error) {
        if (!this.stopping) this.onError(error);
      } finally {
        if (this.reader) {this.reader.releaseLock(); this.reader = null;}
        await this.closePort();
        this.setState('idle');
      }
    }
    async closePort() {
      const port = this.port;
      this.port = null;
      if (port) {
        const writer = this.writer;this.writer = null;
        if (writer) {
          try {await writer.abort();} catch (_) { /* Unplug can already have closed the writer. */ }
          try {await this.writeTask;} catch (_) { /* Pending commands reject on disconnect. */ }
          writer.releaseLock();
        }
        try {await port.setSignals({dataTerminalReady: false});} catch (_) { /* Detached USB cannot accept signals. */ }
        try {await port.close();} catch (_) { /* Detached USB may already be closed. */ }
      }
    }
    async send(text) {
      if (this.state !== 'connected' || !this.port?.writable) throw new Error('串口未连接或不可写。');
      if (!this.writer) this.writer = this.port.writable.getWriter();
      const writer = this.writer;
      const task = this.writeTask.catch(() => {}).then(async () => {
        if (this.state !== 'connected' || this.writer !== writer) throw new Error('串口已断开。');
        await writer.write(new TextEncoder().encode(text));
      });
      this.writeTask = task;
      return task;
    }
    async disconnect() {
      if (this.state === 'connecting') return;
      if (!this.port && !this.reader) return;
      this.stopping = true;
      this.setState('disconnecting');
      if (this.reader) {
        try {await this.reader.cancel();} catch (_) { /* Unplug may already have errored the stream. */ }
      }
      if (this.readTask) await this.readTask;
      else {await this.closePort(); this.setState('idle');}
      this.readTask = null;
    }
  }
  return {SerialSession};
});
