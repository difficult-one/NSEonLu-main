// Run against the local dashboard with Playwright CLI run-code --filename.
async (page) => {
  const check = (condition, message) => { if (!condition) throw new Error(message); };
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  await page.addInitScript(() => {
    class FakePort {
      async open() {
        this.readable = new ReadableStream({start: controller => {this.controller = controller;}});
        this.signals = [];
      }
      async setSignals(signals) { this.signals.push(signals); }
      getInfo() {return {usbVendorId: 0x0483, usbProductId: 0x00ca};}
      push(text) {
        const bytes = new TextEncoder().encode(text);
        for (let i = 0; i < bytes.length; i += 7) this.controller.enqueue(bytes.slice(i, i + 7));
      }
      async close() {
        if (this.readable && this.readable.locked) throw new Error('Reader was not released before port.close');
        this.closed = true;
        this.readable = null;
      }
      unplug() {this.controller.error(new DOMException('USB detached', 'NetworkError'));}
    }
    const provider = new EventTarget();
    provider.requestPort = async () => {
      if (window.__cancelUsbSelection) {
        window.__cancelUsbSelection = false;
        throw new DOMException('User cancelled', 'NotFoundError');
      }
      window.__usbPort = new FakePort();
      return window.__usbPort;
    };
    Object.defineProperty(navigator, 'serial', {configurable: true, value: provider});
    window.__sendUsbSample = (seq, tick, meter = 1234, extras = {}) => {
      const values = Object.fromEntries(TelemetryProtocol.FIELDS.map(key => [key, 0]));
      Object.assign(values, {version: 1, seq, tick, chassis_valid: 1, chassis_tick_ms: tick,
        chassis_mode: 2, vx_cmd_milli: -1250, rc_online: 1, rc_lx: -660, rc_ly: 330,
        rc_sw_left: 1, rc_sw_right: 3, keys: 32769, model_tick_ms: tick,
        motor_online_mask: 15, motor_enabled_mask: 15, rpm_lf: -1234,
        target_lf_rpm: -1500, i_lf_cmd: -120, i_lf_fb: -110, power_valid: 1,
        p_unlimited_cw: 8000, p_limited_cw: 6000, effective_limit_cw: 6000,
        p_meter_cw: meter, meter_online: 1, nominal_budget_cw: 6200, target_power_cw: 6000,
        referee_online: 1, referee_limit_cw: 6000, buffer_j: 42,
        buffer_control_enabled: 1, buffer_estimate_valid: 1, buffer_lower_cj: 4123,
        buffer_predicted_cj: 3999, cap_online: 1, cap_output_enabled: 1,
        cap_energy_pm: 750, cap_chassis_cw: 1278, cap_limit_cw: 12000,
        cap_input_online: 1, cap_input_cw: 5678}, extras);
      window.__usbPort.push(TelemetryProtocol.FIELDS.map(key => values[key]).join(',') + '\r\n');
    };
  });
  await page.goto('http://127.0.0.1:4173');
  await page.setViewportSize({width: 1440, height: 1050});
  const idlePresses = await page.evaluate(() => new Promise(resolve => {
    const start = performance.now(), counts = [];
    function observe(now) {
      counts.push(document.querySelectorAll('#keyboard .pressed, .mouse-key.pressed').length);
      if (now - start >= 300) resolve(counts);
      else requestAnimationFrame(observe);
    }
    requestAnimationFrame(observe);
  }));
  check(idlePresses.every(count => count === 0), 'Disconnected keyboard/mouse flashed pressed: ' + idlePresses.join(','));
  await page.evaluate(() => {window.__cancelUsbSelection = true;});
  await page.locator('#connect-button').click();
  await page.waitForFunction(() => telemetryApp.session.state === 'idle');
  check(await page.evaluate(() => telemetryApp.store.stats.received === 0), 'Cancelled selection added data');

  await page.locator('#connect-button').click();
  await page.waitForFunction(() => telemetryApp.session.state === 'connected');
  await page.evaluate(() => {
    if (!__usbPort.signals.some(signal => signal.dataTerminalReady === true)) throw new Error('DTR not set');
    __usbPort.push(TelemetryProtocol.HEADER + '\r\n');
    __sendUsbSample(1, 1000, 1234, {keys: 4, mouse_left: 1, mouse_right: 1, power_valid: 0});
  });
  await page.waitForFunction(() => document.querySelector('#metric-meter').textContent.includes('12.34'));
  check((await page.locator('#metric-buffer').innerText()).includes('42'), 'Buffer J was not rendered');
  check((await page.locator('#metric-cap').innerText()).includes('75'), 'Cap ratio was not converted');
  check((await page.locator('#motor-lf').innerText()).includes('在线') && (await page.locator('#motor-lf').innerText()).includes('-1234'), 'Invalid power model must not hide live motor feedback');
  check(await page.locator('#keyboard').evaluate(node => {
    const key = name => [...node.children].find(child => child.textContent === name);
    return key('D').classList.contains('pressed') && !key('A').classList.contains('pressed');
  }), 'Remote keyboard bit 2 must light D, not A');
  check(await page.locator('.mouse-key.pressed').count() === 2, 'Valid mouse buttons must light up');

  await page.locator('#pause-button').click();
  const frozen = await page.locator('#metric-meter').innerText();
  await page.evaluate(() => {__sendUsbSample(2, 1020, 2222);});
  await page.waitForFunction(() => telemetryApp.store.stats.received === 2);
  check((await page.locator('#metric-meter').innerText()) === frozen, 'Pause failed to freeze display');
  await page.locator('#pause-button').click();
  await page.waitForFunction(() => document.querySelector('#metric-meter').textContent.includes('22.22'));

  await page.evaluate(() => {__sendUsbSample(4, 1040, 9999, {meter_online: 0});});
  await page.waitForFunction(() => document.querySelector('#metric-meter').textContent.includes('—'));
  check(await page.evaluate(() => telemetryApp.store.stats.missing === 1), 'Sequence gap was not counted');
  await page.waitForFunction(() => telemetryApp.store.isStale(performance.now()));
  await page.waitForFunction(() => /陈旧|超时|无新/.test(document.querySelector('#connection-status').textContent));
  check(await page.locator('#keyboard .pressed, .mouse-key.pressed').count() === 0, 'Stale remote buttons must be cleared');
  await page.evaluate(() => {__sendUsbSample(5, 1600);});
  await page.waitForFunction(() => document.querySelector('#metric-meter').textContent.includes('12.34'));

  const downloadEvent = page.waitForEvent('download');
  await page.locator('#export-button').click();
  const download = await downloadEvent;
  const stream = await download.createReadStream();
  let csv = '';
  for await (const chunk of stream) csv += chunk.toString();
  check(csv.startsWith('version,seq,tick,'), 'Export lacks protocol header');
  check(csv.includes('1234'), 'Export did not preserve raw centiwatts');

  await page.evaluate(() => {__usbPort.unplug();});
  await page.waitForFunction(() => telemetryApp.session.state === 'idle');
  check(await page.evaluate(() => __usbPort.closed), 'Unplug did not release the port');
  await page.locator('#connect-button').click();
  await page.waitForFunction(() => telemetryApp.session.state === 'connected');
  await page.evaluate(() => {__usbPort.push(TelemetryProtocol.HEADER + '\r\n'); __sendUsbSample(1, 5);});
  await page.waitForFunction(() => telemetryApp.store.latest && telemetryApp.store.latest.tick === 5);
  await page.locator('#connect-button').click();
  await page.waitForFunction(() => telemetryApp.session.state === 'idle');
  await page.locator('#demo-button').click();
  await page.waitForFunction(() => telemetryApp.mode === 'demo' && telemetryApp.store.samples.length > 1000);
  await page.waitForFunction(() => !document.querySelector('#metric-meter').textContent.includes('—'));
  check((await page.locator('#connection-status').innerText()).includes('演示'), 'Demo is not labelled');
  check(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), 'Desktop overflows');
  await page.screenshot({path: 'output/playwright/usb-dashboard-desktop.png', fullPage: true});
  await page.setViewportSize({width: 375, height: 812});
  await page.waitForFunction(() => document.documentElement.scrollWidth <= innerWidth);
  await page.screenshot({path: 'output/playwright/usb-dashboard-mobile.png', fullPage: true});
  check(errors.length === 0, 'Browser page errors: ' + errors.join('; '));
  return {passed: true, cases: ['disconnected buttons stay off', 'cancel', 'Web Serial chunks', 'DTR', 'W/J/% units', 'motor health independent of model', 'keyboard bit mapping', 'mouse buttons', 'pause keeps receiving', 'offline', 'sequence gap', 'stale buttons clear', 'CSV export', 'unplug', 'reconnect', 'demo', '375px layout']};
}
