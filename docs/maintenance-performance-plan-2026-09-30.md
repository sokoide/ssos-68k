# SSOS 保守性・性能の追加改善計画

作成日: 2026-09-30 / 静的レビュー対象: `deb0f50`

## 判断と範囲

[2026-09-24のレビュー](maintenance-performance-review-2026-09-24.md)は、初期化・排他・IPC・ビルド・描画の主要な問題を扱い、自動検証と実機検証の境界も明確である。ただし、現在の実施計画としては十分ではない。終了時まで含む DMA の寿命、残る SR 固定変更、task 公開の競合、UI 要求の寿命と処理予算、scheduler/allocator の時間特性が不足している。追記方式の進捗管理も、未完了項目を見落としやすい。

本書を今後の残作業の入口とする。旧文書の R01〜R14 は根拠と実施履歴として保持する。本書は実装指示と完了条件であり、新しい性能測定結果や実機での不具合再現を示すものではない。

今回確認したものは旧計画、現行 C/assembly、Makefile、検証スクリプト、テスト定義、[所有権契約](runtime-ownership.md)、[描画測定手順](gfx-performance.md)。本体の変更、テスト実行、クロスビルド、実機操作、性能測定は実施していない。過去の PASS 件数は再実行結果として扱わない。作業開始時の未追跡 `.eggs/`、`.eggshell.toml` は本計画の対象外。

MC68000・elf2x68k・両 SCHED・`.x`/`.xdf`を対象とする。機種・clock・エミュレータ版は測定時に確定する。共有 scheduler/scene とホスト固有の初期化・終了・DOS 入出力の分離を維持する。

## 旧計画の照合結果

| 旧ID | 現行状態と残作業 | 本書の対応 |
| --- | --- | --- |
| R01 | 専用pool、初期化検査、Native slot境界あり。実リンカ領域でworkerを走らせる確認が残る | N07 |
| R02 | SR保存APIとqueue排他、canaryのmask外初期化あり。ホスト補助assemblyのSR固定変更と初期化中slotの開始は別途確認が必要 | N01、N03、N06 |
| R03 | table外mainの受信を拒否する契約を実装。main向けIPC対応は要件未決定 | N04 |
| R04 | blocking受信、異優先度/同優先度QEMUテストあり。同期TRAPは非同期交差の保証ではない | N06、N07 |
| R05 | 終了状態・OK/FAIL・asm期待断片を判定。ログは一時ファイルで、直近の固定文字列照合変更は今回未実行 | N05 |
| R06 | 段階flagと緊急stackのcleanupあり。SR・再入・DMA停止・初期化途中の境界が未完了 | N01、N02、N07 |
| R07 | 方式/profile別build、C依存とABI header依存あり。共通source一覧、設定変更追跡、no-op確認は残る | N05 |
| R08 | 引数、return→TERMINATED、stack/ctx検査あり。非再利用、unsignedエラーは文書化済み。公開タイミングが追加課題 | N03、N04 |
| R09 | 所有者と外部排他を文書化、allocator QEMU stressあり。UI要求のpayload寿命、ISR可否、処理予算は不足 | N04、N06 |
| R10 | cooperative起床をschedulerへ移動済み。watchdogの無期限復旧とbusy waitは残る | N01、N06、N07 |
| R11 | 旧activeタイトル限定と通常sceneの画素テストあり。通常sceneの実測は未了 | N06、N08 |
| R12 | 両端差分と表示28文字の判定あり。glyph振り分けは意図的保留。通常sceneの実測は未了 | N08 |
| R13 | 停止未確認時の描画凍結、target ABI assert、24条件DMAベンチあり。終了時の停止保証と幅広い領域の閾値根拠は不足 | N02、N08 |
| R14 | MFP/TRAP分離とTCB offset共有あり。frame layout・production/QEMU差分一覧・仕様更新の一元管理が残る | N05、N09 |

