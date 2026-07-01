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
# Only changes under ssos/ (the source tree that builds ssos_cop/pre.x/xdf) are
# classified. Everything else (tools/, tests/, docs/, CI, *.md) does not affect
# the binaries and is reported as "no impact".
#
# Each SSOS source is classified as:
#   covered   - make test / make test-qemu fully cover it -> no HW check needed
#   partial   - partly covered                          -> HW check maybe needed
#   uncovered - not covered by any test                 -> HW check required
#
# Always exits 0 (informational).

set -u

cd "$(git rev-parse --show-toplevel)" || exit 1

REF="${1:-}"
if [ -n "$REF" ]; then
    all_files=$(git diff --name-only "$REF" -- 2>/dev/null)
    range_label="$REF"
else
    all_files=$(git diff --name-only HEAD -- 2>/dev/null)
    range_label="HEAD (working tree)"
fi

# Split into SSOS sources (impact the binaries) vs everything else.
sources=()
noise_count=0
while IFS= read -r f; do
    [ -z "$f" ] && continue
    case "$f" in
        ssos/*) sources+=("$f") ;;
        *)      noise_count=$((noise_count + 1)) ;;
    esac
done <<< "$all_files"

src_count=${#sources[@]}

echo "verify-check: $range_label の変更を分析"
echo
echo "変更ファイルの内訳:"
echo "  SSOS ソース（実行バイナリに影響）: ${src_count}件"
echo "  テスト・ツール・ドキュメント等（影響なし）: ${noise_count}件"
echo

if [ "$src_count" -eq 0 ]; then
    echo "→ 実機確認は不要です。"
    echo "  実行バイナリ(ssos_*.x/.xdf)に影響するソース変更がありません。"
    echo "  念のため make test / make test-qemu を実行してテスト通過を確認してください。"
    exit 0
fi

# classify_interrupts: decide severity for an interrupts.s change by which
# function the edited lines live in.
#   covered   - pure context switch (ss_task_yield / .resume_task / .resume_*
#               / .start_task / ss_context_switch) — qemu coop+preempt cover it
#   partial   - ISR entry (ss_timerd_handler / ss_vdisp_handler / ss_nop_handler)
#               — mixes ctx switch with MFP EOI / vector setup
#   uncovered - MFP init/save/restore (ss_set_interrupts etc.) or data/unknown
# Stdout: "<status>\t<reason>"
classify_interrupts() {
    local file="$1"
    local diffout
    if [ -n "$REF" ]; then
        diffout=$(git diff -U0 "$REF" -- "$file" 2>/dev/null)
    else
        diffout=$(git diff -U0 HEAD -- "$file" 2>/dev/null)
    fi
    # No diff (shouldn't happen — file is in sources[]): default to partial.
    if [ -z "$diffout" ]; then
        printf 'partial\tinterrupts.s: 変更箇所特定不可（安全側 partial）\n'
        return
    fi

    # Label table: "lineno<TAB>labelname" (global + indented local labels).
    local labels
    labels=$(grep -nE '^[[:space:]]*[.A-Za-z_][.A-Za-z_0-9]*:' "$file" 2>/dev/null \
             | sed -E 's/^([0-9]+):[[:space:]]*([.A-Za-z_][.A-Za-z_0-9]*):.*/\1\t\2/')

    # new-side start line of each diff hunk: @@ -o,oc +n,nc @@ -> n
    local starts
    starts=$(printf '%s\n' "$diffout" | sed -nE 's/^@@ -[0-9,]+ \+([0-9]+).*/\1/p')

    local worst=0 touched=""   # 0=covered 1=partial 2=uncovered
    local start label sev
    while IFS= read -r start; do
        [ -z "$start" ] && continue
        # find nearest label at or before `start`
        label="(data/unknown)"
        local ln nm
        while IFS=$'\t' read -r ln nm; do
            [ -z "$ln" ] && continue
            if [ "$ln" -le "$start" ]; then label="$nm"; fi
        done <<< "$labels"

        case "$label" in
            ss_context_switch|.resume_task|.resume_interrupted|.start_task|ss_task_yield|.yield_resume)
                sev=0 ;;
            ss_timerd_handler|ss_vdisp_handler|ss_nop_handler|ss_key_handler)
                sev=1 ;;
            ss_set_interrupts|ss_restore_interrupts|save_interrupts|restore_interrupts|ss_save_data_base)
                sev=2 ;;
            *)
                sev=2 ;;   # .data/.bss or unknown -> safe side
        esac
        [ "$sev" -gt "$worst" ] && worst=$sev
        case " $touched " in *" $label "*) ;; *) touched="${touched}${touched:+ }$label";; esac
    done <<< "$starts"

    case "$worst" in
        0) printf 'covered\t\n' ;;
        1) printf 'partial\tISR 関数(%s): ctx switch 部分は qemu カバー、MFP/EOI 部分は未カバー\n' "$touched" ;;
        2) printf 'uncovered\tMFP/HW 関連(%s): テスト未カバー\n' "$touched" ;;
    esac
}

