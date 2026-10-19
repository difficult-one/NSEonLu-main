// Run with: playwright-cli run-code --filename tools/waveform-viewer/tests/browser-check.js
async (page) => {
  const check = (condition, message) => { if (!condition) throw new Error(message); };
  await page.context().setOffline(true);
  await page.reload();
  check((await page.title()).includes('波形'), 'The local waveform page must load');
  const csv = 'tick,p_meter_cw,p_filtered_cw\n0,0,1\n10,0,2\n20,10,3\n30,0,4\n40,0,5';
  await page.getByLabel('CSV 数据').fill(csv);
  await page.getByRole('button', { name: '读取并绘图', exact: true }).click();
  await page.waitForFunction(() => document.querySelector('#data-status').textContent.includes('5 行'));
  check(await page.getByLabel('显示 p_meter_cw', { exact: true }).isChecked(), 'First non-time numeric field is selected');
  check(!(await page.getByLabel('对 p_meter_cw 额外滤波', { exact: true }).isChecked()), 'Extra filtering must default off');
  check(!(await page.getByLabel('对 p_filtered_cw 额外滤波', { exact: true }).isChecked()), 'Already-filtered values must default unchanged');
  await page.getByLabel('显示 p_filtered_cw', { exact: true }).check();
  await page.waitForFunction(() => document.querySelectorAll('#legend .legend-item').length === 2);
  await page.getByLabel('对 p_meter_cw 额外滤波', { exact: true }).check();
  await page.getByLabel('p_meter_cw 滤波窗口', { exact: true }).fill('3');
  await page.getByLabel('p_meter_cw 滤波窗口', { exact: true }).press('Tab');
  const chart = await page.locator('#waveform-chart').boundingBox();
  check(chart.width > 100 && chart.height > 100, 'Canvas has a usable size');
  // The midpoint is the original sample x=20. Tooltip must use full original data.
  const plotMiddle = chart.x + 72 + (chart.width - 96) / 2;
  await page.mouse.move(plotMiddle, chart.y + chart.height / 2);
  await page.waitForFunction(() => !document.querySelector('#tooltip').hidden);
  const tooltip = await page.locator('#tooltip').innerText();
  check(tooltip.includes('原始值 10') && tooltip.includes('滤波值 3.333'), 'Tooltip must show original and newly filtered values');
  const beforeZoom = await page.locator('#visible-range').textContent();
  await page.mouse.wheel(0, -600);
  await page.waitForFunction(previous => document.querySelector('#visible-range').textContent !== previous, beforeZoom);
  const afterZoom = await page.locator('#visible-range').textContent();
  await page.mouse.down();
  await page.mouse.move(plotMiddle + 50, chart.y + chart.height / 2, { steps: 6 });
  await page.mouse.up();
  await page.waitForFunction(previous => document.querySelector('#visible-range').textContent !== previous, afterZoom);
  await page.getByRole('button', { name: '恢复全图', exact: true }).click();
  await page.waitForFunction(previous => document.querySelector('#visible-range').textContent === previous, beforeZoom);
  await page.getByLabel('对 p_meter_cw 额外滤波', { exact: true }).uncheck();
  await page.getByLabel('显示采样点', { exact: true }).check();
  await page.getByLabel('横轴', { exact: true }).selectOption('-1');
  await page.waitForFunction(() => document.querySelector('#visible-range').textContent.includes('采样序号'));
  await page.getByLabel('显示 p_meter_cw', { exact: true }).uncheck();
  await page.getByLabel('显示 p_filtered_cw', { exact: true }).uncheck();
  await page.waitForFunction(() => document.querySelector('#chart-empty').textContent.includes('勾选'));
  // Bad rows are reported; a failed subsequent import must leave the current data usable.
  await page.locator('#paste-section summary').click();
  await page.getByLabel('CSV 数据').fill('tick,v\n0,1\n1,2,extra\n2,3');
  await page.getByRole('button', { name: '读取并绘图', exact: true }).click();
  await page.waitForFunction(() => document.querySelector('#data-status').textContent.includes('2 行'));
  check((await page.locator('#import-notice').innerText()).includes('第 3 行'), 'Bad-row positions must be visible');
  await page.locator('#paste-section summary').click();
  await page.getByLabel('CSV 数据').fill('tick,tick\n0,1');
  await page.getByRole('button', { name: '读取并绘图', exact: true }).click();
  await page.waitForFunction(() => document.querySelector('#import-error').textContent.includes('表头'));
  check((await page.locator('#data-status').innerText()).includes('2 行'), 'A rejected import must preserve the current data');
  return { passed: true, cases: ['paste import', 'per-field filtering', 'original tooltip values', 'multi-select', 'zoom', 'pan', 'reset', 'sample markers', 'axis switch', 'bad rows', 'rejected import'] };
}
