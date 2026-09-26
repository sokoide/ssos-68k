# SSOS 保守性・性能レビュー

調査日: 2026-09-24〜25 / 対象: `b51c2d1` の作業ツリー（調査開始時に変更なし）

## 判断と前提

最初に改善すべきなのは、タスクの利用範囲を広げると破綻する初期化・排他・IPCと、失敗を成功扱いする検証経路である。描画性能は、その後に通常UIの不要な再描画を減らして測定する。

対象はMC68000、elf2x68kによるクロス開発、Human68K上の `.x` とIPL起動の `.xdf`、両スケジューリング方式。個別機種・クロックでの今回の実測はない。ベアメタルのRAM範囲は現在のlinker scriptの固定設定を前提とし、搭載RAMの異なる機種への対応は別途必要になる。

スケジューラ本体と通常UIの共通化、sleepリスト、部分再合成、文字描画の高速経路、CPU描画のRAM置換テスト、allocatorの境界値検査は既にある。これらを新規導入する提案はしない。`.x` のベンチ・DOS入出力と `.xdf` の初期化を分ける現設計も維持する。

本書ではコードから確認できる事実、追加プローブによる再現、性能仮説を区別する。行番号は調査時点のもの。本文（R01〜R14と「今回の検証結果と限界」）は2026-09-24〜25の調査時点の記録であり、後続の実装状況は「実施計画（P1時点）」と末尾の「P2実装状況」に記録する。

2026-09-26の追加P2対応は末尾の「P2実装状況」に記録する。調査時点の根拠と提案は履歴として残し、現在のコードの説明として読み替えない。

## 優先順位

P1は機能不全・メモリ破壊・検証の誤判定を先に解消する項目。P2は保守性・応答性の改善、P3は測定して採否を決める項目。

| ID | 優先度 | 改善対象 | 確認状況 |
| --- | --- | --- | --- |
| R01 | P1 | `.xdf` のタスクスタック確保 | 割当て失敗を再現 |
| R02 | P1 | SR保存型の排他と起床処理の一元化 | ネスト非対応と競合する経路を確認 |
| R03 | P1 | main taskとIPCのID契約 | UBSanで範囲外アクセスを再現 |
| R04 | P1 | IPCの本当の待機状態 | 優先度による送信側の飢餓をコード上で確認 |
| R05 | P1 | QEMU検証の成否判定 | 起動失敗でも終了コード0を再現 |
| R06 | P1 | 異常終了時のハードウェア復元 | 通常cleanupを迂回する経路を確認 |
| R07 | P2 | ビルド成果物・依存関係の分離 | 設定切替・並列実行の競合要因を確認 |
| R08 | P2 | タスクAPIの起動・終了・戻り値契約 | 宣言と実装の不一致を確認 |
| R09 | P2 | allocator・描画の所有権 | 複数タスク利用時の未保護共有状態を確認 |
| R10 | P2 | watchdogと待機・起床の責務 | 復旧時のMFP設定差とbusy waitを確認 |
| R11 | P2 | 通常UIのドラッグ再描画 | ベンチと通常UIの差を確認、効果は要実測 |
| R12 | P3 | 文字列差分・クリップの早期判定 | 削減可能なループを確認、効果は要実測 |
| R13 | P2/P3 | DMA停止確認・設定検証・転送コスト | 停止確認不足はコード上、性能は要実測 |
| R14 | P2 | assemblyと仕様文書の重複 | 重複・古い記述を確認 |

## R01: `.xdf` のスタック用メモリをallocatorの上限に合わせる

