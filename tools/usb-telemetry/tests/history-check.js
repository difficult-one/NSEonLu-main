// Run against the local dashboard with Playwright CLI run-code --filename.
async (page) => {
  const check = (condition, message) => {if (!condition) throw new Error(message);};
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  await page.goto('http://127.0.0.1:4173');
  await page.setViewportSize({width: 1440, height: 1050});
  await page.getByRole('button', {name: '放大功率观测', exact: true}).click();
  check(await page.locator('#history-position').isDisabled() && await page.locator('#return-live-button').isDisabled(), 'Empty history must disable navigation');
  await page.keyboard.press('Escape');
  await page.locator('#demo-button').click();
  await page.waitForFunction(() => telemetryApp.store.samples.length > 25000);
  check(await page.evaluate(() => telemetryApp.store.latest.arrivalMs - telemetryApp.store.samples[0].arrivalMs > 590000), 'Demo must provide ten minutes of navigable history');
  await page.getByRole('button', {name: '放大功率观测', exact: true}).click();
  await page.waitForFunction(() => document.querySelector('#expanded-chart').getAttribute('aria-label').startsWith('W'));
  const canvas = page.locator('#expanded-chart');
  const heading = () => canvas.locator('..').locator('.chart-tooltip>strong').innerText();
  const tick = async () => Number((await heading()).match(/tick (\d+) ms/)[1]);
  const center = async () => {
    await canvas.scrollIntoViewIfNeeded();
    const box = await canvas.boundingBox(), plot = await canvas.evaluate(node => TelemetryCharts.getPlotBounds(node));
    const point = {x: box.x + plot.left + plot.width * .5, y: box.y + plot.top + 50, width: plot.width};
    await page.mouse.move(point.x, point.y);return point;
  };
  const drag = async fraction => {
    const point = await center();
    await page.mouse.down();await page.mouse.move(point.x + point.width * fraction, point.y, {steps: 8});await page.mouse.up();
    await page.mouse.move(point.x, point.y);
  };
  await center();const before = await tick();
  await drag(.2);
  check(await page.evaluate(() => telemetryApp.paused), 'Dragging a live waveform must pause following');
  check(await tick() < before - 1000, 'Dragging paused waveform must reveal earlier samples');
  const frozen = await page.evaluate(() => ({at: telemetryApp.view.snapshot(performance.now()).at, received: telemetryApp.store.stats.received}));
  const frozenHeading = await heading();
  await page.waitForFunction(received => telemetryApp.store.stats.received > received + 5, frozen.received);
  check(await heading() === frozenHeading && await page.evaluate(at => telemetryApp.view.snapshot(performance.now()).at === at, frozen.at), 'Paused axis or sample moved while reception continued');

  await page.locator('#history-position').focus();await page.keyboard.press('Home');
  await center();
  check(await tick() < 120000, 'History slider must reach samples older than the last 60 seconds');
  const oldest = await page.locator('#history-readout').innerText();
  await drag(.2);
  check(await page.locator('#history-readout').innerText() === oldest, 'Drag passed the oldest history boundary');
  await page.locator('#history-position').focus();await page.keyboard.press('End');
  await center();
  check(await tick() > 500000, 'Slider could not return to the newest frozen window');

  await page.keyboard.press('Control+=');await drag(.2);
  const browsed = await page.locator('#history-readout').innerText();
  const zoomed = await page.locator('#expanded-chart-range').innerText();
  await page.locator('#expanded-trace-picker input').first().uncheck();
  check(await page.locator('#history-readout').innerText() === browsed && await page.locator('#expanded-chart-range').innerText() === zoomed, 'Selecting curves lost the historical range');
  await page.keyboard.press('Escape');
  await page.getByRole('button', {name: '放大缓冲能量', exact: true}).click();
  await page.keyboard.press('Escape');
  await page.getByRole('button', {name: '放大功率观测', exact: true}).click();
  check(await page.locator('#history-readout').innerText() === browsed && await page.locator('#expanded-chart-range').innerText() === zoomed, 'Reopening the paused chart lost its position');

  // Pointer cancellation must stop changing the range, even with the mouse held down.
  const point = await center();
  await canvas.evaluate(node => node.addEventListener('pointerdown', event => {window.__historyPointer = event.pointerId;}, {once: true}));
  await page.mouse.down();await page.mouse.move(point.x + 30, point.y, {steps: 3});
  await canvas.evaluate(node => node.dispatchEvent(new PointerEvent('pointercancel', {pointerId: window.__historyPointer, bubbles: true})));
  const cancelled = await page.locator('#history-readout').innerText();
  await page.mouse.move(point.x + 80, point.y);await page.mouse.up();
  check(await page.locator('#history-readout').innerText() === cancelled && !await canvas.evaluate(node => node.classList.contains('panning')), 'Cancelled drag remained active');

  const zoomPoint = await center();
  await page.mouse.down();await page.mouse.move(zoomPoint.x + 30, zoomPoint.y, {steps: 3});
  await page.keyboard.press('Control+=');
  const interruptedZoom = await page.locator('#expanded-chart-range').innerText();
  await page.mouse.move(zoomPoint.x + 70, zoomPoint.y);await page.mouse.up();
  check(await page.locator('#expanded-chart-range').innerText() === interruptedZoom && !await canvas.evaluate(node => node.classList.contains('panning')), 'Zoom during a drag must finish panning rather than restore its old scale');

  await page.locator('#return-live-button').click();
  check(!await page.evaluate(() => telemetryApp.paused), 'Return live did not resume display');
  check(await page.locator('#history-readout').innerText().then(text => text.includes('60.00 s ～ 0.00 s')), 'Return live did not reset the latest 60 seconds');
  await center();const liveHeading = await heading();
  await page.waitForFunction(previous => document.querySelector('#expanded-chart').parentElement.querySelector('.chart-tooltip>strong').textContent !== previous, liveHeading);

  // Check the same navigation with the independent Wz axis.
  await page.keyboard.press('Escape');
  await page.getByRole('button', {name: '放大四轮转速', exact: true}).click();
  await drag(.2);
  await page.locator('#history-position').focus();await page.keyboard.press('Home');
  await center();
  check((await canvas.locator('..').locator('.chart-tooltip').innerText()).includes('底盘旋转目标 Wz'), 'History browsing lost Wz data');
  await page.screenshot({path: 'output/playwright/usb-history-pan.png'});
  await page.setViewportSize({width: 375, height: 812});
  await page.waitForFunction(() => {
    const node = document.querySelector('#expanded-chart');
    return node.width === Math.round(node.getBoundingClientRect().width * Math.min(devicePixelRatio || 1, 2));
  });
  check(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), 'Mobile history controls overflow');
  await page.screenshot({path: 'output/playwright/usb-history-pan-mobile.png'});
  await page.keyboard.press('Escape');
  await page.locator('#pause-button').click();
  check(!await page.evaluate(() => telemetryApp.paused), 'Toolbar resume did not leave history mode');
  await page.getByRole('button', {name: '放大四轮转速', exact: true}).click();
  check(await page.locator('#history-readout').innerText().then(text => text.includes('60.00 s ～ 0.00 s')), 'Toolbar resume retained a stale historical range');
  check(errors.length === 0, 'Browser errors: ' + errors.join('; '));
  return {passed: true, cases: ['empty navigation', '10-minute history', 'drag auto-pauses', 'frozen time keeps receiving', 'old/new slider bounds', 'zoom and selection preserved', 'reopen paused position', 'pointer cancellation', 'zoom interrupts drag', 'return live', 'Wz history', '375px controls', 'toolbar resume']};
}
