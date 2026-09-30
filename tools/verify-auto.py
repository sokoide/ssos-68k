#!/usr/bin/env python3
"""Sequential automated verification with durable logs and artifact hashes."""
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
os.chdir(root)
out = root / 'build' / 'verification' / datetime.datetime.now().strftime('%Y%m%dT%H%M%S%f')
out.mkdir(parents=True)

def capture(args):
    try:
        result = subprocess.run(args, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        return result.stdout.strip()
    except OSError as e:
        return str(e)

manifest = {
    'commit': capture(['git', 'rev-parse', 'HEAD']),
    'status': capture(['git', 'status', '--short']),
    'versions': {name: capture([name, '--version']) for name in ['cc', 'm68k-xelf-gcc', 'qemu-system-m68k']},
    'steps': [], 'artifacts': [], 'sources': [],
}
for name in capture(['git', 'ls-files', '-co', '--exclude-standard']).splitlines():
    if name.startswith(('ssos/', 'tools/', 'tests/', 'docs/')) or name in ('Makefile', 'README.md', '.gitignore'):
        path = root / name
        if path.is_file():
            manifest['sources'].append({'path': name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
(out / 'source.diff').write_text(capture(['git', 'diff', 'HEAD', '--', 'ssos', 'tools', 'tests', 'docs', 'Makefile', 'README.md', '.gitignore']) + '\n')
env = dict(os.environ, SS_TEST_LOG_DIR=str(out / 'qemu'), SS_EXPORT_DIR=str(root / 'build/published'))
commands = [ ['make', 'test'], ['make', 'test', 'SANITIZE=1'], ['make', 'test-tools'],
             ['make', 'test-qemu'], ['make', 'test-asm'], ['python3', 'tools/check-context.py'], ['python3', 'tools/bench-native.py'], ['python3', 'tools/check-build.py'],
             ['make', 'verify-check'] ]
status = 0
try:
    for i, args in enumerate(commands):
        log = out / f'{i:02d}.log'
        print(' '.join(args) + ' -> ' + str(log), flush=True)
        with log.open('w') as f:
            result = subprocess.run(args, env=env, stdout=f, stderr=subprocess.STDOUT)
        manifest['steps'].append({'command': args, 'exit': result.returncode, 'log': str(log)})
        if result.returncode:
            print(log.read_text()[-5000:])
            status = result.returncode
            break
    disasm = out / 'disassembly'
    disasm.mkdir()
    objects = list((root / 'ssos').glob('**/build/**/interrupts.o')) + list((root / 'ssos').glob('**/build/**/trap14.o')) + list((root / 'ssos').glob('**/build/**/mfp_state.o'))
    objects += list((root / 'tests/qemu').glob('*/ctx_switch.o')) + list((root / 'tests/qemu').glob('*/preempt_ctx_switch.o'))
    for obj in objects:
        name = str(obj.relative_to(root)).replace('/', '_') + '.txt'
        (disasm / name).write_text(capture(['m68k-xelf-objdump', '-dr', str(obj)]) + '\n')
    for p in sorted((root / 'ssos').glob('**/build/**/*')):
        if p.is_file() and p.suffix in ('.x', '.xdf', '.elf'):
            manifest['artifacts'].append({'path': str(p.relative_to(root)), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest(), 'bytes': p.stat().st_size})
finally:
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Verification manifest: ' + str(out / 'manifest.json'))
sys.exit(status)
