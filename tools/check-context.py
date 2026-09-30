#!/usr/bin/env python3
"""Compare shared switch instruction sequences; list deliberate port differences."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parent.parent

def block(path, label):
    text = (root / path).read_text()
    result = []
    active = False
    for line in text.splitlines():
        line = line.split('|')[0].strip()
        if line == label + ':':
            active = True
            continue
        if active and re.match(r'[.\w]+:', line):
            break
        if active and line and not line.startswith(('.', '#')):
            line = re.sub(r'\s+', ' ', line.lower()).replace('%', '')
            line = re.sub(r',\s*', ',', line)
            line = re.sub(r'\b(bsr|bra)\.w\b', r'\1', line)
            result.append(line)
    return result

errors = []
for policy, port in [('cooperative', 'coop/ctx_switch.s'), ('preemptive', 'pre/preempt_ctx_switch.s')]:
    production = f'ssos/os/kernel/{policy}/interrupts.s'
    qemu = 'tests/qemu/' + port
    for label in ['ss_task_yield', '.resume_task', '.resume_existing', '.resume_interrupted', '.start_task']:
        a, b = block(production, label), block(qemu, label)
        if a != b:
            errors.append((policy, label, a, b))
if errors:
    for policy, label, a, b in errors:
        print(policy, label, '\n production:', a, '\n QEMU:', b)
    sys.exit(1)
print('Context common instruction sequences: identical (relocations/layout not compared)')
print('Excluded: entry/TTY, Timer D source/TRAP, MFP EOI, real asynchronous timing, linked addresses.')