過去の 4 形式通常起動報告は旧文書に記録済みだが、後続の`87bd297`/`deb0f50`の成果物に対する確認と同一視しない。今回のコード変更後の実機確認結果は取得していない。

## 実施順と依存

| 順序 | 項目 | 優先度 | 開始条件 / 完了の要点 |
| --- | --- | --- | --- |
| 1 | N01 復旧とSRの境界 | P1 | 今すぐ。固定SR変更・無期限復旧を解消し、故障時の有限な動作を定義 |
| 2 | N02 DMAの終了時寿命 | P1 | 今すぐ設計。停止未確認でプロセス解放・表示復元へ進む条件を解消 |
| 3 | N03 taskの公開状態 | P1 | 今すぐ。初期化途中のstartを不可能にする |
| 4 | N04 UI要求とAPI契約 | P2 | N03後。payload寿命・満杯・drain予算・main IPC方針を定義 |
| 5 | N05 検証・buildの信頼性 | P2 | 1〜4と独立して文書/仕組みを整備。成果物と証拠を結び付ける |
| 6 | N06 時間特性の基準測定 | P2 | 1〜4の安全性修正後。描画以外も測り、許容値を決める |
| 7 | N07 実機の故障・負荷確認 | P1の完了条件 | N01〜N03の修正後。未確認ならP1を完了にしない |
| 8 | N08 描画の採否測定 | P3 | N02/N07のDMA安全性確認後。通常sceneとDMA領域を比較 |
| 継続 | N09 仕様と進捗の正本 | P2 | 各項目の変更と同時に更新 |

P1 はコード変更・自動検証・必要な実機検証を分けて記録する。実機を使えない場合も設計と自動検証は進め、状態を「実機確認待ち」とする。

## N01: 復旧処理が外側のSR契約を壊さないようにする

**確認事実:** [standalone/trap14.s](../ssos/standalone/trap14.s)の`ss_init_trap14`と`ss_restore_trap14`は SR を固定値へ変更し、最後に割り込み許可状態へ戻す。[standalone/main.c](../ssos/standalone/main.c)の`ss_abort_cleanup()`は`ss_irq_save()`後に後者を呼ぶので、外側の保存 SR を戻す前に mask が解除される。段階 flag があっても、cleanup 全区間を排他的にはできていない。再入時に同じ緊急 stack 先頭へ切り替える assembly も、C の再入 flag だけでは安全性を証明できない。

同ファイルの`wait_vsync()`は spin 上限ごとに MFP/vector を排他なしで再設定し、成功しなくても無期限に繰り返す。[scene.c](../ssos/os/app/scene.c)の baremetal 待機も停止条件がない。これらは旧 R10 の「手動確認」だけでは完了しない実装課題である。

**作業:** ホスト補助 assembly を含め SR を変える関数の入口/出口契約を列挙し、保存復元型か caller-held 型に統一する。初期化だけが意図的に割り込みを有効化する箇所は別契約にする。cleanup の初回・再入・部分初期化を状態遷移で定義する。watchdog は保存済み状態を上書きせず、有限回の復旧後に`.x`では安全な終了、`.xdf`では定義した停止/診断へ進む。Timer D も停止する場合は同じ tick を脱出の唯一の基準にしない。

**完了条件:** 外側 IPL とネストを含め、復元前の早期 enable がない。V-DISP のみ停止、Timer D のみ停止、両方停止、復旧失敗の終端が定義される。初期化途中と cleanup 再入の故障注入を通す。IOCS 呼出しと緊急 stack の扱いは Human68K 環境で別途確認する。

## N02: DMA停止未確認をプロセス終了まで追跡する

**確認事実:** [vram.c](../ssos/os/gfx/vram.c)は停止未確認後、source/descriptor 変更と描画を凍結する。しかし source/descriptor はプロセス内の static 領域である。`.x`の`run_dma_benchmark()`は停止未確認で return した後、呼出し側の`goto cleanup`へ進む。通常 scene の stop 条件は ESC のみで、`ss_abort_cleanup()`に DMAC 停止確認はない。

