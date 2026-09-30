#!/usr/bin/env python3
"""Collect host C/RAM measurements; never substitute them for target timing."""
import datetime
import hashlib
import json
from pathlib import Path
import platform
import subprocess

root = Path(__file__).resolve().parent.parent
out = root / 'build/native-performance' / datetime.datetime.now().strftime('%Y%m%dT%H%M%S%f')
out.mkdir(parents=True)
metadata = {'host': platform.platform(), 'batch_operations': 100,
            'commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root).decode().strip(),
            'status': subprocess.check_output(['git', 'status', '--short'], cwd=root).decode(),
            'scope': 'host C logic and RAM pixels only; no real switch/IRQ/MMIO/DMA timing'}
(out / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
checksums = {}
for policy in ('cooperative', 'preemptive'):
    for profile in (0, 1):
        binary = f'build/native-bench-{policy}-profile{profile}'
        with (out / f'build-{policy}-{profile}.log').open('w') as log:
            subprocess.run(['make', '-C', 'tests', '-f', 'Makefile.native', binary,
                            f'SCHED={policy}', f'BENCH_PROFILE={profile}'], cwd=root,
                           stdout=log, stderr=subprocess.STDOUT, check=True)
        for mode in (8, 16):
            raw = subprocess.check_output([str(root / 'tests' / binary), str(mode)], cwd=root)
            (out / f'{policy}-profile{profile}-mode{mode}.jsonl').write_bytes(raw)
            for line in raw.decode().splitlines():
                record = json.loads(line)
                if record['phase'] == 'scene-pixels':
                    value = record['frames'], record['input'], record['pixels']
                    if record['frames'] != 517:
                        raise RuntimeError('incomplete replay')
                    if mode in checksums and checksums[mode] != value:
                        raise RuntimeError('replay checksum mismatch across runs/policies/profiles')
                    checksums[mode] = value
(out / 'checksum-verdict.json').write_text(json.dumps(checksums, indent=2) + '\n')
print('Host measurements and replay checksums: PASS; logs: ' + str(out))
