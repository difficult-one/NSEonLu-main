const {test}=require('node:test');
const assert=require('node:assert/strict');
const A=require('../autotune.js');
const pid=Array.from({length:4},()=>[.75,0,0,15000,3000,0]).flat();
const metric=(error,reason='none')=>({state:'done',reason,samples:500,rmse:error,overshoot:0,steady:error,jitter:0,settle:100,limited:0});
test('bounded candidates retain Kd and do not mutate other wheel parameters',()=>{
  const original=pid.slice();original[8]=.01;
  const rows=A.candidates(original,1);
  assert(rows.length<=15);assert.deepEqual(original.slice(0,6),pid.slice(0,6));
  assert(rows.every(p=>p[0]>=.01 && p[0]<=5 && p[1]>=0 && p[1]<=10 && p[2]===.01));
  assert.equal(new Set(rows.map(p=>p.join(','))).size,rows.length);
});
test('invalid or limited trials cannot win',()=>{
  assert.equal(A.score(metric(1,'limited'),500,1000,3000),Infinity);
  assert(A.score(metric(10),500,1000,3000)<A.score(metric(100),500,1000,3000));
});
test('independent validation rejects a candidate that only won once',async()=>{
  let calls=0;
  const result=await A.searchWheel(async()=>metric(++calls===1 ? 100 : calls===2 ? 10 : 150),pid,{wheel:0,rpm:500,hold:1000,cap:3000});
  assert.equal(result.recommended,false);assert.deepEqual(result.values,pid.slice(0,3));
});
test('successful validation improves baseline and only selected wheel gains are adopted',async()=>{
  let calls=0;const result=await A.searchWheel(async()=>metric(++calls===1 ? 100 : 10),pid,{wheel:2,rpm:500,hold:1000,cap:3000});
  assert(result.recommended);assert.equal(result.wheel,2);
  assert.deepEqual(A.mergeResults(pid,[result]).slice(0,12),pid.slice(0,12));
});
test('cancellation stops search before trying more candidates',async()=>{
  const signal=new AbortController();let calls=0;
  await assert.rejects(A.searchWheel(async()=>{++calls;signal.abort();return metric(100);},pid,{wheel:0,rpm:500,hold:1000,cap:3000,signal:signal.signal}),/停止/);
  assert.equal(calls,1);
});
test('A1 client rejects malformed metrics and disconnect rejects pending request',async()=>{
  let line;const client=new A.AutotuneClient({send:s=>{line=s;}});
  const p=client.begin(1);assert(line.startsWith('@A1,1,BEGIN,1'));
  client.receive('@A1,1,OK,1,running,0,1,none,1,NaN,0,0,0,0,0');
  await assert.rejects(p,/格式/);
  const disconnected=client.begin(1);client.disconnect();await assert.rejects(disconnected,/断开/);
});