**推論と境界:** 実際に DMA が継続しているかは未確認だが、ACT 停止未確認のまま表示復元・プロセス領域解放へ進めば、描画 API 内の凍結だけでは source 寿命と GVRAM 所有権を守れない。旧 R13 の「停止して原因確認まで再起動しない」という運用説明と、現行 cleanup 経路の整合を取る必要がある。

**作業:** DMA 状態を正常・停止済み失敗・停止未確認に分け、通常終了と abort の両方から参照する。停止未確認は scene/host へ通知する。停止保証が得られるまでは、source/descriptor 解放、画面モード復元、他の描画主体への所有権返却へ進まない方針を設計する。停止不能時の最終動作を定義し、未確認の reset 手順を推測で実装しない。既存の`void`描画 API を一括変更する前に、host が取得する sticky 状態で足りるか評価する。

**完了条件:** 正常終了・例外 abort・ベンチ終了の各経路で、ACT 解除済み/未確認を区別する。Native では cleanup へ渡す状態と禁止操作の順序を故障注入し、実機では停止・復元の境界を確認する。速度評価によって安全条件を緩めない。

## N03: 初期化中のtaskをstartできない状態にする

**確認事実:** [scheduler.c](../ssos/os/kernel/scheduler.c)の`ss_task_create()`は slot を DORMANT にし、mask を戻して canary を書き込む。`ss_task_start(id)`は DORMANT なら即 READY にする。ID は呼出し元に未返却でも 1〜32 の公開値であり、別 task からの start を拒否する契約/状態がない。

**条件付き競合:** 別 task が初期化中の ID を start して yield すると、canary 書込み完了前に worker が動き得る。通常 UI でこの競合が起きたという意味ではない。現行の「DORMANT なので不可視」という説明は、start API まで含めた保証として不足する。

**作業:** INITIALIZING など予約専用状態を使い、TCB/stack 初期化後の短い mask 区間で DORMANT へ公開する。状態追加は TCB の既存 ABI offset を変えずに行う。作成失敗時の slot 消費と IPC 事前送信の契約も維持・照合する。

**完了条件:** 初期化中に start を試みる決定的な交差テストで拒否し、完了後は一度だけ開始できる。QEMU で実 stack 上の entry/return/canary を確認する。canary 全体を mask 内へ戻す対処は避ける。

## N04: UIへの要求を安全に処理できる契約を仕上げる

**確認事実:** [work_queue.c](../ssos/os/kernel/work_queue.c)は`handler`と`arg`ポインタをコピーし、`ss_work_drain()`は空になるまで実行する。callback 自身の再 enqueue や継続 producer があれば drain が終了しない経路がある。scene は drain 後に yield する。[work_queue.h](../ssos/os/kernel/work_queue.h)と所有権文書は payload 寿命・consumer 数・ISR 可否・処理上限を定義していない。

**作業:** 単一 UI consumer、payload を保持する主体と解放時点、enqueue 失敗時の責任、許可する ISR と callback 操作を明記する。IPC は値コピー、work はポインタ保持であることを区別する。drain を開始時件数または件数 budget で区切り、残りを次 frame へ回す。callback 一件が無制限に動く場合は件数 budget でも時間保証できないため、handler の実行条件も決める。

main IPC 拒否、全 task 待機時の`SS_ERR_STATE`、終了 slot 非再利用、unsigned の負エラー表現は意図した制限として API 表へ集約する。main IPC や join/reuse を追加するのは具体的な利用要件がある場合に限る。

**完了条件:** 要求満杯時に所有権を失わず、短命な stack payload を誤用しない使用例がある。再 enqueue・継続 producer 下でも frame/yield が進む。非所有者 task は描画 API を直接呼ばず、要求 queue 経由で画面を更新する結合テストがある。

## N05: 検証結果と成果物を再現可能にする

