#!/usr/bin/env node
// Compare already-produced native pixels with the committed independent CPU fixtures.
import { readFile } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { inflateSync } from 'node:zlib';
const root = resolve(dirname(fileURLToPath(import.meta.url)), '../native/gpu/webgpu');
const actual = process.argv[2];
if (!actual || process.argv.length !== 3) throw new Error('Usage: node scripts/check-pixels.mjs RAW-OUTPUT-DIRECTORY');
function png(bytes) {
  if (bytes.subarray(0, 8).toString('hex') !== '89504e470d0a1a0a') throw new Error('Invalid PNG signature');
  let width, height;
  const data = [];
  for (let offset = 8; offset < bytes.length;) {
    const length = bytes.readUInt32BE(offset);
    const type = bytes.toString('ascii', offset + 4, offset + 8);
    const chunk = bytes.subarray(offset + 8, offset + 8 + length);
    if (type === 'IHDR') {
      width = chunk.readUInt32BE(0); height = chunk.readUInt32BE(4);
      if (chunk[8] !== 8 || chunk[9] !== 6 || chunk[10] || chunk[11] || chunk[12]) throw new Error('Expected noninterlaced RGBA8 fixture');
    } else if (type === 'IDAT') data.push(chunk);
    offset += 12 + length;
  }
  const stride = width * 4;
  const packed = inflateSync(Buffer.concat(data));
  if (packed.length !== (stride + 1) * height) throw new Error('PNG data length mismatch');
  const output = Buffer.alloc(stride * height);
  for (let y = 0; y < height; y++) {
    const filter = packed[y * (stride + 1)];
    if (filter > 4) throw new Error('Unsupported PNG filter');
    for (let x = 0; x < stride; x++) {
      const left = x >= 4 ? output[y * stride + x - 4] : 0;
      const up = y ? output[(y - 1) * stride + x] : 0;
      const corner = y && x >= 4 ? output[(y - 1) * stride + x - 4] : 0;
      const p = left + up - corner;
      const a = Math.abs(p - left), b = Math.abs(p - up), c = Math.abs(p - corner);
      const predictor = [0, left, up, Math.floor((left + up) / 2), a <= b && a <= c ? left : b <= c ? up : corner][filter];
      output[y * stride + x] = packed[y * (stride + 1) + x + 1] + predictor;
    }
  }
  return output;
}
const fixtures = JSON.parse(await readFile(resolve(root, 'browser/fixtures.json'), 'utf8'));
for (const fixture of fixtures.samples) {
  const expected = png(await readFile(resolve(root, 'browser', fixture.png)));
  const observed = await readFile(resolve(actual, `${fixture.name}.rgba`));
  if (expected.length !== 64 * 64 * 4 || observed.length !== expected.length) throw new Error(`${fixture.name}: invalid byte length`);
  let different = 0;
  for (let pixel = 0; pixel < expected.length; pixel += 4) {
    if ([0, 1, 2, 3].some(channel => Math.abs(expected[pixel + channel] - observed[pixel + channel]) > fixture.tolerance)) different++;
  }
  if (different > fixture.maxDifferentPixels) throw new Error(`${fixture.name}: ${different} pixels exceed tolerance ${fixture.tolerance}, cap ${fixture.maxDifferentPixels}`);
  if (fixture.compute) {
    const bytes = await readFile(resolve(actual, 'compute.u32'));
    const { count, multiplier, addend } = fixture.compute;
    if (bytes.length !== count * 4) throw new Error('Invalid compute byte length');
    for (let index = 0; index < count; index++) if (bytes.readUInt32LE(index * 4) !== index * multiplier + addend) throw new Error(`compute[${index}] differs`);
  }
  console.log(`PASS ${fixture.name}: ${different}/${fixture.maxDifferentPixels} differing pixels`);
}
