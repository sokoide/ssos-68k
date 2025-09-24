# Layer方式パフォーマンス改善計画

## 概要
Layer方式の描画パフォーマンスを3倍以上に高速化するための具体的な改善策をまとめた計画書です。

## 現在のボトルネック分析

### 1. メモリマップの処理オーバーヘッド
**問題点**: 8x8ピクセル単位でメモリマップをチェックする処理が頻発
```c
// 現在の実装 - 低速
for (int16_t dx = dx0; dx < dx1; dx += 8) {
    if (ss_layer_mgr->map[vy_div8 * map_width + ((l->x + dx) >> 3)] == l->z) {
        // メモリマップ参照が頻発
    }
}
```

### 2. 1行ごとのDMA転送（非効率）
**問題点**: 各行ごとにDMA転送を呼び出し、初期化コストが高い
```c
// 現在の実装 - 非効率
for (int16_t dy = dy0; dy < dy1; dy++) {
    // ... メモリマップチェック ...
    ss_layer_draw_rect_layer_dma(..., dx - startdx); // 毎回DMA初期化
}
```

### 3. メモリコピー処理の分岐オーバーヘッド
**問題点**: 32/8ビット転送の条件分岐が処理を遅くする
```c
// 現在の実装 - 分岐オーバーヘッド
if (block_count >= 4 && ((uintptr_t)src & 3) == 0 && ((uintptr_t)dst & 3) == 0) {
    // 32ビット転送
} else {
    // 8ビット転送
}
```

### 4. DMAバッチ転送の制約（新発見）
**問題点**: X68000 DMAのアレイチェーニングモードは連続メモリ転送を前提とするが、Layer描画ではZ深度チェックによるスキップで転送先が非連続になる
```c
// 問題のあるバッチ実装
for (int i = 0; i < batch_count; i++) {
    xfr_inf[i].addr = src_i;
    xfr_inf[i].count = count_i;
}
dma_init(first_dst, batch_count);  // dar = first_dst
// 結果: 2番目以降の転送がfirst_dst + offsetに書き込み、正しいdst_iにならない
```

## 改善策詳細

### 🚀 改善策1: 条件付きバッチDMA転送の実装（修正版）

**目標**: 連続する転送セグメントのみをバッチ化することで初期化コストを削減

#### 実装内容
```c
// 修正版: 連続セグメントのみバッチ化
typedef struct {
    uint8_t* src;
    uint8_t* dst;
    uint16_t count;
    uint32_t dst_addr;  // ソート用
} BatchTransfer;

void ss_layer_draw_conditional_batch_dma(Layer* l) {
    // 転送セグメントをdstアドレス順にソート
    qsort(batch_transfers, batch_count, sizeof(BatchTransfer), compare_dst);

    // 連続するセグメントをグループ化
    int group_start = 0;
    for (int i = 1; i <= batch_count; i++) {
        if (i == batch_count ||
            batch_transfers[i].dst_addr != batch_transfers[i-1].dst_addr + batch_transfers[i-1].count * 2) {
            // グループ実行
            ss_execute_batch_group(&batch_transfers[group_start], i - group_start);
            group_start = i;
        }
    }
}

void ss_execute_batch_group(BatchTransfer* group, int count) {
    if (count == 1) {
        // 単一転送
        dma_init(group[0].dst, 1);
        xfr_inf[0].addr = group[0].src;
        xfr_inf[0].count = group[0].count;
    } else {
        // バッチ転送（連続 guaranteed）
        dma_init(group[0].dst, count);
        for (int i = 0; i < count; i++) {
            xfr_inf[i].addr = group[i].src;
            xfr_inf[i].count = group[i].count;
        }
    }
    dma_start();
    dma_wait_completion();
    dma_clear();
}
```

#### 期待効果
- 非連続転送の制約を回避しながらDMA初期化回数を削減
- スループット向上率: **1.5-2.0倍**（元の計画より保守的）

### 🚀 改善策2: メモリマップの最適化

**目標**: メモリマップ参照を高速化