**確認事実:** [verify-check.sh](../tools/verify-check.sh)は production assembly の一部を QEMU で「fully cover」として扱うが、QEMU は別の移植 assembly と同期 TRAP である。また`ssos/`外を一律バイナリ影響なしとするため、disk 生成 tool 変更を見落とす。Git diff 失敗は stderr を捨て、未追跡 source は通常 diff に含まれない。[run_test.sh](../tests/qemu/common/run_test.sh)の per-case 出力は終了時に削除される。

**作業:** verify-check を「自動検証範囲」と「実機確認要否」の別軸にし、production/QEMU 差分と非同期競合を理由へ反映する。無効 REF は非 0、未追跡/削除 source は安全側判定、disk 生成 tool は`.xdf`への影響として扱う。判定自体の fixture を追加する。情報表示コマンドの成功は手動確認完了を意味しない。

build は clean build、方式/profile 切替、header/assembly include/linker/makedisk 変更、連続並列 build を確認する。2 回目に object/link/disk 生成が不要なことを条件とし、公開先へのコピーは別に扱う。Makefile や compiler flag 変更で既存 object が更新されるかも確認する。共通 source 一覧は同一であるべき集合とホスト固有集合を明示してから小さな include へまとめる。

**完了条件:** 失敗時 log が case・SCHED ごとに保存され、成功件数と対象 commit、dirty 状態、compiler/QEMU 版、設定を記録できる。`.x`/`.xdf`の hash と実機確認記録が対応する。古い object に依存せず全構成を生成できる。直近`deb0f50`の差分描画/固定文字列照合も対象に含める。

## N06: 描画以外の時間・メモリ特性を測る

旧計画の 5ms tick/50ms 強制切替の説明は適切だが、許容する起床遅延・最大 mask 時間・worker 進捗・queue 遅延の基準がない。まず基準値を取得し、用途に応じた許容値を決める。計測前に scheduler 構造や timeslice を変更しない。

| 対象 | 負荷と指標 |
| --- | --- |
| IRQ/切替 | task数1/2/8/32、sleepers 0/1/32。最大mask時間、ISR時間、yield/switch時間、実Timer D/V-DISPの遅延 |
| sleep/IPC | 1/5/10 tick待ち、異/同優先度、描画/DMA併走。期限到達→READYとREADY→実行を分け、中央値・上位分位・最大値を記録 |
| allocator | 代表サイズ、枯渇/断片化、2 task外部排他。alloc/free最大時間、mask時間、最大割当て可能サイズ、容量回復、stack high-water |
| UI/要求queue | producer連続投入、queue満杯、長いcallback、drag。frame最悪時間、queue高水位、拒否数、要求到着→適用、worker進捗 |
| VSync待機 | busy wait中の他worker進捗、待機時間、frame欠落。cooperative/preemptiveと優先度を分ける |

**計測条件:** IRQ 禁止中の経過時間を、禁止された Timer D ISR の counter だけで測らない。独立に進む時間源またはエミュレータ trace 等を確認し、その分解能・誤差・計測負荷を記録する。実機 clock を超える精度をログから捏造しない。最大値は観測最大であり、未試験入力を含む上限証明とは区別する。

**採否:** 優先度による飢餓は既存 policy と実装不具合を分ける。32task 上限では、複雑な queue 構造より測定した長い mask 区間や無駄な待機を先に改善する。VSync イベント待機＋idle は測定と全 task 待ち契約を整えてから別変更にする。

## N07: 実機確認を操作可能なケースにする

