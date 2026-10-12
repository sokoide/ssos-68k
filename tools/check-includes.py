#!/usr/bin/env python3
"""Check the established C include boundaries (not full C dependency analysis)."""
from collections import deque
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent.parent
PUBLIC = (
    'ssos/os/kernel/status.h', 'ssos/os/kernel/task.h',
    'ssos/os/kernel/irq.h', 'ssos/os/kernel/clock.h',
    'ssos/os/kernel/work_queue.h', 'ssos/os/kernel/runtime_guard.h',
    'ssos/os/mem/memory.h', 'ssos/os/ipc/ipc.h',
    'ssos/os/gfx/gfx.h', 'ssos/os/gfx/palette.h', 'ssos/os/gfx/profile.h',
    'ssos/os/win/win.h', 'ssos/os/app/scene.h', 'ssos/os/app/scene_replay.h',
)
APPLICATION = ('ssos/os/app/scene.c', 'ssos/os/app/scene_replay.c')
CORE = ('ssos/os/kernel/scheduler.c', 'ssos/os/kernel/work_queue.c',
        'ssos/os/kernel/sched_profile.c', 'ssos/os/ipc/message.c',
        'ssos/os/mem/buddy.c', 'ssos/os/mem/slab.c')
HARDWARE = {'ssos/os/kernel/x68k_mfp.h', 'ssos/os/kernel/boot.h',
            'ssos/os/gfx/gfx_hw.h'}
INTERNAL = HARDWARE | {'ssos/os/kernel/scheduler.h',
                       'ssos/os/kernel/scheduler_abi.h',
                       'ssos/os/kernel/context_frame.h',
                       'ssos/os/kernel/main_task.h',
                       'ssos/os/kernel/sched_profile.h'}


def include_graph(root):
    graph = {}
    search = [root / 'ssos/os' / name for name in
              ('kernel', 'gfx', 'mem', 'ipc', 'win', 'util', 'app')]
    for path in sorted((root / 'ssos').rglob('*')):
        if path.suffix not in ('.c', '.h') or 'build' in path.parts:
            continue
        text = re.sub(r'/\*.*?\*/|//[^\n]*', '', path.read_text(), flags=re.S)
        edges = []
        for delim, name in re.findall(r'^\s*#\s*include\s*(["<])([^">]+)[">]',
                                      text, flags=re.M):
            candidates = [path.parent / name] + [directory / name for directory in search]
            resolved = next((p.resolve() for p in candidates if p.is_file()), None)
            if resolved is None:
                edges.append('<' + name + '>' if delim == '<' else 'missing:' + name)
            elif resolved.is_relative_to(root):
                edges.append(resolved.relative_to(root).as_posix())
            else:
                edges.append('external:' + str(resolved))
        graph[path.relative_to(root).as_posix()] = edges
    return graph


def check(root, public=PUBLIC, application=APPLICATION, core=CORE):
    root = root.resolve()
    graph = include_graph(root)
    errors = []
    for role, sources, denied in (('public', public, INTERNAL),
                                  ('application', application, INTERNAL),
                                  ('core', core, HARDWARE)):
        for source in sources:
            if source not in graph:
                errors.append('missing boundary source: ' + source)
                continue
            pending = deque([(source, [source])])
            visited = set()
            while pending:
                node, chain = pending.popleft()
                if node in visited:
                    continue
                visited.add(node)
                # IOCS/DOS may still be used by scene until E03. Kernel core
                # and public headers must never acquire that device dependency.
                device = node.startswith('<x68k/') and role != 'application'
                if node in denied or device or node.startswith(('missing:', 'external:')):
                    errors.append(' -> '.join(chain))
                    continue
                pending.extend((edge, chain + [edge]) for edge in graph.get(node, ()))
    return sorted(set(errors))


def main():
    errors = check(ROOT)
    if errors:
        print('Include boundaries: FAIL')
        print('\n'.join(errors))
        return 1
    print('Include boundaries: PASS (public headers, scene, kernel core; transitive includes)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
