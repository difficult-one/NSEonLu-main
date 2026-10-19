(function(){
  'use strict';
  const A=window.TelemetryAutotune,$=id=>document.getElementById(id),names=['左前 LF','右前 RF','左后 LB','右后 RB'],keys=['lf','rf','lb','rb'];
  const reason={none:'有效',limited:'限流，已剔除',no_response:'响应不足',guard:'台架条件改变',feedback:'反馈过期',timeout:'心跳超时',overspeed:'转速越界',can:'CAN 发送失败',stopped:'已停止'};
  const fmt=n=>Number.isFinite(n)?Number(n.toPrecision(5)).toString():'—';
  const wait=ms=>new Promise(resolve=>setTimeout(resolve,ms));
  window.createAutotuneUI=function({client,tuningClient,lockManual,getSamples}){
    let connected=false,busy=false,controller=null,original=null,results=[],rows=[],demo=false,session=0,selected=0,epoch=0,pollTimer=null,adopted=null,syncPending=null,demoSamples=[];
    const status=(text,error=false)=>{$('auto-status').textContent=text;$('auto-status').classList.toggle('error',error);};
    function controls(){
      $('auto-start').disabled=!connected||busy;$('auto-demo').disabled=connected||busy;$('auto-stop').disabled=!busy||!controller;
      $('auto-adopt').disabled=!connected||busy||demo||!results.some(r=>r.recommended)||!!adopted;
      $('auto-sync').hidden=!syncPending;$('auto-sync').disabled=busy;
      $('auto-save').hidden=!adopted||!['error','volatile'].includes(adopted.storage);$('auto-save').disabled=busy;
      document.querySelectorAll('#auto-settings input,#auto-settings select').forEach(el=>{el.disabled=busy;});
      $('auto-mode').textContent=demo?'演示 · 合成台架':busy?'台架试验进行中':connected?'真实设备':'尚未连接';
    }
    function appendCells(parent,values){const tr=document.createElement('tr');for(const value of values){const td=document.createElement('td');td.textContent=value;tr.append(td);}parent.append(tr);}
    function addRow(row){
      rows.push(row);selected=row.wheel;
      appendCells($('auto-trials'),[rows.length,names[row.wheel],{baseline:'基线',search:'搜索',verify:'独立复测'}[row.stage],...row.values.map(fmt),fmt(row.metrics.rmse),fmt(row.metrics.overshoot),fmt(row.metrics.steady),row.metrics.settle===0xffffffff?'未稳定':row.metrics.settle+' ms',reason[row.metrics.reason]||row.metrics.reason]);
      $('auto-progress').max=Math.max(rows.length+1,$('auto-wheel').value==='all'?60:15);$('auto-progress').value=rows.length;
      $('auto-progress-text').textContent='已完成 '+rows.length+' 次试验 · '+names[row.wheel];$('auto-latest').textContent=fmt(row.metrics.rmse)+' rpm';
    }
    function compare(){
      $('auto-results').replaceChildren();
      for(const r of results)appendCells($('auto-results'),[names[r.wheel],original.values.slice(r.wheel*6,r.wheel*6+3).map(fmt).join(' / '),r.values.map(fmt).join(' / '),fmt(r.baseline.rmse),r.validation?fmt(r.validation.rmse):'—',r.recommended?'复测通过':'保留原参数']);
    }
    function settings(){
      const rpm=Number($('auto-rpm').value),hold=Number($('auto-hold').value),cap=Number($('auto-cap').value);
      if(![rpm,hold,cap].every(Number.isInteger)||rpm<100||rpm>1500||hold<500||hold>2000||cap<500||cap>5000)throw new Error('请检查转速、保持时间和电流上限范围。');
      return {rpm,hold,cap};
    }
    async function hardwareTrial(values,options){
      A.check(options.signal);await client.trial(session,values,options);
      const deadline=performance.now()+options.hold+5000;
      while(performance.now()<deadline){
        await wait(200);A.check(options.signal);const state=await client.status(session);A.check(options.signal);
        if(state.state==='done')return state;
        if(state.state!=='running')throw new Error('板端停止试验：'+(reason[state.reason]||state.reason)+'。请进入零力模式后重新准备。');
      }
      throw new Error('试验结果超时。');
    }
    async function demoTrial(values,options){
      await wait(90);A.check(options.signal);
      const error=15+Math.abs(values[0]-.94)*100+Math.abs(values[1]-.5)*18,now=performance.now();demoSamples=[];
      for(let i=0;i<100;++i){const t=i/99*1.6,rpm=t<.3?0:t<1.3?options.rpm*(1-Math.exp(-(t-.3)*12))*(1-error/700):options.rpm*Math.exp(-(t-1.3)*18);
        demoSamples.push({arrivalMs:now-1600+i/99*1600,segment:1,normalized:{values:{['rpm_'+keys[options.wheel]]:rpm,['target_'+keys[options.wheel]+'_rpm']:t>=.3&&t<1.3?options.rpm:0}}});}
      return {state:'done',reason:'none',samples:1000,rmse:error,overshoot:0,steady:error*.4,settle:250,jitter:10,limited:0};
    }
    async function start(isDemo){
      if(busy)return;let options;
      try{options=settings();if(!isDemo&&!$('auto-confirm').checked)throw new Error('请先确认底盘已架空固定、四轮离地。');}catch(e){status(e.message,true);return;}
      const token=epoch;busy=true;demo=isDemo;controller=new AbortController();results=[];rows=[];adopted=null;syncPending=null;session=0;demoSamples=[];clearTimeout(pollTimer);
      $('auto-trials').replaceChildren();$('auto-results').replaceChildren();$('auto-persistence').textContent='候选参数仅用于试验，尚未采用。';$('auto-progress').value=0;
      lockManual(true);controls();status(isDemo?'演示整定中，不连接设备、不保存参数。':'正在读取原参数并建立台架会话…');
      let completed=false,endConfirmed=isDemo;
      try{
        while(tuningClient.pending){A.check(controller.signal);await wait(30);}
        original=isDemo?{revision:0,values:Array.from({length:4},()=>[.75,0,0,15000,3000,0]).flat()}:await tuningClient.get('pid');
        A.check(controller.signal);if(!isDemo){const begun=await client.begin(original.revision);session=begun.session;}
        const wheels=$('auto-wheel').value==='all'?[0,1,2,3]:[Number($('auto-wheel').value)];
        for(const wheel of wheels){A.check(controller.signal);selected=wheel;status((isDemo?'演示 · ':'')+'正在整定 '+names[wheel]+'，Kd 保持原值。');
          results.push(await A.searchWheel(isDemo?demoTrial:hardwareTrial,original.values,{...options,wheel,signal:controller.signal,onTrial:addRow}));compare();}
        completed=true;
      }catch(e){status(e.message,true);}
      finally{
        if(session&&connected)try{while(client.pending)await wait(25);await client.end(session);endConfirmed=true;}catch(e){status('结束未确认：'+e.message+' 请进入零力模式，结果不可采用。',true);}
        session=0;controller=null;busy=false;lockManual(false);
        if(token!==epoch){results=[];controls();return;}
        if(!completed||!endConfirmed)results=[];
        if(completed&&endConfirmed){const improved=results.filter(r=>r.recommended).length;
          status((isDemo?'演示完成。':'试调完成，原参数已恢复；请进入零力模式。')+(improved?' '+improved+' 个轮子的候选通过复测，可查看对比后采用。':' 未发现通过复测的改善，保留原参数。'));
          $('auto-progress').max=Math.max(1,rows.length);$('auto-progress').value=rows.length;}
        controls();
      }
    }
    function savedText(){const state=adopted?.storage;
      $('auto-persistence').textContent=state==='saved'?'已保存到板端，重启后恢复。':state==='pending'?'等待板端保存：请进入零力模式，保持电机停止。':state==='error'?'板端保存失败，可停机后重试。':state==='unsupported'?'旧固件未提供保存状态，请升级。':'尚未确认板端保存。';controls();}
    function schedulePoll(){clearTimeout(pollTimer);if(connected&&adopted?.storage==='pending')pollTimer=setTimeout(pollSave,1000);}
    async function pollSave(){
      if(busy||tuningClient.pending){schedulePoll();return;}const token=epoch,record=adopted;
      try{const state=await tuningClient.get('pid');if(token!==epoch)return;
        if(adopted!==record)return;
        if(state.revision!==record.revision){adopted=null;syncPending=null;status('板端参数已改变，请重新整定或读取。',true);controls();return;}
        adopted=state;savedText();}catch(e){if(token===epoch)$('auto-persistence').textContent='保存状态未确认：'+e.message;}
      schedulePoll();
    }
    async function synchronize(token=epoch){
      const record=syncPending;if(!record)return;
      const response=await fetch('/api/tuning',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({group:'pid',...record})});
      const data=await response.json();if(!response.ok||!data.saved)throw new Error(data.error||'本地文件同步失败。');
      if(token!==epoch)return;
      if(syncPending===record)syncPending=null;status('候选参数已采用，项目参数文件已同步。');
    }
    async function adopt(){
      if(busy||demo||!original)return;busy=true;lockManual(true);controls();const token=epoch;
      try{while(tuningClient.pending)await wait(30);const current=await tuningClient.get('pid');
        if(token!==epoch)throw new Error('连接已改变，请重新整定。');
        if(current.revision!==original.revision||current.values.some((v,i)=>v!==original.values[i]))throw new Error('原参数已发生变化，禁止覆盖；请重新整定。');
        adopted=await tuningClient.set('pid',current.revision,A.mergeResults(original.values,results));if(token!==epoch)return;
        syncPending=adopted;savedText();await synchronize(token);
      }catch(e){if(token===epoch)status((adopted?'板端已生效，文件尚未同步：':'')+e.message,true);}
      finally{busy=false;lockManual(false);controls();if(token===epoch)schedulePoll();}
    }
    $('auto-start').addEventListener('click',()=>start(false));$('auto-demo').addEventListener('click',()=>start(true));
    $('auto-stop').addEventListener('click',()=>{controller?.abort();status('正在结束试调并恢复原参数…');});$('auto-adopt').addEventListener('click',adopt);
    $('auto-sync').addEventListener('click',async()=>{busy=true;lockManual(true);controls();const token=epoch,record=syncPending;
      try{const current=await tuningClient.get('pid');if(token!==epoch)return;
        if(current.revision!==record.revision||current.values.some((v,i)=>v!==record.values[i]))throw new Error('参数已改变，请重新读取。');await synchronize(token);
      }catch(e){if(token===epoch)status(e.message,true);}finally{busy=false;lockManual(false);controls();}});
    $('auto-save').addEventListener('click',async()=>{busy=true;lockManual(true);controls();const token=epoch;
      try{const state=await tuningClient.save('pid',adopted.revision);if(token!==epoch)return;adopted=state;savedText();}catch(e){if(token===epoch)status(e.message,true);}finally{busy=false;lockManual(false);controls();if(token===epoch)schedulePoll();}});
    controls();
    return {
      setConnected(value){if(connected===value)return;connected=value;++epoch;clearTimeout(pollTimer);
        if(!value){controller?.abort();client.disconnect();results=[];adopted=null;syncPending=null;status('串口已断开；板端将停止试调。请进入零力模式，重连后重新读取。',true);}
        else{demo=false;results=[];original=null;status('先架空固定底盘，再按说明准备遥控器。');}controls();},
      renderChart(){if($('autotune-panel').hidden)return;const samples=demo?demoSamples:getSamples(),key=keys[selected],now=demoSamples.length&&demo?demoSamples.at(-1).arrivalMs:performance.now();
        window.TelemetryCharts.drawChart($('auto-chart'),samples,[{key:'target_'+key+'_rpm',name:'目标',color:'#8a65d6',dash:true},{key:'rpm_'+key,name:'实际',color:'#3478f6'}],{now,unit:'rpm',range:[-5000,0]});},
      stop(){controller?.abort();},get busy(){return busy;}
    };
  };
})();
