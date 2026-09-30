# SSOS 保守性・性能の追加改善計画

作成日: 2026-09-30 / 静的レビュー対象: `deb0f50`

## 判断と範囲

[2026-09-24のレビュー](maintenance-performance-review-2026-09-24.md)は、初期化・排他・IPC・ビルド・描画の主要な問題を扱い、自動検証と実機検証の境界も明確である。ただし、現在の実施計画としては十分ではない。終了時まで含むDMAの寿命、残るSR固定変更、task公開の競合、UI要求の寿命と処理予算、scheduler/allocatorの時間特性が不足している。追記方式の進捗管理も、未完了項目を見落としやすい。

本書を今後の残作業の入口とする。旧文書のR01〜R14は根拠と実施履歴として保持する。本書は実装指示と完了条件であり、新しい性能測定結果や実機での不具合再現を示すものではない。

今回確認したものは旧計画、現行C/assembly、Makefile、検証スクリプト、テスト定義、[所有権契約](runtime-ownership.md)、[描画測定手順](gfx-performance.md)。本体の変更、テスト実行、クロスビルド、実機操作、性能測定は実施していない。過去のPASS件数は再実行結果として扱わない。作業開始時の未追跡 `.eggs/`、`.eggshell.toml` は本計画の対象外。

MC68000・elf2x68k・両SCHED・`.x`/`.xdf`を対象とする。機種・clock・エミュレータ版は測定時に確定する。共有scheduler/sceneとホスト固有の初期化・終了・DOS入出力の分離を維持する。

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

過去の4形式通常起動報告は旧文書に記録済みだが、後続の`87bd297`/`deb0f50`の成果物に対する確認と同一視しない。今回のコード変更後の実機確認結果は取得していない。

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

P1はコード変更・自動検証・必要な実機検証を分けて記録する。実機を使えない場合も設計と自動検証は進め、状態を「実機確認待ち」とする。

## N01: 復旧処理が外側のSR契約を壊さないようにする

**確認事実:** [standalone/trap14.s](../ssos/standalone/trap14.s)の`ss_init_trap14`と`ss_restore_trap14`はSRを固定値へ変更し、最後に割り込み許可状態へ戻す。[standalone/main.c](../ssos/standalone/main.c)の`ss_abort_cleanup()`は`ss_irq_save()`後に後者を呼ぶので、外側の保存SRを戻す前にmaskが解除される。段階flagがあっても、cleanup全区間を排他的にはできていない。再入時に同じ緊急stack先頭へ切り替えるassemblyも、Cの再入flagだけでは安全性を証明できない。

同ファイルの`wait_vsync()`はspin上限ごとにMFP/vectorを排他なしで再設定し、成功しなくても無期限に繰り返す。[scene.c](../ssos/os/app/scene.c)のbaremetal待機も停止条件がない。これらは旧R10の「手動確認」だけでは完了しない実装課題である。

**作業:** ホスト補助assemblyを含めSRを変える関数の入口/出口契約を列挙し、保存復元型かcaller-held型に統一する。初期化だけが意図的に割り込みを有効化する箇所は別契約にする。cleanupの初回・再入・部分初期化を状態遷移で定義する。watchdogは保存済み状態を上書きせず、有限回の復旧後に`.x`では安全な終了、`.xdf`では定義した停止/診断へ進む。Timer Dも停止する場合は同じtickを脱出の唯一の基準にしない。

**完了条件:** 外側IPLとネストを含め、復元前の早期enableがない。V-DISPのみ停止、Timer Dのみ停止、両方停止、復旧失敗の終端が定義される。初期化途中とcleanup再入の故障注入を通す。IOCS呼出しと緊急stackの扱いはHuman68K環境で別途確認する。

## N02: DMA停止未確認をプロセス終了まで追跡する

**確認事実:** [vram.c](../ssos/os/gfx/vram.c)は停止未確認後、source/descriptor変更と描画を凍結する。しかしsource/descriptorはプロセス内のstatic領域である。`.x`の`run_dma_benchmark()`は停止未確認でreturnした後、呼出し側の`goto cleanup`へ進む。通常sceneのstop条件はESCのみで、`ss_abort_cleanup()`にDMAC停止確認はない。

