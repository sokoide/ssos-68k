#pragma once

#include <stdbool.h>
#include <stdint.h>

// メインエントリーポイント
void ssosmain(void);

// CLI コマンドプロセッサ（実装は cli.c に移動）
void ss_cli_processor(void);
