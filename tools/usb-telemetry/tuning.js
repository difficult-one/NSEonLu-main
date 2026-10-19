(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.TelemetryTuning = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {
  'use strict';
  const field = (label, unit, min, max, section, modes) => ({label, unit, min, max, section, modes});
  const pidFields = [['Kp','',0,1000],['Ki','',0,1000],['Kd','',0,1000],['输出限幅','电流原值',.001,15000],['积分限幅','电流原值',0,15000],['死区','度/秒',0,10000]];
  const groups = {
    pid: {title:'四轮速度环 PID', description:'误差单位为度/秒，电流使用 DJI 原始单位。四轮参数一起应用。', fields:['左前 LF','右前 RF','左后 LB','右后 RB'].flatMap(wheel => pidFields.map(f => field(...f,wheel)))},
    power: {title:'功率控制', description:'调整功率慢外环与能量预算。裁判限制、快保护和电流保护继续生效。', fields:[
      field('比例增益 Kp','',0,100,'功率慢外环'),field('积分增益 Ki','',0,100,'功率慢外环'),
      field('测量低通时间常数','s',.000001,10,'功率慢外环'),field('介入余量','W',0,1000,'功率慢外环'),
      field('误差死区','W',0,100,'功率慢外环'),field('最大正向修正','W',.000001,1000,'功率慢外环'),
      field('正向修正上升速率','W/s',.000001,10000,'功率慢外环'),field('预算跌落复位阈值','W',0,1000,'功率慢外环'),
      field('积分回算增益','1/s',0,1000,'功率慢外环'),field('低需求延迟','s',0,60,'功率慢外环'),
      field('低需求衰减速率','W/s',0,1000,'功率慢外环'),
      field('电容最大增益功率','W',.000001,1000,'电容与需求预算'),field('预算上升速率','W/s',.000001,10000,'电容与需求预算'),
      field('需求余量','W',0,1000,'电容与需求预算'),field('空电容能量比例','比例',0,1,'电容与需求预算'),
      field('满电容能量比例','比例',.000001,1,'电容与需求预算'),field('缓冲最大增益功率','W',0,1000,'缓冲能量预算'),
      field('最大恢复降额','W',.000001,1000,'缓冲能量预算'),field('恢复目标时间','s',.000001,60,'缓冲能量预算')]},
    spin: {title:'小陀螺', description:'Wz 为底盘控制指令值。仅显示当前固件模式使用的参数；应用后按现有斜率重新建立目标。', fields:[
      field('固定目标 / 正弦中心','Wz',.000001,6000,'目标与边界'),
      field('最小目标','Wz',.000001,6000,'目标与边界',[1,2]),field('最大目标','Wz',.000001,6000,'目标与边界',[1,2]),
      field('功率调节增益','Wz/(W·s)',.000001,1000,'功率反馈',[1,2]),
      field('上升速率','Wz/s',.000001,10000,'自适应调节',[1]),field('下降速率','Wz/s',.000001,10000,'自适应调节',[1]),
      field('功率低通时间常数','s',.000001,10,'功率反馈',[1,2]),field('功率死区','W',0,1000,'功率反馈',[1,2]),
      field('稳定等待时间','s',0,60,'功率反馈',[1,2]),field('预算跌落复位阈值','W',0,1000,'功率反馈',[1]),
      field('正弦振幅','Wz',0,6000,'正弦波形',[2]),field('正弦周期','s',.000001,120,'正弦波形',[2]),
      field('振幅建立时间','s',.000001,60,'正弦波形',[2]),field('正弦上升速率','Wz/s',.000001,10000,'正弦波形',[2]),
      field('正弦下降速率','Wz/s',.000001,10000,'正弦波形',[2]),field('旋转方向','±1',-1,1,'正弦波形',[2])]}
  };
  function encodeFloat(value) {
    const view = new DataView(new ArrayBuffer(4));view.setFloat32(0,value);
    if (!Number.isFinite(value) || !Number.isFinite(view.getFloat32(0))) throw new Error('参数必须是有限数值。');
    return view.getUint32(0).toString(16).padStart(8,'0');
  }
  function decodeFloat(text) {
    if (!/^[a-f0-9]{8}$/i.test(text)) throw new Error('参数回执格式错误。');
    const view = new DataView(new ArrayBuffer(4));view.setUint32(0,parseInt(text,16));
    const value = view.getFloat32(0);if (!Number.isFinite(value)) throw new Error('参数不是有限数值。');return value;
  }
  function validate(group, values, mode = 2) {
    const schema = groups[group];
    if (!schema || values.length !== schema.fields.length) throw new Error('参数数量不正确。');
    schema.fields.forEach((f,i) => {
      if (!Number.isFinite(values[i]) || values[i] < f.min || values[i] > f.max) throw new Error(f.section+' · '+f.label+' 超出范围 '+f.min+'～'+f.max+'。');
    });
    if (group === 'pid') for (let i=0;i<24;i+=6) if (values[i+4] > values[i+3]) throw new Error('积分限幅不能大于输出限幅。');
    if (group === 'power' && values[14] >= values[15]) throw new Error('空电容比例必须小于满电容比例。');
    if (group === 'spin') {
      if (values[1] > values[0] || values[0] > values[2]) throw new Error('目标必须位于最小和最大目标之间。');
      if (mode === 2 && (values[0]-values[10] < values[1] || values[0]+values[10] > values[2])) throw new Error('中心 ± 振幅必须位于最小和最大目标之间。');
      if (![1,-1].includes(values[15])) throw new Error('旋转方向只能为 1 或 -1。');
    }
    return values.map(value => decodeFloat(encodeFloat(value)));
  }
  const errors = {conflict:'板端参数版本已改变，请重新读取后应用。',constraint:'板端拒绝参数组合，请检查范围和约束。',unavailable:'当前固件未启用这组参数。',value:'板端拒绝无效数值。',count:'参数字段数量与固件不一致。',group:'固件不支持这组参数。',operation:'固件不支持此操作。',version:'参数版本格式错误。'};
  errors.unsupported='当前固件不支持板端掉电保存，请升级固件。';
  errors.busy='自动整定正在使用临时参数，请先结束整定。';
  class TuningClient {
    constructor({send,timeoutMs=2000}) {this.send=send;this.timeoutMs=timeoutMs;this.sequence=0;this.pending=null;}
    request(group,operation,revision,values) {
      if (!groups[group]) return Promise.reject(new Error('未知参数组。'));
      if (this.pending) return Promise.reject(new Error('请等待当前参数操作完成。'));
      const id=++this.sequence;
      const line=['@T1',id,operation,group];
      if (operation==='SET') line.push(revision,...values.map(encodeFloat));
      if (operation==='SAVE') line.push(revision);
      return new Promise((resolve,reject) => {
        const finish=(error,result) => {if (this.pending?.id!==id) return;clearTimeout(this.pending.timer);this.pending=null;error ? reject(error) : resolve(result);};
        const timer=setTimeout(() => finish(new Error('参数响应超时，结果尚未确认。请重新读取；旧固件需先升级。')),this.timeoutMs);
        this.pending={id,group,timer,finish};
        try {Promise.resolve(this.send(line.join(',')+'\r\n')).catch(error => finish(error));}
        catch(error) {finish(error);}
      });
    }
    get(group) {return this.request(group,'GET');}
    set(group,revision,values) {return this.request(group,'SET',revision,values);}
    save(group,revision) {return this.request(group,'SAVE',revision);}
    receive(line) {
      if (!line.startsWith('@T1,')) return false;
      const cells=line.split(','), p=this.pending;
      if (!p || cells[1]!==String(p.id) || cells[3]!==p.group) return true;
      if (cells[2]==='ERR') {p.finish(new Error(errors[cells[4]] || '板端拒绝请求：'+cells[4]));return true;}
      try {
        const count=groups[p.group].fields.length;
        if (cells[2]!=='OK' || ![count+6,count+7].includes(cells.length) || !/^\d+$/.test(cells[4]) || !/^[0-3]$/.test(cells[5])) throw new Error('参数回执格式与网页不一致。');
        const revision=Number(cells[4]);if (revision>0xffffffff) throw new Error('参数版本无效。');
        const storage=cells.length===count+7 ? cells.at(-1) : 'unsupported';
        if (!['saved','pending','error','volatile','unsupported'].includes(storage)) throw new Error('掉电保存状态无效。');
        p.finish(null,{revision,mode:Number(cells[5]),values:cells.slice(6,6+count).map(decodeFloat),storage});
      } catch(error) {p.finish(error);}
      return true;
    }
    disconnect() {this.pending?.finish(new Error('串口已断开，操作结果未确认，请重连读取。'));}
  }
  return {groups,encodeFloat,decodeFloat,validate,TuningClient};
});
