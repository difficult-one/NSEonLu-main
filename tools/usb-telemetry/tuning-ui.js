(function () {
  'use strict';
  const {groups,validate}=window.TelemetryTuning;
  const $=id=>document.getElementById(id);
  const number=value=>Number(value.toPrecision(7)).toString();
  window.createTuningUI=function(client) {
    let connected=false,busy=false,active='pid',epoch=0,polling=false,pollTimer=null,locked=false;
    const states=Object.fromEntries(Object.keys(groups).map(key=>[key,{data:null,draft:null,status:'连接 C 板后读取当前参数。',error:false,sync:null,persistError:null}]));
    function renderPersistence() {
      const state=states[active],status=state.data?.storage;
      const messages={
        saved:'已保存到板端 · 断电重启后自动恢复',
        pending:'等待板端保存 · 请进入零力模式并保持电机停止；保存前断电会恢复上一次保存值。',
        error:'板端保存失败 · 当前参数仍在 RAM 中生效，请停机后重试。',
        volatile:'当前为启动参数 · 可保存到板端，作为下次上电的参数。',
        unsupported:'此固件未提供掉电保存状态，请升级固件。'
      };
      $('tuning-persistence-status').textContent=connected ? state.persistError || messages[status] || '读取参数后显示板端掉电保存状态。' : '连接 C 板后确认掉电保存状态。';
      $('tuning-persistence-status').className=status==='saved' ? 'saved' : status==='error' ? 'error' : '';
      $('tuning-save').hidden=!connected||!['error','volatile'].includes(status);
      $('tuning-save').disabled=busy||polling;
      $('tuning-save').textContent=status==='error' ? '重试板端保存' : '保存到板端';
      $('tuning-read').disabled=!connected||busy||polling;
      $('tuning-apply').disabled=!connected||busy||polling||!state.data;
      $('tuning-sync').disabled=busy||polling||!state.sync;
      document.querySelectorAll('[data-tuning-group]').forEach(button=>{button.disabled=busy||polling;});
      if(locked) document.querySelectorAll('#tuning-panel button,#tuning-fields input').forEach(el=>{el.disabled=true;});
    }
    function schedulePoll() {
      clearTimeout(pollTimer);
      if (connected && states[active].data?.storage==='pending')
        pollTimer=setTimeout(pollPersistence,1000);
    }
    async function pollPersistence() {
      if (!connected || locked) return;
      if (busy || client.pending) {schedulePoll();return;}
      const key=active,state=states[key],token=epoch;
      polling=true;renderPersistence();
      try {
        const result=await client.get(key);
        if (token===epoch && state.data?.revision===result.revision) {
          state.data.storage=result.storage;
          state.persistError=null;
        }
      } catch(error) {
        if (token===epoch) state.persistError='无法确认保存状态，请重新读取。'+error.message;
      } finally {
        if (token===epoch) {polling=false;renderPersistence();schedulePoll();}
      }
    }
    function render() {
      const state=states[active],schema=groups[active],loaded=state.data!==null;
      $('tuning-title').textContent=schema.title;
      $('tuning-description').textContent=schema.description;
      $('tuning-status').textContent=state.status;
      $('tuning-status').classList.toggle('error',state.error);
      $('tuning-mode').textContent=loaded ? active==='spin' ? ['固定模式','功率自适应模式','同向正弦模式'][state.data.mode] || '未知模式' : active==='power' ? '功控阶段 '+state.data.mode : '底盘四轮 · 速度环' : '等待板端参数';
      $('tuning-revision').textContent=loaded ? '板端版本 '+state.data.revision : '尚未读取';
      $('tuning-read').disabled=!connected||busy;
      $('tuning-apply').disabled=!connected||busy||!loaded;
      $('tuning-revert').disabled=busy||!loaded;
      $('tuning-sync').hidden=!state.sync;
      $('tuning-sync').disabled=busy||!state.sync;
      $('tuning-apply').textContent=busy ? '正在处理…' : '应用并同步文件';
      document.querySelectorAll('[data-tuning-group]').forEach(button=>{
        button.setAttribute('aria-selected',String(button.dataset.tuningGroup===active));
        button.disabled=busy;
      });
      const container=$('tuning-fields');container.replaceChildren();
      const sectionGrids=new Map();let grid;
      schema.fields.forEach((field,index)=>{
        if (field.modes && loaded && !field.modes.includes(state.data.mode)) return;
        if (!sectionGrids.has(field.section)) {
          const block=document.createElement('section'),heading=document.createElement('h3');
          heading.textContent=field.section;grid=document.createElement('div');grid.className='tuning-field-grid';
          block.className='tuning-field-section';block.append(heading,grid);container.append(block);
          sectionGrids.set(field.section,grid);
        }
        grid=sectionGrids.get(field.section);
        const label=document.createElement('label'),caption=document.createElement('span'),input=document.createElement('input'),current=document.createElement('small');
        label.className='tuning-field';caption.textContent=field.label+(field.unit ? ' · '+field.unit : '');
        input.type='number';input.step='any';input.min=field.min;input.max=field.max;
        input.id='tuning-value-'+index;input.dataset.index=index;input.disabled=!connected||busy||!loaded;
        input.value=loaded ? number(state.draft[index]) : '';input.placeholder='—';
        input.setAttribute('aria-label',field.section+' '+field.label);
        current.textContent=loaded ? '已生效 '+number(state.data.values[index])+' · 范围 '+field.min+'～'+field.max : '等待从板端读取';
        input.addEventListener('input',()=>{
          state.draft[index]=input.value==='' ? NaN : Number(input.value);
          input.classList.toggle('edited',state.draft[index]!==state.data.values[index]);
          $('tuning-status').textContent='有未应用的修改，点击应用后才会下发。';
          $('tuning-status').classList.remove('error');
        });
        label.append(caption,input,current);grid.append(label);
      });
      renderPersistence();
    }
    async function sync(key,record,token) {
      const state=states[key];
      try {
        const response=await fetch('/api/tuning',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({group:key,...record})});
        const result=await response.json();
        if (!response.ok || !result.saved) throw new Error(result.error || '本地启动服务未提供参数同步，请重启 start.cmd。');
        if (token!==epoch) return;
        state.sync=null;state.status='板端已实时生效，项目参数文件已同步。';state.error=false;
      } catch(error) {
        if (token!==epoch) return;
        state.sync=record;state.status='板端已生效，文件同步失败：'+error.message+' 可重试同步。';state.error=true;
      }
    }
    async function operate(kind) {
      if (busy || polling || !connected || locked) return;
      const key=active,state=states[key],token=epoch;
      let values;
      try {
        if (kind==='apply') values=validate(key,state.draft,state.data.mode);
      } catch(error) {state.status=error.message;state.error=true;render();return;}
      busy=true;state.error=false;state.status=kind==='read' ? '正在读取板端参数…' : kind==='sync' ? '正在重试文件同步…' : '等待板端校验并应用…';render();
      try {
        if (kind==='sync') await sync(key,state.sync,token);
        else {
          const result=await (kind==='read' ? client.get(key) : kind==='save' ? client.save(key,state.data.revision) : client.set(key,state.data.revision,values));
          if (token!==epoch) return;
          state.data=result;
          if (kind!=='save') state.draft=result.values.slice();
          state.error=false;state.persistError=null;
          if (kind==='read' && state.sync) state.sync=result;
          state.status=kind==='read' ? '已读取当前生效参数，可以编辑。' : kind==='save' ? '已请求保存当前生效参数，等待板端完成。' : '板端已生效，正在同步项目文件…';
          if (kind==='apply') {state.sync=result;render();await sync(key,result,token);}
        }
      } catch(error) {
        if (token!==epoch) return;
        state.status=error.message;state.error=true;
      } finally {
        if (token===epoch) {busy=false;render();schedulePoll();}
      }
    }
    document.querySelectorAll('[data-tuning-group]').forEach(button=>button.addEventListener('click',()=>{
      active=button.dataset.tuningGroup;render();
      if (connected && !states[active].data) operate('read');
      else schedulePoll();
    }));
    $('tuning-read').addEventListener('click',()=>operate('read'));
    $('tuning-apply').addEventListener('click',()=>operate('apply'));
    $('tuning-sync').addEventListener('click',()=>operate('sync'));
    $('tuning-save').addEventListener('click',()=>operate('save'));
    $('tuning-revert').addEventListener('click',()=>{
      const state=states[active];state.draft=state.data.values.slice();
      state.status='已撤销草稿，恢复板端最后确认值。';state.error=false;render();
    });
    render();
    return {
      setLocked(value) {locked=value;clearTimeout(pollTimer);render();if(!locked)schedulePoll();},
      setConnected(value) {
        if (value===connected) return;
        connected=value;++epoch;client.disconnect();busy=false;polling=false;clearTimeout(pollTimer);
        for (const state of Object.values(states)) {
          state.data=null;state.draft=null;state.sync=null;state.persistError=null;
          state.status=value ? '等待读取当前生效值。' : '未连接 C 板，调参已停用。重新连接后读取实际值。';state.error=false;
        }
        render();if (connected) operate('read');
      },
      get active() {return active;}
    };
  };
})();
