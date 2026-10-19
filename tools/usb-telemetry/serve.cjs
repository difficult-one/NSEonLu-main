'use strict';
const http = require('node:http');
const fs = require('node:fs/promises');
const path = require('node:path');
const {validate} = require('./tuning.js');

const types = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.svg': 'image/svg+xml' };

function createServer(root = __dirname, configPath = path.join(__dirname, '../../modules/telemetry/tuning_overrides.h')) {
  let writing = Promise.resolve();
  async function synchronize(group, mode, values) {
    if (!['pid','power','spin'].includes(group) || !Number.isInteger(mode) ||
        mode < 0 || mode > (group==='pid' ? 0 : group==='spin' ? 2 : 3))
      throw new Error('未知参数组或模式。');
    const checked = validate(group,values,mode), name = group.toUpperCase();
    const operation = writing.catch(() => {}).then(async () => {
      let content = await fs.readFile(configPath,'utf8');
      for (const key of ['OVERRIDE','MODE','VALUES'])
        if (!new RegExp('^#define TUNING_'+name+'_'+key+' .+$','m').test(content)) throw new Error('配置文件结构不匹配，未写入。');
      const literal = value => {
        let text = String(value);if (!/[.e]/i.test(text)) text += '.0';return text+'f';
      };
      content = content.replace(new RegExp('^#define TUNING_'+name+'_OVERRIDE .+$','m'),'#define TUNING_'+name+'_OVERRIDE 1')
        .replace(new RegExp('^#define TUNING_'+name+'_MODE .+$','m'),'#define TUNING_'+name+'_MODE '+mode)
        .replace(new RegExp('^#define TUNING_'+name+'_VALUES .+$','m'),'#define TUNING_'+name+'_VALUES {'+checked.map(literal).join(', ')+'}');
      const temporary = configPath+'.tmp';
      await fs.writeFile(temporary,content,'utf8');
      await fs.rename(temporary,configPath);
    });
    writing = operation;await operation;
  }
  return http.createServer(async (request, response) => {
    response.setHeader('Cache-Control', 'no-store');
    response.setHeader('X-Content-Type-Options', 'nosniff');
    response.setHeader('Permissions-Policy', 'serial=(self)');
    if (request.url === '/api/tuning' && request.method === 'POST') {
      const host = request.headers.host || '';
      if (!/^(127\.0\.0\.1|localhost)(:\d+)?$/.test(host) || request.headers.origin !== 'http://'+host) {
        response.writeHead(403);response.end('只允许本地页面同步参数。');return;
      }
      if (request.headers['content-type'] !== 'application/json') {
        response.writeHead(415);response.end('需要 JSON 参数。');return;
      }
      try {
        let body = '';
        for await (const chunk of request) {
          body += chunk.toString('utf8');
          if (body.length > 8192) throw new Error('参数请求过大。');
        }
        const data = JSON.parse(body);
        await synchronize(data.group,data.mode,data.values);
        response.writeHead(200,{'Content-Type':'application/json; charset=utf-8'});
        response.end(JSON.stringify({saved:true,file:'modules/telemetry/tuning_overrides.h'}));
      } catch(error) {
        response.writeHead(error instanceof SyntaxError || !error.code ? 400 : 500,{'Content-Type':'application/json; charset=utf-8'});
        response.end(JSON.stringify({error:error.code ? '配置文件写入失败，请检查文件权限或占用。' : error.message}));
      }
      return;
    }
    if (request.method !== 'GET' && request.method !== 'HEAD') {
      response.writeHead(405, { Allow: 'GET, HEAD' });
      response.end('Method not allowed');
      return;
    }
    try {
      const url = new URL(request.url, 'http://localhost');
      const asset = url.pathname === '/' ? '/index.html' : url.pathname;
      if (!/^\/(?:index\.html|[a-z][a-z0-9-]*\.(?:css|js|svg))$/.test(asset)) {
        response.writeHead(404); response.end('Not found'); return;
      }
      const content = await fs.readFile(path.join(root, asset.slice(1)));
      response.writeHead(200, { 'Content-Type': `${types[path.extname(asset)]}; charset=utf-8` });
      response.end(request.method === 'HEAD' ? undefined : content);
    } catch (error) {
      response.writeHead(error.code === 'ENOENT' ? 404 : 400);
      response.end(error.code === 'ENOENT' ? 'Not found' : 'Bad request');
    }
  });
}

if (require.main === module) {
  const port = Number(process.argv[2] || 4173);
  if (!Number.isInteger(port) || port < 1 || port > 65535) {
    console.error('端口必须是 1 到 65535 的整数。'); process.exitCode = 1;
  } else {
    const server = createServer();
    server.on('error', error => {
      console.error(error.code === 'EADDRINUSE' ? `端口 ${port} 已被占用，请使用 node serve.cjs 4174 选择其他端口。` : error.message);
      process.exitCode = 1;
    });
    server.listen(port, '127.0.0.1', () => {
      console.log(`USB 遥测仪表盘：http://127.0.0.1:${port}`);
      console.log('请用电脑端 Edge / Chrome 打开上面的地址。按 Ctrl+C 停止。');
    });
  }
}

module.exports = { createServer };
