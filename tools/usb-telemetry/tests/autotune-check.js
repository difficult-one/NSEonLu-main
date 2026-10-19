async (page)=>{
  const check=(ok,message)=>{if(!ok)throw new Error(message);},errors=[];
  page.on('pageerror',e=>errors.push(e.message));
  await page.goto('http://127.0.0.1:4176');
  await page.getByRole('tab',{name:/自动整定/}).click();
  await page.getByRole('button',{name:'演示整定流程'}).click();
  await page.waitForFunction(()=>document.querySelector('#auto-status').textContent.includes('演示完成'));
  check(await page.locator('#auto-results tr').count()===1,'demo result');
  check(await page.locator('#auto-adopt').isDisabled(),'demo cannot adopt');
  await page.locator('#auto-wheel').selectOption('all');
  await page.getByRole('button',{name:'演示整定流程'}).click();
  await page.waitForFunction(()=>document.querySelector('#auto-trials').children.length>1);
  await page.getByRole('button',{name:'停止并恢复'}).click();
  await page.waitForFunction(()=>!document.querySelector('#auto-demo').disabled);
  check(await page.locator('#auto-adopt').isDisabled(),'cancelled results cannot adopt');
  await page.getByRole('button',{name:'演示整定流程'}).click();
  await page.waitForFunction(()=>document.querySelector('#auto-status').textContent.includes('演示完成'));
  check(await page.locator('#auto-results tr').count()===4,'all four wheels complete independently');
  await page.setViewportSize({width:390,height:844});
  check(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'mobile overflow');
  await page.screenshot({path:'output/playwright/autotune-mobile.png',fullPage:true});
  await page.setViewportSize({width:1440,height:1100});
  await page.locator('#auto-wheel').selectOption('0');
  await page.getByRole('button',{name:'演示整定流程'}).click();
  await page.waitForFunction(()=>document.querySelector('#auto-status').textContent.includes('演示完成'));
  await page.screenshot({path:'output/playwright/autotune-desktop.png',fullPage:true});
  let syncCalls=0,synced;
  await page.route('**/api/tuning',async route=>{
    synced=route.request().postDataJSON();++syncCalls;
    await route.fulfill({status:syncCalls===1?500:200,contentType:'application/json',body:syncCalls===1?'{"error":"simulated disk failure"}':'{"saved":true}'});
  });
  await page.addInitScript(()=>{
    window.__commands=[];window.__board={revision:1,values:Array.from({length:4},()=>[.75,0,0,15000,3000,0]).flat(),storage:'saved'};
    window.__trial={session:0,state:'idle',wheel:0,trial:0,rmse:100};window.__saveReady=false;
    class Port{
      async open(){
        this.readable=new ReadableStream({start:c=>{this.controller=c;}});
        this.writable=new WritableStream({write:bytes=>{
          const line=new TextDecoder().decode(bytes).trim();__commands.push(line);const c=line.split(',');let reply;
          if(c[0]==='@T1'){
            if(c[2]==='SET'){__board.values=c.slice(5).map(TelemetryTuning.decodeFloat);++__board.revision;__board.storage='pending';}
            if(__saveReady&&__board.storage==='pending')__board.storage='saved';
            reply=['@T1',c[1],'OK',c[3],__board.revision,0,...__board.values.map(TelemetryTuning.encodeFloat),__board.storage].join(',');
          }else{
            if(c[2]==='BEGIN'){++__trial.session;__trial.state='ready';__trial.trial=0;}
            if(c[2]==='TRIAL'){__trial.state='running';__trial.wheel=+c[4];++__trial.trial;const kp=TelemetryTuning.decodeFloat(c[8]),ki=TelemetryTuning.decodeFloat(c[9]);__trial.rmse=15+Math.abs(kp-.94)*100+Math.abs(ki-.5)*18;}
            if(c[2]==='STATUS')__trial.state='done';if(c[2]==='END')__trial.state='aborted';
            reply=['@A1',c[1],'OK',__trial.session,__trial.state,__trial.wheel,__trial.trial,c[2]==='END'?'stopped':'none',1000,Math.round(__trial.rmse*1000),0,250,Math.round(__trial.rmse*400),10000,0].join(',');
          }
          const data=new TextEncoder().encode(reply+'\r\n');for(let i=0;i<data.length;i+=7)this.controller.enqueue(data.slice(i,i+7));
        }});
      }
      async close(){}async setSignals(){}
    }
    Object.defineProperty(navigator,'serial',{configurable:true,value:{requestPort:async()=>new Port(),addEventListener(){}}});
  });
  await page.reload();await page.locator('#connect-button').click();await page.waitForFunction(()=>!document.querySelector('#tuning-apply').disabled);
  await page.getByRole('tab',{name:/自动整定/}).click();await page.locator('#auto-confirm').check();
  await page.locator('#auto-start').click();
  await page.getByRole('tab',{name:/在线调参/}).click();check(await page.locator('#tuning-apply').isDisabled(),'manual SET locked during experiment');
  await page.getByRole('tab',{name:/自动整定/}).click();
  await page.waitForFunction(()=>document.querySelector('#auto-status').textContent.includes('试调完成'));
  check(await page.evaluate(()=>__commands.some(l=>l.includes(',END,'))),'END sent before adoption');
  check(await page.evaluate(()=>!__commands.some(l=>l.includes(',SET,'))),'trial never performs SET');
  await page.evaluate(()=>{++__board.revision;});await page.locator('#auto-adopt').click();
  await page.waitForFunction(()=>document.querySelector('#auto-status').textContent.includes('禁止覆盖'));
  check(await page.evaluate(()=>!__commands.some(l=>l.includes(',SET,'))),'stale baseline never overwrites');
  await page.locator('#auto-start').click();await page.waitForFunction(()=>document.querySelector('#auto-status').textContent.includes('试调完成'));
  await page.locator('#auto-adopt').click();await page.waitForFunction(()=>!document.querySelector('#auto-sync').hidden);
  const sets=await page.evaluate(()=>__commands.filter(l=>l.includes(',SET,')).length);check(sets===1,'only one formal SET');
  await page.locator('#auto-sync').click();await page.waitForFunction(()=>document.querySelector('#auto-status').textContent.includes('文件已同步'));
  check(await page.evaluate(()=>__commands.filter(l=>l.includes(',SET,')).length)===1,'sync retry does not reapply');
  check(synced.group==='pid'&&synced.values.length===24,'confirmed complete PID group synchronized');
  await page.evaluate(()=>{window.__saveReady=true;});await page.waitForFunction(()=>document.querySelector('#auto-persistence').textContent.includes('已保存到板端'));
  await page.reload();await page.locator('#connect-button').click();await page.waitForFunction(()=>!document.querySelector('#tuning-apply').disabled);
  await page.getByRole('tab',{name:/自动整定/}).click();await page.locator('#auto-confirm').check();
  await page.locator('#auto-start').click();await page.waitForFunction(()=>document.querySelector('#auto-status').textContent.includes('试调完成'));
  await page.unroute('**/api/tuning');let releaseSync,syncStarted;
  const started=new Promise(resolve=>{syncStarted=resolve;}),release=new Promise(resolve=>{releaseSync=resolve;});
  await page.route('**/api/tuning',async route=>{syncStarted();await release;await route.fulfill({status:200,contentType:'application/json',body:'{"saved":true}'});});
  await page.locator('#auto-adopt').click();await started;
  await page.evaluate(()=>telemetryApp.session.disconnect());await page.locator('#connect-button').click();
  releaseSync();await page.waitForTimeout(250);
  check(!await page.locator('#auto-start').isDisabled(),'reconnection during file sync restores start button');
  check((await page.locator('#auto-status').textContent()).includes('先架空固定'),'old sync cannot overwrite new connection status');
  check(errors.length===0,'browser errors: '+errors.join(';'));
  console.log('PASS: demo/cancel/mobile/serial/search/verify/conflict/adopt/file-retry/Flash status');
}