**推論と境界:** 実際にDMAが継続しているかは未確認だが、ACT停止未確認のまま表示復元・プロセス領域解放へ進めば、描画API内の凍結だけではsource寿命とGVRAM所有権を守れない。旧R13の「停止して原因確認まで再起動しない」という運用説明と、現行cleanup経路の整合を取る必要がある。

**作業:** DMA状態を正常・停止済み失敗・停止未確認に分け、通常終了とabortの両方から参照する。停止未確認はscene/hostへ通知する。停止保証が得られるまでは、source/descriptor解放、画面モード復元、他の描画主体への所有権返却へ進まない方針を設計する。停止不能時の最終動作を定義し、未確認のreset手順を推測で実装しない。既存の`void`描画APIを一括変更する前に、hostが取得するsticky状態で足りるか評価する。

**完了条件:** 正常終了・例外abort・ベンチ終了の各経路で、ACT解除済み/未確認を区別する。Nativeではcleanupへ渡す状態と禁止操作の順序を故障注入し、実機では停止・復元の境界を確認する。速度評価によって安全条件を緩めない。

## N03: 初期化中のtaskをstartできない状態にする

**確認事実:** [scheduler.c](../ssos/os/kernel/scheduler.c)の`ss_task_create()`はslotをDORMANTにし、maskを戻してcanaryを書き込む。`ss_task_start(id)`はDORMANTなら即READYにする。IDは呼出し元に未返却でも1〜32の公開値であり、別taskからのstartを拒否する契約/状態がない。

**条件付き競合:** 別taskが初期化中のIDをstartしてyieldすると、canary書込み完了前にworkerが動き得る。通常UIでこの競合が起きたという意味ではない。現行の「DORMANTなので不可視」という説明は、start APIまで含めた保証として不足する。

**作業:** INITIALIZINGなど予約専用状態を使い、TCB/stack初期化後の短いmask区間でDORMANTへ公開する。状態追加はTCBの既存ABI offsetを変えずに行う。作成失敗時のslot消費とIPC事前送信の契約も維持・照合する。

**完了条件:** 初期化中にstartを試みる決定的な交差テストで拒否し、完了後は一度だけ開始できる。QEMUで実stack上のentry/return/canaryを確認する。canary全体をmask内へ戻す対処は避ける。

## N04: UIへの要求を安全に処理できる契約を仕上げる

**確認事実:** [work_queue.c](../ssos/os/kernel/work_queue.c)は`handler`と`arg`ポインタをコピーし、`ss_work_drain()`は空になるまで実行する。callback自身の再enqueueや継続producerがあればdrainが終了しない経路がある。sceneはdrain後にyieldする。[work_queue.h](../ssos/os/kernel/work_queue.h)と所有権文書はpayload寿命・consumer数・ISR可否・処理上限を定義していない。

**作業:** 単一UI consumer、payloadを保持する主体と解放時点、enqueue失敗時の責任、許可するISRとcallback操作を明記する。IPCは値コピー、workはポインタ保持であることを区別する。drainを開始時件数または件数budgetで区切り、残りを次frameへ回す。callback一件が無制限に動く場合は件数budgetでも時間保証できないため、handlerの実行条件も決める。

main IPC拒否、全task待機時の`SS_ERR_STATE`、終了slot非再利用、unsignedの負エラー表現は意図した制限としてAPI表へ集約する。main IPCやjoin/reuseを追加するのは具体的な利用要件がある場合に限る。

**完了条件:** 要求満杯時に所有権を失わず、短命なstack payloadを誤用しない使用例がある。再enqueue・継続producer下でもframe/yieldが進む。非所有者taskは描画APIを直接呼ばず、要求queue経由で画面を更新する結合テストがある。

## N05: 検証結果と成果物を再現可能にする