| ケース | 対象 | 合否と記録 |
| --- | --- | --- |
| 起動/worker | 両方式・`.x`/`.xdf` | 成果物hashを記録し、通常UIに加え独立workerのstack書込み・sleep・IPC・returnを確認。`.x`のIPC非リンク制限は維持 |
| 非同期切替 | 特にpreemptive両形式 | yield/sleep/IPC/作成を反復し、描画負荷と実Timer Dを重ねる。PC/SR、queue整合、canary、worker進捗を記録 |
| 部分初期化/abort | 両方式`.x` | 保存前後の各段階へ注入。正常ESC、例外、再入でMFP/vector/表示/入力を確認し、復帰後に再起動できることを確認 |
| watchdog | 両方式・両形式 | V-DISPのみ、Timer Dのみ、両方停止、復旧成功/失敗。終端動作と時間、復元後の入力を確認 |
| DMA終了 | 両方式・対応画面モード | 正常/error/timeout/停止未確認と終了/abortの組合せ。停止前の領域解放や画面復元がないことを確認 |

故障注入は復帰できるエミュレータ snapshot から始め、実機適用条件を確認する。非同期切替は暫定条件として各形式 30 分以上、再起動を含む 3 回を計画する。時間だけで網羅性を主張せず、操作列、切替回数、発火回数、エミュレータ版/機種/clock、注入位置を残す。過去の「大丈夫そう」という短時間報告をこの完了条件の代用にしない。

## N08: 通常sceneの測定とDMA閾値の範囲を完成させる

[既存手順](gfx-performance.md)の同一入力・画素一致・複数回・CPU/DMA 順序入替え・失敗条件除外を維持し、次を追加する。

1. Native 専用の入力注入だけでなく、target でも共有`ss_scene_run()`を決定的入力で駆動する小さな測定入口を作る。通常入力経路との差分を明記する。Timer/Key/Mouse の内容更新も固定し、入力 checksum、処理 frame 数を残す。
2. 非重複・部分重複・全面遮蔽、drag 中の本文保留、drop、z 再採番、桁減り、画面端 clip を含む入力列で R11/R12 を一件ずつ比較する。通常 runtime の手操作を揃えただけでは再現性の完了条件にしない。
3. 複数回の経過時間・frame 最悪時間・glyph/再合成/予定 write 量と画素結果を保存する。profile 1 で処理量、profile 0 または同等の軽量計測で時間を確認し、計測 counter 自身の負荷を区別する。比較対象 commit と改善一件の差分を明記する。
4. DMA の現 24 条件は幅 32/64/65/128×高さ 4/5/16 であり、幅 512 や大きい高さの性能を保証しない。幅 256/512、無効/範囲外側の policy 境界、画面端 clip、大矩形を追加する。実際に DMA へ渡る寸法と成功全行数を確認する。
5. N02 の停止保証後、両方式・`-8`/`-16`で実測する。十分な反復と warm-up、順序入替え、暫定 5 回以上の採取でばらつきを記録する。粗い tick の僅差は保留し、エミュレータ測定を実機閾値に転用しない。

**採否:** 同一画素・DMA 正常・時間差が測定のばらつきを上回ること。改善対象外の frame 最悪時間・worker 進捗・code size も確認する。glyph の clip 外 skip/包含 fast はこの基準測定後に試す。効果がなければ既存実装を維持する。`.xdf`の時間比較は別のログ出口が必要で、`.x`の数値をそのまま転用しない。

## N09: 仕様・進捗・証拠を一箇所から追えるようにする

各 N 項目を「未着手 / 実装中 / 自動検証済み / 実機確認待ち / 完了 / 保留」に分け、変更 commit、検証 commit と設定、証拠の保存先、未確認条件、次の一手を同じ行に管理する。保留には再開条件を付ける。新しい追記が古い結論を暗黙に上書きする方式は止める。

production/QEMU の差分一覧には手動 yield frame、例外 frame、start/return、register 保存、SR/PC 復元、MFP/EOI 固有部分を含める。TCB offset 共有と frame layout 共有を区別し、共有できない部分は対応表と逆アセンブル比較で管理する。機械的な巨大 assembly 共通化を目的にしない。

README と`ssos/README_ja.md`、tests README、所有権文書、gfx 手順の契約を各変更で照合する。API/ABI 変更、build 設定、検証上の制限に正本へのリンクを付ける。本書から完了した項目を削除せず、証拠付きで状態を更新する。

