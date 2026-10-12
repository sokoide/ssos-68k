# 教育用 SSOS の改善計画

作成: 2026-10-13。対象は[設計方針](educational-os-design.md)の実現と教材の保守である。E01 の文書整理に続き E02 の header 分離を実装した。E03〜E05/E07 は未実装で、E06 は確定した include 境界の検査だけ実装した。

## 既存計画との関係

[2026-09-30 の計画](maintenance-performance-plan-2026-09-30.md)は N01〜N09 の安全性・測定・実機確認の正本として維持する。自動検証済みの task 公開、work budget、共通 source 一覧、frame 定義、固定 scene replay を新規課題として再実装しない。実機確認待ちを完了に変更しない。

本計画は教育・責務分割の E 項目を管理する。N の結果は既存計画へ、E の状態は下表へ更新する。各実装後に対象 commit/dirty 状態、検証設定、証拠の保存先、未確認条件を追記する。文書整理・挙動を変えない分割は進められるが、装置挙動や終了契約の変更は N の該当検証を伴わせる。

## 優先順位と依存

| ID | 優先度 | 変更単位 | 依存 | 現在の状態 |
| --- | --- | --- | --- | --- |
| E01 | P0 | 文書の正本・入口と現行説明 | なし | 今回実施。文書索引・設計・計画・読み順、README の起動/検証説明を更新 |
| E02 | P1 | 公開 API / 内部 scheduler / 装置 header の分離 | E01 | 自動検証済み。Native/sanitizer、QEMU、4 形式 build/依存を確認。実機 smoke は未実施 |
| E03 | P1 | host の組立てと通常実行・計測の分離 | E02 | 未着手 |
| E04 | P2 | raster と X68000 graphics backend の分離 | E02、E03 の境界確定 | 未着手 |
| E05 | P1 | 最小の学習例と段階式演習 | E02。CPU/queue の教材化は先行可能 | 未着手 |
| E06 | P2 | 依存と教材例の自動検証 | E02〜E05 で対象を一つずつ追加 | 一部実装。include 境界と公開 header の単独 compile。教材例・文書リンクの常設検査は未着手 |
| E07 | 拡張 | idle とイベント待機の導入判断 | E03、E05、N06/N07 の測定と契約 | 保留。具体的な全 task 待機例が再開条件 |

最初の実装は E02 の header 分離だけに絞る。次に E03 を段階的に実施し、E05 の教材例で公開境界を確かめる。E04 と E06 は境界が安定した部分から進める。一度の変更で API 改訂・ファイル移動・scheduler policy 変更をまとめない。

## E01: 文書の役割を固定する

- root README は目的・起動・構成の入口、`ssos/README_ja.md` は統合ツリーからの入口、`docs/README.md` は設計・契約・計画・履歴の索引とする。
- API は task 契約、所有権は runtime 契約、frame は context 契約、性能の実行手順は gfx 文書へ集約する。コード片や API 全定義の複製を増やさない。
- 今回、旧補助 README を `docs/legacy-ui-notes.md` に履歴として保存した。旧 mode10、旧 data thread、旧 outline、固定 stride の説明を現行仕様として利用しない。

**完了条件:** 入口から現行契約・E 計画・N 計画へ到達でき、旧実装は履歴と識別できる。残る root README の詳細なバグ履歴や assembly 解説の整理は、リンクを保って小さな別変更で行う。

## E02: application が使う境界を小さくする

1. `kernel.h` の error/status と task 定数を用途別 header へ分け、MMIO/linker/起動宣言は対象固有の header へ移す。assembler が利用する定義は CPP 互換を保つ。
2. `scheduler.h` から application 向け task 宣言を切り出す。TCB/ready queue/global/内部入口は内部 header に置き、scheduler・IPC・main 登録・CPU port・内部検証だけが利用する。
3. `gfx.h` の DMAC register layout と MMIO を実装用 header へ移す。公開描画 API と mode の互換はこの段階では保つ。
4. 全 caller の include を照合し、`common-sources.mk` と既存の依存追跡を維持する。古い header は移行期間の include として必要な場合だけ残し、利用者移行後に削除する。