**確認事実:** [verify-check.sh](../tools/verify-check.sh)はproduction assemblyの一部をQEMUで「fully cover」として扱うが、QEMUは別の移植assemblyと同期TRAPである。また`ssos/`外を一律バイナリ影響なしとするため、disk生成tool変更を見落とす。Git diff失敗はstderrを捨て、未追跡sourceは通常diffに含まれない。[run_test.sh](../tests/qemu/common/run_test.sh)のper-case出力は終了時に削除される。

**作業:** verify-checkを「自動検証範囲」と「実機確認要否」の別軸にし、production/QEMU差分と非同期競合を理由へ反映する。無効REFは非0、未追跡/削除sourceは安全側判定、disk生成toolは`.xdf`への影響として扱う。判定自体のfixtureを追加する。情報表示コマンドの成功は手動確認完了を意味しない。

buildはclean build、方式/profile切替、header/assembly include/linker/makedisk変更、連続並列buildを確認する。2回目にobject/link/disk生成が不要なことを条件とし、公開先へのコピーは別に扱う。Makefileやcompiler flag変更で既存objectが更新されるかも確認する。共通source一覧は同一であるべき集合とホスト固有集合を明示してから小さなincludeへまとめる。

**完了条件:** 失敗時logがcase・SCHEDごとに保存され、成功件数と対象commit、dirty状態、compiler/QEMU版、設定を記録できる。`.x`/`.xdf`のhashと実機確認記録が対応する。古いobjectに依存せず全構成を生成できる。直近`deb0f50`の差分描画/固定文字列照合も対象に含める。

## N06: 描画以外の時間・メモリ特性を測る

旧計画の5ms tick/50ms強制切替の説明は適切だが、許容する起床遅延・最大mask時間・worker進捗・queue遅延の基準がない。まず基準値を取得し、用途に応じた許容値を決める。計測前にscheduler構造やtimesliceを変更しない。

| 対象 | 負荷と指標 |
| --- | --- |
| IRQ/切替 | task数1/2/8/32、sleepers 0/1/32。最大mask時間、ISR時間、yield/switch時間、実Timer D/V-DISPの遅延 |
| sleep/IPC | 1/5/10 tick待ち、異/同優先度、描画/DMA併走。期限到達→READYとREADY→実行を分け、中央値・上位分位・最大値を記録 |
| allocator | 代表サイズ、枯渇/断片化、2 task外部排他。alloc/free最大時間、mask時間、最大割当て可能サイズ、容量回復、stack high-water |
| UI/要求queue | producer連続投入、queue満杯、長いcallback、drag。frame最悪時間、queue高水位、拒否数、要求到着→適用、worker進捗 |
| VSync待機 | busy wait中の他worker進捗、待機時間、frame欠落。cooperative/preemptiveと優先度を分ける |

**計測条件:** IRQ禁止中の経過時間を、禁止されたTimer D ISRのcounterだけで測らない。独立に進む時間源またはエミュレータtrace等を確認し、その分解能・誤差・計測負荷を記録する。実機clockを超える精度をログから捏造しない。最大値は観測最大であり、未試験入力を含む上限証明とは区別する。

**採否:** 優先度による飢餓は既存policyと実装不具合を分ける。32task上限では、複雑なqueue構造より測定した長いmask区間や無駄な待機を先に改善する。VSyncイベント待機＋idleは測定と全task待ち契約を整えてから別変更にする。

## N07: 実機確認を操作可能なケースにする

| ケース | 対象 | 合否と記録 |
| --- | --- | --- |
| 起動/worker | 両方式・`.x`/`.xdf` | 成果物hashを記録し、通常UIに加え独立workerのstack書込み・sleep・IPC・returnを確認。`.x`のIPC非リンク制限は維持 |
| 非同期切替 | 特にpreemptive両形式 | yield/sleep/IPC/作成を反復し、描画負荷と実Timer Dを重ねる。PC/SR、queue整合、canary、worker進捗を記録 |
| 部分初期化/abort | 両方式`.x` | 保存前後の各段階へ注入。正常ESC、例外、再入でMFP/vector/表示/入力を確認し、復帰後に再起動できることを確認 |
| watchdog | 両方式・両形式 | V-DISPのみ、Timer Dのみ、両方停止、復旧成功/失敗。終端動作と時間、復元後の入力を確認 |
| DMA終了 | 両方式・対応画面モード | 正常/error/timeout/停止未確認と終了/abortの組合せ。停止前の領域解放や画面復元がないことを確認 |

