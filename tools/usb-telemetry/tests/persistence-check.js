// Playwright CLI run-code --filename; simulated Flash and bidirectional serial.
async (page) => {
  const check=(value,message)=>{if (!value) throw new Error(message);};
  const errors=[];
  page.on('pageerror',error=>errors.push(error.message));
  await page.route('**/api/tuning',route=>route.fulfill({status:200,contentType:'application/json',body:'{"saved":true}'}));
  await page.addInitScript(()=>{
    const initial=Array.from({length:4},()=>[.75,0,0,15000,3000,0]).flat();
    window.__saved=initial.slice();
    window.__board={values:initial,revision:1,storage:'saved'};
    window.__commands=[];window.__stopped=false;window.__failSave=false;
    class Port {
      async open() {
        if (window.__reboot) {
          __board.values=__saved.slice();__board.storage='saved';__board.revision=1;
          window.__reboot=false;
        }
        this.readable=new ReadableStream({start:c=>{this.controller=c;}});
        this.writable=new WritableStream({write:bytes=>{
          const line=new TextDecoder().decode(bytes).trim();__commands.push(line);
          const [,id,operation,group,revision,...fields]=line.split(',');
          if (operation==='SET') {
            __board.values=fields.map(TelemetryTuning.decodeFloat);
            ++__board.revision;__board.storage='pending';
          }
          if (operation==='SAVE') {
            if (Number(revision)!==__board.revision) throw new Error('Wrong retry revision');
            __board.storage='pending';
          }
          if (operation==='GET' && __board.storage==='pending' && __stopped) {
            if (__failSave) __board.storage='error';
            else {window.__saved=__board.values.slice();__board.storage='saved';}
          }
          const reply=['@T1',id,'OK',group,__board.revision,0,...__board.values.map(TelemetryTuning.encodeFloat),__board.storage].join(',')+'\r\n';
          const data=new TextEncoder().encode(reply);
          for (let i=0;i<data.length;i+=5) this.controller.enqueue(data.slice(i,i+5));
        }});
      }
      async close() {}
      async setSignals() {}
    }
    const serial=new EventTarget();serial.requestPort=async()=>new Port();
    Object.defineProperty(navigator,'serial',{configurable:true,value:serial});
  });
  await page.goto('http://127.0.0.1:4173');
  await page.setViewportSize({width:1440,height:1050});
  await page.locator('#tuning-tab').click();
  await page.locator('#connect-button').click();
  await page.waitForFunction(()=>document.querySelector('#tuning-persistence-status').textContent.startsWith('已保存'));
  await page.locator('#tuning-value-0').fill('1.2');
  await page.locator('#tuning-apply').click();
  await page.waitForFunction(()=>document.querySelector('#tuning-status').textContent.includes('文件已同步'));
  check((await page.locator('#tuning-persistence-status').innerText()).includes('等待板端保存'),'File sync must not imply Flash commit');
  await page.locator('#tuning-value-0').fill('1.3');
  await page.evaluate(()=>{window.__stopped=true;});
  await page.waitForFunction(()=>document.querySelector('#tuning-persistence-status').textContent.startsWith('已保存'));
  check(await page.locator('#tuning-value-0').inputValue()==='1.3','Persistence polling replaced an unapplied draft');
  await page.evaluate(()=>{window.__failSave=true;});
  await page.locator('#tuning-apply').click();
  await page.waitForFunction(()=>document.querySelector('#tuning-persistence-status').textContent.includes('保存失败'));
  const sets=await page.evaluate(()=>__commands.filter(line=>line.includes(',SET,')).length);
  await page.locator('#tuning-value-0').fill('1.4');
  await page.evaluate(()=>{window.__failSave=false;});
  await page.locator('#tuning-save').click();
  await page.waitForFunction(()=>document.querySelector('#tuning-persistence-status').textContent.startsWith('已保存'));
  check(await page.locator('#tuning-value-0').inputValue()==='1.4','Retry must preserve the draft');
  check(await page.evaluate(()=>__commands.filter(line=>line.includes(',SET,')).length)===sets,'Retry must only save the confirmed revision');
  await page.setViewportSize({width:320,height:900});
  check(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'Persistence status overflows mobile');
  await page.locator('#connect-button').click();
  await page.waitForFunction(()=>telemetryApp.session.state==='idle');
  await page.evaluate(()=>{window.__reboot=true;__board.values[0]=999;});
  await page.locator('#connect-button').click();
  await page.waitForFunction(()=>document.querySelector('#tuning-status').textContent.includes('已读取'));
  check(await page.locator('#tuning-value-0').inputValue()==='1.3','Reboot must restore the last committed board value');
  await page.locator('#connect-button').click();
  check(!errors.length,'Browser errors: '+errors.join(', '));
  return {passed:true,cases:['pending separate from file sync','stopped commit','draft preserved while polling','save failure','retry confirmed revision','simulated reboot','320px layout']};
}
