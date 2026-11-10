import { WindowHost } from './window-host.js';

// Browser-side JSPI runtime; the Silk task owns process launch and the test deadline.
export async function runSmoke(encodedWasm) {
  const host = new WindowHost({ container: document.body });
  const lines = [];
  let pending = '';
  const module = await WebAssembly.compile(Uint8Array.from(atob(encodedWasm), c => c.charCodeAt(0)));
  const instance = await WebAssembly.instantiate(module, {
    window: host.suspendingImports,
    smoke: {
      print(ptr, length) {
        pending += host.text(ptr, length);
        const parts = pending.split('\n');
        pending = parts.pop();
        for (const line of parts) { lines.push(line); console.log(line); }
      },
    },
  });
  host.attach(instance);
  const code = await host.promising('smoke_main')();
  return { code, lines };
}
