# QuickDrawスタイルグラフィックスシステムへの移行プラン

現在の複雑なレイヤーシステムを68000 16MHzに最適化されたQuickDrawスタイルに段階的に移行します。各フェーズごとに実装・テストが可能で、段階的に機能を追加していきます。

## 🏗️ システム設計

**QuickDrawスタイルの設計コンセプト:**
- **直接描画**: VRAMに即時描画（レイヤーバッファ不要）
- **整数座標系**: 高速な整数演算のみ使用
- **16色パレット**: 固定パレットでカラー管理
- **基本プリミティブ**: 点、線、矩形、塗りつぶし
- **クリッピング**: 矩形ベースのシンプルなクリッピング

```mermaid
graph TB
    A[QuickDraw Graphics] --> B[Color Palette]
    A --> C[Drawing Primitives]
    A --> D[VRAM Direct Access]
    B --> E[16-Color Management]
    C --> F[Point/Line Drawing]
    C --> G[Rectangle Fill]
    C --> H[Pattern Fill]
    D --> I[Fast Pixel Access]
    D --> J[Block Transfer]
```

## 📋 詳細な移行フェーズ

### **フェーズ0: 準備・分析**
**目的**: 現在のシステムを理解し、移行計画を策定
- [ ] 現在のレイヤーシステムの全機能マッピング
- [ ] VRAMアクセスパターンの分析
- [ ] パフォーマンスボトルネックの特定
- [ ] QuickDraw API設計の確定

### **フェーズ1: 基本描画プリミティブの実装**
**目的**: 最小限の描画機能を実装し、動作確認
- [ ] QuickDrawヘッダーファイルの作成 (`quickdraw.h`)
- [ ] 基本的な初期化関数 `qd_init()`
- [ ] ピクセル描画関数 `qd_set_pixel(x, y, color)`
- [ ] 16色カラーパレットの定義
- [ ] テストプログラムで動作確認

**実装する関数:**
```c
void qd_init();
void qd_set_pixel(uint16_t x, uint16_t y, uint8_t color);
uint8_t qd_get_pixel(uint16_t x, uint16_t y);
void qd_clear_screen(uint8_t color);
```

### **フェーズ2: 16色カラーパレットシステムの実装**
**目的**: カラーパレット管理システムを構築
- [ ] パレット構造体の定義
- [ ] パレット操作関数（設定、取得、変更）
- [ ] カラー定数の定義（白、黒、赤、青など）
- [ ] ハードウェアパレット同期
- [ ] テストプログラムでカラー表示確認

**実装する関数:**
```c
void qd_set_palette(uint8_t index, uint16_t rgb);
uint16_t qd_get_palette(uint8_t index);
void qd_set_color(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
```

### **フェーズ3: 矩形描画と塗りつぶし機能の実装**
**目的**: 基本的な図形描画機能を追加
- [ ] 矩形描画関数 `qd_draw_rect()`
- [ ] 矩形塗りつぶし関数 `qd_fill_rect()`
- [ ] 線描画関数 `qd_draw_line()`
- [ ] 基本クリッピングの実装
- [ ] テストプログラムで図形描画確認

**実装する関数:**
```c
void qd_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t color);
void qd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t color);
void qd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color);
void qd_set_clip_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
```

### **フェーズ4: 既存レイヤーシステムとの統合**
**目的**: 新旧システムの統合を図り、段階的な移行を可能に
- [ ] QuickDraw APIのラッパー関数作成
- [ ] 既存コードとの互換性レイヤー実装
- [ ] 段階的な移行パス設計
- [ ] 既存アプリケーションの動作確認
- [ ] 移行ガイドラインの作成

### **フェーズ5: パフォーマンス最適化**
**目的**: 68000 16MHzでの最適パフォーマンスを実現
- [ ] メモリアクセス最適化（ワードアクセス活用）
- [ ] ブロック転送の高速化
- [ ] 描画ルーチンのアセンブラ最適化
- [ ] ベンチマークテストの実施
- [ ] 実機でのパフォーマンス測定

