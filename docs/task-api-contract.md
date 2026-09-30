# Task・UI要求APIの契約

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

ss_work_enqueueは短いIRQ保存復元区間を持ち、SupervisorのISRからは永続payloadでのみ使える。allocator/slab、描画/window、blocking IPC/sleep/yieldはISRから呼ばない。schedulerのISR入口はcaller-held IPL7、work callbackはtask contextでmask解除後に動く。

UI以外のtaskは描画/windowを直接呼ばず、永続payloadと短いcallbackをqueueへ送る。callback完了後にpayloadを再利用/解放できる。複数producerの共有payloadは別途排他する。callbackを解放済みのstack変数へ向けない。`.x`はwork/IPCをリンクせず、このtransportは`.xdf`用。

work queueのhigh_water、rejected、max_delay_ticksはinitからの集計。delayはTimer D tick差であり、IRQ停止中のwall-clockではない。長期間/2^32tickを超える保留は対象外。件数budgetはhandler自身の無限実行を防げない。
