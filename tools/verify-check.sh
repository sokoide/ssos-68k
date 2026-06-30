#!/bin/bash
# verify-check.sh - report what hardware verification a change set needs.
#
# Usage:
#   tools/verify-check.sh [REF]
#
#   REF   git diff target. Omit to analyze the working tree (staged + unstaged
#         vs HEAD). Pass e.g. HEAD~1 to analyze the last commit, or main..HEAD
#         to analyze a branch.
#
# Each changed source is classified into a tier:
#   Tier 1: fully covered by `make test` / `make test-qemu` -> no HW check needed
#   Tier 2: partially covered                       -> HW check maybe needed (see note)
#   Tier 3: not covered by any test                 -> HW check required
#
# Tier 2/3 results are aggregated into the set of build targets that need a
# manual run: ssos_cop.x / ssos_pre.x / ssos_cop.xdf / ssos_pre.xdf.
#
# Always exits 0 (informational). Output is meant for a human before commit.

set -u

REF="${1:-}"

cd "$(git rev-parse --show-toplevel)" || exit 1

if [ -n "$REF" ]; then
    files=$(git diff --name-only "$REF" -- 2>/dev/null)
    range_label="$REF"
else
    # working tree (staged + unstaged) vs HEAD
    files=$(git diff --name-only HEAD -- 2>/dev/null)
    range_label="HEAD (working tree)"
fi

# Filter to actual content changes, dropping test/doc/build-config noise.
filter_irrelevant() {
    while IFS= read -r f; do
        case "$f" in
            tests/*)          ;;  # keep — test-only changes are reported as covered
            *.md)             ;;
            Makefile|*/Makefile|.gitignore|AGENTS.md|.clangd|.clang-format) ;;
            *.json)           ;;
            .ai-handoff.md)   ;;
            *)                printf '%s\n' "$f" ;;
        esac
    done
}

relevant=$(printf '%s\n' "$files" | filter_irrelevant | sed '/^$/d')

if [ -z "$relevant" ]; then
    echo "verify-check: no relevant source changes vs $range_label"
    echo "  (tests/docs/build-config only, or no changes)"
    exit 0
fi

echo "verify-check: changes vs $range_label"
echo

