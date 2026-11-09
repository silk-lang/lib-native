#!/usr/bin/env node
// Internal hosted-runner provisioning; ordinary consumers use installed Silk and LLVM.
import { createHash } from 'node:crypto';
import { createReadStream, existsSync } from 'node:fs';
import { appendFile, mkdir, mkdtemp, readFile, readdir, rename, rm, stat, writeFile } from 'node:fs/promises';
import { basename, delimiter, dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const cache = join(root, '.cache', 'ci');
const lock = JSON.parse(await readFile(join(root, 'toolchains.lock.json'), 'utf8'));
const ci = JSON.parse(await readFile(join(root, 'modules/gpu/webgpu/scripts/ci-lock.json'), 'utf8'));
const options = new Set(process.argv.slice(2));
for (const option of options) if (!['--browser', '--linux-gpu'].includes(option)) throw new Error(`Unknown option: ${option}`);
const suffix = process.platform === 'win32' ? '.exe' : '';
const arch = { arm64: 'aarch64', x64: 'x86_64' }[process.arch];
const os = { darwin: 'apple-darwin', linux: 'unknown-linux-gnu', win32: 'pc-windows-msvc' }[process.platform];
if (!arch || !os) throw new Error('Unsupported CI host');
const target = `${arch}-${os}`;
function run(command, args, capture = false) {
  const result = spawnSync(command, args, { encoding: 'utf8', stdio: capture ? 'pipe' : 'inherit' });
  if (result.error || result.status !== 0) throw new Error(`${command} failed: ${result.error ?? result.stderr ?? result.status}`);
  return result.stdout?.trim();
}
function which(name) {
  for (const directory of (process.env.PATH ?? '').split(delimiter)) {
    const path = join(directory, name + suffix);
    if (existsSync(path)) return resolve(path);
  }
}
async function digest(path) {
  const hash = createHash('sha256');
  for await (const chunk of createReadStream(path)) hash.update(chunk);
  return hash.digest('hex');
}
async function download(asset) {
  if (!asset) throw new Error(`No pinned asset for ${target}`);
  const directory = join(cache, 'downloads');
  await mkdir(directory, { recursive: true });
  const path = join(directory, `${asset.sha256}-${decodeURIComponent(basename(new URL(asset.url).pathname))}`);
  if (!existsSync(path)) {
    run('curl', ['--fail', '--location', '--output', path + '.part', asset.url]);
    await rename(path + '.part', path);
  }
  if ((await stat(path)).size !== asset.bytes || await digest(path) !== asset.sha256) throw new Error(`Checksum/size mismatch: ${path}`);
  return path;
}
async function install(asset, destination) {
  const marker = join(destination, '.archive-sha256');
  if (existsSync(destination)) {
    if (await readFile(marker, 'utf8') !== asset.sha256) throw new Error(`Unverified installation: ${destination}`);
    return destination;
  }
  const archive = await download(asset);
  await mkdir(dirname(destination), { recursive: true });
  const temporary = await mkdtemp(join(dirname(destination), '.extract-'));
  try {
    run('tar', ['-xf', archive, '-C', temporary]);
    const entries = await readdir(temporary, { withFileTypes: true });
    if (entries.length !== 1 || !entries[0].isDirectory()) throw new Error(`Expected one archive root: ${archive}`);
    await writeFile(join(temporary, entries[0].name, '.archive-sha256'), asset.sha256);
    await rename(join(temporary, entries[0].name), destination);
  } finally { await rm(temporary, { recursive: true, force: true }); }
  return destination;
}
async function exportEnv(name, value) {
  console.log(`${name}=${value}`);
  if (process.env.GITHUB_ENV) await appendFile(process.env.GITHUB_ENV, `${name}=${value}\n`);
}
const silkAsset = lock.silk.platforms[target];
const silk = await install(silkAsset, join(cache, 'silk', target, lock.silk.version));
const seed = JSON.parse(await readFile(await download(lock.silk.native_seed), 'utf8'));
const entry = seed.seeds[target];
if (entry && (entry.tag !== `v${lock.silk.version}` || entry.target !== target || await digest(join(silk, 'bin', `silkc${suffix}`)) !== entry.sha256)) throw new Error('Native compiler differs from pinned seed');
console.log(`PATH: ${join(silk, 'bin')}`);
if (process.env.GITHUB_PATH) await appendFile(process.env.GITHUB_PATH, `${join(silk, 'bin')}\n`);
function clang22(path) {
  if (!path || !existsSync(path)) return false;
  const result = spawnSync(path, ['--version'], { encoding: 'utf8' });
  return result.status === 0 && /clang version 22\./.test(result.stdout);
}
let clang = process.env.SILK_CLANG;
if (clang && !clang22(clang)) throw new Error('SILK_CLANG must name a working LLVM 22 clang');
clang ??= [which('clang-22'), '/opt/homebrew/opt/llvm/bin/clang', '/usr/local/opt/llvm/bin/clang', which('clang')].find(clang22);
if (!clang) clang = join(await install(lock.llvm.platforms[target], join(cache, 'llvm', target, lock.llvm.version)), 'bin', `clang${suffix}`);
if (!clang22(clang)) throw new Error(`LLVM cannot run: ${clang}`);
await exportEnv('SILK_CLANG', clang);
if (process.platform === 'linux') {
  const version = run(clang, ['--version'], true);
  if (!version.includes(`clang version ${ci.runner_clang_version} `)) throw new Error(`Runner LLVM changed; review ci-lock.json: ${version}`);
}
if (options.has('--linux-gpu')) {
  if (process.platform !== 'linux' || process.arch !== 'x64') throw new Error('--linux-gpu requires Ubuntu x86_64');
  const mesa = await download(ci.mesa);
  run('sudo', ['apt-get', 'update', '-qq']);
  run('sudo', ['apt-get', 'install', '--yes', '--no-install-recommends', '--allow-downgrades', mesa]);
  if (run('dpkg-query', ['-W', '-f=${Version}', 'mesa-vulkan-drivers'], true) !== ci.mesa.version) throw new Error('Installed Mesa version differs');
  const icds = run('dpkg-query', ['-L', 'mesa-vulkan-drivers'], true).split('\n').filter(path => /\/lvp_icd[^/]*\.json$/.test(path));
  if (icds.length !== 1 || !existsSync(icds[0])) throw new Error('Expected one lavapipe ICD');
  await exportEnv('VK_DRIVER_FILES', icds[0]);
  await exportEnv('VK_ICD_FILENAMES', icds[0]);
}
if (options.has('--browser')) {
  const chromium = process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE || which('chromium') || which('chromium-browser') || which('google-chrome');
  if (!chromium || !existsSync(chromium)) throw new Error('Existing Chromium required; CI never downloads a browser');
  run(chromium, ['--version']);
  const modules = join(cache, 'playwright', ci.playwright.version, 'node_modules');
  for (const [name, asset] of Object.entries(ci.playwright.packages)) await install(asset, join(modules, name));
  await exportEnv('PLAYWRIGHT_MODULE', join(modules, 'playwright'));
  await exportEnv('PLAYWRIGHT_CHROMIUM_EXECUTABLE', chromium);
}