## 最初の変更単位

まず N01 と N02 の終了状態・SR 契約を設計し、N03 を小さな独立修正として実装する。次に N04/N05 を整え、N06 の測定入口と N07 の故障注入ケースを追加する。実装時は関連 Native/sanitizer/QEMU、両形式のクロス build を実施し、対象が揃った段階で`make verify`を行う。`.x`/`.xdf`の故障・実時間は N07/N08 で別途確認する。

今回の成果物はこの計画書と旧レビューからの導線のみ。改善の実装と計測は未着手である。

## 自動部分の実施結果（2026-09-30）

本節が上の「未着手」を更新する。検証対象は`3b728cc`からの実装作業ツリーで、source hash・diff・成果物 hash/size・tool 版は`build/verification/20260930T141453015499/manifest.json`に保存した。検証後の本節追記は文書のみ。実装 commit は本節を追加した commit を`git log --follow`で確認できる。

| ID | 実装・自動検証 | 状態 / 残る条件 / 次の一手 |
| --- | --- | --- |
| N01 | runtime_guardの部分初期化16組合せ/再入、tick/spin有限待機、snapshotを保存した一度のMFP復旧、TRAP補助のcaller SR復元、緊急stackへの再入をassembly入口で停止。production restoreをQEMUでIPL0〜7/ネスト検証 | 自動部分完了。IOCSを含むabort/再入、MFP復旧と入力は手動確認待ち。N07の注入位置で試す |
| N02 | 所有するin-flight DMAをcleanup入口で有限停止確認。未確認ならIRQ mask下で保持し、表示/プロセスを解放しない。描画だけでなくmode/flip/initも凍結。NativeのACT故障注入とcleanup順序を確認 | 自動部分完了。実DMACの正常/異常/停止確認は手動待ち。reset手順は推測で追加していない |
| N03 | INITIALIZING予約→canary→短いmask下でDORMANT公開。Native/QEMUで途中のstart拒否、entry/return/canaryを確認 | 実装・自動検証完了。実Timer Dとの非同期交差はN07で確認 |
| N04 | 初期batch/budgetによるdrain、NULL要求拒否、high_water/rejected/delay、payload/ISR/consumer契約。再投入をNativeで、worker→UIの永続payload・満杯・budgetを両QEMUで確認 | 自動部分完了。main IPC、join/reuse、戻り値ABI変更は具体的要件が出るまで保留 |
| N05 | 安全側verify-checkと無効REF/未追跡/削除/disk tool fixture、case log保持、逐次verify、hash/size/source snapshot/逆アセンブル保存。共通source includeとflags/config/assembly include依存。clean/profile切替/依存/診断build/2回no-op検証 | 自動部分完了。成果物に対応する実機確認の記録を次に追加する |
| N06 | 両方式/profile/modeのhost測定、時計分解能/最小step、warm-up＋5run、scheduler/allocator/workの100操作batch、容量回復、最大payload、stack残量。任意のruntime profileでdeadline→READY/READY→実行を分離しNativeで検証 | 自動部分完了。実IRQ mask/ISR/switch時間、実worker進捗と待機時間は独立clock/traceで手動測定待ち。host nsをtarget値へ転用しない |
| N07 | 1/2/8/32task、1/5/10tick sleepのQEMU負荷。targetと共通の32worker stack/sleep/return診断をQEMUで実行し、`.xdf`用IPC ringも確認。両形式の診断buildを生成 | 自動部分完了、手動ケースは全て未確認。通常起動/非同期/abort/watchdog/DMAを成果物hash付きで実行する |
| N08 | targetにも共有517frame replay入口、frame tick/入力・画素checksum、profile0/1比較入口。Nativeで両modeのdrag外frameが全面再描画と一致し、host反復checksumも一致。DMAを48条件へ拡張、invalid count/clip/閾値policyをNative確認 | 自動部分完了。実機のR11/R12採否とDMA閾値は未実測。glyph triage・閾値変更は測定後に採否を決める |
| N09 | [API契約](task-api-contract.md)、[frame/QEMU対応表](context-switch-contract.md)、共有frame定義/静的assertと共通instruction比較、README/所有権/測定/tests文書更新、本表へ進捗集約 | 自動部分完了。手動結果を本表と証拠へ追記し、過去節との暗黙上書きを避ける |