# Classify each SSOS source.
# Output fields per source: <status>\t<scheds>\t<modes>\t<reason>
#   status  : covered | partial | uncovered
#   scheds  : space-separated subset of {cop pre}
#   modes   : space-separated subset of {x xdf}
#   reason  : short note shown when the source needs HW verification
classify() {
    local f="$1"
    case "$f" in
        ssos/os/util/numfmt.c|ssos/os/util/numfmt.h)
            printf 'covered\tcop pre\tx xdf\t\n' ;;
        ssos/os/mem/buddy.c|ssos/os/mem/slab.c|ssos/os/mem/memory.h)
            printf 'covered\tcop pre\tx xdf\t\n' ;;
        ssos/os/kernel/cooperative/scheduler.c)
            printf 'covered\tcop\tx xdf\t\n' ;;
        ssos/os/kernel/preemptive/scheduler.c)
            printf 'covered\tpre\tx xdf\t\n' ;;
        ssos/os/win/window.c|ssos/os/win/win.h)
            printf 'partial\tcop pre\tx xdf\tロジックは Native カバー。描画部分は gfx stub で未検証\n' ;;
        ssos/os/kernel/cooperative/interrupts.s|ssos/os/kernel/preemptive/interrupts.s)
            local ir sched
            case "$f" in *cooperative*) sched="cop";; *) sched="pre";; esac
            ir=$(classify_interrupts "$f")
            printf '%s\t%s\tx xdf\t%s\n' "${ir%%$'\t'*}" "$sched" "${ir#*$'\t'}" ;;
        ssos/os/kernel/scheduler.h|ssos/os/kernel/kernel.h|ssos/os/kernel/work_queue.c|ssos/os/kernel/work_queue.h)
            printf 'partial\tcop pre\tx xdf\t構造体レイアウト変更等は asm と整合要。変更内容によって実機必要\n' ;;
        ssos/os/gfx/vram.c|ssos/os/gfx/gfx.h)
            printf 'uncovered\tcop pre\tx xdf\tVRAM/DMA 直接操作でテスト未カバー\n' ;;
        ssos/os/kernel/premain.c|ssos/os/kernel/cooperative/premain.c|ssos/os/kernel/preemptive/premain.c)
            printf 'uncovered\tcop pre\txdf\tIOCS/HW 初期化。.x(standalone)は独自経路\n' ;;
        ssos/os/kernel/entry.s|ssos/os/kernel/linker.ld)
            printf 'uncovered\tcop pre\txdf\tOS エントリ/リンカ（.xdf 専用）\n' ;;
        ssos/os/ipc/message.c|ssos/os/ipc/ipc.h)
            printf 'covered\tcop pre\txdf\t\n' ;;
        ssos/os/app/main.c)
            printf 'uncovered\tcop pre\txdf\tアプリ本体（.xdf 専用）\n' ;;
        ssos/boot/*)
            printf 'uncovered\tcop pre\txdf\tブートローダ（.xdf 専用）\n' ;;
        ssos/standalone/main.c)
            printf 'uncovered\tcop pre\tx\tstandalone 本体（.x 専用）\n' ;;
        *)
            printf 'uncovered\tcop pre\tx xdf\tパターン外 — 安全のため実機確認\n' ;;
    esac
}

# Aggregate: per-target cause lists (space-separated file names), and reasons.
causes_cop_x=""
causes_pre_x=""
causes_cop_xdf=""
causes_pre_xdf=""
reasons=""
all_covered=1
NL='
'

add_reason() {
    # newline-separated; substring match for dedup (reasons contain spaces)
    case "$reasons" in
        *"$1"*) ;;
        *) reasons="${reasons}${reasons:+$NL}$1" ;;
    esac
}

for f in "${sources[@]}"; do
    IFS=$'\t' read -r status scheds modes reason <<< "$(classify "$f")"

    if [ "$status" = "covered" ]; then
        continue
    fi
    all_covered=0

    [ -n "$reason" ] && add_reason "$reason"

    # Each source is visited once, so a plain append can't duplicate within a
    # target. Multiple targets share the same file name — that's intended.
    for s in $scheds; do
        for m in $modes; do
            case "$s:$m" in
                cop:x)   causes_cop_x="${causes_cop_x}${causes_cop_x:+ }$f" ;;
                pre:x)   causes_pre_x="${causes_pre_x}${causes_pre_x:+ }$f" ;;
                cop:xdf) causes_cop_xdf="${causes_cop_xdf}${causes_cop_xdf:+ }$f" ;;
                pre:xdf) causes_pre_xdf="${causes_pre_xdf}${causes_pre_xdf:+ }$f" ;;
            esac
        done
    done
done

if [ "$all_covered" -eq 1 ]; then
    echo "→ 実機確認は不要です。"
    echo "  変更は全て make test / make test-qemu でカバーされています。"
    exit 0
fi

emit_target() {
    # $1 = label, $2 = space-separated causes
    if [ -n "$2" ]; then
        printf '  %-14s ← %s\n' "$1" "$2"
    fi
}

echo "■ 実機確認が必要なターゲット:"
emit_target "ssos_cop.x"   "$causes_cop_x"
emit_target "ssos_pre.x"   "$causes_pre_x"
emit_target "ssos_cop.xdf" "$causes_cop_xdf"
emit_target "ssos_pre.xdf" "$causes_pre_xdf"

# Note scheduler-coverage gaps.
if [ -z "$causes_cop_x" ] && [ -z "$causes_cop_xdf" ]; then
    echo "  （cooperative は影響なし）"
fi
if [ -z "$causes_pre_x" ] && [ -z "$causes_pre_xdf" ]; then
    echo "  （preemptive は影響なし）"
fi

echo
echo "理由:"
while IFS= read -r r; do
    [ -n "$r" ] && echo "  - $r"
done <<< "$reasons"
echo
echo "make test では確認できない部分です。上記ターゲットをエミュレータ/実機で確認してください。"

exit 0