# Per-file classification. Emits lines: <file>\t<tier>\t<covered>\t<scheds>\t<modes>\t<note>
classify() {
    local f="$1"
    case "$f" in
        ssos/os/util/numfmt.c|ssos/os/util/numfmt.h)
            printf '%s\t1\tNative numfmt\tcop pre\tx xdf\t\n' "$f" ;;
        ssos/os/mem/buddy.c|ssos/os/mem/slab.c|ssos/os/mem/memory.h)
            printf '%s\t1\tNative mem\tcop pre\tx xdf\t\n' "$f" ;;
        ssos/os/kernel/cooperative/scheduler.c)
            printf '%s\t1\tNative sched + qemu coop\tcop\tx xdf\t\n' "$f" ;;
        ssos/os/kernel/preemptive/scheduler.c)
            printf '%s\t1\tNative sched + qemu preempt\tpre\tx xdf\t\n' "$f" ;;
        ssos/os/win/window.c|ssos/os/win/win.h)
            printf '%s\t2\tNative logic (gfx stubbed)\tcop pre\tx xdf\tgfx 描画ロジックの変更は実機必要\n' "$f" ;;
        ssos/os/kernel/cooperative/interrupts.s)
            printf '%s\t2\tqemu coop ctx switch (MFP 未カバー)\tcop\tx xdf\tMFP/Timer/ISR 登録の変更は実機必要。純粋 ctx switch は qemu カバー\n' "$f" ;;
        ssos/os/kernel/preemptive/interrupts.s)
            printf '%s\t2\tqemu preempt ctx switch (MFP 未カバー)\tpre\tx xdf\tMFP/Timer/ISR 登録の変更は実機必要。純粋 ctx switch は qemu カバー\n' "$f" ;;
        ssos/os/kernel/scheduler.h|ssos/os/kernel/kernel.h|ssos/os/kernel/work_queue.c|ssos/os/kernel/work_queue.h)
            printf '%s\t2\t部分カバー\tcop pre\tx xdf\tSSTask 構造体レイアウト変更等は asm と整合要→実機必要\n' "$f" ;;
        ssos/os/gfx/vram.c|ssos/os/gfx/gfx.h)
            printf '%s\t3\t未カバー (stub)\tcop pre\tx xdf\t\n' "$f" ;;
        ssos/os/kernel/premain.c|ssos/os/kernel/cooperative/premain.c|ssos/os/kernel/preemptive/premain.c)
            printf '%s\t3\t未カバー (IOCS)\tcop pre\txdf\t.x は standalone 別経路\n' "$f" ;;
        ssos/os/kernel/entry.s|ssos/os/kernel/linker.ld)
            printf '%s\t3\t未カバー\tcop pre\txdf\t\n' "$f" ;;
        ssos/os/ipc/message.c|ssos/os/ipc/ipc.h)
            printf '%s\t3\t未カバー\tcop pre\txdf\t\n' "$f" ;;
        ssos/os/app/main.c)
            printf '%s\t3\t未カバー\tcop pre\txdf\t\n' "$f" ;;
        ssos/boot/*)
            printf '%s\t3\t未カバー (boot path)\tcop pre\txdf\t.x は boot 不使用\n' "$f" ;;
        ssos/standalone/main.c)
            printf '%s\t3\t未カバー (standalone HW)\tcop pre\tx\t.xdf は boot 経路\n' "$f" ;;
        *)
            printf '%s\t?\tUNKNOWN\tcop pre\tx xdf\tパターン外 — 実機確認を推奨\n' "$f" ;;
    esac
}

max_tier=1
need_cop_x=0; need_pre_x=0; need_cop_xdf=0; need_pre_xdf=0
notes=()
unknowns=()

while IFS=$'\t' read -r f tier covered scheds modes note; do
    [ -z "$f" ] && continue
    echo "  [Tier $tier] $f — $covered"
    [ -n "$note" ] && echo "      注: $note"

    if [ "$tier" != "1" ]; then
        [ "$tier" = "?" ] && unknowns+=("$f")
        # aggregate target flags from scheds × modes
        case " $scheds " in *" cop "*) case " $modes " in *" x "*) need_cop_x=1;; esac; case " $modes " in *" xdf "*) need_cop_xdf=1;; esac;; esac
        case " $scheds " in *" pre "*) case " $modes " in *" x "*) need_pre_x=1;; esac; case " $modes " in *" xdf "*) need_pre_xdf=1;; esac;; esac
        [ -n "$note" ] && notes+=("$f: $note")
        # bump max_tier (? treated as 3)
        case "$tier" in
            2) [ "$max_tier" -lt 2 ] && max_tier=2 ;;
            3|"?") max_tier=3 ;;
        esac
    fi
done < <(while IFS= read -r f; do classify "$f"; done <<< "$relevant")

echo

if [ "$max_tier" -eq 1 ]; then
    echo "✓ make test / make test-qemu で確認可能。実機確認は不要。"
    exit 0
fi

# Build the target list.
targets=()
[ "$need_cop_x"   -eq 1 ] && targets+=("ssos_cop.x")
[ "$need_pre_x"   -eq 1 ] && targets+=("ssos_pre.x")
[ "$need_cop_xdf" -eq 1 ] && targets+=("ssos_cop.xdf")
[ "$need_pre_xdf" -eq 1 ] && targets+=("ssos_pre.xdf")

echo "→ 実機確認が必要なターゲット:"
if [ "${#targets[@]}" -gt 0 ]; then
    printf '    %s\n' "${targets[@]}"
else
    echo "    (該当ターゲットなし — Tier 2 の注意事項のみ参照)"
fi

if [ "${#unknowns[@]}" -gt 0 ]; then
    echo
    echo "! 未分類ファイル — 必ず実機確認すること:"
    printf '    %s\n' "${unknowns[@]}"
fi

exit 0