検証結果:

- `make verify`成功。Native 通常・ASan/UBSan とも cooperative/preemptive 各 133 件 PASS。
- QEMU cooperative/preemptive 各 15 件、asm 5 件 PASS。検証 tool fixture 5 件 PASS。
- 両方式・両形式の profile0/1、`SS_DIAG_TASKS=32`＋runtime profile の build 成功。header/ABI/assembly macro/linker/makedisk 変更、flags 変更、連続 2 回 no-op を確認。
- `QEMU=false`を指定した`test-qemu`と`test-asm`はそれぞれ非 0 終了。失敗 log を保持する。
- host 測定 run は`build/native-performance/20260930T141611139916/`。mode ごとに両方式/両 profile の 5run で 517frame の入力・画素 checksum 一致。性能改善率や X68000 の閾値は主張しない。
- build 検証 run は`build/build-check/20260930T141614891882/`。最終の通常/診断公開コピーの hash は同 verification run の`published-artifacts.json`。

手動確認用の通常 4 形式と profile1 は`build/published/`、32worker＋runtime profile 診断 4 形式は`build/published/diagnostic/`に保存した。sandbox 内へ出力先を切り替えて検証したため、`~/tmp`の旧成果物は今回の成果物として扱わない。次はこの hash 付き成果物で N07 を実施し、N06/N08 の実時間と採否を記録する。

### ESC終了遅延への修正（2026-09-30）

- 利用者報告: 両方式の`.x`は通常 UI に問題がないが、ESC から終了まで約 5 秒停止する。報告時点の`~/tmp`の両`.x`は公開成果物と hash 一致。`.xdf`は hash 不一致のため、その起動報告を今回の成果物の検証へ転用しない。
- 調査: 通常 UI でも`SSSceneStats`を渡すため、scene 終了直後に全画面 VRAM を読み、各画素で FNV の 32bit 乗算を行っていた。target 逆アセンブルで`__mulsi3`呼出しを確認。意図した終了待機ではなく、新しい checksum 計測が通常終了にも適用された実装不備。利用者の修正版再確認で遅延解消を確認（下記）。
- 修正: `collect_pixel_checksum`を明示指定する scene bench/host bench だけで画素 checksum を計算する。通常終了および DMA 停止未確認時は走査せず、`pixel_checksum=0`。安価な frame/tick 統計は維持する。
- 回帰検査: 通常 stop 後の VRAM を NULL にして統計収集が読み出さないことを確認。DMA 停止未確認でも checksum 指定を無視すること、mode8/16 の benchmark checksum が引き続き得られることを確認。
- 手動確認結果: 修正版の両`.x`を配置後、利用者から「直りました」と報告。ESC 後の約 5 秒遅延は解消済みとして扱う。終了時間の数値・機種・clock は未記録。この確認は abort/watchdog/DMA 故障経路の検証を兼ねない。
- 修正後の`make verify`成功（`build/verification/20260930T174143735837/manifest.json`）。Native 通常・ASan/UBSan 両方式各 134 件、QEMU 両方式各 15 件、asm5 件、tool fixture5 件、host checksum 反復・build matrix/依存/no-op 確認を通過。検証後の変更は本書への追記のみ。
- 再確認用の両`.x`を`~/tmp/ssos_cop.x`/`ssos_pre.x`へ配置し、`build/published/`との byte/hash 一致を確認。旧版は`build/esc-exit/previous/`、新旧 hash は上記 run の`esc-exit-artifacts.json`。`.xdf`の`~/tmp`コピーは今回更新していない。