**完了条件:** scene と教材例が内部 TCB/queue、MFP/DMAC register を include しない。TCB offset、frame size、signature、error 表現は不変。Native/sanitizer、QEMU、context 比較、4 形式 build が通る。header/assembly include 変更による再 build を確認する。

## E03: 起動 host と demo・計測を分ける

1. `standalone/main.c` から primitive/DMA bench とログを専用ファイルへ、復元・watchdog を host runtime へ順に分ける。main は options と取得→実行→復元の順を示す。異常終了から呼ばれる symbol と緊急 stack は維持する。
2. `.xdf` の `app/main.c:ss_init` にある subsystem 組立てを host/runtime 側へ移す。scene は demo として選ぶ。共通初期化が必要なら、host が渡す arena/stack pool と利用機能を明示し、linker symbol/DOS を共通側へ持ち込まない。
3. 既存 `SSSceneHooks.read_input` / `wait_vsync` を使って、IOCS 入力と既定 wait を X68000 側へ移す。RAM replay と通常入力が同じ frame body を通ることを維持する。
4. `SS_DIAG_TASKS`、`SS_PROFILE_GFX`、`SS_PROFILE_RUNTIME` を使う。最小 demo のためだけに新しい設定軸を増やさず、通常 UI の既定動作を保つ。`.x` の IPC/work 非リンク制限は維持する。

**完了条件:** 通常入口を読むと所有資源・初期化順・実行入口・失敗時の行先が追える。通常実行へ bench の全画面 checksum が入らない。scene の RAM 画素/replay と runtime_guard の故障テスト、両方式・両形式 build が通る。実 IOCS/abort/watchdog/復元は N01/N07 の hash 付きケースで別途確認する。

## E04: 描画アルゴリズムと装置処理を分ける

`vram.c` を raster/font、mode/CRTC、DMAC fill の責務ごとに段階分割する。小さな内部関数の境界を使い、画素ごとの間接呼出しを追加しない。Native は同じ raster を RAM 上で動かす。DMAC は一つの同期 transfer と停止状態を一つの所有者が管理し、descriptor/source の寿命を分散させない。

**完了条件:** clip/stipple/glyph/XOR と scene の部分更新が従来の画素結果に一致する。DMA 停止未確認時の全描画・mode/flip/cleanup 契約を維持する。分割前後の code size、固定入力 checksum、profile の処理量を比較する。速度改善や DMA 閾値変更は本項目に含めず N08 の実測で判断する。

## E05: 最小の学習例を整備する

