# SSOS ドキュメント索引

初めて読む場合は root の[README](../README.md)で実行環境を確認し、[教育用 OS の設計方針と学習順](educational-os-design.md)へ進む。

## 現行設計と契約

| 文書 | 役割 |
| --- | --- |
| [教育用 OS の設計方針](educational-os-design.md) | 現状評価、目標とする責務・依存、学習順。将来の分割と現行を区別 |
| [Runtime ownership](runtime-ownership.md) | state の所有者、排他、ISR、DMA と終了時の寿命 |
| [Task API](task-api-contract.md) | task/IPC/work の成功・失敗・利用制限 |
| [Context switch](context-switch-contract.md) | TCB/frame の正本と production/QEMU の検証差 |
| [テスト README](../tests/README.md) | 実行方法、Native/QEMU/実機の範囲 |
| [gfx 計測手順](gfx-performance.md) | primitive/scene/DMA/worker 診断と比較条件 |

API/ABI の layout はコードの定義と静的 assert が正本である。設計文書は理由と責務を説明し、定義全体を複製しない。実機結果は対象 source・設定・成果物 hash と結び付ける。

## 改善計画と進捗

- [教育・保守性の改善計画 E01〜E07](educational-os-plan.md): 今後の責務分割、最小学習例、文書と境界の維持。
- [安全性・性能・実機確認の計画 N01〜N09](maintenance-performance-plan-2026-09-30.md): 自動部分の実施結果と、残る故障注入・非同期 IRQ・実時間の確認。

それぞれの状態は対応する計画で更新する。古い監査の未実装記述を現在の TODO と読み替えない。

## 調査・旧実装の履歴

- [2026-09 保守性・性能レビュー](maintenance-performance-review-2026-09-24.md): 指摘の根拠と修正履歴。
- [2026-08 OS 監査](os-audit-2026-08-11.md): 当時の監査結果。
- [ホストテスト拡張の旧計画](host-test-expansion-plan.md): 後続の実装より前の計画記録。
- [旧 UI アーキテクチャノート](legacy-ui-notes.md): 旧補助 README の保存。mode10、旧描画 pipeline 等は現行設計として使わない。

設計/API/ビルドを変更したら、正本と入口の要約を同じ変更で更新する。実装済み、自動検証済み、実機確認待ち、完了、保留を区別し、保留には再開条件を添える。