故障注入は復帰できるエミュレータsnapshotから始め、実機適用条件を確認する。非同期切替は暫定条件として各形式30分以上、再起動を含む3回を計画する。時間だけで網羅性を主張せず、操作列、切替回数、発火回数、エミュレータ版/機種/clock、注入位置を残す。過去の「大丈夫そう」という短時間報告をこの完了条件の代用にしない。

## N08: 通常sceneの測定とDMA閾値の範囲を完成させる

[既存手順](gfx-performance.md)の同一入力・画素一致・複数回・CPU/DMA順序入替え・失敗条件除外を維持し、次を追加する。

1. Native専用の入力注入だけでなく、targetでも共有`ss_scene_run()`を決定的入力で駆動する小さな測定入口を作る。通常入力経路との差分を明記する。Timer/Key/Mouseの内容更新も固定し、入力checksum、処理frame数を残す。
2. 非重複・部分重複・全面遮蔽、drag中の本文保留、drop、z再採番、桁減り、画面端clipを含む入力列でR11/R12を一件ずつ比較する。通常runtimeの手操作を揃えただけでは再現性の完了条件にしない。
3. 複数回の経過時間・frame最悪時間・glyph/再合成/予定write量と画素結果を保存する。profile 1で処理量、profile 0または同等の軽量計測で時間を確認し、計測counter自身の負荷を区別する。比較対象commitと改善一件の差分を明記する。
4. DMAの現24条件は幅32/64/65/128×高さ4/5/16であり、幅512や大きい高さの性能を保証しない。幅256/512、無効/範囲外側のpolicy境界、画面端clip、大矩形を追加する。実際にDMAへ渡る寸法と成功全行数を確認する。
5. N02の停止保証後、両方式・`-8`/`-16`で実測する。十分な反復とwarm-up、順序入替え、暫定5回以上の採取でばらつきを記録する。粗いtickの僅差は保留し、エミュレータ測定を実機閾値に転用しない。

**採否:** 同一画素・DMA正常・時間差が測定のばらつきを上回ること。改善対象外のframe最悪時間・worker進捗・code sizeも確認する。glyphのclip外skip/包含fastはこの基準測定後に試す。効果がなければ既存実装を維持する。`.xdf`の時間比較は別のログ出口が必要で、`.x`の数値をそのまま転用しない。

## N09: 仕様・進捗・証拠を一箇所から追えるようにする

各N項目を「未着手 / 実装中 / 自動検証済み / 実機確認待ち / 完了 / 保留」に分け、変更commit、検証commitと設定、証拠の保存先、未確認条件、次の一手を同じ行に管理する。保留には再開条件を付ける。新しい追記が古い結論を暗黙に上書きする方式は止める。

production/QEMUの差分一覧には手動yield frame、例外frame、start/return、register保存、SR/PC復元、MFP/EOI固有部分を含める。TCB offset共有とframe layout共有を区別し、共有できない部分は対応表と逆アセンブル比較で管理する。機械的な巨大assembly共通化を目的にしない。

READMEと`ssos/README_ja.md`、tests README、所有権文書、gfx手順の契約を各変更で照合する。API/ABI変更、build設定、検証上の制限に正本へのリンクを付ける。本書から完了した項目を削除せず、証拠付きで状態を更新する。

## 最初の変更単位

まずN01とN02の終了状態・SR契約を設計し、N03を小さな独立修正として実装する。次にN04/N05を整え、N06の測定入口とN07の故障注入ケースを追加する。実装時は関連Native/sanitizer/QEMU、両形式のクロスbuildを実施し、対象が揃った段階で`make verify`を行う。`.x`/`.xdf`の故障・実時間はN07/N08で別途確認する。

