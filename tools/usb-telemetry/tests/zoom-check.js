// Run against the local dashboard with Playwright CLI run-code --filename.
async (page) => {
  const check = (condition, message) => {if (!condition) throw new Error(message);};
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  await page.goto('http://127.0.0.1:4173');
  await page.setViewportSize({width: 1440, height: 1050});
  await page.locator('#demo-button').click();
  await page.waitForFunction(() => telemetryApp.store.samples.length > 1000);
  await page.locator('#pause-button').click();
  await page.getByRole('button', {name: '放大功率观测', exact: true}).click();
  await page.waitForFunction(() => document.querySelector('#expanded-chart').getAttribute('aria-label').startsWith('W'));
  const box = await page.locator('#expanded-chart').boundingBox();
  const point = {x: box.x + 42 + (box.width - 54) * .4, y: box.y + 80};
  const wheel = (deltaY, ctrlKey = true, target = '#expanded-chart', deltaMode = 0) => page.evaluate(({point, deltaY, ctrlKey, target, deltaMode}) => {
    const event = new WheelEvent('wheel', {bubbles: true, cancelable: true, ctrlKey, deltaY, deltaMode, clientX: point.x, clientY: point.y});
    return !document.querySelector(target).dispatchEvent(event);
  }, {point, deltaY, ctrlKey, target, deltaMode});
  const span = () => page.locator('#expanded-chart').getAttribute('aria-label').then(label => Number(label.match(/显示 ([\d.]+) 秒/)[1]));
  const heading = () => page.locator('#expanded-chart').locator('..').locator('.chart-tooltip>strong').innerText();
  const geometry = () => page.evaluate(() => ({width: innerWidth, height: innerHeight, ratio: devicePixelRatio, scale: visualViewport.scale}));
  const beforeGeometry = await geometry();
  await page.mouse.move(point.x, point.y);
  const anchoredReading = await heading();
  check(await wheel(-120), 'Ctrl wheel must cancel browser page zoom inside the expanded chart');
  check(await span() < 60 && await span() > .1, 'Ctrl wheel must zoom the waveform');
  check(await heading() === anchoredReading, 'Wheel zoom must preserve the sample under the mouse');
  check(await page.evaluate(() => telemetryApp.paused), 'Zoom must not resume paused display');
  check(await page.locator('#power-chart').getAttribute('aria-label').then(label => label.includes('显示 60 秒')), 'Overview must keep its 60-second range');

  const zoomedSpan = await span();
  check(!await wheel(-120, false), 'Plain wheel must keep default scrolling');
  check(await span() === zoomedSpan, 'Plain wheel changed the waveform range');
  check(await wheel(-120, true, '#expanded-chart-title'), 'Ctrl wheel over the dialog heading must block page zoom');
  check(await span() === zoomedSpan, 'Heading wheel must not shift the waveform range');

  // Exercise a real Ctrl + wheel gesture as well as cancellable synthetic events.
  await page.keyboard.down('Control');
  await page.mouse.wheel(0, -120);
  await page.keyboard.up('Control');
  await page.waitForFunction(before => Number(document.querySelector('#expanded-chart').getAttribute('aria-label').match(/显示 ([\d.]+) 秒/)[1]) < before, zoomedSpan);
  check(JSON.stringify(await geometry()) === JSON.stringify(beforeGeometry), 'Ctrl wheel changed browser page scale or viewport');
  for (let index = 0; index < 8; index++) await wheel(-120);
  await page.screenshot({path: 'output/playwright/usb-chart-zoomed.png'});

  await page.keyboard.press('Control+0');
  check(await span() === 60, 'Ctrl+0 must restore the full range');
  check(await page.locator('#reset-chart-button').isDisabled(), 'Full range should disable reset');
  await page.keyboard.press('Control+=');
  check(await span() === 48, 'Ctrl+plus must zoom the waveform around its center');
  await page.keyboard.press('Control+-');
  check(await span() === 60, 'Ctrl+minus must zoom out');
  check(JSON.stringify(await geometry()) === JSON.stringify(beforeGeometry), 'Keyboard shortcuts changed browser page scale');

  for (let index = 0; index < 12; index++) check(await wheel(-1000), 'Minimum zoom must still cancel browser default');
  check(await span() === .1, 'Minimum time range must be 100ms');
  for (let index = 0; index < 12; index++) check(await wheel(1000), 'Maximum zoom must still cancel browser default');
  check(await span() === 60, 'Maximum time range must be 60s');
  await wheel(-3, true, '#expanded-chart', 1);
  check(await span() < 60, 'Line-mode wheel did not zoom');
  await page.locator('#reset-chart-button').click();
  check(await span() === 60, 'Reset button did not restore full range');

  // A zoom boundary may cut a segment gap while its other endpoint lies off-screen.
  for (const [times, fraction] of [[[8800, 9200], .1], [[9800, 10200], .9]]) {
    await page.evaluate(times => {
      const samples = times.map((arrivalMs, index) => ({arrivalMs, tick: arrivalMs, segment: index, normalized: {values: {p_meter_cw: 12.34}}}));
      TelemetryCharts.drawChart(document.querySelector('#expanded-chart'), samples, [{key: 'p_meter_cw', name: '功率计实测', color: '#3478f6'}], {now: 20000, unit: 'W', range: [-11000, -10000]});
    }, times);
    await page.mouse.move(box.x + 42 + (box.width - 54) * fraction, point.y);
    check(!await page.locator('#expanded-chart').locator('..').locator('.chart-tooltip').isVisible(), 'Zoom boundary inside a communication gap must not borrow an off-segment reading');
  }
  await page.keyboard.press('Control+0');

  await wheel(-120);
  await page.keyboard.press('Escape');
  check(!await wheel(-120, true, '#power-chart'), 'Closed dialog still captured Ctrl wheel');
  check(await page.evaluate(() => document.body.dispatchEvent(new KeyboardEvent('keydown', {key: '+', ctrlKey: true, bubbles: true, cancelable: true}))), 'Closed dialog still captured browser keyboard shortcuts');
  for (const title of ['缓冲能量', '四轮转速']) {
    await page.getByRole('button', {name: '放大' + title, exact: true}).click();
    await page.waitForFunction(() => document.querySelector('#reset-chart-button').disabled);
    check(await span() === 60, 'Reopened chart retained the previous chart range');
    check(await wheel(-120), title + ' did not capture Ctrl wheel');
    check(await span() < 60, title + ' did not zoom');
    await page.keyboard.press('Escape');
  }

  await page.locator('#pause-button').click();
  await page.getByRole('button', {name: '放大功率观测', exact: true}).click();
  await wheel(-120);
  const liveSpan = await span();
  await page.mouse.move(point.x, point.y);
  const liveHeading = await heading();
  await page.waitForFunction(previous => document.querySelector('#expanded-chart').parentElement.querySelector('.chart-tooltip>strong').textContent !== previous, liveHeading);
  check(await span() === liveSpan, 'New live samples reset zoom');
  await page.setViewportSize({width: 375, height: 812});
  await page.waitForFunction(() => {
    const canvas = document.querySelector('#expanded-chart');
    return canvas.width === Math.round(canvas.getBoundingClientRect().width * Math.min(devicePixelRatio || 1, 2));
  });
  check(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), 'Zoom controls overflow narrow viewport');
  await page.screenshot({path: 'output/playwright/usb-chart-zoomed-mobile.png'});
  await page.keyboard.press('Escape');
  check(errors.length === 0, 'Browser errors: ' + errors.join('; '));
  return {passed: true, cases: ['Ctrl wheel default cancelled', 'mouse anchor', 'unchanged page scale', 'paused zoom', 'overview unchanged', 'plain wheel', 'Ctrl shortcuts/reset', '100ms/60s bounds', 'zoom boundary gaps', 'all three charts', 'closed dialog defaults', 'live scrolling', '375px controls']};
}
