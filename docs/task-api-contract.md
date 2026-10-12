# Task・UI要求APIの契約

公開 task 宣言は `ssos/os/kernel/task.h`、error は `status.h`。`scheduler.h` は TCB/queue/切替入口の内部 header で、application は include しない。IRQ の宣言は `irq.h`、counter は `clock.h`。signature と error の符号化は従来と同じ。

| API | 成功 | 制限と失敗 |
| --- | --- | --- |
| ss_task_create | uint16_tのID 1〜32 | 負のSS_ERR_*はuint16_tへ符号化される。INITIALIZING中はstart不可、完了後DORMANT。custom stackは終了後もcaller所有 |
| ss_task_start | SS_OK | DORMANTだけを一度READYへ。INITIALIZING/TERMINATEDはSS_ERR_STATE |
| ss_task_sleep | SS_OK | 実行可能peerがない場合SS_ERR_STATE。最大delayは0x7fffffff tick。復帰後はcaller SRを復元 |
| ss_recv | 値コピーのmessage | table外mainはSS_ERR_STATE。唯一の実行可能taskが空queueで待つ場合もSS_ERR_STATE |
| ss_send | 値コピー、待機receiverをREADYへ | 未開始slotへの事前送信可。TERMINATED宛はSS_ERR_STATE、満杯はSS_ERR_LIMIT |
| task entry return | TERMINATEDへ | 戻り値は破棄。join/cancel/slot再利用なし。現在のstackを解放しない |
| ss_work_enqueue | handler/argポインタをqueueへ保持 | argはcallback完了までproducerが保持。満杯ならSS_ERR_LIMITで所有権を移さない。NULL queue/handlerはSS_ERR_PARAM |
| ss_work_drain_budget | 開始時件数のうちbudget件を処理 | UI単一consumer専用。callbackは短時間でreturnし、再帰drainしない。新規投入分は次batchへ |

ss_work_enqueue は短い IRQ 保存復元区間を持ち、Supervisor の ISR からは永続 payload でのみ使える。allocator/slab、描画/window、blocking IPC/sleep/yield は ISR から呼ばない。scheduler の ISR 入口は caller-held IPL7、work callback は task context で mask 解除後に動く。

UI 以外の task は描画/window を直接呼ばず、永続 payload と短い callback を queue へ送る。callback 完了後に payload を再利用/解放できる。複数 producer の共有 payload は別途排他する。callback を解放済みの stack 変数へ向けない。`.x`は work/IPC をリンクせず、この transport は`.xdf`用。

work queue の high_water、rejected、max_delay_ticks は init からの集計。delay は Timer D tick 差であり、IRQ 停止中の wall-clock ではない。長期間/2^32tick を超える保留は対象外。件数 budget は handler 自身の無限実行を防げない。
