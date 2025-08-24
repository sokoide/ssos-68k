#pragma once

// Error codes now defined in ss_errors.h for consistency
// This file maintained for backward compatibility only
// Include ss_errors.h instead for new code

// Legacy error codes for backward compatibility
#define E_OK (0) // 正常終了
#define E_SYS (-5)     // システムエラー
#define E_RSATR (-11)  // 不正属性
#define E_PAR (-17)    // パラメータ不正
#define E_ID (-18)     // ID不正
#define E_LIMIT (-34)  // 上限値エラー
#define E_OBJ (-41)    // オブジェクト状態エラー
