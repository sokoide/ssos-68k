# gfx パフォーマンス計測手順

この文書は、SSOS のグラフィックス経路を比較するときの実行手順をまとめる。
測定結果の採否と今後の作業計画は [保守性・性能レビュー](maintenance-performance-review-2026-09-24.md) を参照する。値の記録と比較は、実機またはエミュレータで取得した `SSPERF` ログに基づいて行う。

## 対象

- `SS_PROFILE_GFX=1` を有効にした `standalone` ビルド
- 協調スケジューラとプリエンプティブスケジューラの両方
- `standalone/.x` の `-8 -bench 100` 実行

## 事前確認

256 色モードの比較をする前に、実際に選ばれている CRTMOD と表示領域を確認する。

### ソース上の前提

- `-8` は `ssos/standalone/main.c` で `SS_CRTMOD_8` に解釈される
- `ssos/os/gfx/vram.c` の mode table では `SS_CRTMOD_8` が `crtmod=8`、`display_w=512`、`display_h=512`、`color_count=256` で定義されている

### 起動前の確認ポイント

1. 256 色比較なら、起動引数に `-8` を使う
2. 実際の画面モードは、起動後の初期ログか、必要なら `ss_current_mode` の出力を追加して確認する
3. `display_w` / `display_h` が想定と一致しない場合、そのログを比較対象に含めない

### 推奨する確認コマンド

```sh
cd /Users/scott/repo/sokoide/ssos-68k/ssos
make SCHED=cooperative standalone
make SCHED=preemptive standalone
```

必要なら、起動時にモードを出す一時ログを入れてから確認する。
その場合は `crtmod` と `display_w x display_h` を同時に表示する。

## ビルド

### 協調スケジューラ

```sh
cd /Users/scott/repo/sokoide/ssos-68k/ssos
SS_PROFILE_GFX=1 make SCHED=cooperative standalone
```

### プリエンプティブスケジューラ

```sh
cd /Users/scott/repo/sokoide/ssos-68k/ssos
SS_PROFILE_GFX=1 make SCHED=preemptive standalone
```

### 256 色ベンチマーク

```sh
cd /Users/scott/repo/sokoide/ssos-68k/ssos
SS_PROFILE_GFX=1 make SCHED=cooperative standalone
cp ~/tmp/ssos_cop_profile1.x ~/tmp/ssos_cop_8bench.x
```

```sh
cd /Users/scott/repo/sokoide/ssos-68k/ssos
SS_PROFILE_GFX=1 make SCHED=preemptive standalone
cp ~/tmp/ssos_pre_profile1.x ~/tmp/ssos_pre_8bench.x
```

実行時は `-8 -bench 100` を付ける。
`-8` は 256 色モード、`-bench 100` は各決定的フェーズを100回実行する指定である。実行順は `full`、`region`、`z-expose`、`text-update`、`drag-region`、`xor-move`。`drag-region` は固定した2位置の間でhide → 旧領域再合成 → XOR → move/show → 新領域再合成を繰り返すが、共有`scene.c`の通常drag終了処理とは同一ではない。旧active windowの再合成は両経路ともタイトル領域のみになった。`text-update`も固定文字列の直接描画であり、`draw_content_dirty()`の差分検出を測らない。マウスやキーボードの操作は不要で、終了後は通常の復元処理を通る。

## 実行

### 協調スケジューラ

```sh
~/tmp/ssos_cop_profile1.x -8 -bench 100
```

### プリエンプティブスケジューラ

```sh
~/tmp/ssos_pre_profile1.x -8 -bench 100
```

### SSPERF ログの収集

`SSPERF` はベンチ中ではなく、終了処理でCRTMODを復元した後に標準コンソールへ再出力される。さらに同じ内容をHuman68Kのカレントディレクトリに `bench.txt` として保存し、最後に `fclose` する。毎回 `"w"` で開くため、前回の内容は上書きされる。

画面がクリアされても、エミュレータ内で次のように確認できる。

```text
type bench.txt
```

`SSPERF file=bench.txt` が表示されれば、ファイルを開いて閉じる処理まで完了している。`open-failed` の場合は、実行したカレントディレクトリの書き込み可否またはHuman68Kのファイルシステム設定を確認する。

### 通常操作時の runtime ログ

`-bench` なしで起動した場合も、ESC終了時に同じSSPERF形式を `runtime.txt` に保存する。Windowの重なりやドラッグを含む実使用の描画量を測る用途であり、決定的ベンチマークの `bench.txt` とは混在させない。

```text
ssos_cop_profile1.x -8
cp runtime.txt runtime-cop.txt

ssos_pre_profile1.x -8
cp runtime.txt runtime-pre.txt
```

