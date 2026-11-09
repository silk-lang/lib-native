#!/usr/bin/env node
// Internal harness utility: locate an already-built artifact, never build one.
import { readdir } from 'node:fs/promises';
import { resolve, dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const [name, target, packagePath = '.'] = process.argv.slice(2);
if (!name || !target) throw new Error('Usage: node scripts/artifact.mjs PACKAGE-NAME TARGET [PACKAGE-PATH]');
const filename = name + (target.includes('windows') ? '.exe' : '');
const found = [];
for (const directory of new Set([root, resolve(root, packagePath)])) {
  const output = join(directory, 'build/llvm', target, 'debug');
  try { if ((await readdir(output)).includes(filename)) found.push(join(output, filename)); }
  catch (error) { if (error.code !== 'ENOENT') throw error; }
}
if (found.length !== 1) throw new Error(`Expected one ${filename} for ${target}, found ${found.length}`);
console.log(found[0]);
