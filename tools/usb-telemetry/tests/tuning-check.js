// Playwright CLI run-code --filename; simulates a bidirectional USB CDC board.
async (page) => {
  const check=(value,message)=>{if (!value) throw new Error(message);};
  const errors=[],saved=[];
  let failSync=true;
  page.on('pageerror',error=>errors.push(error.message));
  await page.route('**/api/tuning',async route=>{
    saved.push(route.request().postDataJSON());
    await route.fulfill({status:failSync ? 500 : 200,contentType:'application/json',body:JSON.stringify(failSync ? {error:'模拟文件占用'} : {saved:true})});
    failSync=false;
  });
  await page.addInitScript(()=>{
    const board={
      pid:{revision:1,mode:0,values:Array.from({length:4},()=>[.75,0,0,15000,3000,0]).flat()},
      power:{revision:1,mode:3,values:[1,1.5,.03,5,.3,8,60,1,10,.5,1,100,200,5,.1,.3,100,30,2]},
      spin:{revision:1,mode:2,values:[800,200,6000,40,400,2000,.1,1,.2,1,200,4,1,400,2000,1]}
    };
    window.__tuningBoard=board;window.__tuningWrites=[];
    class FakePort {
      async open() {
        this.readable=new ReadableStream({start:c=>{this.controller=c;}});
        this.writable=new WritableStream({write:bytes=>{
          const line=new TextDecoder().decode(bytes).trim();window.__tuningWrites.push(line);
          const [,id,operation,group,revision,...cells]=line.split(','),record=board[group];
          if (window.__dropTuningReply) return;
          if (operation==='SET') {
            if (window.__rejectTuning) {this.push('@T1,'+id+',ERR,'+group+',constraint\r\n');return;}
            if (record.revision!==Number(revision)) {this.push('@T1,'+id+',ERR,'+group+',conflict\r\n');return;}
            record.values=cells.map(TelemetryTuning.decodeFloat);++record.revision;
          }
          this.push(['@T1',id,'OK',group,record.revision,record.mode,...record.values.map(TelemetryTuning.encodeFloat)].join(',')+'\r\n');
        }});
      }
      push(text) {
        const bytes=new TextEncoder().encode(text);
        for (let i=0;i<bytes.length;i+=3) this.controller.enqueue(bytes.slice(i,i+3));
      }
      async setSignals() {}
      async close() {
        if (this.readable.locked || this.writable.locked) throw new Error('Port lock leaked');
      }
    }
    const serial=new EventTarget();
    serial.requestPort=async()=>{window.__tuningPort=new FakePort();return window.__tuningPort;};
    Object.defineProperty(navigator,'serial',{configurable:true,value:serial});
  });
  await page.goto('http://127.0.0.1:4173');
  await page.setViewportSize({width:1440,height:1080});
  await page.locator('#tuning-tab').click();
  check(await page.locator('#monitor-panel').isHidden(),'Monitor must have a separate panel');
  check(await page.locator('#tuning-apply').isDisabled(),'Offline tuning must be disabled');
  await page.locator('#connect-button').click();
  await page.waitForFunction(()=>document.querySelector('#tuning-status').textContent.includes('已读取'));
  check(await page.locator('#tuning-fields input').count()===24,'All four wheel PID fields must be editable');
  await page.locator('#tuning-value-0').fill('1.25');
  check(await page.locator('#tuning-value-0').evaluate(n=>n.nextElementSibling.textContent.includes('0.75')),'Draft must not change confirmed value');
  await page.locator('#tuning-apply').click();
  await page.waitForFunction(()=>document.querySelector('#tuning-status').textContent.includes('文件同步失败'));
  check(await page.evaluate(()=>__tuningBoard.pid.values[0]===1.25),'PID did not reach board');
  check(saved[0].values[0]===1.25,'File sync must use acknowledged board values');
  await page.evaluate(()=>{__tuningBoard.pid.values[0]=1.5;++__tuningBoard.pid.revision;});
  await page.locator('#tuning-read').click();
  await page.waitForFunction(()=>document.querySelector('#tuning-status').textContent.includes('已读取'));
  await page.locator('#tuning-sync').click();
  await page.waitForFunction(()=>document.querySelector('#tuning-status').textContent.includes('文件已同步'));
  check(saved.at(-1).values[0]===1.5,'Retry after GET must save the latest confirmed snapshot');
  check(await page.evaluate(()=>__tuningWrites.filter(line=>line.includes(',SET,')).length===1),'File retry must not apply the board twice');
  await page.locator('#tuning-value-4').fill('16000');
  await page.locator('#tuning-apply').click();
  check((await page.locator('#tuning-status').innerText()).includes('范围'),'Invalid drafts must be rejected');
  await page.locator('#tuning-revert').click();
  check(await page.locator('#tuning-value-4').inputValue()==='3000','Revert should restore confirmed values');
  await page.locator('[data-tuning-group=power]').click();
  await page.waitForFunction(()=>document.querySelector('#tuning-status').textContent.includes('已读取'));
  check(await page.locator('#tuning-fields input').count()===19,'Power field count mismatch');
  await page.locator('#tuning-value-0').fill('2');
  await page.locator('#tuning-apply').click();
  await page.waitForFunction(()=>document.querySelector('#tuning-status').textContent.includes('文件已同步'));
  check(saved.at(-1).group==='power','Power file sync missing');
  await page.locator('[data-tuning-group=spin]').click();
  await page.waitForFunction(()=>document.querySelector('#tuning-status').textContent.includes('已读取'));
  check((await page.locator('#tuning-mode').innerText()).includes('正弦'),'Compiled spin mode must be visible');
  check(await page.locator('#tuning-value-4').count()===0,'Inactive adaptive controls should not clutter sine mode');
  check(await page.locator('#tuning-value-3').count()===1,'Sine must expose its power regulation gain');
  await page.locator('#tuning-value-11').fill('6');
  await page.locator('#tuning-apply').click();
  await page.waitForFunction(()=>document.querySelector('#tuning-status').textContent.includes('文件已同步'));
  check(saved.at(-1).values[11]===6,'Spin period sync missing');
  await page.evaluate(()=>{window.__rejectTuning=true;});
  await page.locator('#tuning-value-11').fill('5');
  await page.locator('#tuning-apply').click();
  await page.waitForFunction(()=>document.querySelector('#tuning-status').textContent.includes('拒绝'));
  check(await page.evaluate(()=>__tuningBoard.spin.values[11]===6),'Rejected update changed board');
  check(saved.at(-1).values[11]===6,'Rejected update changed source file');
  await page.evaluate(()=>{window.__rejectTuning=false;window.__dropTuningReply=true;});
  await page.locator('#tuning-read').click();
  await page.waitForFunction(()=>document.querySelector('#tuning-status').textContent.includes('超时'));
  await page.evaluate(()=>{window.__dropTuningReply=false;});
  await page.locator('#tuning-read').click();
  await page.waitForFunction(()=>document.querySelector('#tuning-status').textContent.includes('已读取'));
  for (const width of [1440,1000,390,320]) {
    await page.setViewportSize({width,height:1080});
    check(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'Horizontal overflow at '+width);
    if (width===1440 || width===390) await page.screenshot({path:'output/playwright/tuning-'+width+'.png',fullPage:true});
  }
  check(await page.evaluate(()=>telemetryApp.parser.stats.bad===0),'Control replies must not count as malformed telemetry');
  await page.locator('#connect-button').click();
  await page.waitForFunction(()=>telemetryApp.session.state==='idle');
  check(await page.locator('#tuning-apply').isDisabled(),'Disconnect should disable tuning');
  await page.locator('#connect-button').click();
  await page.waitForFunction(()=>document.querySelector('#tuning-status').textContent.includes('已读取'));
  check(await page.locator('#tuning-value-11').inputValue()==='6','Reconnect must read actual board values');
  await page.locator('#connect-button').click();
  check(errors.length===0,'Browser errors: '+errors.join(', '));
  return {passed:true,synchronized:saved.length};
}