[設計方針の読み順](educational-os-design.md#学習順と確かめる問い)に沿って、各題材を「前提・読む範囲・実行コマンド・予測する問い・期待観測・変更演習・検証限界」の同じ形にする。

| 学習例 | 最小構成 | 観測する結果 |
| --- | --- | --- |
| 2 worker の yield | 共有 scheduler と QEMU port。既存 round-robin case を入口にする | 同優先度の実行順、register 保存、entry/return |
| 優先度と sleep | 同/異優先度 worker と tick | yield と待機の違い、期限→READY→実行の違い、peer 不在の error |
| producer/consumer | worker 間 IPC と、別の UI work 例 | 値コピー、queue 満杯、blocking 待ち、永続 payload の再利用時点 |
| allocator | Native arena と buddy/slab | 分割・再結合、OOM、解放後の容量回復 |
| window と damage | RAM framebuffer と既存 replay | drop 後の画素一致、clip、z 再採番、drag 中の本文保留 |

既存 tests を教材入口として再利用し、完成例で production core を複製しない。期待出力は同優先度・固定入力等の決定的条件で記す。異優先度や実非同期 IRQ の順序は固定文字列を保証しない。target 診断は worker slot を消費するため起動し直して実行する。

**完了条件:** 学習者が GUI なしで scheduler/IPC/memory を順に観測でき、失敗例の戻り値と理由を説明できる。各実行可能な完成例は該当 Native/QEMU target に登録される。QEMU の準備がない環境でも Native 題材から始められる。

## E06: 境界と教材の維持を自動化する

確定した include 方針に限って小さな検査を追加する。application の内部 scheduler header 利用、core の IOCS/DOS/MMIO 利用を検出し、host/port/tests の正当な内部利用は明示する。文字列検査を完全な C 依存解析と扱わない。教材完成例の build/run と local Markdown link 検査を既存の test/verify の適切な工程へ組み込む。

**完了条件:** 意図的な境界違反や壊れた教材リンクで非 0 終了する。既存 build matrix/profile/no-op 検証を重複実装しない。失敗 log と対象 source/成果物が対応する。

## E07: idle とイベント待機は契約から設計する

全 task を WAIT にできる例が必要になった段階で、低優先度 idle、割り込みを失わない待機、VSync event の通知・待機を検討する。最後の READY task が sleep/recv したときの結果、lost wakeup、cooperative の進行、停止 tick/watchdog、task exit 後の行先を先に定義する。

**採用条件:** deterministic queue tests と QEMU の frame/SR 検証、実 Timer D/V-DISP の非同期確認を備える。busy wait と比較した worker 進捗・待機時間を N06 で測る。`sleep/recv` の peer 不在 error を黙って変更しない。join/cancel/slot 再利用、user mode/syscall はそれぞれ独立した将来設計とする。

## 次の具体的な作業

次は E03 の最初の変更として、standalone の計測処理を専用ファイルへ分け、通常入口から計測とログの詳細を外す。options、計測結果、cleanup 後のログ出力、scene の hook は現在の契約を維持する。host runtime、`.xdf` 組立て、既定 input/wait の分離は別の変更として進める。

## E02 と E06 の実施記録（2026-10-13）

- 公開 `task.h`、`status.h`、`irq.h`、`clock.h`、内部 `boot.h`、`x68k_mfp.h`、`gfx_hw.h` を作成。全 caller を移行し `kernel.h` を削除した。TCB/state/queue/ABI assert は内部 `scheduler.h`、DMAC/CRTC layout は `gfx_hw.h` に集約した。
- scene は内部 scheduler/起動/装置 header を include しない。関数 signature、TCB/frame/register layout、MMIO 値、task policy、初期化/終了順は維持した。公開 header のみで application が task API を利用できる。
- `make check-includes` を追加し、`make test-tools` / `make verify` から実行する。直接・間接・角括弧 include、core→IOCS、missing include、cycle/comment、host の内部利用、公開 header の独立 compile を fixture で確認する。
- `make verify` 9/9 PASS。Native 通常・ASan/UBSan とも両方式各 134 件、QEMU 両方式各 15 件、asm 5 件、tool fixture 13 件を通過した。公開 header 14 個の独立 compile、include 境界、context instruction 比較、固定 scene の host 測定と checksum 反復も確認した。
- 両方式・両形式の profile0/1 と診断 build、flags/設定切替、新しい `task.h` / `x68k_mfp.h` / `gfx_hw.h` の依存再 build、連続 no-op を確認した。公開コピーは `build/published/` に保存し、`~/tmp` は変更していない。
- 証拠: `build/verification/20261013T111415959759/manifest.json`、各工程 log、source hash/diff、成果物 hash/size と逆アセンブル。対象は HEAD に E02 と前回の文書整理を加えた未コミット作業ツリー。検証後の追加変更は結果記録の文書のみ。
- 静的比較でも C 本体 40 ファイル（include/comment を除く）、TCB 定義、旧 kernel の定数 46 個、DMAC register/descriptor 定義が不変であることを確認した。
- 検証環境: Linux、公式 elf2x68k 20261004（GCC 13.5.0）を `/tmp/ssos-toolchain` に展開。配布版の通常 driver は Human68K の `ld.x` を経由して ELF をアドレス 0 へ再配置するため、そのままの一時 alias では変更前 HEAD でも QEMU round-robin が失敗した。QEMU 用 alias だけ元の ELF specs を選択し、LOAD=0x10000 を確認して全検証を実行した。`.x` は Human68K specs のまま build した。検証用 specs/alias は `/tmp` にあり、リポジトリの compiler 設定は変更していない。
- 最初の verify はベンチの POSIX feature macro より前に追加 include を置いたため compile で失敗した。`_POSIX_C_SOURCE` を先頭に戻した後、上記 run で全工程を再実行した。
- 実機/エミュレータ上の `.x` / `.xdf` smoke は未実施。verify-check は 4 成果物の確認を要求する。非同期 MFP、IOCS/abort/watchdog と実 DMAC の未確認条件は N 計画のまま維持し、今回の自動検証で完了扱いしない。
