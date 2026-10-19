(function(root,factory){const api=factory(typeof module==='object'&&module.exports ? require('./tuning.js') : root.TelemetryTuning);if(typeof module==='object'&&module.exports) module.exports=api;else root.TelemetryAutotune=api;})(typeof globalThis!=='undefined'?globalThis:this,function(T){
  'use strict';
  function check(signal){if(signal?.aborted) throw new Error('已停止自动整定。');}
  function candidates(values,wheel){
    const [kp,ki,kd]=values.slice(wheel*6,wheel*6+3),rows=[];
    for(const p of [kp*.75,kp,kp*1.25]) for(const i of [ki,0,.5,1,2]) {
      const row=[...([Math.min(5,Math.max(.01,p)),Math.min(10,Math.max(0,i))].map(v=>T.decodeFloat(T.encodeFloat(v)))),kd];
      if(!rows.some(r=>r.every((v,j)=>v===row[j]))) rows.push(row);
    }
    return rows;
  }
  function score(m,rpm,hold,cap){
    if(m.state!=='done'||m.reason!=='none'||m.samples<20||m.limited>50||![m.rmse,m.overshoot,m.steady,m.jitter].every(Number.isFinite)) return Infinity;
    return m.rmse/rpm+2*m.overshoot/rpm+m.steady/rpm+.1*(m.settle===0xffffffff?1:m.settle/hold)+.1*m.jitter/cap;
  }
  async function searchWheel(run,original,options){
    const {wheel,rpm,hold,cap,signal,onTrial=()=>{}}=options;
    check(signal);const initial=original.slice(wheel*6,wheel*6+3),baseline=await run(initial,options);
    check(signal);onTrial({wheel,stage:'baseline',values:initial,metrics:baseline});
    const baseScore=score(baseline,rpm,hold,cap);
    if(!Number.isFinite(baseScore)) throw new Error('基线试验无效（'+baseline.reason+'），请降低测试转速或检查电机反馈。');
    let best={values:initial,metrics:baseline,score:baseScore};
    for(const values of candidates(original,wheel)) {
      check(signal);if(values.every((v,i)=>v===initial[i])) continue;
      const metrics=await run(values,options);check(signal);const value=score(metrics,rpm,hold,cap);
      onTrial({wheel,stage:'search',values,metrics});if(value<best.score) best={values,metrics,score:value};
    }
    check(signal);
    if(best.values===initial) return {wheel,baseline,validation:null,values:initial,recommended:false};
    const validation=await run(best.values,options);check(signal);onTrial({wheel,stage:'verify',values:best.values,metrics:validation});
    const recommended=score(validation,rpm,hold,cap)<baseScore*.95;
    return {wheel,baseline,validation,values:recommended?best.values:initial,recommended};
  }
  function mergeResults(original,results){const values=original.slice();for(const r of results) if(r.recommended) values.splice(r.wheel*6,3,...r.values);return values;}
  const messages={format:'自动整定协议格式错误。',guard:'台架条件未满足，请检查遥控器模式、摇杆居中和电机静止状态。',conflict:'参数版本改变或手动操作尚未结束，请重新开始。',busy:'板端已有试调会话。',session:'试调会话已失效。',expired:'试调已停止，请先进入零力模式再重新准备。',config:'本轮参数无效或电机尚未停止。'};
  class AutotuneClient {
    constructor({send,timeoutMs=2000}){this.send=send;this.timeoutMs=timeoutMs;this.sequence=0;this.pending=null;}
    request(operation,...fields){
      if(this.pending) return Promise.reject(new Error('自动整定请求尚未完成。'));
      const id=++this.sequence;
      return new Promise((resolve,reject)=>{
        const finish=(error,result)=>{if(this.pending?.id!==id)return;clearTimeout(this.pending.timer);this.pending=null;error?reject(error):resolve(result);};
        const timer=setTimeout(()=>finish(new Error('自动整定响应超时，板端会按心跳期限停止；旧固件需升级。')),this.timeoutMs);
        this.pending={id,finish,timer};
        try{Promise.resolve(this.send(['@A1',id,operation,...fields].join(',')+'\r\n')).catch(e=>finish(e));}catch(e){finish(e);}
      });
    }
    begin(revision){return this.request('BEGIN',revision);}
    trial(session,values,{wheel,rpm,hold,cap}){return this.request('TRIAL',session,wheel,rpm,hold,cap,...values.map(T.encodeFloat));}
    status(session){return this.request('STATUS',session);}
    end(session){return this.request('END',session);}
    receive(line){
      if(!line.startsWith('@A1,'))return false;
      const c=line.split(','),p=this.pending;if(!p||c[1]!==String(p.id))return true;
      if(c[2]==='ERR'){p.finish(new Error(messages[c[3]]||'试调被拒绝：'+c[3]));return true;}
      const indexes=[3,5,6,8,9,10,11,12,13,14];
      if(c.length!==15||c[2]!=='OK'||!['idle','ready','running','done','aborted'].includes(c[4])||
        !['none','stopped','timeout','guard','feedback','overspeed','limited','no_response','config','can'].includes(c[7])||
        indexes.some(i=>!/^\d+$/.test(c[i])||Number(c[i])>0xffffffff)||Number(c[5])>3||Number(c[14])>1000) {
        p.finish(new Error('自动整定回复格式错误。'));return true;
      }
      p.finish(null,{session:+c[3],state:c[4],wheel:+c[5],trial:+c[6],reason:c[7],samples:+c[8],rmse:+c[9]/1000,overshoot:+c[10]/1000,settle:+c[11],steady:+c[12]/1000,jitter:+c[13]/1000,limited:+c[14]});return true;
    }
    disconnect(){this.pending?.finish(new Error('串口已断开，板端将停止试调；请重连读取实际参数。'));}
  }
  return {candidates,score,searchWheel,mergeResults,AutotuneClient,check};
});
