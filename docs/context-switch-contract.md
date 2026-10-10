# Context switchの契約とQEMUとの差分

正本: `ssos/os/kernel/scheduler_abi.h`（TCB）、`context_frame.h`（frame）、`scheduler.c`（queue/lifecycle）。

保存 register は両方式とも d0–d7/a0–a6 の 15 long＝60 bytes。以下の offset は保存 SP からの値。C の packed/aligned(2)構造と静的 assert、`tools/check-context.py`の instruction 列比較で維持する。

| frame | SR | PC | 復帰 |
| --- | --- | --- | --- |
| cooperative手動yield | +60 | +62 | register→SR→PC、jmp→yield呼出し元へrts |
| preemptive手動yield | +64 | +60 | register→PC→SR、jmp→yield呼出し元へrts |
| MC68000通常割り込み | +60 | +62 | register→rte |

手動 frame と通常例外 frame はいずれも 66 bytes。これは bus/address error の特殊 frame を scheduler で復帰できるという意味ではない。entry は arg を stack へ渡して呼び、return は ss_task_exit へ進む。ID/stack を再利用しない。canary 初期化中は INITIALIZING で、start できない。

| 境界 | production | QEMU | 自動確認の範囲 |
| --- | --- | --- | --- |
| yield/resume/start | 方式別interrupts.s | 方式別port | 共通instruction列、TCB/frame assert、引数/return/SR、1/2/8/32task |
| preemption | 実MFP Timer D | taskの同期trap #0 | rteと10tick cadence。任意命令の非同期競合は手動確認 |
| MFP/EOI | setup/save/restore、割り込みsource | TTY/virt vector、MFPなし | 未検証 |
| 起動とstack | IPL/Human68Kと実linker pool | QEMU linker＋静的pool | slot/entry/canaryの機構。実RAM配置は手動確認 |
| TRAP復元 | standalone/trap14.s | 同じproduction objectのrestore関数のみ | IPL0〜7＋ネストを実CPU命令で確認。IOCS初期化/abortは手動確認 |

`check-context.py`はリンク後アドレス、relocation、opcode を比較するものではない。object/ELF の逆アセンブルは検証 run へ保存し、実機 trace と照合する。QEMU との共通 instruction 列の一致だけで非同期安全性を宣言しない。