**根拠:** [app/main.c](../ssos/os/app/main.c#L14) の `ss_init()` は `ss_alloc(SS_MAX_TASKS * SS_TASK_STACK)` を呼ぶ。定数は32タスク×16,384バイト＝524,288バイト。一方、[buddy.c](../ssos/os/mem/buddy.c#L90) は `2^16 - sizeof(SSBuddyBlock)` を超える要求を必ず拒否する。空きメモリ総量に関係なく `ss_task_stack_base == NULL` になる。

追加プローブでは1 MiB arenaを初期化して同じ要求を実行し、NULLと、その後の自動スタックによる `ss_task_create()` の `SS_ERR_STATE` を確認した。通常UIのmain taskは別TCBと起動時スタックを使うため、UIが動くことだけでは検出できない。`.x` は静的なスタックarenaなのでこの確保失敗の対象外。

**提案:** `.xdf` はlinker上で専用スタックpoolを予約し、その領域を除いてbuddyを初期化する。既存の「slotごとの固定スタック」設計を維持できる。通常の `.bss` に512 KiBを足すと現在の80 KiB RAMセクションに収まらないため、[linker.ld](../ssos/os/kernel/linker.ld) のSSOSRAM配置として設計する。初期化結果を検査して、失敗した状態で通常起動を続けない。

タスク単位の動的確保も選択肢だが、16 KiBにheaderを加えると現buddyでは32 KiBブロックになり、解放・所有権も追加になる。現構成では専用poolを優先する。

**検証:** allocatorと起動初期化を結合したテスト、全32slotの非重複・境界、枯渇、両 `.xdf` でworkerを実際に起動する確認。現在のテスト用静的arenaだけではこの問題を検出できない。

## R02: 割り込みマスクを保存・復元し、起床処理を排他的にする

**根拠:** 両方式の [interrupts.s](../ssos/os/kernel/preemptive/interrupts.s#L15) はdisableでSRを `0x2700`、enableで `0x2000` に固定する。外側が割り込み禁止でも、内側のIPC・work queue・window APIの終了時に割り込みを許可してしまう。

さらにpreemptiveではTimer D ISRと、[preemptive/wakeups.c](../ssos/os/kernel/preemptive/wakeups.c) 経由のsceneから、同じ `ss_do_wakeups()` を呼ぶ。後者は排他なしでsleep/readyリストを変更する。期限到達TCBを取得した直後にISRが同じTCBを取り出してenqueueすると、復帰したC側が同じTCBを再enqueueする経路が成立する。現在の通常UIにはsleepするworkerがないため表面化しにくい。

**提案:** `ss_irq_save()` / `ss_irq_restore(saved_sr)` を導入し、通常のcritical sectionを移行する。起床リスト操作は割り込み禁止を必須契約にし、task側入口とISR側入口を明示する。ISRから呼べる関数を一覧化する。Supervisor状態で使用するAPIとし、Human68KのUser/Supervisor切替とは分ける。SR・割り込みマスクの根拠は資料H1。

context switchとsleepは、次のタスクのSR復元まで含む特殊な区間である。単純に既存のenableをrestoreへ置換せず、yield時に保存するSRと、sleep復帰後のSRの契約を先に決める。

**性能面:** [scheduler.c](../ssos/os/kernel/scheduler.c#L116) は割り込み禁止中に標準16 KiBスタック全体のcanaryを書き込む。TCB予約、非公開状態でのcanary初期化、短い区間での公開に分ければ、タスク生成時の最大割り込み遅延を減らせる。TCBが初期化途中でstartされない状態管理が必要。

**検証:** SRの異なるIPL・二重ネスト・エラーreturn、ISRとtask側起床処理の交差、sleep/yield後のSRをQEMUで確認する。Nativeのinterrupt stubはno-opなので、既存Native PASSは排他の保証にならない。実機ではTimer D/V-DISPの最大遅延を測る。

## R03: main taskをIPCで扱うためのID契約を作る

**根拠:** [main_task.c](../ssos/os/kernel/main_task.c#L17) は意図的に `tcb_table` 外のTCBを登録する。[message.c](../ssos/os/ipc/message.c#L35) の `ss_recv()` と `ss_recv_nb()` は `curr - tcb_table` でIDを算出する。別オブジェクト間のポインタ減算はCの未定義動作であり、その結果をキュー添字に使っている。

実際に `ss_main_task_register(&main_tcb, 8)` の直後に `ss_recv_nb()` を呼ぶ追加プローブで、UBSanが `SSMsgQueue[32]` の範囲外アクセスを検出した。`.x` は現在IPCをリンクせず、通常 `.xdf` UIも受信しないので、現行UIのクラッシュとしては確認していない。

**提案:** TCBから論理task IDを取得するAPIを作る。main task用ID/キューを用意するか、IPC非対応として明確にエラーを返すかを決める。OSとしてmainも利用可能にするなら前者が自然。workerの既存1-based IDとslot数の扱いを維持できるようにする。ABI固定offsetを持つTCBにfieldを足す場合は末尾追加とassertで管理する。

**検証:** main/workerそれぞれへの送受信、未登録TCB、未作成IDを対象にNativeとsanitizerで確認する。既存IPCテストは常に `ss_curr_task = &tcb_table[i]` とするため、この問題を覆わない。

## R04: `ss_recv()` をready queueから外れる待機にする

**根拠:** [message.c](../ssos/os/ipc/message.c#L46) は空キューでyieldを繰り返すだけで、受信側はREADYのまま。[scheduler.c](../ssos/os/kernel/scheduler.c#L100) は常に最高優先度から選ぶ。

受信側がpri=0、唯一の送信側がpri=8なら、受信側がyieldしても再び受信側が選ばれ、送信側は実行できない。両方式で成立する。これはCPU浪費に加え、優先度の異なるタスク間通信が進まない問題である。

**提案:** IPC待ちをready queueから外し、sendで待機中receiverを起こす。空の確認→待機登録→切替と、send→起床の間で通知を取りこぼさないよう、R02の排他を使う。sleep待ちとIPC待ちの理由を区別し、idle taskまたは「全task待ち」の扱いも定義する。

**検証:** 高優先度receiver・低優先度sender、同優先度、待機登録直前のsend、満杯、FIFO。空キューから実際に切り替わるQEMUテストを追加する。既存のblocking recvテストは送信済みキューだけを使う。

## R05: QEMU起動成功とテスト成功を区別する

**根拠:** [coop/Makefile](../tests/qemu/coop/Makefile#L44)、[pre/Makefile](../tests/qemu/pre/Makefile#L56)、[asm/Makefile](../tests/asm/Makefile#L41) は `timeout ... || true` で終了コードを捨て、出力の成功判定も行わない。

`make test-qemu QEMU=false` を実行すると、全ケースが動作しないまま終了コード0になった。テストが無限ループで停止するためtimeoutを許容する意図は理解できるが、起動失敗・FAIL出力・成功前の停止まで成功扱いになる。

**提案:** ケースごとのstdout/stderrを保存し、期待する最終 `OK`、`FAIL` の不在、許可した終了状態を検査する。timeoutを成功とみなすのは必要な成功マーカーを確認できた場合だけにする。asmはサンプルごとの期待出力を定義する。ビルドループも途中の失敗を最終コマンドで隠さないようにする。

**検証:** 通常成功に加え、`QEMU=false`、存在しない実行ファイル、FAIL出力、OK未出力のtimeoutで非0終了すること。`make verify` がCIの判定として機能するための優先修正である。

## R06: 通常終了・異常終了を同じ復元状態管理につなぐ

**根拠:** [interrupts.s](../ssos/os/kernel/preemptive/interrupts.s#L584) の `ss_trap14_abort` はTRAP #14のみ戻して `_ABORTRST` へ進み、独自Timer D/V-DISPやCOPY/NMIを通常cleanupで復元しない。[standalone/main.c](../ssos/standalone/main.c#L825) の例外flag分岐も `ss_restore_interrupts()` より前に `_exit(1)` する。cooperativeにも同じabort経路がある。

**提案:** 初期化済みの段階と保存した状態をhost側で保持し、通常終了・途中失敗・異常終了から使用できる復元処理を設ける。壊れたtask stackや再入を想定する異常経路では、専用stackと最小限の処理を検討する。まず独自割り込みを停止・復元し、その後に表示・入力の復元と診断を行う。復元前にプロセス領域が解放されない順序を保証する。

`.x` はSupervisor移行後にハードウェアを所有し、復元を済ませてUserへ戻る。`.xdf` はHuman68Kへ復帰する同じ処理を流用しない。根拠はH1、H3。

**検証:** `.x` 両方式で意図的な例外、初期化途中の失敗、再入を試す。復帰後のTimer D/V-DISP/COPY/NMI/TRAP #14、MFP、画面・入力状態を確認する。過去監査の保留事項であり、今回もハードウェア上の再現は未実施。

## R07: ビルドを方式・実行形式・設定ごとに分離する

**根拠:** [os/Makefile](../ssos/os/Makefile#L40) は共有source直下の `.o` と固定名 `SSOS.X.elf/.bin` を使う。[standalone/Makefile](../ssos/standalone/Makefile#L41) は方式別objdirがあるが、`SS_PROFILE_GFX` の変更は依存として追跡しない。両方ともC headerの自動依存を生成しない。QEMU側もheader依存がない。

[ルートMakefile](../Makefile#L12) の両方式ターゲットは兄弟なので、`make -j` ではboot・OSの共通出力を同時に更新し得る。`tools/makedisk` との順序も個々のdiskターゲットの依存として表現されていない。また [ssos/Makefile](../ssos/Makefile#L22) の `MAKEDISK` は個人環境の絶対path。

**提案:** `build/<host>/<sched>/<profile>/` 単位でobject・ELF・binを分離し、`-MMD -MP` と `.d` 読込み、フラグ変更を反映する設定stampまたは構成別dirを使う。shared source一覧は小さな共通make includeへまとめる。diskには対応するboot/OS出力とmakediskの実依存を付け、出力先は変数にする。

**効果:** clean忘れによる古いコード混入を防ぎ、両方式の並列buildを安全にできる。実行時速度には直接影響しない。

**検証:** headerだけの変更、profile 0→1→0、方式切替、連続2回の `make -j`。再構築されるobjectと各成果物の対応を確認する。`.x` と `.xdf` のobj配置は既に異なるため、「LOCAL_MODE切替だから全体clean必須」という一律の説明も整理する。

## R08: タスクAPIの契約を実装と一致させる

**根拠:** [scheduler.h](../ssos/os/kernel/scheduler.h) のentry型は `void* (*)(void*)` だが、両assemblyの `.start_task` は直接jmpし、通常C関数の引数・戻り先を組み立てない。ctx_levelは保存するだけで、切替実装はその値に関係なく全汎用registerを保存する。custom stackはサイズ4バイトでも作成を受理する。APIの戻り値は `uint16_t` なのにエラー定数は負数で、今回の `SS_ERR_STATE` は65531として返る。

**提案:** 当面は「entryは引数なし・戻らない」という契約に型と説明を合わせるか、trampolineから `entry(arg)` を呼び、returnをtask終了へつなぐ。後者にはqueue除去とstackの遅延回収が必要。保存frameとentry使用分を満たす最低stackサイズを定める。ctx_levelは未実装であることを明示するか削除する。戻り値は `int16_t` などでIDと負エラーを表現するか、statusと出力IDを分離する。

**検証:** 即returnするtask、最小stack境界、無効なctx_level、負エラーの判定。TCBのoffset assertを維持し、QEMUで新規taskの起動から終了まで確認する。ABIの根拠・スタック整列はH1とskillのMC68000共通制約を参照する。

## R09: 共有サービスの所有権を明文化する

**根拠:** [buddy.c](../ssos/os/mem/buddy.c)、[slab.c](../ssos/os/mem/slab.c) のfree list更新は無保護。GFXはdraw page、DMA fill buffer、転送descriptor、Ch.2をglobalに持つ。windowは一部setterだけが割り込み禁止で、[scene.c](../ssos/os/app/scene.c) とwindowの両方にcontent/snapshotがある。

現在の単一UI taskと初期化中心のallocator利用では成立する設計だが、preemptiveの任意taskから使えるAPIとしては安全性を保証できない。DMAの待機中に別taskが同じbufferやCh.2を設定すると、転送中の内容・設定を破壊する。

**提案:** 描画・windowはUI taskが所有し、他taskはIPC/work queue経由で要求する方針を明文化する。allocatorを共有するならR02の短い排他を加える。buddyのfree-list探索全体をマスクすると遅延が増えるため、最大時間を測って必要な場合だけ構造変更する。長いDMA待ち全体を割り込み禁止にする解決は避ける。

**検証:** 2 taskの割当て・解放stress、canaryと総空き容量、所有者以外からの描画要求、要求キュー満杯時の挙動。UI一本化という既存の決定を維持し、性能目的だけで描画taskを増やさない。

## R10: watchdog・VSync待機・起床処理を整理する

**根拠:** [standalone/main.c](../ssos/standalone/main.c#L381) のwatchdogはspin回数で時間を近似し、復旧時にIMRA/IMRBを `0x21/0x10` へ変える。初期化の `0xFF/0x7F` と異なり、ベクタ・状態register更新も排他なし。復旧失敗時の上限もない。通常のVSync待機は `.x` / `.xdf` ともbusy wait。

cooperativeの起床はsceneが `ss_process_wakeups()` を呼ぶことに依存する。mainがsleepしてworkerだけがyieldを続ける構成では、worker自身が起床処理を呼ばなければmainが起きない。preemptiveのISR起床は10 tickごとで、task側からの起床処理も存在する。

**提案:** MFPの保存・設定・復元と「保存内容を上書きしない復旧」を共通の小さな境界へ集める。復旧は必要なsourceだけを、定義した排他下で操作する。Timer Dが生きているときはtickを時間基準にし、Timer Dも止まる場合の有限の最終脱出を別途用意する。根拠はH2。

cooperativeの起床はschedulerの安全なyield地点などで処理し、UI関数への依存を外す。次の段階で、VSync待ちはイベント待機＋idle taskへ移行する。単純なyield追加だけでは、最高優先度taskが再選択される問題は解消しない。

**応答性:** 現設定は4 MHz / 200 / 100＝200 Hz、tickは5 ms、ISRでの強制切替は10 tick＝50 ms。起床をISRだけに任せる場合は期限到達から追加0〜9 tickの遅れがあり、その後の実行時刻は優先度にも依存する。tick周期とtimesliceを別定数にして、起床判定だけ毎tick行う必要があるか測る。すべてのtaskの実行保証を「5 ms」と記述しない。

**検証:** UIなしのcooperative sleep/wakeup、V-DISPのみ停止・Timer Dのみ停止・両方停止、復旧後の入力、1/5/10 tick sleepとtick wraparound。busy wait削減の評価には別workerの進捗と起床遅延を使う。

## R11: 通常UIのドラッグ終了時は旧active windowのタイトルだけ再合成する

**根拠:** [scene.c](../ssos/os/app/scene.c#L361) の `drag_end()` は前active windowを幅・高さ全体で `ss_win_render_region()` する。active状態で変わる表示はタイトル。一方、[standaloneのbench_drag_region](../ssos/standalone/main.c#L642) は `redraw_title_region(previous)` を使う。既存ベンチの改善値を、そのまま通常UIの改善値とは扱えない。

**提案:** 前active領域の再合成をタイトル帯へ縮小する。通常fixtureは240×48で、タイトル高12なので、その一回の要求矩形面積は11,520→2,880 pixelsになる。これは当該要求面積の75%減であり、ドラッグ全体の速度向上率ではない。移動先との重複を除く処理は、単純な縮小の効果を測った後に検討する。

sceneの入力と1 frameの更新を分離し、通常のdrag_begin/move/endを決定的な入力列で駆動できるようにする。既存primitiveベンチは維持し、実UI操作の測定を追加する。

**検証:** 非重複・部分重複・全面重複、前activeが隠れるケース、ドラッグ中に本文値が変わるケース、255回以上の前面化。Nativeのframebuffer比較と両 `.x` / `.xdf` の表示確認を行い、`runtime` のGVRAM write・frame当たり描画量・VSync数を比較する。

## R12: 文字の差分末尾と、見えないglyphの処理を減らす

**根拠:** [scene.c](../ssos/os/app/scene.c#L130) の `draw_content_dirty()` は最初の差分から28文字固定行の末尾まで再描画する。末尾の同じ空白も処理する。[vram.c](../ssos/os/gfx/vram.c#L509) の `ss_gfx_draw_text_region()` はclipと無関係なglyphにも行・pixel単位の判定を行う。部分的に隠れた文字は、さらに上位windowごとの判定になる。

**提案:** pad済みの新旧行から最初と最後の差分を求め、描画する文字数を指定できるAPIへ渡す。短くなった数値を消すための空白差分も対象に含める。region描画ではglyph矩形がclip外ならskip、完全包含かつ画面内なら既存fast経路、端だけ既存clip経路へ振り分ける。

**効果と制約:** GVRAM writeと分岐の削減候補。増える判定・code sizeとの比較が必要で、現段階では速度改善を断定しない。5×8 fontに巨大なcacheや全面back bufferを追加するより先に試せる。

**検証:** 桁上がり・桁減り・1文字だけの変更・同じ行、glyphの左右上下clip、上位windowとの重なりについてpixel一致を確認する。`text-update` と通常UIの `glyph clip/fast`、GVRAM write、VSyncを測定する。

## R13: DMAは停止確認と構造検証を先に固める

**根拠:** [vram.c](../ssos/os/gfx/vram.c#L217) はtimeout時にSABを書き、ACT解除を有限回待つが、再timeoutでも通常のCPU fallbackへ進む。停止を確認できない状態で同じ行へCPU描画を開始できる。SABによる停止時にACTが解除される仕様は [Hitachi HD63450データシート](https://datasheet4u.com/pdf-down/H/D/6/HD63450-Hitachi.pdf) の「Operation Abort by Software」にも記載される。

**P2提案:** 「転送失敗・停止済み」と「停止未確認」を区別し、後者では対象領域へのCPU描画を開始しない。停止未確認からの復旧・描画中止方針を決める。descriptor寿命とCh.2所有権はR09と合わせる。現時点で停止失敗が実機で発生したという意味ではない。

[gfx.h](../ssos/os/gfx/gfx.h) のMMIO構造はpointer幅に依存する。target限定の `_Static_assert(offsetof(...))` と `sizeof(SSXfrInf) == 6` を追加する。Nativeは64bit pointerかつDMA無効なので、この配置検証を代替できない。alignment指定済みdescriptorを「未整列」として再指摘しない。

**P3提案:** 現実装は行ごとにBTC=1のchain設定→start→pollを行う。小矩形はCPU、広い矩形はDMAという現方針を維持し、幅・高さ別に閾値を測る。単色固定sourceを使う転送やchain overhead削減は原典と実機で比較してから採用する。

狭い矩形を単純に複数行chainへ伸ばしても、descriptorはMAR/MTCでありDARの行strideを自動設定するものではない。転送方向と両アドレスの進め方を設計せず、「DMAを一回にまとめれば速い」としない。GVRAMはVRAM領域、DMACはシステムI/Oとして区別し、直接操作は既存Supervisor所有期間内に限定する。根拠はH4。

**過去監査の訂正:** `OCR=0x19` はH4のOCR定義では、RAM→GVRAM方向、word、array chain、最大速度auto requestと整合する。`BAR/BTC` を使うこと自体を不具合とする根拠はない。旧監査の疑義を再掲する前に、資料間の矛盾を除く必要がある。

**検証:** targetのMMIO offset/descriptor size、DMA正常・error・timeout・停止未確認の経路、行の間隔・境界、CPU fallbackとの画像一致。通常ベンチでDMA success/error/timeoutとVSyncを併記する。非同期化は所有権と完了通知が整うまで後回しにする。

## R14: assemblyの共通部分と文書の正本を整理する

**根拠:** 両 `interrupts.s` はMFP保存復元、TRAP #14、resume処理などが大きく重複し、QEMUにもcontext switchの移植版がある。TCB offsetはC側にassertがある一方、assemblyには数値が繰り返される。

**提案:** まずMFP保存復元と例外処理を共通assemblyへ抽出し、方式別ISRは小さく残す。TCB offsetとframe layoutを共有includeまたはtarget compilerからの生成物で管理する。QEMU固有のTTY・trap導入部分を境界に置き、本番との差分を一覧にする。異なる実行環境を無理に一つの巨大な条件分岐へまとめない。

文書の次の不一致も解消する。

- READMEの「cooperativeは全taskでmainのstack共有」は、作成taskに独立stackを持つ現在の実装と異なる。
- 「1 ms timer」「5 msごとの強制切替」「50 ms切替」が混在する。R10の定数とテストを正本にする。
- `ssos/README_ja.md` に旧data threadの記述が残る。通常UIは現在単一task。
- XOR描画の「GVRAM readなし」は実装と異なる。`^=` はread-modify-writeであり、「保存用buffer・以前の広い範囲のreadbackが不要」と説明する。
- 過去のベンチ数値にはcommit、機種/エミュレータ、clock、mode、profile設定、通常UIかprimitiveベンチかを付記する。

**検証:** 抽出前後の逆アセンブル差分、両方式QEMU、両 `.x` / `.xdf` buildとハードウェア確認。既存 `_Static_assert` は維持する。

## 実施計画（P1時点の記録）

2026-09-26のP1作業計画。R05 → R01/R03 → R02/R04の順で第1〜3段階として実施した。当時R07は本計画から外していたが、その後のP2で着手した。

| 段階 | 項目 | 状態（2026-09-26） |
| --- | --- | --- |
| 1 | R05 | 実装・検証済み |
| 2 | R01・R03 | 実装済み、結合検証とハードウェア確認が残り |
| 3 | R02・R04 | コア実装済み、R02の起床排他と検証が残り |

第1〜3段階の実装は2026-09-25〜26に作業ツリーへ反映した。各段階の内容と残作業:

- **R05**: [run_test.sh](../tests/qemu/common/run_test.sh) がQEMUの終了コードと`OK`/`FAIL`出力を判定し、三つのテストMakefileのループを`set -e`化した。存在しない実行ファイル、`FAIL`出力、`OK`なしtimeoutで非0終了を確認済み。
- **R01**: [linker.ld](../ssos/os/kernel/linker.ld) が`.ssos`先頭に32×16 KiBの専用スタックpoolを予約し、buddyはその残りで初期化する。[app/main.c](../ssos/os/app/main.c) の`ss_init()`はpool先頭を使い、`ss_alloc`の上限と無関係になった。残り: 全32slotの非重複・境界・枯渇の結合テスト、両`.xdf`でのworker起動確認。
- **R03**: [message.c](../ssos/os/ipc/message.c) の`current_queue_index()`が`tcb_table`内の一致でキューを決め、main taskはIPC未対応として`SS_ERR_STATE`を返す。Nativeの`recv_rejects_task_outside_table`で検証済み。残り: main taskをIPC対応にするかどうかの要件判断。
- **R02**: `ss_irq_save()`/`ss_irq_restore(saved_sr)`を両assemblyに導入し、scheduler・work queue・window・IPC・standaloneの排他区間を移行した。yieldは現在のSRを保存し、`ss_task_sleep()`は復帰後にSRを復元する。残り: `ss_do_wakeups()`のtask入口・ISR入口の排他契約と一元化、ISRから呼べる関数の一覧化、`ss_task_create()`内canary書き込みによる割り込み遅延の短縮、IPL・二重ネストを含むSR契約の検証。
- **R04**: `ss_recv()`は空キューで`ipc_waiting`を立ててready queueから外れ、`ss_send()`が待機中の受信者を起こす。実行可能な他taskがない場合はデッドロック防止として`SS_ERR_STATE`を返す（idle taskは未導入）。coop/t05・pre/t06で高優先度receiver・低優先度senderとネストしたSR maskの復元をQEMU検証した。残り: 待機登録とsendの交差、同優先度の組合せ。

検証結果（2026-09-26、コミット前の作業ツリー）:

- `make test` / `make test SANITIZE=1`: 両方式 各107件PASS
- `make test-qemu`: coop 5件・pre 6件、全ケース自動判定でOK
- `make test-asm`: 5件PASS。`make test-qemu QEMU=false` と `make test-asm QEMU=false` は非0で失敗。
- `. ~/.elf2x68k` 後に方式を切り替える前にcleanし、両方式の `.x` / `.xdf` をビルド。成果物は `~/tmp/ssos_{cop,pre}.{x,xdf}`。ELFの `.ssos` はRAM上のNOBITS 0xAB0000 byteで、先頭0x80000 byteをスタックに確保。
- `make verify-check` は両方式・両形式のX68000エミュレータ/実機確認を要求。QEMU virtはMFPや実際の起動経路を検証しない。

後続は次の順で行う:

1. R02の残作業を完了させ、R08・R09（タスクAPI契約・共有サービス所有権）を進める。
2. R06・R10・R13の復旧経路をX68000エミュレータ/実機で確認する。
3. R11、R12、DMA閾値の順で一件ずつ測定し、画面同一性と実測改善が揃ったものを採用する。
4. R14は上記変更の単位で進め、仕様・実装・検証を同時に更新する。

32task・32window上限の現段階で、ready queueを複雑な木構造に変える、未使用page flipを有効化する、全面double bufferを標準化するといった変更は優先しない。単一UIの測定だけからscheduler全体の性能を評価しない。

## 今回の検証結果と限界

| 確認 | 結果 |
| --- | --- |
| `make test` | cooperative/preemptive 各105件PASS |
| `make test SANITIZE=1` | 両方式 各105件PASS |
| `make -B test-qemu` | 再build後、coop 4件・pre 5件の最終OKを個別に確認 |
| `make test-qemu QEMU=false` | 全ケース未実行でも終了コード0。R05を再現 |
| 本番buddy/schedulerをリンクした追加プローブ | 512 KiB stack pool要求がNULL、task作成がSS_ERR_STATE |
| 本番main_task/IPCをリンクした追加プローブ | main taskのrecv_nbでUBSanが配列範囲外を検出 |

追加プローブは一時ファイルで実施し、本体・既存テストには変更を加えていない。QEMUのOKは本番MFPを動かした結果ではなく、trapで駆動する移植版の確認である。`.x` / `.xdf` 全build、asmサンプル、X68000実機・専用エミュレータでの今回の確認、描画性能の新規測定は未実施。文書のみの変更なので、性能値は既存のものを新規測定値として扱っていない。

追加プローブの再現要点:

```c
/* R01: 有効で十分大きいarenaでss_mem_init済みでもNULLになる。 */
ss_task_stack_base = ss_alloc(SS_MAX_TASKS * SS_TASK_STACK);

/* R03: main_tcbはtcb_table外。schedulerとIPCの初期化後に実行。 */
static SSTask main_tcb;
SSMessage msg;
ss_main_task_register(&main_tcb, 8);
ss_recv_nb(&msg); /* 現状、ASan/UBSan付きホスト実行で範囲外検出 */
```

## ハードウェア資料と扱い

使用したskill: `x68k-master`。SKILL.mdでは `references/` と記載されるが、この環境の実体は `/Users/scott/.agents/skills/x68k-master/resources/`。以下はその配下の資料名。

| 記号 | ファイル・参照箇所 | 本書での用途 |
| --- | --- | --- |
| H1 | `03_割り込み.md` §2、`32_IOMAP.md` 例外ベクタ | SR/IPL、例外・ベクタ、復元の設計 |
| H2 | `04_MFP.md` §5–6、`32_IOMAP.md` MFP | Timer A/D、4 MHz、prescaler、MFP設定 |
| H3 | `21_システム初期化と終了.md` 初期化・終了復元 | Supervisor移行と復帰、COPY/NMI、画面・入力の復元 |
| H4 | `32_IOMAP.md` DMAC、`02_DMA.md` | MMIO配置、OCR/SCR/CCR、MAR/MTC descriptor |

`02_DMA.md` にはCSR/CCRのoffsetやbit、OCRのCHAIN位置について `32_IOMAP.md` と食い違う記述がある。本書の具体的DMAC値は詳細表の `32_IOMAP.md` を採用し、矛盾する概要表をコード変更の根拠にしない。新しい転送方式・停止失敗時の復旧条件など、資料で保証できない細部は**要原典確認**。VaultのDMAノートにも16bit MTCに表現できない全画面転送長の例があるため、そのまま転記しない。skill/Vault自体の変更は今回の対象外。

## P2実装状況（2026-09-26）

P1は `80a5318` でコミットした。以下はその後に着手したP2の状態であり、上の調査時点の記述を置き換える。

- **R06 異常終了復元**: `.x` 専用の `standalone/trap14.s` に例外処理を集約した。abort時は緊急スタック上で `ss_abort_cleanup()` を呼び、Timer D/V-DISPを含むベクタとMFP、COPY/NMI、TRAP #14、表示・入力状態を段階フラグに応じて復元する。通常終了も同じcleanupを通る。`.xdf`にはHuman68Kのabort処理をリンクしない。実機・Human68Kエミュレータでの故障注入と、初期化途中・再入の確認は未実施。
- **R08 タスク契約**: `entry(arg)` を両方式の起動assemblyから呼び、戻り値は破棄し、return時は `ss_task_exit()` が `TERMINATED` に移す。ID・stackは再利用しない。`ctx_level`の不正値とcustom stackの最小サイズ・整列を拒否する。NativeとQEMUの引数・returnテストを追加した。
- **R09 所有者**: [runtime-ownership.md](runtime-ownership.md) にscheduler、IPC/work、allocator、描画、DMAC Ch.2の所有者・排他契約を記載し、公開headerにも主要制約を追記した。DMAC abort後の停止未確認問題は解決しておらず、実機確認が必要。
- **R10 起床責務**: cooperativeの起床をsceneからschedulerの切替点へ移し、IRQ mask下で処理する。main taskがsleep中にworkerだけがyieldするQEMUテストを追加した。watchdogのMFP再設定とVSync busy waitは未変更。
- **R07 ビルド分離**: `os/`、`standalone/`、`boot/`の中間物を方式・形式・profile別に分離した。`.xdf`生成場所も分離し、profile 1は公開ファイル名に `_profile1` を付ける。Cのheader依存は `.d` で追跡する。両方式のprofile 0/1とprofile切替後の並列buildは成功。header変更後に対象objectが再構築されることも確認したが、二度連続の完全no-op判定は残る。
- **R14 重複・仕様**: TRAP #14をstandalone専用に、MFP状態保存復元を `os/kernel/mfp_state.s` に抽出した。READMEのtick/switch周期、stack、wakeups、ビルド手順を更新した。方式別context switchとQEMU portの差分、TCB offset共通化は残る。

自動検証: Native各109件、QEMU cooperative 7件・preemptive 7件、asm 5件がPASS。両方式の `.x` / `.xdf` はクロスビルド・リンク済み。これはMFP実割り込み、Human68K abort、実画面・DMAを検証したことを意味しない。