先頭行の `phase=runtime` にある `rounds` は描画frame数、`vsync` は計測区間のVSync数である。比較時は実行時間を揃えるか、`gvram write`、`glyph clip`、`dma attempts` を `rounds` で割って比較する。`SSPERF file=runtime.txt` が表示されれば保存完了である。

DMAエラーが残る場合は、まず `-bench 1` で診断ログを取得する。

```text
ssos_cop_profile1.x -8 -bench 1
cp bench.txt bench-cop-dma-diagnostic.txt
```

`SSPERF dma` 行の `status_samples`、`csr`、`cer` を確認する。`error` はDMAエラーが発生した矩形行数であり、`cer` が原因コードである。`config_samples` と `dcr/ocr/scr/mfc/dfc/bfc` はDMA開始時の設定値を示す。`ok=0` のままではDMA高速化とは判定せず、CPUフォールバックによる高速化と区別する。

```sh
~/tmp/ssos_cop_profile1.x -8 -bench 100 > logs/gfx-cop-8bench.log 2>&1
```

```sh
~/tmp/ssos_pre_profile1.x -8 -bench 100 > logs/gfx-pre-8bench.log 2>&1
```

必要なら `tee` で画面表示と保存を両立する。

```sh
~/tmp/ssos_cop_profile1.x -8 -bench 100 2>&1 | tee logs/gfx-cop-8bench.log
```

```sh
~/tmp/ssos_pre_profile1.x -8 -bench 100 2>&1 | tee logs/gfx-pre-8bench.log
```

## 比較方法

基準は「ベースライン」と「改善版」の 2 系列に分ける。
両系列で、同じ起動条件、同じフレーム数、同じ CRTMOD、同じウィンドウ構成を使う。

### 測定値の扱い

過去のログはcommit、機種またはエミュレータ、clock、画面モード、`SS_PROFILE_GFX`、通常UIか決定的ベンチかを添えて保存する。条件が不明な数値は現行コードの性能値として再掲しない。改善判定には同一条件の`vsync`、DMA成功・失敗・timeout、GVRAM write、rendered windowsを併記する。現在の`SSPERF`に直接のwall-clock `total`/`frame`時間はない。`vsync`は整数カウントなので短い処理では差が量子化される。

### 比較の流れ

1. ベースライン版を `SS_PROFILE_GFX=1` でビルドして `-8 -bench 100` を実行する
2. 改善版を同じ条件で再ビルドして実行する
3. `SSPERF` の同名項目を横並びで比較する
4. 変化が出た項目だけを解釈対象にする

### 比較表の書き方

| 項目 | baseline | improvement | 解釈 |
| :--- | :--- | :--- | :--- |
| phase / rounds |  |  | 同じ処理・反復数か確認 |
| vsync |  |  | フェーズ中の表示同期回数。短い処理では粗い |
| GVRAM read |  |  | VRAM から読んだ量。少ない方が望ましい |
| GVRAM write |  |  | 描画側が計上した語数。DMA失敗時の実バス書込量とは限らない |
| primitive |  |  | 描画APIの呼出・glyph処理数。CPU専用とは限らない |
| DMA attempts / ok / error / timeout / fallback_rows |  |  | 経路と失敗の内訳 |
| windows rendered / render region |  |  | ウィンドウ再描画回数 |
| dirty submitted / clipped |  |  | 要求・clip後の再描画面積 |

## 指標の読み方

### vsync

`vsync` はフェーズ中のV-DISPカウンタ差であり、直接の処理時間ではない。増減だけで良し悪しを決めず、他の描画項目と一緒に見る。

- 増える場合: 描画が VBlank 周辺に寄っている、または待機が増えている可能性
- 減る場合: 同期待ちが減ったか、そもそも描画負荷が下がった可能性

### GVRAM read / write

- `GVRAM read`: 画面から読んでいる量
  - 減るほどよい
  - XOR 枠線や保存復元、オーバーレイ再描画のような経路で増えやすい
- `GVRAM write`: 画面へ書いている量
  - 現行のprofileは経路によって予定面積を先に計上する。DMA失敗やfallback時に実際のバス書込語数を厳密に示す値ではない
  - dirty 更新やクリッピングの改善で減ることがある

### primitive

基本描画関数の呼出数を見る。`primitive`はDMA経由の矩形も数えるため、CPU専用の仕事量ではない。

- `rect`や`hline`の増減は呼出回数の差であり、CPU/DMAの選択は`dma attempts/ok`で判定する

### DMA

矩形塗りつぶしのような大きい書き込みを DMAC に逃がしたかを見る。