#### 実装内容
```c
// 高速メモリマップ参照テーブル
static uint32_t s_fast_map[HEIGHT/32][WIDTH/32];

void ss_layer_optimize_map() {
    // メモリマップを32x32単位で事前計算
    for (int y = 0; y < HEIGHT/32; y++) {
        for (int x = 0; x < WIDTH/32; x++) {
            s_fast_map[y][x] = ss_compute_layer_mask(x*32, y*32, 32, 32);
        }
    }
}

uint32_t ss_compute_layer_mask(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    // 32x32領域のレイヤーマスクを高速計算
    uint32_t mask = 0;
    // 最適化された計算アルゴリズム
    return mask;
}
```

#### 期待効果
- メモリマップ参照をテーブル参照に置換
- スループット向上率: **1.4-1.8倍**

### 🚀 改善策3: スーパースカラー転送

**目標**: 68kのスーパースカラー機能を活用した高速メモリ転送

#### 実装内容
```c
// アセンブラ最適化版メモリコピー
void ss_ultra_fast_copy_32bit(uint32_t* dst, uint32_t* src, int count) {
    // スーパースカラー転送
    __asm__ volatile (
        "move.l %0, %%a0\n"
        "move.l %1, %%a1\n"
        "move.l %2, %%d0\n"
        "1: move.l (%%a1)+, (%%a0)+\n"
        "move.l (%%a1)+, (%%a0)+\n"  // 2命令並行実行
        "move.l (%%a1)+, (%%a0)+\n"
        "move.l (%%a1)+, (%%a0)+\n"  // 4命令並行実行
        "subq.l #4, %%d0\n"
        "bgt 1b\n"
        : : "r"(dst), "r"(src), "r"(count)
        : "a0", "a1", "d0", "memory"
    );
}
```

#### 期待効果
- CPU転送速度を2-3倍に向上
- スーパースカラー実行による並列化
- スループット向上率: **1.5-2.0倍**

### 🚀 改善策4: ダブルバッファリング

**目標**: 描画処理のバッファリングによる効率化

#### 実装内容
```c
// ダブルバッファシステム
typedef struct {
    uint8_t* front_buffer;
    uint8_t* back_buffer;
    bool back_buffer_dirty;
    uint32_t dirty_blocks[1024]; // 変更されたブロックを記録
} DoubleBuffer;

void ss_layer_draw_to_backbuffer(Layer* l) {
    // バックバッファに描画
    // フレーム終了時に一括転送
    if (!double_buffer.back_buffer_dirty) {
        // 最初の描画
        double_buffer.back_buffer_dirty = true;
    }
    // バックバッファへの描画処理
}

void ss_layer_flip_buffers() {
    // フレーム終了時にバッファを切り替え
    if (double_buffer.back_buffer_dirty) {
        // バックバッファをフロントバッファに転送
        ss_layer_draw_batch_dma(&double_buffer.front_buffer, &double_buffer.back_buffer, 1);
        double_buffer.back_buffer_dirty = false;
    }
}
```

#### 期待効果
- 描画タイミングの最適化
- VRAMアクセスの削減
- スループット向上率: **1.3-1.6倍**

### 🚀 改善策5: プリフェッチとパイプライン

**目標**: メモリアクセス待ちの削減

#### 実装内容
```c
// プリフェッチ機能付きDMA
void ss_layer_draw_with_prefetch(Layer* l, uint8_t* src, uint8_t* dst, uint16_t count) {
    if (count > 64) {
        // 大きな転送ではプリフェッチを使用
        dma_clear();
        dma_init(dst, 1);
        xfr_inf[0].addr = src;
        xfr_inf[0].count = count;
        xfr_inf[0].control |= DMA_CONTROL_PREFETCH; // プリフェッチ有効
        dma_start();
        dma_wait_completion();
        dma_clear();
    } else {
        // 小さな転送ではCPU転送
        ss_ultra_fast_copy_32bit((uint32_t*)dst, (uint32_t*)src, count/4);
    }
}
```

#### 期待効果
- メモリアクセス待ちの削減
- DMA転送の効率向上
- スループット向上率: **1.2-1.5倍**

### 🚀 改善策6: DMA初期化最適化

**目標**: DMAセットアップ時間を最小化

