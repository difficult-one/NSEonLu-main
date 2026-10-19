const test = require('node:test');
const assert = require('node:assert/strict');
let SerialSession;
try { ({SerialSession} = require('../serial.js')); } catch (error) { if (error.code !== 'MODULE_NOT_FOUND') throw error; }

function hardware() {
  let controller;
  const calls = [];
  const port = {readable: new ReadableStream({start(c) {controller = c;}, cancel() {calls.push('cancel');}}), async open(options) {calls.push(['open', options]);}, async setSignals(signals) {calls.push(['signals', signals]);}, async close() {assert.equal(port.readable.locked, false); calls.push('close');}};
  const serial = new EventTarget();
  serial.requestPort = async () => port;
  return {serial, port, calls, get controller() {return controller;}};
}
const flush = () => new Promise(resolve => setImmediate(resolve));

test('writes serialized bytes and releases writer on disconnect', async () => {
  const hw = hardware(), written = [];
  hw.port.writable = new WritableStream({write(bytes) {written.push(new TextDecoder().decode(bytes));}});
  const session = new SerialSession({serial: hw.serial});
  await assert.rejects(async () => session.send('x'));
  await session.connect();
  await Promise.all([session.send('@T1,1,GET,pid\r\n'), session.send('@T1,2,GET,spin\r\n')]);
  assert.deepEqual(written, ['@T1,1,GET,pid\r\n', '@T1,2,GET,spin\r\n']);
  await session.disconnect();
  assert.equal(hw.port.writable.locked, false);
});

test('cancelled chooser returns idle without reporting an error', async () => {
  assert.equal(typeof SerialSession, 'function');
  const errors = [], states = [];
  const session = new SerialSession({serial: {requestPort: async () => {throw new DOMException('Cancelled', 'NotFoundError');}}, onError: error => errors.push(error), onState: state => states.push(state)});
  assert.equal(await session.connect(), false);
  assert.equal(session.state, 'idle');
  assert.equal(errors.length, 0);
});
test('sets DTR, reads actual byte stream, cancels and releases reader before close', async () => {
  const hw = hardware(), chunks = [];
  const session = new SerialSession({serial: hw.serial, onChunk: bytes => chunks.push(...bytes)});
  await session.connect();
  assert.deepEqual(hw.calls.slice(0, 2), [['open', {baudRate: 115200}], ['signals', {dataTerminalReady: true}]]);
  hw.controller.enqueue(new Uint8Array([1, 2, 3]));
  await flush();
  assert.deepEqual(chunks, [1, 2, 3]);
  await session.disconnect();
  assert.equal(hw.port.readable.locked, false);
  assert.equal(session.state, 'idle');
  assert.ok(hw.calls.includes('cancel'));
  assert.equal(hw.calls.at(-1), 'close');
});
test('EOF closes cleanly and a fresh port can reconnect', async () => {
  const first = hardware(), second = hardware();
  let count = 0;
  first.serial.requestPort = async () => (++count === 1 ? first.port : second.port);
  const session = new SerialSession({serial: first.serial});
  await session.connect();
  first.controller.close();
  await flush();
  assert.equal(session.state, 'idle');
  assert.equal(first.port.readable.locked, false);
  await session.connect();
  assert.equal(session.state, 'connected');
  await session.disconnect();
  assert.equal(second.port.readable.locked, false);
});
test('physical unplug and stream error release resources and allow reconnect', async () => {
  const hw = hardware(), errors = [];
  const session = new SerialSession({serial: hw.serial, onError: error => errors.push(error)});
  await session.connect();
  const event = new Event('disconnect');
  Object.defineProperty(event, 'port', {value: hw.port});
  hw.serial.dispatchEvent(event);
  await flush();
  assert.equal(hw.port.readable.locked, false);
  assert.equal(session.state, 'idle');
  const next = hardware();
  hw.serial.requestPort = async () => next.port;
  await session.connect();
  next.controller.error(new Error('USB removed'));
  await flush();
  assert.equal(session.state, 'idle');
  assert.equal(next.port.readable.locked, false);
  assert.equal(errors.length, 1);
});
