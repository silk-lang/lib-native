#!/usr/bin/env python3
"""Regenerate the mechanical slices from the pinned metal-rs checkout.

The hand-maintained resources, raytracing, and value-type modules are not rewritten.
SPDX-License-Identifier: MIT OR Apache-2.0
"""
import argparse
import importlib.util
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
REVISION = 'e47bc888d3837f85ea8b01c9e66ccf73236293b9'

def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'tools' / (name + '.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('upstream', type=Path)
    args = parser.parse_args()
    upstream = args.upstream.resolve()
    actual = subprocess.check_output(['git', '-C', str(upstream), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != REVISION:
        parser.error('expected metal-rs revision ' + REVISION + ', got ' + actual)
    subprocess.run([sys.executable, str(ROOT / 'tools/core.py')], check=True)
    subprocess.run([sys.executable, str(ROOT / 'tools/library.py')], check=True)
    subprocess.run([sys.executable, str(ROOT / 'tools/device.py'), str(upstream / 'src/device.rs')], check=True)
    pipeline = module('pipeline')
    pipeline.SOURCE = upstream / 'src'
    outputs = pipeline.generate() | module('encoder').render(upstream)
    for name, content in outputs.items():
        (ROOT / name).write_text(content)

if __name__ == '__main__':
    main()
