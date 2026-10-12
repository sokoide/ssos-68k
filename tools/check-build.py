#!/usr/bin/env python3
"""Build matrix, dependency mutation and no-op checks; source bytes stay unchanged."""
import datetime
import os
from pathlib import Path
import subprocess
import sys
import time

root = Path(__file__).resolve().parent.parent
out = root / 'build' / 'build-check' / datetime.datetime.now().strftime('%Y%m%dT%H%M%S%f')
out.mkdir(parents=True)
env = dict(os.environ, SS_EXPORT_DIR=str(root / 'build/published'))
step = 0

def run(*args):
    global step
    step += 1
    log = out / f'{step:02d}.log'
    with log.open('w') as f:
        result = subprocess.run(['make', *args], cwd=root, env=env, stdout=f, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f'{args}: {log}\n' + log.read_text()[-3000:])


def outputs():
    return {str(p): p.stat().st_mtime_ns for p in (root / 'ssos').glob('**/build/**/*')
            if p.is_file() and p.suffix in ('.o', '.elf', '.bin', '.xdf', '.x')}


def touch_check(source, target, *args):
    p = root / source
    t = root / target
    old = t.stat().st_mtime_ns
    st = p.stat()
    try:
        # Advance past existing object to avoid filesystem timestamp granularity.
        # GNU make on this host compares whole seconds for some dependencies.
        delay = old // 1000000000 + 1.05 - time.time()
        if delay > 0: time.sleep(delay)
        timestamp = time.time_ns()
        os.utime(p, ns=(st.st_atime_ns, timestamp))
        run(*args)
        if t.stat().st_mtime_ns == old:
            raise RuntimeError('dependency not rebuilt: ' + source + ' -> ' + target)
    finally:
        os.utime(p, ns=(st.st_atime_ns, st.st_mtime_ns))

try:
    run('clean')
    for profile in (0, 1, 0):
        run('-j4', 'all', f'SS_PROFILE_GFX={profile}')
    for iteration in range(2):
        before = outputs()
        run('-j4', 'all', 'SS_PROFILE_GFX=0')
        changed = [p for p, stamp in outputs().items() if before.get(p) != stamp]
        if changed:
            raise RuntimeError('no-op changed outputs: ' + str(changed))
    touch_check('ssos/os/kernel/task.h', 'ssos/os/build/cooperative/profile-0/kernel/scheduler.o', '-j4', 'all')
    touch_check('ssos/os/kernel/x68k_mfp.h', 'ssos/standalone/build/cooperative/profile-0/main.o', '-j4', 'all')
    touch_check('ssos/os/gfx/gfx_hw.h', 'ssos/os/build/cooperative/profile-0/gfx/vram.o', '-j4', 'all')
    touch_check('ssos/os/kernel/scheduler.h', 'ssos/os/build/cooperative/profile-0/kernel/scheduler.o', '-j4', 'all')
    touch_check('ssos/os/kernel/scheduler_abi.h', 'ssos/standalone/build/cooperative/profile-0/interrupts.o', '-j4', 'all')
    touch_check('ssos/include/iocscall.mac', 'ssos/standalone/build/cooperative/profile-0/trap14.o', '-j4', 'all')
    touch_check('ssos/os/kernel/linker.ld', 'ssos/os/build/cooperative/profile-0/SSOS.X.elf', '-j4', 'all')
    touch_check('tools/makedisk/main.c', 'ssos/build/disk/cooperative/profile-0/ssos_cop.xdf', '-j4', 'all')
    before = outputs()
    run('-j4', 'all', 'EXTRA_CFLAGS=-DSS_DIAG_TASKS=32 -DSS_PROFILE_RUNTIME=1')
    after = outputs()
    obj = str(root / 'ssos/os/build/cooperative/profile-0/kernel/scheduler.o')
    if before[obj] == after[obj]:
        raise RuntimeError('flag change did not rebuild C objects')
    run('-j4', 'all', 'EXTRA_CFLAGS=')
    before = outputs()
    run('-j4', 'all')
    if before != outputs():
        raise RuntimeError('final no-op changed outputs')
    print('Build matrix/dependencies/flags/two no-op runs: PASS; logs: ' + str(out))
except Exception as e:
    print(e, file=sys.stderr)
    sys.exit(1)
