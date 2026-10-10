# SSOS 保守性・性能レビュー

調査日: 2026-09-24〜25 / 対象: `b51c2d1` の作業ツリー（調査開始時に変更なし）

2026-09-30 に現行コードを再照合した。今後の残作業・追加確認・完了条件は[追加改善計画](maintenance-performance-plan-2026-09-30.md)を入口とする。本書は当初の根拠と実施履歴として保持する。

## 判断と前提

最初に改善すべきなのは、タスクの利用範囲を広げると破綻する初期化・排他・IPC と、失敗を成功扱いする検証経路である。描画性能は、その後に通常 UI の不要な再描画を減らして測定する。

対象は MC68000、elf2x68k によるクロス開発、Human68K 上の `.x` と IPL 起動の `.xdf`、両スケジューリング方式。個別機種・クロックでの今回の実測はない。ベアメタルの RAM 範囲は現在の linker script の固定設定を前提とし、搭載 RAM の異なる機種への対応は別途必要になる。

スケジューラ本体と通常 UI の共通化、sleep リスト、部分再合成、文字描画の高速経路、CPU 描画の RAM 置換テスト、allocator の境界値検査は既にある。これらを新規導入する提案はしない。`.x` のベンチ・DOS 入出力と `.xdf` の初期化を分ける現設計も維持する。

本書ではコードから確認できる事実、追加プローブによる再現、性能仮説を区別する。行番号は調査時点のもの。本文（R01〜R14 と「今回の検証結果と限界」）は 2026-09-24〜25 の調査時点の記録であり、後続の実装状況は「実施計画（P1 時点）」と末尾の「P2 実装状況」に記録する。

2026-09-26 の追加 P2 対応は末尾の「P2 実装状況」に記録する。調査時点の根拠と提案は履歴として残し、現在のコードの説明として読み替えない。

## 優先順位

P1 は機能不全・メモリ破壊・検証の誤判定を先に解消する項目。P2 は保守性・応答性の改善、P3 は測定して採否を決める項目。

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