- DMA の増減だけでは優劣を決めない。同じ矩形ならCPU/DMAで書き込む画素数は概ね同じで、`GVRAM write`の減少はDMA高速化の必要条件ではない。setup/pollを含めた実時間と`vsync`を比較し、`ok`・`error`・`timeout`・`fallback_rows`を必ず確認する

### window

ウィンドウ全体の再描画やレイアウト更新のコストを見る。

- 増える: move / focus change / full redraw が多い
- 減る: dirty 更新や部分再描画が効いている

### dirty

dirty は「変わった部分だけ更新できたか」を示す。

- `marks`の増減だけでは優劣を決めない。同じ入力列で`submitted`/`clipped`面積と最終画素を比較する

## 2 系列の見方

ベースラインと改善版を比較するときは、次の順で見る。

1. `phase`・`rounds`・機種・画面モード・入力列が一致するか
2. 同じ画素結果で`vsync`または別途取得した実時間が改善したか
3. `GVRAM read/write`、glyph数、`dirty submitted/clipped`、window再描画回数が期待方向か
4. DMAを使う場合、成功率とtimeout・fallbackが悪化していないか

### 典型的な解釈

- `GVRAM read` 減少 + `dirty clipped` 減少:
  - 保存復元や全面再描画を局所化できている可能性が高い
- `DMA ok` 増加 + 同一画素で実時間改善 + timeout なし:
  - 対象矩形ではDMA化が有利だった可能性がある
- `vsync` だけ変化して他が不変:
  - 描画改善ではなく同期条件や測定条件差を疑う

## P3を判定する追加測定

4形式の通常起動がOKでも、以下の性能値は得られない。P3の採否には同じ機種・clock・CRTMOD・入力列で変更前後を比較する。`-bench`は`.x`専用であり、`.xdf`は同じ描画結果の目視・故障確認を別途行う。

| 対象 | 必要な追加測定 | 採用条件 |
| --- | --- | --- |
| 旧activeタイトルの部分再合成 | 共有`scene.c`のdrag begin/move/endを固定入力で再生し、drop時の`dirty submitted/clipped`、`GVRAM write`、画素を比較。既存`drag-region`ベンチだけでは判定しない | 重なり・隠れ・本文更新を含め画素一致、同一入力列で面積と実時間が改善 |
| dirty textの差分末尾 | 数字の増減、桁減り、1文字更新、上位windowによるclipを固定入力で再生し、描画glyph数とGVRAM write・画素を比較。既存`text-update`ベンチは差分検出を通らない | 消去用の空白も含め画素一致、追加の差分探索コストを含む実時間が改善 |
| DMA停止未確認 | ACT解除あり/なしを分けた故障注入で、後者にCPU fallback、次のDMA開始、source/descriptor更新がないことを確認 | 安全性の完了条件。性能評価の前提であり、速度による採否はしない |
| DMA閾値 | 同じ矩形をCPU強制とDMA強制で描く専用ベンチを追加。幅は64の前後を含め、高さも現境界の4/5前後を含む複数値を測る。`-8`/`-16`、両方式で反復し、実時間、`vsync`、DMA `ok/error/timeout/fallback_rows`、画素を記録 | 安定して成功し、setup/poll込みでCPUより速い領域だけDMAを選ぶ。現行の幅`>64`・高さ`>4`は実測前に変更しない |

DMA閾値の比較では、現行の`SSPERF`は`vsync`が整数でwall-clock時間を出さないため、短い矩形を単発で比較しない。十分な反復数か追加の経過時間計測を用意し、実行順を入れ替えて複数回測る。転送成功率と画素一致が満たせない条件は、速く見えても採用しない。機種・clock・媒体・commit・profile設定・矩形の幅/高さ・反復数をログに残す。

2026-09-26時点で旧activeタイトル限定、dirty textの両端差分、およびDMAのACT停止未確認時の描画停止をコードへ反映した。NativeのDMA故障注入はACT解除あり/なしを通すが、DMAC実機の停止保証にはならない。共有sceneのdrag/dirty textは決定的な画素比較テストが未整備であり、性能採否は未了。DMA閾値は実機のCPU強制/DMA強制の同条件ログがないため、幅`>64`・高さ`>4`のまま維持する。

## ログ整理

比較用ログは、以下のように分けて保存する。

- `logs/baseline/cop-8bench.log`
- `logs/baseline/pre-8bench.log`
- `logs/improvement/cop-8bench.log`
- `logs/improvement/pre-8bench.log`

同一のフレーム数、同一の起動引数、同一の CRTMOD で揃えたログだけを比較対象にする。

## 注意

- この文書は手順書であり、性能の優劣や実測値は示さない
- `SSPERF` の出力形式はコード側の実装に依存するため、ログの項目名は実際の出力に合わせて読み替える
- 256 色モードの比較では、`-8` 以外の CRTMOD を混ぜない
