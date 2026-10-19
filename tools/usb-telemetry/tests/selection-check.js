// Run against the local dashboard with Playwright CLI run-code --filename.
async (page) => {
  const check = (condition, message) => {if (!condition) throw new Error(message);};
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  await page.goto('http://127.0.0.1:4173');
  await page.setViewportSize({width: 1440, height: 1050});
  const picker = id => page.locator(`main [data-chart="${id}"]`);
  const expandedPicker = page.locator('#expanded-trace-picker');
  check(await picker('rpm-chart').locator('input[type="checkbox"]').count() === 9, 'RPM chart must list eight wheel traces and the chassis Wz target');
  check(await picker('power-chart').locator('input').count() === 5 && await picker('energy-chart').locator('input').count() === 3, 'Each chart must list all of its traces');
  await page.evaluate(() => {
    const {store, view} = telemetryApp, P = TelemetryProtocol, end = performance.now();
    store.clear();view.resume();
    for (let index = 0; index < 3000; index++) {
      const tick = 1000 + index * 20, sample = Object.fromEntries(P.FIELDS.map(key => [key, 0]));
      Object.assign(sample, {version: 1, seq: index, tick, chassis_valid: 1, chassis_tick_ms: tick,
        model_tick_ms: tick, motor_online_mask: 15, motor_enabled_mask: 15, power_valid: 1,
        meter_online: 1, p_meter_cw: 1234, referee_online: 1, buffer_j: 42,
        buffer_estimate_valid: 1, buffer_lower_cj: 4123, buffer_predicted_cj: 3999,
        wz_cmd_milli: -800000, rpm_lf: -1234, target_lf_rpm: -1500,
        rpm_rf: 2345, target_rf_rpm: 2400, rpm_lb: 3456, target_lb_rpm: 3500,
        rpm_rb: -4567, target_rb_rpm: -4600});
      store.add(sample, end - 60000 + index * 20);
    }
    view.pause(end);window.dispatchEvent(new Event('resize'));
  });
  await page.waitForFunction(() => document.querySelector('#rpm-chart').getAttribute('aria-label').includes('3000'));
  const hover = async selector => {
    await page.locator(selector).scrollIntoViewIfNeeded();
    const box = await page.locator(selector).boundingBox();
    const plot = await page.locator(selector).evaluate(canvas => TelemetryCharts.getPlotBounds(canvas));
    const point = {x: box.x + plot.left + plot.width * .5, y: box.y + plot.top + 40};
    await page.mouse.move(point.x, point.y);
    return point;
  };
  const tooltip = selector => page.locator(selector).locator('..').locator('.chart-tooltip');
  await hover('#rpm-chart');
  let text = await tooltip('#rpm-chart').innerText();
  check(text.includes('-1500 rpm') && text.includes('-1234 rpm') && text.includes('-800.000 指令值'), 'Targets, feedback, and Wz must keep their own values and units');
  check(!text.includes('-800 rpm'), 'Wz command must not be labelled as motor rpm');
  check(await page.locator('#rpm-chart').getAttribute('aria-label').then(label => label.includes('Wz 指令')), 'RPM view must identify its independent command axis');

  await picker('rpm-chart').getByRole('button', {name: '清空', exact: true}).click();
  check(!(await page.locator('.motor-plot .chart-legend').innerText()).includes('左前'), 'Overview legend retained cleared traces');
  check(!await page.locator('#chart-dialog').isVisible(), 'Picker click accidentally expanded the card');
  check(await page.locator('#rpm-chart').locator('..').locator('.chart-empty').innerText().then(value => value.includes('请选择')), 'Empty selection needs a prompt');
  await hover('#rpm-chart');
  check(!await tooltip('#rpm-chart').isVisible(), 'Empty selection must hide hover readings');
  await picker('rpm-chart').locator('[data-trace-key="target_lf_rpm"]').check();
  await hover('#rpm-chart');
  text = await tooltip('#rpm-chart').innerText();
  check(text.includes('左前 LF · 目标') && text.includes('-1500 rpm') && !text.includes('实际') && !text.includes('Wz'), 'Only the checked wheel target may appear');

  await page.getByRole('button', {name: '放大四轮转速', exact: true}).click();
  await page.waitForFunction(() => document.querySelector('#expanded-chart').getAttribute('aria-label').includes('1 条曲线'));
  check(await expandedPicker.locator('input:checked').count() === 1, 'Expanded picker did not inherit selection');
  await page.keyboard.press('Control+=');
  const zoomed = await page.locator('#expanded-chart-range').innerText();
  await expandedPicker.getByRole('button', {name: '清空', exact: true}).click();
  await expandedPicker.locator('[data-trace-key="wz_cmd_milli"]').check();
  check(await picker('rpm-chart').locator('input:checked').count() === 1 && await picker('rpm-chart').locator('[data-trace-key="wz_cmd_milli"]').isChecked(), 'Expanded selection did not sync back to overview');
  check(await page.locator('#expanded-chart-range').innerText() === zoomed && await page.evaluate(() => telemetryApp.paused), 'Changing traces reset zoom or resumed paused display');
  check(await page.locator('#expanded-chart-unit').innerText() === '指令值', 'Wz-only view must update its heading unit');
  check(!(await page.locator('#expanded-chart').getAttribute('aria-label')).startsWith('rpm'), 'Wz-only view must not announce an rpm axis');
  check((await page.locator('.motor-plot .chart-legend').innerText()).includes('底盘旋转目标 Wz'), 'Overview legend omitted the selected chassis target');
  await hover('#expanded-chart');
  text = await tooltip('#expanded-chart').innerText();
  check(text.includes('-800.000 指令值') && !text.includes(' rpm'), 'Wz-only view has incorrect units');
  const point = await hover('#expanded-chart');
  const anchor = await tooltip('#expanded-chart').locator('strong').innerText();
  await page.evaluate(point => document.querySelector('#expanded-chart').dispatchEvent(new WheelEvent('wheel', {ctrlKey: true, deltaY: -120, bubbles: true, cancelable: true, clientX: point.x, clientY: point.y})), point);
  check(await page.locator('#expanded-chart-range').innerText() !== zoomed, 'Ctrl wheel did not zoom the right-axis-only view');
  check(await tooltip('#expanded-chart').locator('strong').innerText() === anchor, 'Right-axis wheel zoom moved the sample under the mouse');
  await expandedPicker.getByRole('button', {name: '全选', exact: true}).click();
  check(await expandedPicker.locator('input:checked').count() === 9 && await picker('rpm-chart').locator('input:checked').count() === 9, 'Select all did not sync nine RPM traces');
  await page.screenshot({path: 'output/playwright/usb-trace-selection.png'});

  await page.evaluate(() => {
    const frozen = telemetryApp.view.snapshot(performance.now());
    frozen.samples.forEach(sample => {sample.normalized.values.wz_cmd_milli = null;});
    window.dispatchEvent(new Event('resize'));
  });
  await hover('#expanded-chart');
  await page.waitForFunction(() => {
    const rows = [...document.querySelector('#expanded-chart').parentElement.querySelectorAll('.chart-tooltip-values>div')];
    return rows.some(row => row.textContent.includes('底盘旋转目标') && row.querySelector('b').textContent === '—');
  });
  await page.keyboard.press('Escape');

  for (const [id, title, key, expected] of [['power-chart', '功率观测', 'p_meter_cw', '12.34 W'], ['energy-chart', '缓冲能量', 'buffer_j', '42.00 J']]) {
    await picker(id).getByRole('button', {name: '清空', exact: true}).click();
    await picker(id).locator(`[data-trace-key="${key}"]`).check();
    await page.getByRole('button', {name: '放大' + title, exact: true}).click();
    await page.waitForFunction(() => document.querySelector('#expanded-chart').getAttribute('aria-label').includes('1 条曲线'));
    await hover('#expanded-chart');
    check(await tooltip('#expanded-chart').innerText().then(value => value.includes(expected)), title + ' did not show its selected reading');
    await expandedPicker.getByRole('button', {name: '全选', exact: true}).click();
    await page.keyboard.press('Escape');
  }
  await page.setViewportSize({width: 375, height: 812});
  await page.getByRole('button', {name: '放大四轮转速', exact: true}).click();
  await page.waitForFunction(() => {
    const canvas = document.querySelector('#expanded-chart');
    return canvas.width === Math.round(canvas.getBoundingClientRect().width * Math.min(devicePixelRatio || 1, 2));
  });
  check(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), 'Narrow trace selection overflows viewport');
  check(await expandedPicker.getByRole('checkbox').count() === 9, 'Narrow selector lost fields');
  await page.screenshot({path: 'output/playwright/usb-trace-selection-mobile.png'});
  check(errors.length === 0, 'Browser errors: ' + errors.join('; '));
  return {passed: true, cases: ['all trace lists', 'wheel targets/feedback', 'independent Wz units', 'empty/all selection', 'picker does not expand card', 'overview/dialog sync', 'paused selection', 'zoom preserved', 'right-axis zoom', 'invalid Wz', 'power/energy selection', '375px layout']};
}
