#!/usr/bin/env python3
"""Audit declared-method coverage, not runtime correctness.

With --upstream, refresh the pinned public-method inventory from a local checkout.
Without it, use the checked-in inventory. No Rust toolchain is required.
SPDX-License-Identifier: MIT OR Apache-2.0
"""
import argparse
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REVISION = 'e47bc888d3837f85ea8b01c9e66ccf73236293b9'

def canonical(name):
    return re.sub(r'Ref(?=\.)', '', name)

def end_block(text, start):
    depth = 0
    for at in range(start, len(text)):
        if text[at] == '{':
            depth += 1
        elif text[at] == '}':
            depth -= 1
            if not depth:
                return at
    raise ValueError('unclosed block')

def inventory(upstream):
    actual = subprocess.check_output(['git', '-C', str(upstream), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != REVISION:
        raise ValueError('upstream revision does not match ' + REVISION)
    items = []
    for path in sorted((upstream / 'src').rglob('*.rs')):
        text = path.read_text()
        text = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"', lambda match: ' ' * len(match[0]), text, flags=re.S)
        scopes = []
        for match in re.finditer(r'\bimpl(?:<[^{}]*?>)?\s+(\w+)(?:<[^{}]*?>)?\s*(?:where[^{}]*?)?\{', text):
            start = match.end() - 1
            scopes.append((start, end_block(text, start), match[1]))
        for match in re.finditer(r'\bpub\s+(?:unsafe\s+)?fn\s+(\w+)', text):
            owner = next((owner for start, end, owner in scopes if start < match.start() < end), None)
            if not owner and match[1] != 'mps_supports_device':
                raise ValueError('unclassified public method ' + str(path) + ':' + match[1])
            name = (owner + '.' if owner else '') + match[1]
            if path.name == 'mps.rs':
                name = 'mps::' + name
            items.append({'rust': canonical(name), 'source': str(path.relative_to(upstream))})
    return {'repository': 'https://github.com/gfx-rs/metal-rs', 'revision': REVISION, 'methods': items}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--upstream', type=Path)
    args = parser.parse_args()
    pinned = ROOT / 'api/upstream.json'
    if args.upstream:
        pinned.write_text(json.dumps(inventory(args.upstream.resolve()), indent=2) + '\n')
    source = json.loads(pinned.read_text())
    expected = {item['rust'] for item in source['methods']}
    entries = []
    for path in sorted((ROOT / 'api').glob('*.json')):
        if path.name not in {'upstream.json', 'type-map.json'}:
            entries.extend(json.loads(path.read_text()))
    by_rust = {canonical(item['rust']): item for item in entries}
    missing = expected - by_rust.keys()
    missing_targets = []
    for item in entries:
        parts = item['silk'].split('.')
        path = ROOT / 'src' / (parts[0] + '.silk')
        if not path.exists() or not re.search(r'\b(?:fn|const|struct)\s+' + re.escape(parts[-1]) + r'\b', path.read_text()):
            missing_targets.append(item['silk'])
    for name in sorted(missing):
        print('MISSING upstream method:', name)
    for name in sorted(missing_targets):
        print('MISSING Silk declaration:', name)
    if missing or missing_targets:
        raise SystemExit(1)
    print(f'{len(expected)} / {len(expected)} upstream public methods mapped; {len(entries)} total API entries.')
    print('Inventory coverage only; native execution is verified separately by make smoke.')

if __name__ == '__main__':
    main()
