// Run against the local dashboard with Playwright CLI run-code --filename.
async (page) => {
  const check = (condition, message) => {if (!condition) throw new Error(message);};
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  await page.goto('http://127.0.0.1:4173');
  await page.setViewportSize({width: 1440, height: 1050});
  await page.getByRole('button', {name: '放大功率观测', exact: true}).click();
  check(await page.getByRole('dialog', {name: '功率观测'}).isVisible(), 'Empty chart must still open');
  check(await page.locator('#chart-dialog .chart-empty').isVisible(), 'Empty expanded chart needs its waiting state');
  await page.keyboard.press('Escape');
  check(await page.getByRole('button', {name: '放大功率观测', exact: true}).evaluate(node => node === document.activeElement), 'Closing must restore focus');

  const fixture = async (invalidMeter = false) => {
    await page.evaluate(invalid => {
      const {store, view} = telemetryApp, P = TelemetryProtocol, end = performance.now();
      view.resume();store.clear();
      for (let index = 0; index < 3000; index++) {
        const tick = 1000 + index * 20;
        const sample = Object.fromEntries(P.FIELDS.map(key => [key, 0]));
        Object.assign(sample, {version: 1, seq: index, tick, chassis_valid: 1, chassis_tick_ms: tick,
          model_tick_ms: tick, motor_online_mask: 15, motor_enabled_mask: 15,
          power_valid: 1, p_meter_cw: 1234, meter_online: invalid ? 0 : 1,
          p_unlimited_cw: 8000, p_limited_cw: 6000, effective_limit_cw: 6000, target_power_cw: 6200,
          referee_online: 1, buffer_j: 42, buffer_estimate_valid: 1, buffer_lower_cj: 4123,
          buffer_predicted_cj: 3999, rpm_lf: -1234, target_lf_rpm: -1500,
          rpm_rf: 2345, target_rf_rpm: 2400, rpm_lb: 3456, target_lb_rpm: 3500,
          rpm_rb: -4567, target_rb_rpm: -4600});
        store.add(sample, end - 60000 + index * 20);
      }
      view.pause(end);
      window.dispatchEvent(new Event('resize'));
    }, invalidMeter);
    await page.waitForFunction(invalid => document.querySelector('#power-chart').getAttribute('aria-label').includes('3000') && document.querySelector('#metric-meter').textContent.includes(invalid ? '—' : '12.34'), invalidMeter);
  };
  const hover = async (selector, fraction = .5) => {
    await page.locator(selector).scrollIntoViewIfNeeded();
    const box = await page.locator(selector).boundingBox();
    const plot = await page.locator(selector).evaluate(canvas => TelemetryCharts.getPlotBounds(canvas));
    const point = {x: box.x + plot.left + plot.width * fraction, y: box.y + plot.top + 45};
    await page.mouse.move(point.x, point.y);
    return point;
  };
  const tooltip = selector => page.locator(selector).locator('..').locator('.chart-tooltip');
  const checkReadings = async (selector, expected) => {
    await hover(selector);
    check(await tooltip(selector).isVisible(), selector + ' must show hover readings');
    const text = await tooltip(selector).innerText();
    for (const value of expected) check(text.includes(value), selector + ' lacks reading: ' + value + ' in ' + text);
  };
  await fixture();
  await checkReadings('#power-chart', ['功率计实测', '12.34 W', '80.00 W', '距窗口末端 30.00 s', 'tick 31000 ms']);
  await checkReadings('#energy-chart', ['裁判原始值', '42.00 J', '41.23 J', '39.99 J']);
  await checkReadings('#rpm-chart', ['左前 LF · 目标', '-1500 rpm', '-1234 rpm', '2345 rpm', '-4567 rpm']);
  const rpmTooltip = await tooltip('#rpm-chart').boundingBox();
  check(rpmTooltip && rpmTooltip.y >= 0 && rpmTooltip.y + rpmTooltip.height <= 1050, 'All eight RPM readings must remain visible');
  await page.screenshot({path: 'output/playwright/usb-chart-rpm-hover.png'});
  await hover('#power-chart');
  await page.mouse.move(5, 5);
  check(!await tooltip('#power-chart').isVisible(), 'Leaving plot must hide readings');

  for (const [id, title, expected] of [['power-chart', '功率观测', '12.34 W'], ['energy-chart', '缓冲能量', '41.23 J'], ['rpm-chart', '四轮转速', '-1234 rpm']]) {
    const original = await page.locator('#' + id).boundingBox();
    await page.getByRole('button', {name: '放大' + title, exact: true}).click();
    await page.waitForFunction(unit => document.querySelector('#expanded-chart').width > 900 && document.querySelector('#expanded-chart').getAttribute('aria-label').startsWith(unit), id === 'power-chart' ? 'W' : id === 'energy-chart' ? 'J' : 'rpm');
    const expanded = await page.locator('#expanded-chart').boundingBox();
    check(expanded.width > original.width && expanded.height > original.height, title + ' did not enlarge');
    await checkReadings('#expanded-chart', [expected, 'tick 31000 ms']);
    check(await page.evaluate(() => telemetryApp.paused), 'Expanding resumed paused display');
    if (id === 'rpm-chart') {
      const pointer = await hover('#expanded-chart', .4);
      await page.setViewportSize({width: 900, height: 700});
      await page.waitForFunction(() => document.querySelector('#expanded-chart').width < 900);
      const guide = await page.locator('#expanded-chart').locator('..').locator('.chart-crosshair').boundingBox();
      check(guide && Math.abs(guide.x - pointer.x) < 1, 'Resized hover must track the stationary mouse position');
      await checkReadings('#expanded-chart', ['-1234 rpm', 'tick 31000 ms']);
      await page.setViewportSize({width: 1440, height: 1050});
    }
    await page.locator('#close-chart-button').click();
    check(!await page.locator('#chart-dialog').isVisible(), 'Close button did not close dialog');
  }

  // The canvas itself supports both click and keyboard expansion.
  await page.locator('#energy-chart').click();
  check(await page.getByRole('dialog', {name: '缓冲能量'}).isVisible(), 'Clicking the chart did not expand it');
  await page.mouse.click(2, 2);
  check(!await page.locator('#chart-dialog').isVisible(), 'Backdrop did not close dialog');
  await page.locator('#rpm-chart').focus();
  await page.keyboard.press('Enter');
  check(await page.getByRole('dialog', {name: '四轮转速'}).isVisible(), 'Keyboard did not expand chart');
  await page.keyboard.press('Escape');
  check(await page.locator('#rpm-chart').evaluate(node => node === document.activeElement), 'Keyboard focus was lost after closing');

  await fixture(true);
  await checkReadings('#power-chart', ['功率计实测', '—', '80.00 W']);
  check(!(await tooltip('#power-chart').innerText()).includes('12.34 W'), 'Offline reading leaked an old value');
  await page.evaluate(() => {
    const {view} = telemetryApp, frozen = view.snapshot(performance.now());
    // A 200ms segment gap must hide readings even though both ends are within 500ms.
    frozen.samples = frozen.samples.filter(sample => sample.arrivalMs <= frozen.at - 30100 || sample.arrivalMs >= frozen.at - 29900);
    frozen.samples.forEach(sample => {if (sample.arrivalMs >= frozen.at - 29900) sample.segment = 1;});
    window.dispatchEvent(new Event('resize'));
  });
  await hover('#power-chart', .2);
  await page.waitForFunction(() => document.querySelector('#power-chart').getAttribute('aria-label').includes('2991'));
  await hover('#power-chart');
  check(!await tooltip('#power-chart').isVisible(), 'Communication segment gap must not borrow a nearby reading');
  await page.evaluate(() => {
    telemetryApp.view.snapshot(performance.now()).at += 1000;
    window.dispatchEvent(new Event('resize'));
  });
  await page.waitForFunction(() => document.querySelector('#power-chart').getAttribute('aria-label').includes('2941'));
  await hover('#power-chart', .999);
  check(!await tooltip('#power-chart').isVisible(), 'Empty tail must not borrow a reading older than 500ms');

  await page.locator('#demo-button').click();
  await page.waitForFunction(() => telemetryApp.mode === 'demo' && !telemetryApp.paused);
  await page.getByRole('button', {name: '放大功率观测', exact: true}).click();
  await hover('#expanded-chart', .98);
  const previous = await tooltip('#expanded-chart').textContent();
  await page.waitForFunction(text => document.querySelector('#expanded-chart').parentElement.querySelector('.chart-tooltip').textContent !== text, previous);
  await page.screenshot({path: 'output/playwright/usb-chart-expanded.png'});
  await page.setViewportSize({width: 375, height: 812});
  await page.waitForFunction(() => {
    const canvas = document.querySelector('#expanded-chart');
    return canvas.width === Math.round(canvas.getBoundingClientRect().width * Math.min(devicePixelRatio || 1, 2));
  });
  await hover('#expanded-chart');
  check(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), 'Expanded mobile layout overflows');
  const box = await tooltip('#expanded-chart').boundingBox();
  check(box && box.x >= 0 && box.x + box.width <= 375, 'Mobile tooltip escapes viewport');
  await page.screenshot({path: 'output/playwright/usb-chart-expanded-mobile.png'});
  await page.keyboard.press('Escape');
  check(errors.length === 0, 'Browser errors: ' + errors.join('; '));
  return {passed: true, cases: ['empty chart', 'hover values and units', 'all three dialogs', 'paused expansion', 'click', 'keyboard/focus', 'close/escape/backdrop', 'offline values', 'communication gap', 'live tooltip', '375px dialog']};
}