**最適化対象:**
- ピクセルアクセスのワードアライメント
- 矩形塗りつぶしの高速アルゴリズム
- 線描画のブレゼンハムアルゴリズム最適化
- VRAMアクセスパターンの改善

### **フェーズ6: 包括的なテストスイートの実装**
**目的**: 信頼性が高くメンテナブルなシステムの構築
- [ ] ユニットテストスイートの作成
- [ ] 描画精度テスト
- [ ] パフォーマンス回帰テスト
- [ ] 負荷テスト
- [ ] ドキュメント整備

## 🎯 実装ロードマップ

| フェーズ | 期間 | 成果物 | テスト項目 |
|---------|------|--------|-----------|
| 0. 準備 | 1日 | 設計書、分析レポート | 既存システムの動作確認 |
| 1. 基本プリミティブ | 2-3日 | `quickdraw.h`, `quickdraw.c` | ピクセル描画、画面クリア |
| 2. カラーパレット | 1-2日 | パレット管理関数 | 16色表示、色設定 |
| 3. 図形描画 | 3-4日 | 矩形・線描画関数 | 図形精度、クリッピング |
| 4. 統合 | 2-3日 | 互換レイヤー | 既存アプリ動作確認 |
| 5. 最適化 | 3-4日 | 最適化版関数 | ベンチマークテスト |
| 6. テスト | 2-3日 | テストスイート | 全機能テスト、回帰テスト |

## ⚡ 68000 16MHz向け最適化戦略

**メモリアクセス最適化:**
```c
// ワードアクセス活用（68000 16MHzで高速）
static inline void qd_set_pixel_fast(uint16_t x, uint16_t y, uint8_t color) {
    uint32_t offset = (y * SCREEN_WIDTH + x) >> 1; // 2ピクセル/ワード
    uint16_t* vram_word = (uint16_t*)VRAM_BASE + offset;

    if (x & 1) {
        // 奇数ピクセル: 上位4ビット
        *vram_word = (*vram_word & 0xFFF0) | (color & 0x0F);
    } else {
        // 偶数ピクセル: 下位4ビット
        *vram_word = (*vram_word & 0xFF0F) | ((color & 0x0F) << 4);
    }
}
```

**ブロック転送最適化:**
```c
// 矩形塗りつぶしの高速化
void qd_fill_rect_fast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t color) {
    uint16_t fill_word = (color << 4) | color; // 2ピクセル分のデータ

    for (uint16_t dy = 0; dy < h; dy++) {
        uint16_t* dst = (uint16_t*)(VRAM_BASE + ((y + dy) * SCREEN_WIDTH + x) / 2);
        uint16_t words = w / 2;

        for (uint16_t dx = 0; dx < words; dx++) {
            *dst++ = fill_word;
        }

        // 端数の処理（奇数幅の場合）
        if (w & 1) {
            qd_set_pixel(x + w - 1, y + dy, color);
        }
    }
}
```

## 🧪 テスト戦略

各フェーズごとに独立したテストスイートを実装：
- **ユニットテスト**: 個別関数の正確性検証
- **統合テスト**: 複数関数の組み合わせテスト
- **パフォーマンステスト**: 68000 16MHzでの速度測定
- **回帰テスト**: 既存機能の継続動作確認

## 📊 期待される成果

- **メモリ使用量**: 70-90%削減（レイヤーバッファ不要）
- **描画速度**: 2-5倍向上（直接描画による）
- **CPU負荷**: 50-80%低減（管理ロジック簡素化）
- **応答性**: 格段に向上（即時描画）

このプランにより、68000 16MHzでも実用的で高速な16色カラーグラフィックスシステムが実現できます。段階的に実装・テストが可能で、リスクを最小限に抑えながら移行を進められます。