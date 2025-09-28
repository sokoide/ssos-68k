#pragma once

// Error codes now defined in ss_errors.h for consistency
// This file maintained for backward compatibility only
// Include ss_errors.h instead for new code

// Legacy error codes for backward compatibility
// Only define if not already defined by ss_errors.h
#ifndef E_OK
#define E_OK (0)  // 正常終了
#endif

#ifndef E_SYS
#define E_SYS (-5)  // システムエラー
#endif

#ifndef E_RSATR
#define E_RSATR (-11)  // 不正属性
#endif

#ifndef E_PAR
#define E_PAR (-17)  // パラメータ不正
#endif

#ifndef E_ID
#define E_ID (-18)  // ID不正
#endif

#ifndef E_LIMIT
#define E_LIMIT (-34)  // 上限値エラー
#endif

#ifndef E_OBJ
#define E_OBJ (-41)  // オブジェクト状態エラー
#endif
