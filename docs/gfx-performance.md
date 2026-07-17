# gfx パフォーマンス計測手順

この文書は、SSOS のグラフィックス経路を比較するときの実行手順だけをまとめる。
ここでは測定結果そのものは扱わない。値の記録と比較は、実機またはエミュレータで取得した `SSPERF` ログに基づいて行う。

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
make clean
make SCHED=cooperative standalone
make SCHED=preemptive standalone
```

必要なら、起動時にモードを出す一時ログを入れてから確認する。
その場合は `crtmod` と `display_w x display_h` を同時に表示する。

## ビルド

### 協調スケジューラ

```sh
cd /Users/scott/repo/sokoide/ssos-68k/ssos
make clean
SS_PROFILE_GFX=1 make SCHED=cooperative standalone
```

### プリエンプティブスケジューラ

```sh
cd /Users/scott/repo/sokoide/ssos-68k/ssos
make clean
SS_PROFILE_GFX=1 make SCHED=preemptive standalone
```

### 256 色ベンチマーク

```sh
cd /Users/scott/repo/sokoide/ssos-68k/ssos
SS_PROFILE_GFX=1 make SCHED=cooperative standalone
cp ~/tmp/ssos_cop.x ~/tmp/ssos_cop_8bench.x
```

```sh
cd /Users/scott/repo/sokoide/ssos-68k/ssos
SS_PROFILE_GFX=1 make SCHED=preemptive standalone
cp ~/tmp/ssos_pre.x ~/tmp/ssos_pre_8bench.x
```

実行時は `-8 -bench 100` を付ける。
`-8` は 256 色モード、`-bench 100` は各決定的フェーズを100回実行する指定である。実行順は `full`、`region`、`z-expose`、`text-update`、`xor-move`。マウスやキーボードの操作は不要で、終了後は通常の復元処理を通る。

## 実行

### 協調スケジューラ

```sh
~/tmp/ssos_cop.x -8 -bench 100
```

### プリエンプティブスケジューラ

```sh
~/tmp/ssos_pre.x -8 -bench 100
```

### SSPERF ログの収集

`SSPERF` はベンチ中ではなく、終了処理でCRTMODを復元した後に標準コンソールへ再出力される。さらに同じ内容をHuman68Kのカレントディレクトリに `bench.txt` として保存し、最後に `fclose` する。毎回 `"w"` で開くため、前回の内容は上書きされる。

画面がクリアされても、エミュレータ内で次のように確認できる。

```text
type bench.txt
```

`SSPERF file=bench.txt` が表示されれば、ファイルを開いて閉じる処理まで完了している。`open-failed` の場合は、実行したカレントディレクトリの書き込み可否またはHuman68Kのファイルシステム設定を確認する。

```sh
~/tmp/ssos_cop.x -8 -bench 100 > logs/gfx-cop-8bench.log 2>&1
```

```sh
~/tmp/ssos_pre.x -8 -bench 100 > logs/gfx-pre-8bench.log 2>&1
```

必要なら `tee` で画面表示と保存を両立する。

```sh
~/tmp/ssos_cop.x -8 -bench 100 2>&1 | tee logs/gfx-cop-8bench.log
```

```sh
~/tmp/ssos_pre.x -8 -bench 100 2>&1 | tee logs/gfx-pre-8bench.log
```

## 比較方法

基準は「ベースライン」と「改善版」の 2 系列に分ける。
両系列で、同じ起動条件、同じフレーム数、同じ CRTMOD、同じウィンドウ構成を使う。

### 現在の実測結果

`-8`（256色）と、`-8`なし（16色）を同じ `rounds=100` で比較した結果は次の通りである。

| phase | 256色 vsync | 16色 vsync | 16色の差 | GVRAM write |
| :--- | ---: | ---: | ---: | ---: |
| full | 2385 | 2758 | +15.6% | 29,671,600 → 42,778,800（+44.2%） |
| region | 1099 | 1099 | 0% | 2,919,200 → 2,919,200 |
| z-expose | 1782 | 1782 | 0% | 8,497,200 → 8,497,200 |
| text-update | 15 | 15 | 0% | 112,000 → 112,000 |
| xor-move | 26 | 25 | -3.8% | read/writeとも116,352 |

16色モードは `full` で遅く、VRAM書き込みも増えたため、現時点では採用しない。`region` と `z-expose` が同値なのは、今回のテスト経路ではモード差が測定対象の論理書き込み量に現れていないためであり、16色モード全体が同等に高速という意味ではない。

また、cooperative と preemptive の差は全フェーズで約0.2%以下だった。スケジューラ変更は性能改善の対象から外し、描画経路を優先する。

### 更新後の改善計画

優先順位は次の通りとする。

1. DMA timeoutを解消する。旧実装ではDMACのCSRについて `0x10` を完了、`0x02` をエラーとして扱っていたが、X68000 Ch.2では `COC=0x80` が完了、`ERR=0x10` がエラーである。`BFC=0x05`も明示し、timeout時はSABでチャネルを停止してからCPUフォールバックへ進む。DMAが実際に成功するかはエミュレータで再測定する。
2. full redrawの発生を実アプリ側で減らす。背景stippleと全ウィンドウ描画を初回・必要時だけに限定し、通常更新はdirty regionにする。
3. DMAを使わないCPU矩形塗りつぶしを最適化する。ライン単位の連続書き込み、ループ展開、モード別の書き込み単位を測定する。
4. z-exposeの更新範囲を狭める。z順変更で影響を受けるウィンドウだけを再描画し、無関係なウィンドウのrenderを避ける。
5. `skip_occluded` が発生するケースをベンチに追加し、zmap再構築コストと描画削減量を別々に測定する。

改善判定は、同じモード・同じroundsで `vsync` を第一指標とし、DMA timeout、GVRAM write、rendered windowsを併記する。16色モードは、DMA修正後に再測定してもfullで256色を下回らない限り採用しない。

### 比較の流れ

1. ベースライン版を `SS_PROFILE_GFX=1` でビルドして `-8 -bench 100` を実行する
2. 改善版を同じ条件で再ビルドして実行する
3. `SSPERF` の同名項目を横並びで比較する
4. 変化が出た項目だけを解釈対象にする

### 比較表の書き方

| 項目 | baseline | improvement | 解釈 |
| :--- | :--- | :--- | :--- |
| total |  |  | 全体時間。まずここで総量を見る |
| frame |  |  | 1 フレームあたりの負荷 |
| V-DISP |  |  | 表示更新に伴う割り込み回数や待ち時間 |
| GVRAM read |  |  | VRAM から読んだ量。少ない方が望ましい |
| GVRAM write |  |  | VRAM に書いた量。描画量の主要指標 |
| primitive |  |  | CPU 描画プリミティブの使用量 |
| DMA |  |  | 試行・成功・失敗・CPU fallback の内訳 |
| zmap |  |  | オクルージョン判定や可視領域計算のコスト |
| window |  |  | ウィンドウ再描画・再構成のコスト |
| dirty |  |  | dirty 領域だけ更新できているかの指標 |

## 指標の読み方

### V-DISP

V-DISP は画面同期の基準になる。増減だけで良し悪しを決めず、他の描画項目と一緒に見る。

- 増える場合: 描画が VBlank 周辺に寄っている、または待機が増えている可能性
- 減る場合: 同期待ちが減ったか、そもそも描画負荷が下がった可能性

### GVRAM read / write

- `GVRAM read`: 画面から読んでいる量
  - 減るほどよい
  - XOR 枠線や保存復元、オーバーレイ再描画のような経路で増えやすい
- `GVRAM write`: 画面へ書いている量
  - 画面更新の主負荷
  - dirty 更新やクリッピングの改善で減ることがある

### primitive

CPU 側の基本描画関数の使用量を見る。

- `ss_gfx_rect` や `ss_gfx_hline` が増えるなら、直描画依存が強い
- `primitive` が減って `DMA` が増えるなら、矩形系は DMAC に寄っている

### DMA

矩形塗りつぶしのような大きい書き込みを DMAC に逃がしたかを見る。

- DMA の増減だけでは優劣を決めない。setup/poll のコストがあるため、同じフェーズで `V-DISP` または `GVRAM write` が改善した場合だけ採用根拠になる

### zmap

zmap は、どのブロックがどの z 順ウィンドウに覆われているかを判定する補助情報。

- 増える: オクルージョン判定の対象ブロックが広い、または更新回数が多い
- 減る: 再計算範囲が狭い、あるいは dirty 更新で済んでいる

### window

ウィンドウ全体の再描画やレイアウト更新のコストを見る。

- 増える: move / focus change / full redraw が多い
- 減る: dirty 更新や部分再描画が効いている

### dirty

dirty は「変わった部分だけ更新できたか」を示す。

- 増える: 変更箇所が局所的で、部分更新が使えている
- 減る: full redraw が増えている、または dirty 判定が広すぎる

## 2 系列の見方

ベースラインと改善版を比較するときは、次の順で見る。

1. `GVRAM read` が減ったか
2. `GVRAM write` が減ったか
3. `DMA` の試行・成功・fallback が描画量の削減に結び付いたか
4. `dirty` が増えて `window` が減ったか
5. `zmap` が必要以上に増えていないか
6. 最後に `total` と `frame` を確認する

### 典型的な解釈

- `GVRAM read` 減少 + `dirty` 増加:
  - 保存復元や全面再描画を局所化できている可能性が高い
- `DMA ok` 増加 + `GVRAM write` または `V-DISP` 改善:
  - 大きい矩形のDMA化が実際に有利だった可能性が高い
- `zmap` 増加 + `window` 横ばい:
  - オクルージョン計算だけ増えて、描画削減に結びついていない可能性がある
- `V-DISP` だけ変化して他が不変:
  - 描画改善ではなく同期条件や測定条件差を疑う

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
