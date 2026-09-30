#!/usr/bin/env python3
"""Report binary impact without equating QEMU ports with production MMIO."""
import subprocess
import sys


def git(*args):
    return subprocess.check_output(['git', *args], stderr=subprocess.PIPE).decode()


def classify(path):
    if path.startswith('tools/makedisk/'):
        return ('cop', 'pre'), ('xdf',), 'disk生成tool: 起動画像が変わる'
    if path == 'ssos/common-sources.mk' or path == 'Makefile' or path.startswith('tools/build-config'):
        return ('cop', 'pre'), ('x', 'xdf'), 'build依存/設定: 成果物対応を確認'
    if not path.startswith('ssos/'):
        return (), (), '本体外: テスト/文書/検証tool'
    scheds = ('cop',) if '/cooperative/' in path else ('pre',) if '/preemptive/' in path else ('cop', 'pre')
    modes = ('x',) if '/standalone/' in path else ('xdf',) if (
        '/boot/' in path or path.endswith(('premain.c', 'entry.s', 'linker.ld', 'app/main.c')) or
        '/ipc/' in path or '/work_queue.' in path) else ('x', 'xdf')
    pure = ('os/util/numfmt.c', 'os/util/numfmt.h')
    if path[5:] in pure:
        return (), (), 'Nativeで純粋ロジックを検証可能'
    if path.endswith('interrupts.s') or path.endswith('scheduler.c') or path.endswith('wakeups.c'):
        reason = 'C/QEMU移植版は自動検証あり。production/非同期MFPは未検証'
    elif '/mem/' in path or '/win/' in path:
        reason = 'Nativeロジック/画素は自動検証あり。target時間/所有権は別確認'
    else:
        reason = 'target初期化/MMIO/ABI/構成は手動確認が必要'
    return scheds, modes, reason


def main():
    try:
        root = git('rev-parse', '--show-toplevel').strip()
        import os
        os.chdir(root)
        ref = sys.argv[1] if len(sys.argv) > 1 else 'HEAD'
        # diff errors must not become a clean no-impact result.
        paths = set(git('diff', '--name-only', '-z', ref, '--').split('\0'))
        paths.update(git('ls-files', '--others', '--exclude-standard', '-z').split('\0'))
    except subprocess.CalledProcessError as e:
        print('verify-check: Git/REF error: ' + e.stderr.decode().strip(), file=sys.stderr)
        return 1
    paths.discard('')
    print('verify-check: ' + ref + ' + untracked sources')
    targets = set()
    for path in sorted(paths):
        scheds, modes, reason = classify(path)
        if scheds:
            for sched in scheds:
                for mode in modes:
                    targets.add(f'ssos_{sched}.{mode}')
            print(f'  {path}: {reason}')
    if targets:
        print('手動確認対象: ' + ' '.join(sorted(targets)))
        print('自動検証の成功は実機確認完了を意味しない。成果物hashと確認条件を記録する。')
    else:
        print('本体/画像への影響なし。検証tool変更は別途fixtureで確認する。')
    return 0


if __name__ == '__main__':
    sys.exit(main())