**根拠:** [app/main.c](../ssos/os/app/main.c#L14) の `ss_init()` は `ss_alloc(SS_MAX_TASKS * SS_TASK_STACK)` を呼ぶ。定数は 32 タスク×16,384 バイト＝524,288 バイト。一方、[buddy.c](../ssos/os/mem/buddy.c#L90) は `2^16 - sizeof(SSBuddyBlock)` を超える要求を必ず拒否する。空きメモリ総量に関係なく `ss_task_stack_base == NULL` になる。

追加プローブでは 1 MiB arena を初期化して同じ要求を実行し、NULL と、その後の自動スタックによる `ss_task_create()` の `SS_ERR_STATE` を確認した。通常 UI の main task は別 TCB と起動時スタックを使うため、UI が動くことだけでは検出できない。`.x` は静的なスタック arena なのでこの確保失敗の対象外。

**提案:** `.xdf` は linker 上で専用スタック pool を予約し、その領域を除いて buddy を初期化する。既存の「slot ごとの固定スタック」設計を維持できる。通常の `.bss` に 512 KiB を足すと現在の 80 KiB RAM セクションに収まらないため、[linker.ld](../ssos/os/kernel/linker.ld) の SSOSRAM 配置として設計する。初期化結果を検査して、失敗した状態で通常起動を続けない。

タスク単位の動的確保も選択肢だが、16 KiB に header を加えると現 buddy では 32 KiB ブロックになり、解放・所有権も追加になる。現構成では専用 pool を優先する。

**検証:** allocator と起動初期化を結合したテスト、全 32slot の非重複・境界、枯渇、両 `.xdf` で worker を実際に起動する確認。現在のテスト用静的 arena だけではこの問題を検出できない。

## R02: 割り込みマスクを保存・復元し、起床処理を排他的にする

**根拠:** 両方式の [interrupts.s](../ssos/os/kernel/preemptive/interrupts.s#L15) は disable で SR を `0x2700`、enable で `0x2000` に固定する。外側が割り込み禁止でも、内側の IPC・work queue・window API の終了時に割り込みを許可してしまう。

さらに preemptive では Timer D ISR と、[preemptive/wakeups.c](../ssos/os/kernel/preemptive/wakeups.c) 経由の scene から、同じ `ss_do_wakeups()` を呼ぶ。後者は排他なしで sleep/ready リストを変更する。期限到達 TCB を取得した直後に ISR が同じ TCB を取り出して enqueue すると、復帰した C 側が同じ TCB を再 enqueue する経路が成立する。現在の通常 UI には sleep する worker がないため表面化しにくい。

**提案:** `ss_irq_save()` / `ss_irq_restore(saved_sr)` を導入し、通常の critical section を移行する。起床リスト操作は割り込み禁止を必須契約にし、task 側入口と ISR 側入口を明示する。ISR から呼べる関数を一覧化する。Supervisor 状態で使用する API とし、Human68K の User/Supervisor 切替とは分ける。SR・割り込みマスクの根拠は資料 H1。

context switch と sleep は、次のタスクの SR 復元まで含む特殊な区間である。単純に既存の enable を restore へ置換せず、yield 時に保存する SR と、sleep 復帰後の SR の契約を先に決める。

**性能面:** [scheduler.c](../ssos/os/kernel/scheduler.c#L116) は割り込み禁止中に標準 16 KiB スタック全体の canary を書き込む。TCB 予約、非公開状態での canary 初期化、短い区間での公開に分ければ、タスク生成時の最大割り込み遅延を減らせる。TCB が初期化途中で start されない状態管理が必要。

**検証:** SR の異なる IPL・二重ネスト・エラーreturn、ISR と task 側起床処理の交差、sleep/yield 後の SR を QEMU で確認する。Native の interrupt stub は no-op なので、既存 Native PASS は排他の保証にならない。実機では Timer D/V-DISP の最大遅延を測る。

## R03: main taskをIPCで扱うためのID契約を作る

**根拠:** [main_task.c](../ssos/os/kernel/main_task.c#L17) は意図的に `tcb_table` 外の TCB を登録する。[message.c](../ssos/os/ipc/message.c#L35) の `ss_recv()` と `ss_recv_nb()` は `curr - tcb_table` で ID を算出する。別オブジェクト間のポインタ減算は C の未定義動作であり、その結果をキュー添字に使っている。

実際に `ss_main_task_register(&main_tcb, 8)` の直後に `ss_recv_nb()` を呼ぶ追加プローブで、UBSan が `SSMsgQueue[32]` の範囲外アクセスを検出した。`.x` は現在 IPC をリンクせず、通常 `.xdf` UI も受信しないので、現行 UI のクラッシュとしては確認していない。

**提案:** TCB から論理 task ID を取得する API を作る。main task 用 ID/キューを用意するか、IPC 非対応として明確にエラーを返すかを決める。OS として main も利用可能にするなら前者が自然。worker の既存 1-based ID と slot 数の扱いを維持できるようにする。ABI 固定 offset を持つ TCB に field を足す場合は末尾追加と assert で管理する。

**検証:** main/worker それぞれへの送受信、未登録 TCB、未作成 ID を対象に Native と sanitizer で確認する。既存 IPC テストは常に `ss_curr_task = &tcb_table[i]` とするため、この問題を覆わない。

## R04: `ss_recv()` をready queueから外れる待機にする

**根拠:** [message.c](../ssos/os/ipc/message.c#L46) は空キューで yield を繰り返すだけで、受信側は READY のまま。[scheduler.c](../ssos/os/kernel/scheduler.c#L100) は常に最高優先度から選ぶ。

受信側が pri=0、唯一の送信側が pri=8 なら、受信側が yield しても再び受信側が選ばれ、送信側は実行できない。両方式で成立する。これは CPU 浪費に加え、優先度の異なるタスク間通信が進まない問題である。

**提案:** IPC 待ちを ready queue から外し、send で待機中 receiver を起こす。空の確認→待機登録→切替と、send→起床の間で通知を取りこぼさないよう、R02 の排他を使う。sleep 待ちと IPC 待ちの理由を区別し、idle task または「全 task 待ち」の扱いも定義する。

**検証:** 高優先度 receiver・低優先度 sender、同優先度、待機登録直前の send、満杯、FIFO。空キューから実際に切り替わる QEMU テストを追加する。既存の blocking recv テストは送信済みキューだけを使う。

## R05: QEMU起動成功とテスト成功を区別する

**根拠:** [coop/Makefile](../tests/qemu/coop/Makefile#L44)、[pre/Makefile](../tests/qemu/pre/Makefile#L56)、[asm/Makefile](../tests/asm/Makefile#L41) は `timeout ... || true` で終了コードを捨て、出力の成功判定も行わない。

`make test-qemu QEMU=false` を実行すると、全ケースが動作しないまま終了コード 0 になった。テストが無限ループで停止するため timeout を許容する意図は理解できるが、起動失敗・FAIL 出力・成功前の停止まで成功扱いになる。

**提案:** ケースごとの stdout/stderr を保存し、期待する最終 `OK`、`FAIL` の不在、許可した終了状態を検査する。timeout を成功とみなすのは必要な成功マーカーを確認できた場合だけにする。asm はサンプルごとの期待出力を定義する。ビルドループも途中の失敗を最終コマンドで隠さないようにする。

**検証:** 通常成功に加え、`QEMU=false`、存在しない実行ファイル、FAIL 出力、OK 未出力の timeout で非 0 終了すること。`make verify` が CI の判定として機能するための優先修正である。

## R06: 通常終了・異常終了を同じ復元状態管理につなぐ

**根拠:** [interrupts.s](../ssos/os/kernel/preemptive/interrupts.s#L584) の `ss_trap14_abort` は TRAP #14 のみ戻して `_ABORTRST` へ進み、独自 Timer D/V-DISP や COPY/NMI を通常 cleanup で復元しない。[standalone/main.c](../ssos/standalone/main.c#L825) の例外 flag 分岐も `ss_restore_interrupts()` より前に `_exit(1)` する。cooperative にも同じ abort 経路がある。

**提案:** 初期化済みの段階と保存した状態を host 側で保持し、通常終了・途中失敗・異常終了から使用できる復元処理を設ける。壊れた task stack や再入を想定する異常経路では、専用 stack と最小限の処理を検討する。まず独自割り込みを停止・復元し、その後に表示・入力の復元と診断を行う。復元前にプロセス領域が解放されない順序を保証する。

`.x` は Supervisor 移行後にハードウェアを所有し、復元を済ませて User へ戻る。`.xdf` は Human68K へ復帰する同じ処理を流用しない。根拠は H1、H3。

**検証:** `.x` 両方式で意図的な例外、初期化途中の失敗、再入を試す。復帰後の Timer D/V-DISP/COPY/NMI/TRAP #14、MFP、画面・入力状態を確認する。過去監査の保留事項であり、今回もハードウェア上の再現は未実施。

## R07: ビルドを方式・実行形式・設定ごとに分離する

**根拠:** [os/Makefile](../ssos/os/Makefile#L40) は共有 source 直下の `.o` と固定名 `SSOS.X.elf/.bin` を使う。[standalone/Makefile](../ssos/standalone/Makefile#L41) は方式別 objdir があるが、`SS_PROFILE_GFX` の変更は依存として追跡しない。両方とも C header の自動依存を生成しない。QEMU 側も header 依存がない。

[ルートMakefile](../Makefile#L12) の両方式ターゲットは兄弟なので、`make -j` では boot・OS の共通出力を同時に更新し得る。`tools/makedisk` との順序も個々の disk ターゲットの依存として表現されていない。また [ssos/Makefile](../ssos/Makefile#L22) の `MAKEDISK` は個人環境の絶対 path。

**提案:** `build/<host>/<sched>/<profile>/` 単位で object・ELF・bin を分離し、`-MMD -MP` と `.d` 読込み、フラグ変更を反映する設定 stamp または構成別 dir を使う。shared source 一覧は小さな共通 make include へまとめる。disk には対応する boot/OS 出力と makedisk の実依存を付け、出力先は変数にする。

**効果:** clean 忘れによる古いコード混入を防ぎ、両方式の並列 build を安全にできる。実行時速度には直接影響しない。

**検証:** header だけの変更、profile 0→1→0、方式切替、連続 2 回の `make -j`。再構築される object と各成果物の対応を確認する。`.x` と `.xdf` の obj 配置は既に異なるため、「LOCAL_MODE 切替だから全体 clean 必須」という一律の説明も整理する。

## R08: タスクAPIの契約を実装と一致させる

**根拠:** [scheduler.h](../ssos/os/kernel/scheduler.h) の entry 型は `void* (*)(void*)` だが、両 assembly の `.start_task` は直接 jmp し、通常 C 関数の引数・戻り先を組み立てない。ctx_level は保存するだけで、切替実装はその値に関係なく全汎用 register を保存する。custom stack はサイズ 4 バイトでも作成を受理する。API の戻り値は `uint16_t` なのにエラー定数は負数で、今回の `SS_ERR_STATE` は 65531 として返る。

**提案:** 当面は「entry は引数なし・戻らない」という契約に型と説明を合わせるか、trampoline から `entry(arg)` を呼び、return を task 終了へつなぐ。後者には queue 除去と stack の遅延回収が必要。保存 frame と entry 使用分を満たす最低 stack サイズを定める。ctx_level は未実装であることを明示するか削除する。戻り値は `int16_t` などで ID と負エラーを表現するか、status と出力 ID を分離する。

**検証:** 即 return する task、最小 stack 境界、無効な ctx_level、負エラーの判定。TCB の offset assert を維持し、QEMU で新規 task の起動から終了まで確認する。ABI の根拠・スタック整列は H1 と skill の MC68000 共通制約を参照する。

## R09: 共有サービスの所有権を明文化する

**根拠:** [buddy.c](../ssos/os/mem/buddy.c)、[slab.c](../ssos/os/mem/slab.c) の free list 更新は無保護。GFX は draw page、DMA fill buffer、転送 descriptor、Ch.2 を global に持つ。window は一部 setter だけが割り込み禁止で、[scene.c](../ssos/os/app/scene.c) と window の両方に content/snapshot がある。

現在の単一 UI task と初期化中心の allocator 利用では成立する設計だが、preemptive の任意 task から使える API としては安全性を保証できない。DMA の待機中に別 task が同じ buffer や Ch.2 を設定すると、転送中の内容・設定を破壊する。

**提案:** 描画・window は UI task が所有し、他 task は IPC/work queue 経由で要求する方針を明文化する。allocator を共有するなら R02 の短い排他を加える。buddy の free-list 探索全体をマスクすると遅延が増えるため、最大時間を測って必要な場合だけ構造変更する。長い DMA 待ち全体を割り込み禁止にする解決は避ける。

**検証:** 2 task の割当て・解放 stress、canary と総空き容量、所有者以外からの描画要求、要求キュー満杯時の挙動。UI 一本化という既存の決定を維持し、性能目的だけで描画 task を増やさない。

## R10: watchdog・VSync待機・起床処理を整理する

**根拠:** [standalone/main.c](../ssos/standalone/main.c#L381) の watchdog は spin 回数で時間を近似し、復旧時に IMRA/IMRB を `0x21/0x10` へ変える。初期化の `0xFF/0x7F` と異なり、ベクタ・状態 register 更新も排他なし。復旧失敗時の上限もない。通常の VSync 待機は `.x` / `.xdf` とも busy wait。

cooperative の起床は scene が `ss_process_wakeups()` を呼ぶことに依存する。main が sleep して worker だけが yield を続ける構成では、worker 自身が起床処理を呼ばなければ main が起きない。preemptive の ISR 起床は 10 tick ごとで、task 側からの起床処理も存在する。

**提案:** MFP の保存・設定・復元と「保存内容を上書きしない復旧」を共通の小さな境界へ集める。復旧は必要な source だけを、定義した排他下で操作する。Timer D が生きているときは tick を時間基準にし、Timer D も止まる場合の有限の最終脱出を別途用意する。根拠は H2。

cooperative の起床は scheduler の安全な yield 地点などで処理し、UI 関数への依存を外す。次の段階で、VSync 待ちはイベント待機＋idle task へ移行する。単純な yield 追加だけでは、最高優先度 task が再選択される問題は解消しない。

**応答性:** 現設定は 4 MHz / 200 / 100＝200 Hz、tick は 5 ms、ISR での強制切替は 10 tick＝50 ms。起床を ISR だけに任せる場合は期限到達から追加 0〜9 tick の遅れがあり、その後の実行時刻は優先度にも依存する。tick 周期と timeslice を別定数にして、起床判定だけ毎 tick 行う必要があるか測る。すべての task の実行保証を「5 ms」と記述しない。

**検証:** UI なしの cooperative sleep/wakeup、V-DISP のみ停止・Timer D のみ停止・両方停止、復旧後の入力、1/5/10 tick sleep と tick wraparound。busy wait 削減の評価には別 worker の進捗と起床遅延を使う。

## R11: 通常UIのドラッグ終了時は旧active windowのタイトルだけ再合成する

**根拠:** [scene.c](../ssos/os/app/scene.c#L361) の `drag_end()` は前 active window を幅・高さ全体で `ss_win_render_region()` する。active 状態で変わる表示はタイトル。一方、[standaloneのbench_drag_region](../ssos/standalone/main.c#L642) は `redraw_title_region(previous)` を使う。既存ベンチの改善値を、そのまま通常 UI の改善値とは扱えない。

**提案:** 前 active 領域の再合成をタイトル帯へ縮小する。通常 fixture は 240×48 で、タイトル高 12 なので、その一回の要求矩形面積は 11,520→2,880 pixels になる。これは当該要求面積の 75%減であり、ドラッグ全体の速度向上率ではない。移動先との重複を除く処理は、単純な縮小の効果を測った後に検討する。

scene の入力と 1 frame の更新を分離し、通常の drag_begin/move/end を決定的な入力列で駆動できるようにする。既存 primitive ベンチは維持し、実 UI 操作の測定を追加する。

**検証:** 非重複・部分重複・全面重複、前 active が隠れるケース、ドラッグ中に本文値が変わるケース、255 回以上の前面化。Native の framebuffer 比較と両 `.x` / `.xdf` の表示確認を行い、`runtime` の GVRAM write・frame 当たり描画量・VSync 数を比較する。

## R12: 文字の差分末尾と、見えないglyphの処理を減らす

**根拠:** [scene.c](../ssos/os/app/scene.c#L130) の `draw_content_dirty()` は最初の差分から 28 文字固定行の末尾まで再描画する。末尾の同じ空白も処理する。[vram.c](../ssos/os/gfx/vram.c#L509) の `ss_gfx_draw_text_region()` は clip と無関係な glyph にも行・pixel 単位の判定を行う。部分的に隠れた文字は、さらに上位 window ごとの判定になる。

**提案:** pad 済みの新旧行から最初と最後の差分を求め、描画する文字数を指定できる API へ渡す。短くなった数値を消すための空白差分も対象に含める。region 描画では glyph 矩形が clip 外なら skip、完全包含かつ画面内なら既存 fast 経路、端だけ既存 clip 経路へ振り分ける。

**効果と制約:** GVRAM write と分岐の削減候補。増える判定・code size との比較が必要で、現段階では速度改善を断定しない。5×8 font に巨大な cache や全面 back buffer を追加するより先に試せる。

**検証:** 桁上がり・桁減り・1 文字だけの変更・同じ行、glyph の左右上下 clip、上位 window との重なりについて pixel 一致を確認する。`text-update` と通常 UI の `glyph clip/fast`、GVRAM write、VSync を測定する。

## R13: DMAは停止確認と構造検証を先に固める

**根拠:** [vram.c](../ssos/os/gfx/vram.c#L217) は timeout 時に SAB を書き、ACT 解除を有限回待つが、再 timeout でも通常の CPU fallback へ進む。停止を確認できない状態で同じ行へ CPU 描画を開始できる。SAB による停止時に ACT が解除される仕様は [Hitachi HD63450データシート](https://datasheet4u.com/pdf-down/H/D/6/HD63450-Hitachi.pdf) の「Operation Abort by Software」にも記載される。

**P2提案:** 「転送失敗・停止済み」と「停止未確認」を区別し、後者では対象領域への CPU 描画を開始しない。停止未確認からの復旧・描画中止方針を決める。descriptor 寿命と Ch.2 所有権は R09 と合わせる。現時点で停止失敗が実機で発生したという意味ではない。

[gfx.h](../ssos/os/gfx/gfx.h) の MMIO 構造は pointer 幅に依存する。target 限定の `_Static_assert(offsetof(...))` と `sizeof(SSXfrInf) == 6` を追加する。Native は 64bit pointer かつ DMA 無効なので、この配置検証を代替できない。alignment 指定済み descriptor を「未整列」として再指摘しない。

**P3提案:** 現実装は行ごとに BTC=1 の chain 設定→start→poll を行う。小矩形は CPU、広い矩形は DMA という現方針を維持し、幅・高さ別に閾値を測る。単色固定 source を使う転送や chain overhead 削減は原典と実機で比較してから採用する。

狭い矩形を単純に複数行 chain へ伸ばしても、descriptor は MAR/MTC であり DAR の行 stride を自動設定するものではない。転送方向と両アドレスの進め方を設計せず、「DMA を一回にまとめれば速い」としない。GVRAM は VRAM 領域、DMAC はシステム I/O として区別し、直接操作は既存 Supervisor 所有期間内に限定する。根拠は H4。

**過去監査の訂正:** `OCR=0x19` は H4 の OCR 定義では、RAM→GVRAM 方向、word、array chain、最大速度 auto request と整合する。`BAR/BTC` を使うこと自体を不具合とする根拠はない。旧監査の疑義を再掲する前に、資料間の矛盾を除く必要がある。

**検証:** target の MMIO offset/descriptor size、DMA 正常・error・timeout・停止未確認の経路、行の間隔・境界、CPU fallback との画像一致。通常ベンチで DMA success/error/timeout と VSync を併記する。非同期化は所有権と完了通知が整うまで後回しにする。

## R14: assemblyの共通部分と文書の正本を整理する

**根拠:** 両 `interrupts.s` は MFP 保存復元、TRAP #14、resume 処理などが大きく重複し、QEMU にも context switch の移植版がある。TCB offset は C 側に assert がある一方、assembly には数値が繰り返される。

**提案:** まず MFP 保存復元と例外処理を共通 assembly へ抽出し、方式別 ISR は小さく残す。TCB offset と frame layout を共有 include または target compiler からの生成物で管理する。QEMU 固有の TTY・trap 導入部分を境界に置き、本番との差分を一覧にする。異なる実行環境を無理に一つの巨大な条件分岐へまとめない。

文書の次の不一致も解消する。

- README の「cooperative は全 task で main の stack 共有」は、作成 task に独立 stack を持つ現在の実装と異なる。
- 「1 ms timer」「5 ms ごとの強制切替」「50 ms 切替」が混在する。R10 の定数とテストを正本にする。
- `ssos/README_ja.md` に旧 data thread の記述が残る。通常 UI は現在単一 task。
- XOR 描画の「GVRAM read なし」は実装と異なる。`^=` は read-modify-write であり、「保存用 buffer・以前の広い範囲の readback が不要」と説明する。
- 過去のベンチ数値には commit、機種/エミュレータ、clock、mode、profile 設定、通常 UI か primitive ベンチかを付記する。

**検証:** 抽出前後の逆アセンブル差分、両方式 QEMU、両 `.x` / `.xdf` build とハードウェア確認。既存 `_Static_assert` は維持する。

## 実施計画（P1時点の記録）

2026-09-26 の P1 作業計画。R05 → R01/R03 → R02/R04 の順で第 1〜3 段階として実施した。当時 R07 は本計画から外していたが、その後の P2 で着手した。

| 段階 | 項目 | 状態（2026-09-26） |
| --- | --- | --- |
| 1 | R05 | 実装・検証済み |
| 2 | R01・R03 | 実装済み、結合検証とハードウェア確認が残り |
| 3 | R02・R04 | コア実装済み、R02の起床排他と検証が残り |

第 1〜3 段階の実装は 2026-09-25〜26 に作業ツリーへ反映した。各段階の内容と残作業:

- **R05**: [run_test.sh](../tests/qemu/common/run_test.sh) が QEMU の終了コードと`OK`/`FAIL`出力を判定し、三つのテスト Makefile のループを`set -e`化した。存在しない実行ファイル、`FAIL`出力、`OK`なし timeout で非 0 終了を確認済み。
- **R01**: [linker.ld](../ssos/os/kernel/linker.ld) が`.ssos`先頭に 32×16 KiB の専用スタック pool を予約し、buddy はその残りで初期化する。[app/main.c](../ssos/os/app/main.c) の`ss_init()`は pool 先頭を使い、`ss_alloc`の上限と無関係になった。残り: 全 32slot の非重複・境界・枯渇の結合テスト、両`.xdf`での worker 起動確認。
- **R03**: [message.c](../ssos/os/ipc/message.c) の`current_queue_index()`が`tcb_table`内の一致でキューを決め、main task は IPC 未対応として`SS_ERR_STATE`を返す。Native の`recv_rejects_task_outside_table`で検証済み。残り: main task を IPC 対応にするかどうかの要件判断。
- **R02**: `ss_irq_save()`/`ss_irq_restore(saved_sr)`を両 assembly に導入し、scheduler・work queue・window・IPC・standalone の排他区間を移行した。yield は現在の SR を保存し、`ss_task_sleep()`は復帰後に SR を復元する。残り: `ss_do_wakeups()`の task 入口・ISR 入口の排他契約と一元化、ISR から呼べる関数の一覧化、`ss_task_create()`内 canary 書き込みによる割り込み遅延の短縮、IPL・二重ネストを含む SR 契約の検証。
- **R04**: `ss_recv()`は空キューで`ipc_waiting`を立てて ready queue から外れ、`ss_send()`が待機中の受信者を起こす。実行可能な他 task がない場合はデッドロック防止として`SS_ERR_STATE`を返す（idle task は未導入）。coop/t05・pre/t06 で高優先度 receiver・低優先度 sender とネストした SR mask の復元を QEMU 検証した。残り: 待機登録と send の交差、同優先度の組合せ。

検証結果（2026-09-26、コミット前の作業ツリー）:

- `make test` / `make test SANITIZE=1`: 両方式各 107 件 PASS
- `make test-qemu`: coop 5 件・pre 6 件、全ケース自動判定で OK
- `make test-asm`: 5 件 PASS。`make test-qemu QEMU=false` と `make test-asm QEMU=false` は非 0 で失敗。
- `. ~/.elf2x68k` 後に方式を切り替える前に clean し、両方式の `.x` / `.xdf` をビルド。成果物は `~/tmp/ssos_{cop,pre}.{x,xdf}`。ELF の `.ssos` は RAM 上の NOBITS 0xAB0000 byte で、先頭 0x80000 byte をスタックに確保。
- `make verify-check` は両方式・両形式の X68000 エミュレータ/実機確認を要求。QEMU virt は MFP や実際の起動経路を検証しない。

後続は次の順で行う:

1. R02 の残作業を完了させ、R08・R09（タスク API 契約・共有サービス所有権）を進める。
2. R06・R10・R13 の復旧経路を X68000 エミュレータ/実機で確認する。
3. R11、R12、DMA 閾値の順で一件ずつ測定し、画面同一性と実測改善が揃ったものを採用する。
4. R14 は上記変更の単位で進め、仕様・実装・検証を同時に更新する。

32task・32window 上限の現段階で、ready queue を複雑な木構造に変える、未使用 page flip を有効化する、全面 double buffer を標準化するといった変更は優先しない。単一 UI の測定だけから scheduler 全体の性能を評価しない。

## 今回の検証結果と限界

| 確認 | 結果 |
| --- | --- |
| `make test` | cooperative/preemptive 各105件PASS |
| `make test SANITIZE=1` | 両方式 各105件PASS |
| `make -B test-qemu` | 再build後、coop 4件・pre 5件の最終OKを個別に確認 |
| `make test-qemu QEMU=false` | 全ケース未実行でも終了コード0。R05を再現 |
| 本番buddy/schedulerをリンクした追加プローブ | 512 KiB stack pool要求がNULL、task作成がSS_ERR_STATE |
| 本番main_task/IPCをリンクした追加プローブ | main taskのrecv_nbでUBSanが配列範囲外を検出 |

追加プローブは一時ファイルで実施し、本体・既存テストには変更を加えていない。QEMU の OK は本番 MFP を動かした結果ではなく、trap で駆動する移植版の確認である。`.x` / `.xdf` 全 build、asm サンプル、X68000 実機・専用エミュレータでの今回の確認、描画性能の新規測定は未実施。文書のみの変更なので、性能値は既存のものを新規測定値として扱っていない。

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

使用した skill: `x68k-master`。SKILL.md では `references/` と記載されるが、この環境の実体は `/Users/scott/.agents/skills/x68k-master/resources/`。以下はその配下の資料名。

| 記号 | ファイル・参照箇所 | 本書での用途 |
| --- | --- | --- |
| H1 | `03_割り込み.md` §2、`32_IOMAP.md` 例外ベクタ | SR/IPL、例外・ベクタ、復元の設計 |
| H2 | `04_MFP.md` §5–6、`32_IOMAP.md` MFP | Timer A/D、4 MHz、prescaler、MFP設定 |
| H3 | `21_システム初期化と終了.md` 初期化・終了復元 | Supervisor移行と復帰、COPY/NMI、画面・入力の復元 |
| H4 | `32_IOMAP.md` DMAC、`02_DMA.md` | MMIO配置、OCR/SCR/CCR、MAR/MTC descriptor |

`02_DMA.md` には CSR/CCR の offset や bit、OCR の CHAIN 位置について `32_IOMAP.md` と食い違う記述がある。本書の具体的 DMAC 値は詳細表の `32_IOMAP.md` を採用し、矛盾する概要表をコード変更の根拠にしない。新しい転送方式・停止失敗時の復旧条件など、資料で保証できない細部は**要原典確認**。Vault の DMA ノートにも 16bit MTC に表現できない全画面転送長の例があるため、そのまま転記しない。skill/Vault 自体の変更は今回の対象外。

## P2実装状況（2026-09-26）

P1 は `80a5318` でコミットした。以下はその後に着手した P2 の状態であり、上の調査時点の記述を置き換える。

- **R06 異常終了復元**: `.x` 専用の `standalone/trap14.s` に例外処理を集約した。abort 時は緊急スタック上で `ss_abort_cleanup()` を呼び、Timer D/V-DISP を含むベクタと MFP、COPY/NMI、TRAP #14、表示・入力状態を段階フラグに応じて復元する。通常終了も同じ cleanup を通る。`.xdf`には Human68K の abort 処理をリンクしない。実機・Human68K エミュレータでの故障注入と、初期化途中・再入の確認は未実施。
- **R08 タスク契約**: `entry(arg)` を両方式の起動 assembly から呼び、戻り値は破棄し、return 時は `ss_task_exit()` が `TERMINATED` に移す。ID・stack は再利用しない。`ctx_level`の不正値と custom stack の最小サイズ・整列を拒否する。Native と QEMU の引数・return テストを追加した。
- **R09 所有者**: [runtime-ownership.md](runtime-ownership.md) に scheduler、IPC/work、allocator、描画、DMAC Ch.2 の所有者・排他契約を記載し、公開 header にも主要制約を追記した。DMAC abort 後の停止未確認問題は解決しておらず、実機確認が必要。
- **R10 起床責務**: cooperative の起床を scene から scheduler の切替点へ移し、IRQ mask 下で処理する。main task が sleep 中に worker だけが yield する QEMU テストを追加した。watchdog の MFP 再設定と VSync busy wait は未変更。
- **R07 ビルド分離**: `os/`、`standalone/`、`boot/`の中間物を方式・形式・profile 別に分離した。`.xdf`生成場所も分離し、profile 1 は公開ファイル名に `_profile1` を付ける。C の header 依存は `.d` で追跡する。両方式の profile 0/1 と profile 切替後の並列 build は成功。header 変更後に対象 object が再構築されることも確認したが、二度連続の完全 no-op 判定は残る。
- **R14 重複・仕様**: TRAP #14 を standalone 専用に、MFP 状態保存復元を `os/kernel/mfp_state.s` に抽出した。README の tick/switch 周期、stack、wakeups、ビルド手順を更新した。方式別 context switch と QEMU port の差分、TCB offset 共通化は残る。

自動検証: Native 各 109 件、QEMU cooperative 7 件・preemptive 7 件、asm 5 件が PASS。両方式の `.x` / `.xdf` はクロスビルド・リンク済み。これは MFP 実割り込み、Human68K abort、実画面・DMA を検証したことを意味しない。

## 4形式の動作確認後の残課題（2026-09-27更新）

利用者から `ssos_cop.x`、`ssos_pre.x`、および両 `.xdf` の実機通常起動確認が完了したとの報告を受けた（2026-09-27）。これは通常起動の確認であり、異常終了後の復元、DMA の正常転送・停止失敗、性能改善まで確認済みとは扱わない。上の「実機未確認」は調査当時の記述であり、現在の通常起動状況は本節を正とする。

残作業は安全性を性能より先に扱う。下表の優先度は現在の実装状況によるもので、R11〜R13 の調査時点の区分を変更する。

| 順序 | 対象 | 現行コードから確認できること | 完了条件 |
| --- | --- | --- | --- |
| 1・安全性 | R13 DMA停止未確認 | SAB後も`ACT`が残る場合は`-3`で区別し、source/descriptor更新、再DMA、CPU fallbackを止める。さらに描画APIのGVRAM書き込みもprocess-lifetimeで停止する。NativeでACT解除あり/なしを故障注入済み。 | 実機でACT判定・DMA異常時の振る舞いを確認する。`void`描画APIにエラーが伝播しないため、利用者への異常通知と復旧方針は別途必要。 |
| 2・描画量 | R11 drag終了 | 共有`scene.c`の旧active再合成をタイトル高12pxへ限定した。通常UIの決定的入力で非重複・部分重複・全面遮蔽・z再番号付け後の画素一致をNativeで確認した。 | `runtime`で同一入力列の描画量・実時間を測る。240×48→240×12はこの1要求の面積75%減に限る。 |
| 3・文字描画 | R12 dirty text | 共有`draw_content_dirty()`は先頭と末尾の差分を求め、その区間だけ描く。Nativeの通常UIでは重なり・遮蔽、4桁から1桁への変化、右画面端でのclipを含め全面再描画との画素一致を確認した。`text-update`ベンチはこの処理を測らない。 | 通常UIのglyph数・GVRAM write・実時間を同一入力列で測る。 |
| 4・実測採否 | R13 DMA閾値 | 現条件は幅`>64`、幅`<=512`、高さ`>4`。profile版`.x`に`-dma-bench`を追加し、同一矩形のCPU強制/DMA強制を24条件で比較できる。 | 実機の両モード・両方式で成功率、fallback、Timer D tick、画素一致を測る。停止未確認が0である構成だけで閾値を決める。値64の変更は測定後。 |

手動検証として残る項目は R06 の TRAP #14/abort 故障注入と復元後の入力・割り込み確認、R10 の watchdog 復旧動作と VSync busy wait の時間特性、R11/R12 の通常 UI 描画量・実時間測定、R13 の DMA ハードウェア挙動・閾値測定である。XOR の`^=`による read-modify-write と矛盾したコメントは訂正済み。R01/R02/R04/R13 ABI/R14 の自動検証は次節に記録する。

### 自動検証で完了した残項目（2026-09-27）

通常起動確認後、実機操作を要しない残項目を自動検証へ追加し、完了した。

| 対象 | 完了内容 | 根拠 |
| --- | --- | --- |
| R01 stack pool | Nativeで32 slotの開始・終了境界、隙間・重複のない連続配置、各stack両端のcanary、32個作成後の枯渇を検証 | `tests/unit/test_scheduler.c` の `task_create_exhaustion` |
| R02 SR復元 | QEMUでIPL 0〜7それぞれについてsave/restoreと二重ネスト後のmaskを検証 | `tests/qemu/common/t_ipc_blocking.c`、両方式でPASS |
| R04 IPC待機・起床 | 高優先度receiverと低優先度sender間で32回の空queue待機・送信・起床・FIFO受信を検証 | 同QEMUテスト、両方式でPASS |
| R13 target ABI | 32-bit target限定でDMAC channel registerのoffset/sizeと、6-byte転送descriptorのoffset/sizeを静的assert | `ssos/os/gfx/gfx.h`、cooperative/preemptive target build成功 |
| R14 TCB offset | Cの`SSTask` assert、production ISR、両QEMU context-switch portで同じ`scheduler_abi.h`のoffset定義を共有 | target buildとQEMU両方式が成功 |

Native 通常・ASan/UBSan 各方式 113 件 PASS、QEMU cooperative 7 件・preemptive 7 件 PASS、cooperative/preemptive の target `.x` build 成功。両 target ELF の`.ssos` NOBITS 領域は`0xAB0000` byte で、stack pool と残りの SSOSRAM が linker 領域を占めることを確認した。`git diff --check`も成功した。R01 の Native 検証は scheduler allocator の slot 計算を対象にし、実機リンカ上の stack 領域へタスクが実際に書き込む動作までは実行していない。

### 手動確認が必要な残項目

通常起動は実機で確認済みだが、次の故障経路・時間特性は未確認である。

- R02: 実 MFP Timer D と task 側 wakeups の非同期競合、および Timer D/V-DISP 最大遅延。
- R06: Human68K `.x` の TRAP #14/abort 故障注入、復帰後の MFP・割り込み・入力状態。
- R10: V-DISP/Timer D 停止時の watchdog 復旧動作、MFP mask・vector の実機状態、および busy-wait 停止条件の時間特性。
- R11/R12: 通常 UI の実描画量と実時間の測定。
- R13: DMA 正常転送・異常・停止未確認のハードウェア挙動と、各画面モード/方式での閾値測定。

DMA の詳細レジスタ値は資料間に不一致がある。ここでは既存設定を変更せず、`x68k-master/resources/02_DMA.md` と本書 H4 で指摘した差異を要原典確認として維持する。性能測定とログ条件は[gfx-performance.md](gfx-performance.md)を参照する。

## Xeijの約1分後の例外調査（2026-09-26）

利用者の`ssos_pre.xdf`で PC=`0x0004EE4C`、命令語=`0x583A`、SR=`0x0418`の undefined instruction が発生した。変更前の preemptive ELF をシンボル付きで同条件再リンクした配置では、`0x4EE4C`は`win_content[2].line[0]`の先頭で、表示文字列`X:`の ASCII が`0x583A`となる。CPU が実行コードでなくマウス表示用データへ分岐したのであり、Xeij の未知命令解釈だけを原因として無視できない。

`ss_task_yield()`は割り込みをマスクせずに manual 復帰フレームを作成し、TCB の`resume_type=1`を書いていた。Timer D がこの書込みから context 保存までの間に入ると`resume_type=0`に上書きされ、元の manual フレームを`rte`フレームとして復帰し得る。SR/PC の解釈が 2 バイトずれ、異常 SR やデータ領域への分岐と整合する。これを有力なコード上の競合として、元 SR をフレームへ保存した直後に IPL7 へ上げ、復帰時に SR を戻すよう変更した。QEMU の同期 TRAP テストは復帰経路を通すが、Xeij の非同期 Timer D の同タイミングを再現していない。修正版`.xdf`で長時間・反復操作の再試験が完了するまで、単一の例外をこの競合と断定しない。

利用者は修正版`ssos_pre.xdf`を Xeij で試し「大丈夫そう」と報告した。短時間の再発確認として記録するが、長時間・負荷・割り込みタイミングの網羅的検証ではない。2026-09-26 の修正と文書は`f2a32cb`にコミット済み。

その後、共有 scene を Native RAM framebuffer 上で決定的に駆動し、3 回の drag drop（非重複・部分重複・Mouse の全面遮蔽）、通常文字更新、および 252 回の再前面化で z 値を再番号付けした後の表示を全面再描画と画素比較するテストを追加した。両方式で PASS。ドラッグ中は本文更新を意図的に保留するため、全面再描画との比較点から除外する。profile 版`.x`には`-dma-bench`を追加し、幅 32/64/65/128×高さ 4/5/16×CPU/DMA の 24 条件で Timer D tick・VSync・DMA 結果・画素一致を記録する。実機測定値は未取得で、閾値は不変。使用法と採否条件は[gfx-performance.md](gfx-performance.md)に記す。

続く Native テストでは、z 再番号付け後に Mouse 窓をテスト側から右画面端へ一部はみ出す位置へ配置し、X 表示を 1000 から 9 へ縮めた。UI の drag 自体は窓を画面内へ clamp するため、この配置は clip 経路の故障注入である。各通常フレームの画素を全面再描画と比較し、両方式で一致した。これは実画面の性能測定ではない。

## 監査で指摘された残項目の実装（2026-09-27）

4 形式の実機通常起動確認後、本書の実装状況を現行コードと照合する監査を行い、プランにありながら残作業リストから漏れていた項目と実装・文書の不整合を指摘した。この節はその対応記録であり、上の各節の記述を本節が優先する。

実装したもの:

- **R01 初期化検査**: `ss_mem_healthy()` を追加し、`ss_init()` は allocator 初期化失敗で `SS_ERR_MEMORY` を返す。`premain()` は `(uintptr_t)&__ssosstack_size == SS_MAX_TASKS * SS_TASK_STACK` の実行時検査と `ss_init()` の失敗を割り込み禁止の停止ループで扱う。linker の `32 * 16384` と kernel.h 定数の不一致が起きても破損した stack で起動しない。
- **R02 canary短縮**: `ss_task_create()` の canary 書き込みを割り込み禁止区間の外へ移動した。slot はこの時点で `DORMANT` 予約済みかつスケジューラから不可視のため、16 KiB 書き込みをマスク内に置く必要がない。
- **IPC 宛先状態検査**: `ss_send()` は `SS_TS_TERMINATED` の宛先を `SS_ERR_STATE` で拒否する。未開始 slot への事前送信（既存テストが契約とする使い方）は維持する。
- **standalone bench の差分ロジック一致**: ベンチ側 `draw_content_dirty` に共有 `scene.c` と同じ先頭・末尾差分を移植した。`text-update` ベンチが新旧どちらの文字更新経路を測るかが一致した。
- **デッドコード削除**: `standalone/main.c` cleanup 後の `ss_trapbuf_flag` 分岐は `trap14.s` が例外を終端処理するため到達不能であり、削除した。
- **cooperative yield の不変条件**: mask せずに `resume_type`/`context` を書く前提（coop Timer D ISR はコンテキスト切替しない）を assembly コメントに明記した。
- **R14 README整合**: IMR を両方式 `$FF/$7F` として統一（`$21/$10` は `.x` watchdog 復旧時のみと明記）、boot 手順・MFP 表・IMR 節・制限表の 4 箇所を修正。yield/Timer D ISR サンプルを現行実装（実 SR 保存、`a1` ロード、SR mask）へ更新。ベンチ数値に `4e31dd4`（2026-07-18、primitive ベンチ、profile 1 `.x`、機種記録なし・再計測要）の出典を付記。
- **R05 asm期待出力**: `run_test.sh` に `EXPECT` 環境変数による追加断片照合を導入し、asm 5 サンプルに固有の期待出力を定義した。断片不在で非 0 終了を確認。
- **R07 テスト依存**: `tests/qemu/{coop,pre}` の Makefile へ `-MMD -MP` と `.d` 読込みを追加した（`scheduler.h` 変更で `sched.o` が再構築される）。asm は純 assembly で C header 依存が存在しないため対象外。
- **standalone Makefileの組込みバグ修正**: `standalone/Makefile` の `%.o: %.s` 汎用ルールは cpp を通さず、`interrupts.s` の `#include "scheduler_abi.h"` が as 側でコメントとして無視され、`SS_TCB_*` が未解決のままリンクが失敗する。`os/Makefile` と同じ `interrupts.o` 専用の `-x assembler-with-cpp` ルールを追加した。この不具合は `scheduler_abi.h` 共通化のコミット以降、新規に `.s` が変更されない限り古い `.o` で隠れていた。今回のコメント追加による再ビルドで顕在化した。

追加した自動検証:

- **Native** `test_gfx.c`: `dma_stop_unconfirmed` 成立後、`ss_gfx_clear`/`rect`/`fill_stipple`/`char`/`char_fast`/`xor_rect`/`draw_text_region`/`char_clip` の全 GVRAM 書き込み経路が停止することを画素不変で検証。
- **Native** `test_ipc.c`: TERMINATED 宛 `ss_send` の拒否と、未開始 slot 事前送信・生存宛先への影響なしを検証。
- **QEMU** `t_sleep_sr`（両方式）: IPL 0/3/5/7 の各設定で `ss_task_sleep` 復帰後に呼び出し前の IPL へ戻ることを検証（`scheduler.c` の SR 契約）。
- **QEMU** `t_ipc_samepri`（両方式）: 同優先度 receiver/sender 間の blocking 往復 32 回（R04 の残組合せ）。
- **QEMU** `t_alloc_stress`（両方式）: 2 task が IRQ mask 下の割当て・解放を交互に繰り返し、内容 pattern 検証・canary・総空き容量の不変を確認（R09 の検証項目）。

意図的に後回しとするもの（未実施の記録）:

- **R12 glyph triage**: `ss_gfx_draw_text_region` の glyph 単位振り分け（clip 外 skip・完全包含 fast・端のみ clip）は、通常 UI の実測で効果を確認してから採否を決める。本書 R12 節の「残り」に測定のみ記載していたのは不備であった。
- **R07 共通make include**: `os/Makefile`・`standalone/Makefile`・`tests/Makefile.native` の 3 ソースリストは現時点で一致している。次回のソース追加時に共通化を検討する。
- **R05 per-case log永続化**: `run_test.sh` の出力は一時ファイルで廃棄される。失敗時の CI ログ保持は必要になった時に追加する。
- **R08 task API戻り値表現**: `uint16_t` + 負エラー定数の現状は `scheduler.h` に文書化済み。`int16_t` 化は ABI 更新の際にまとめて行う。

検証結果（本節の実装後）: Native 通常・SANITIZE=1 とも両方式 115 件 PASS、QEMU cooperative/preemptive 各 10 件 PASS、asm 5 件 PASS（期待断片照合込み）。`make test-qemu QEMU=false`・`make test-asm QEMU=false` は非 0 終了。
