#!/usr/bin/env node
// Runs the browser smoke test (tests/browser) in the already installed Playwright
// Chromium; never downloads a browser.
//
//   node browser/test.mjs --wasm FILE [--timeout 60000]
import { createRequire } from 'node:module';
import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { resolve, dirname, extname, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const options = new Map();
for (let i = 2; i < process.argv.length; i += 2) options.set(process.argv[i], process.argv[i + 1]);
if (!options.get('--wasm')) throw new Error('Usage: node browser/test.mjs --wasm FILE [--timeout 60000]');
const wasm = await readFile(resolve(options.get('--wasm')));
const timeout = Number(options.get('--timeout') ?? 60000);
const require = createRequire(import.meta.url);
let chromium;
try { ({ chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright')); }
catch (error) { throw new Error('An existing Playwright installation is required (set PLAYWRIGHT_MODULE to its absolute module path); do not run playwright install', { cause: error }); }

const page = `<!doctype html><meta charset="utf-8"><title>platform-window smoke</title>
<style>body { margin: 0 }</style><body><script type="module">
import { WindowHost } from '/browser/window-host.js';
window.runSmoke = async () => {
  const host = new WindowHost({ container: document.body });
  const lines = [];
  let pending = '';
  const module = await WebAssembly.compileStreaming(fetch('/smoke.wasm'));
  const instance = await WebAssembly.instantiate(module, {
    window: host.suspendingImports,
    smoke: {
      print(ptr, length) {
        pending += host.text(ptr, length);
        const parts = pending.split('\\n');
        pending = parts.pop();
        for (const line of parts) { lines.push(line); console.log(line); }
      },
    },
  });
  host.attach(instance);
  const code = await host.promising('smoke_main')();
  return { code, lines };
};
</script>`;

const types = { '.js': 'text/javascript', '.wasm': 'application/wasm', '.html': 'text/html' };
const server = createServer(async (request, response) => {
  const url = new URL(request.url, 'http://localhost');
  try {
    if (url.pathname === '/') { response.writeHead(200, { 'content-type': types['.html'] }); response.end(page); return; }
    if (url.pathname === '/smoke.wasm') { response.writeHead(200, { 'content-type': types['.wasm'] }); response.end(wasm); return; }
    const path = resolve(root, '.' + url.pathname);
    if (!path.startsWith(root + sep + 'browser' + sep) || extname(path) !== '.js') { response.writeHead(404); response.end(); return; }
    response.writeHead(200, { 'content-type': types['.js'] });
    response.end(await readFile(path));
  } catch { response.writeHead(404); response.end(); }
});
const listening = Promise.withResolvers();
server.once('error', listening.reject);
server.listen(0, '127.0.0.1', listening.resolve);
await listening.promise;

// A supplied executable is authoritative, as in native/gpu/webgpu/browser/test.mjs. JSPI ships in
// Chromium 137 and later; the flag covers older builds.
const browser = await chromium.launch({ executablePath: process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE || undefined,
  args: ['--enable-features=WebAssemblyExperimentalJSPI'] });
let code = 1;
try {
  console.log(`Chromium ${browser.version()}: platform-window browser smoke test`);
  const context = await browser.newContext({ viewport: { width: 1024, height: 768 } });
  const tab = await context.newPage();
  tab.on('console', message => { if (message.type() !== 'log') console.log(`[${message.type()}] ${message.text()}`); });
  tab.on('pageerror', error => console.error(`[pageerror] ${error.message}`));
  await tab.goto(`http://127.0.0.1:${server.address().port}/`);
  await tab.waitForFunction(() => typeof window.runSmoke === 'function');
  const result = await Promise.race([
    tab.evaluate(() => window.runSmoke()),
    new Promise((_, reject) => setTimeout(() => reject(new Error('the smoke test timed out')), timeout)),
  ]);
  for (const line of result.lines) console.log(line);
  code = result.code;
} finally {
  await browser.close();
  server.close();
}
process.exit(code);