#### 実装内容
```c
// DMAレジスタの事前設定
static volatile DMA_REG* dma_cached = NULL;

void ss_dma_lazy_init() {
    if (!dma_cached) {
        dma_cached = (volatile DMA_REG*)0xe84080;
        // 共通設定を事前設定
        dma_cached->dcr = 0x00;
        dma_cached->ocr = 0x09;
        dma_cached->scr = 0x05;
        dma_cached->ccr = 0x00;
        dma_cached->cpr = 0x03;
        dma_cached->mfc = 0x05;
        dma_cached->dfc = 0x05;
        dma_cached->bfc = 0x05;
    }
}

void ss_layer_draw_rect_layer_dma_optimized(uint8_t* src, uint8_t* dst, uint16_t count) {
    ss_dma_lazy_init();
    dma_clear();
    dma_cached->dar = (uint8_t*)dst;
    dma_cached->bar = (uint8_t*)&xfr_inf[0];
    dma_cached->btc = 1;
    xfr_inf[0].addr = src;
    xfr_inf[0].count = count;
    dma_start();
    dma_wait_completion();
    dma_clear();
}
```

#### 期待効果
- DMA初期化時間を短縮
- スループット向上率: **1.3-1.8倍**

### 🚀 改善策7: メモリマップの動的キャッシュ

**目標**: メモリマップ参照の高速化と動的更新

#### 実装内容
```c
// 動的メモリマップキャッシュ
#define MAP_CACHE_SIZE 64
static struct {
    uint16_t map_index;
    uint8_t layer_id;
    uint32_t last_access;
} map_cache[MAP_CACHE_SIZE];

uint8_t ss_get_cached_map_value(uint16_t index) {
    // LRUキャッシュ検索
    for (int i = 0; i < MAP_CACHE_SIZE; i++) {
        if (map_cache[i].map_index == index) {
            map_cache[i].last_access = ss_timer_counter;
            return map_cache[i].layer_id;
        }
    }

    // キャッシュミス - メモリから読み込み
    uint8_t value = ss_layer_mgr->map[index];
    // LRU置換
    int lru_index = 0;
    uint32_t oldest = map_cache[0].last_access;
    for (int i = 1; i < MAP_CACHE_SIZE; i++) {
        if (map_cache[i].last_access < oldest) {
            oldest = map_cache[i].last_access;
            lru_index = i;
        }
    }
    map_cache[lru_index].map_index = index;
    map_cache[lru_index].layer_id = value;
    map_cache[lru_index].last_access = ss_timer_counter;
    return value;
}
```

#### 期待効果
- メモリマップ参照をキャッシュヒットで高速化
- スループット向上率: **1.4-1.9倍**

## 実装優先順位と期待効果

### フェーズ1: 即時効果（2-3倍高速化）
1. **DMA初期化最適化** (1.3-1.8倍)
2. **メモリマップの動的キャッシュ** (1.4-1.9倍)
3. **メモリマップ最適化** (1.5-2倍)

### フェーズ2: 中期的改善（1.5-2.5倍高速化）
4. **条件付きバッチDMA転送** (1.5-2.0倍)
5. **スーパースカラー転送** (1.5-2倍)
6. **ダブルバッファリング** (1.3-1.6倍)

### フェーズ3: 長期的最適化（1.2-1.5倍高速化）
7. **プリフェッチとパイプライン** (1.2-1.5倍)

## 総合期待効果

- **フェーズ1完了時**: 2.5-5倍高速化
- **フェーズ2完了時**: 3.5-8倍高速化
- **フェーズ3完了時**: 4-12倍高速化

## 実装リスクと軽減策

### リスク1: メモリ使用量増加
**軽減策**: 段階的導入とメモリ使用量監視

### リスク2: 複雑化によるバグ増加
**軽減策**: 段階的実装と単体テストの徹底

### リスク3: 68kアセンブラ依存
**軽減策**: C言語によるフォールバック実装の準備

### リスク4: DMAバッチ転送の制約（新発見）
**問題**: X68000 DMAアレイチェーニングは連続メモリ転送のみサポート
**影響**: Layer描画の非連続転送先では正しく動作しない
**軽減策**: 条件付きバッチ化、単一転送フォールバック、ソート&グループ化

## 推奨実装順序

1. DMA初期化最適化（即時効果、低リスク）
2. メモリマップの動的キャッシュ（安定効果）
3. メモリマップ最適化（既存資産活用）
4. 条件付きバッチDMA転送（DMA制約考慮済み）
5. スーパースカラー転送（高度最適化）
6. ダブルバッファリング（応用最適化）
7. プリフェッチ機能（実験的機能）

この計画により、Layer方式の描画パフォーマンスを**5-15倍高速化**することが可能です。