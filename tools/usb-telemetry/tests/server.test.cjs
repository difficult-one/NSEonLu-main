'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const {execFileSync} = require('node:child_process');
const { createServer } = require('../serve.cjs');

test('acknowledged values synchronize to a source header; foreign origins and invalid values cannot write', async () => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'telemetry-tuning-'));
  const config = path.join(root, 'tuning_overrides.h');
  await fs.copyFile(path.join(__dirname, '../../../modules/telemetry/tuning_overrides.h'), config);
  const server = createServer(root, config);
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const base = 'http://127.0.0.1:'+server.address().port;
  const fields = [.75,0,0,15000,3000,0];
  const values = Array.from({length:4}, () => fields).flat();
  const post = (origin, values) => fetch(base+'/api/tuning', {method:'POST',headers:{Origin:origin,'Content-Type':'application/json'},body:JSON.stringify({group:'pid',mode:0,values})});
  try {
    assert.equal((await post('https://example.com',values)).status,403);
    const original=await fs.readFile(config,'utf8');
    assert.equal((await post(base,[...values.slice(0,23),-1])).status,400);
    assert.equal(await fs.readFile(config,'utf8'), original);
    assert.equal((await post(base,values)).status,200);
    const saved=await fs.readFile(config,'utf8');
    assert.match(saved,/#define TUNING_PID_OVERRIDE 1/);
    assert.match(saved,/#define TUNING_PID_MODE 0/);
    assert.match(saved,/#define TUNING_PID_VALUES \{0.75f, 0.0f/);
    assert.match(saved,/#define TUNING_POWER_OVERRIDE 0/);
    /* Compile the persisted header and exercise firmware startup restore. */
    const source=path.join(root,'runtime_tuning.c');
    await fs.copyFile(path.join(__dirname,'../../../modules/telemetry/runtime_tuning.c'),source);
    await fs.writeFile(path.join(root,'restore.c'), [
      '#include "runtime_tuning.h"',
      '#include <assert.h>',
      '#include <string.h>',
      'static unsigned restored;',
      'static bool apply(const float *v) { assert(v[0] == .75f && v[3] == 15000); ++restored; return true; }',
      'int main(void) { float original[24]={0}; char reply[512];',
      'RuntimeTuningRegister(TUNING_PID,original,0); RuntimeTuningRestore(TUNING_PID,apply);',
      'assert(restored==1); RuntimeTuningSubmit("@T1,1,GET,pid");',
      'assert(RuntimeTuningReply(reply,sizeof reply)>0); assert(strstr(reply,"3f400000"));',
      'original[0]=4; restored=0; RuntimeTuningRegister(TUNING_PID,original,1);',
      'RuntimeTuningRestore(TUNING_PID,apply); assert(restored==0); return 0; }'
    ].join('\n'));
    const binary=path.join(root,'restore.exe');
    execFileSync('gcc',['-std=c11','-Wall','-Wextra','-Werror',
      '-I'+path.join(__dirname,'../../../tests/usb_telemetry/stubs'),
      '-I'+path.join(__dirname,'../../../modules/telemetry'),source,path.join(root,'restore.c'),'-o',binary],{stdio:'pipe'});
    execFileSync(binary,[],{stdio:'pipe'});
  } finally {
    await new Promise(resolve => server.close(resolve));
    await fs.rm(root,{recursive:true,force:true});
  }
});

test('serves dashboard assets on loopback and rejects unrelated files and write methods', async () => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'telemetry-web-'));
  await fs.writeFile(path.join(root, 'index.html'), '<h1>仪表盘</h1>');
  await fs.writeFile(path.join(root, 'protocol.js'), 'window.version = 1;');
  await fs.writeFile(path.join(root, 'private.txt'), 'not a dashboard asset');
  const server = createServer(root);
  try {
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    const base = `http://127.0.0.1:${server.address().port}`;
    const index = await fetch(base + '/');
    assert.equal(index.status, 200);
    assert.match(index.headers.get('content-type'), /text\/html.*utf-8/);
    assert.equal(await index.text(), '<h1>仪表盘</h1>');
    const script = await fetch(base + '/protocol.js');
    assert.match(script.headers.get('content-type'), /javascript/);
    assert.equal((await fetch(base + '/private.txt')).status, 404);
    assert.equal((await fetch(base + '/%2e%2e/AGENTS.md')).status, 404);
    assert.equal((await fetch(base + '/index.html', { method: 'POST' })).status, 405);
    const head = await fetch(base + '/index.html', { method: 'HEAD' });
    assert.equal(head.status, 200);
    assert.equal(await head.text(), '');
    assert.equal(index.headers.get('permissions-policy'), 'serial=(self)');
  } finally {
    await new Promise(resolve => server.close(resolve));
    await fs.rm(root, { recursive: true, force: true });
  }
});