今回の成果物はこの計画書と旧レビューからの導線のみ。改善の実装と計測は未着手である。

## 自動部分の実施結果（2026-09-30）

本節が上の「未着手」を更新する。検証対象は`3b728cc`からの実装作業ツリーで、source hash・diff・成果物hash/size・tool版は`build/verification/20260930T141453015499/manifest.json`に保存した。検証後の本節追記は文書のみ。実装commitは本節を追加したcommitを`git log --follow`で確認できる。

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

- `make verify`成功。Native通常・ASan/UBSanともcooperative/preemptive各133件PASS。
- QEMU cooperative/preemptive各15件、asm 5件PASS。検証tool fixture 5件PASS。
- 両方式・両形式のprofile0/1、`SS_DIAG_TASKS=32`＋runtime profileのbuild成功。header/ABI/assembly macro/linker/makedisk変更、flags変更、連続2回no-opを確認。
- `QEMU=false`を指定した`test-qemu`と`test-asm`はそれぞれ非0終了。失敗logを保持する。
- host測定runは`build/native-performance/20260930T141611139916/`。modeごとに両方式/両profileの5runで517frameの入力・画素checksum一致。性能改善率やX68000の閾値は主張しない。
- build検証runは`build/build-check/20260930T141614891882/`。最終の通常/診断公開コピーのhashは同verification runの`published-artifacts.json`。

手動確認用の通常4形式とprofile1は`build/published/`、32worker＋runtime profile診断4形式は`build/published/diagnostic/`に保存した。sandbox内へ出力先を切り替えて検証したため、`~/tmp`の旧成果物は今回の成果物として扱わない。次はこのhash付き成果物でN07を実施し、N06/N08の実時間と採否を記録する。


### ESC終了遅延への修正（2026-09-30）

- 利用者報告: 両方式の`.x`は通常UIに問題がないが、ESCから終了まで約5秒停止する。報告時点の`~/tmp`の両`.x`は公開成果物とhash一致。`.xdf`はhash不一致のため、その起動報告を今回の成果物の検証へ転用しない。
- 調査: 通常UIでも`SSSceneStats`を渡すため、scene終了直後に全画面VRAMを読み、各画素でFNVの32bit乗算を行っていた。target逆アセンブルで`__mulsi3`呼出しを確認。意図した終了待機ではなく、新しいchecksum計測が通常終了にも適用された実装不備。利用者の修正版再確認で遅延解消を確認（下記）。
- 修正: `collect_pixel_checksum`を明示指定するscene bench/host benchだけで画素checksumを計算する。通常終了およびDMA停止未確認時は走査せず、`pixel_checksum=0`。安価なframe/tick統計は維持する。
- 回帰検査: 通常stop後のVRAMをNULLにして統計収集が読み出さないことを確認。DMA停止未確認でもchecksum指定を無視すること、mode8/16のbenchmark checksumが引き続き得られることを確認。
- 手動確認結果: 修正版の両`.x`を配置後、利用者から「直りました」と報告。ESC後の約5秒遅延は解消済みとして扱う。終了時間の数値・機種・clockは未記録。この確認はabort/watchdog/DMA故障経路の検証を兼ねない。
- 修正後の`make verify`成功（`build/verification/20260930T174143735837/manifest.json`）。Native通常・ASan/UBSan両方式各134件、QEMU両方式各15件、asm5件、tool fixture5件、host checksum反復・build matrix/依存/no-op確認を通過。検証後の変更は本書への追記のみ。
- 再確認用の両`.x`を`~/tmp/ssos_cop.x`/`ssos_pre.x`へ配置し、`build/published/`とのbyte/hash一致を確認。旧版は`build/esc-exit/previous/`、新旧hashは上記runの`esc-exit-artifacts.json`。`.xdf`の`~/tmp`コピーは今回更新していない。
