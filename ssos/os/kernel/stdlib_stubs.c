/**
 * @file stdlib_stubs.c
 * @brief 標準ライブラリ関数のスタブ実装
 *
 * ベアメタル環境で不足している標準ライブラリ関数を簡易実装。
 * 68000 16MHz環境に最適化。
 */

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include "memory.h"
#include <math.h>

/**
 * @brief メモリ領域を指定した値で埋める
 *
 * @param dest 対象メモリ領域
 * @param c 埋め込み値
 * @param n バイト数
 * @return 対象メモリ領域へのポインタ
 */
void* memset(void* dest, int c, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    uint8_t value = (uint8_t)c;

    // 4バイト単位で高速処理
    while (n >= 4) {
        *d++ = value;
        *d++ = value;
        *d++ = value;
        *d++ = value;
        n -= 4;
    }

    // 残りのバイトを処理
    while (n > 0) {
        *d++ = value;
        n--;
    }

    return dest;
}

/**
 * @brief メモリ領域をコピー
 *
 * @param dest コピー先メモリ領域
 * @param src コピー元メモリ領域
 * @param n バイト数
 * @return コピー先メモリ領域へのポインタ
 */
void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    // 4バイト単位で高速処理
    while (n >= 4) {
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        n -= 4;
    }

    // 残りのバイトを処理
    while (n > 0) {
        *d++ = *s++;
        n--;
    }

    return dest;
}

/**
 * @brief メモリ領域を移動（重複対応）
 *
 * @param dest 移動先メモリ領域
 * @param src 移動元メモリ領域
 * @param n バイト数
 * @return 移動先メモリ領域へのポインタ
 */
void* memmove(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    // コピー元とコピー先が重なる場合の処理
    if (d < s) {
        // 前方コピー（重複なし）
        return memcpy(dest, src, n);
    } else if (d > s) {
        // 後方コピー（重複対応）
        d += n;
        s += n;
        while (n > 0) {
            *(--d) = *(--s);
            n--;
        }
    }

    return dest;
}

/**
 * @brief 文字列の長さを取得
 *
 * @param str 文字列
 * @return 文字列の長さ（NULL文字は含まない）
 */
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

/**
 * @brief 文字列をコピー
 *
 * @param dest コピー先文字列
 * @param src コピー元文字列
 * @return コピー先文字列へのポインタ
 */
char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++) != '\0') {
        // コピー実行
    }
    return dest;
}

/**
 * @brief メモリ領域を比較
 *
 * @param s1 比較元メモリ領域
 * @param s2 比較先メモリ領域
 * @param n バイト数
 * @return 0: 等しい, 正: s1 > s2, 負: s1 < s2
 */
int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* p1 = (const uint8_t*)s1;
    const uint8_t* p2 = (const uint8_t*)s2;

    while (n > 0) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
        n--;
    }

    return 0;
}

/**
 * @brief 文字列を比較
 *
 * @param s1 比較元文字列
 * @param s2 比較先文字列
 * @return 0: 等しい, 正: s1 > s2, 負: s1 < s2
 */
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

/**
 * @brief メモリ領域を検索
 *
 * @param haystack 検索対象メモリ領域
 * @param needle 検索する値
 * @param n 検索バイト数
 * @return 見つかった位置、またはNULL
 */
void* memchr(const void* haystack, int needle, size_t n) {
    const uint8_t* h = (const uint8_t*)haystack;
    uint8_t n_val = (uint8_t)needle;

    while (n > 0) {
        if (*h == n_val) {
            return (void*)h;
        }
        h++;
        n--;
    }

    return NULL;
}

/**
 * @brief 文字列内の文字を検索
 *
 * @param s 検索対象文字列
 * @param c 検索する文字
 * @return 見つかった位置、またはNULL
 */
char* strchr(const char* s, int c) {
    char ch = (char)c;
    while (*s != '\0') {
        if (*s == ch) {
            return (char*)s;
        }
        s++;
    }
    return NULL;
}

/**
 * @brief 文字列を複製（簡易版、malloc未使用）
 *
 * @param s 複製元文字列
 * @return 複製された文字列（staticバッファ使用）
 */
char* strdup(const char* s) {
    // 簡易実装: staticバッファを使用
    static char buffer[256];
    size_t len = strlen(s);
    if (len >= sizeof(buffer) - 1) {
        len = sizeof(buffer) - 1;
    }

    memcpy(buffer, s, len);
    buffer[len] = '\0';
    return buffer;
}

/**
 * @brief メモリ領域をゼロクリア
 *
 * @param s 対象メモリ領域
 * @param n バイト数
 * @return 対象メモリ領域へのポインタ
 */
void* bzero(void* s, size_t n) {
    return memset(s, 0, n);
}

/**
 * @brief メモリ領域を比較（サイズ指定）
 *
 * @param s1 比較元メモリ領域
 * @param s2 比較先メモリ領域
 * @param n 比較バイト数
 * @return 0: 等しい, 正: s1 > s2, 負: s1 < s2
 */
int bcmp(const void* s1, const void* s2, size_t n) {
    return memcmp(s1, s2, n);
}

/**
 * @brief メモリ領域をコピー（サイズ指定）
 *
 * @param s1 コピー先メモリ領域
 * @param s2 コピー元メモリ領域
 * @param n コピーバイト数
 * @return コピー先メモリ領域へのポインタ
 */
void* bcopy(const void* s2, void* s1, size_t n) {
    return memmove(s1, s2, n);
}

/**
 * @brief フォーマット文字列をバッファに書き込む
 *
 * @param buffer 出力バッファ
 * @param format フォーマット文字列
 * @param ... 引数
 * @return 書き込んだ文字数
 */
int sprintf(char* buffer, const char* format, ...) {
    va_list va;
    va_start(va, format);
    int result = vsprintf(buffer, format, va);
    va_end(va);
    return result;
}

/**
 * @brief 可変引数版sprintf
 *
 * @param buffer 出力バッファ
 * @param format フォーマット文字列
 * @param va 可変引数リスト
 * @return 書き込んだ文字数
 */
int vsprintf(char* buffer, const char* format, va_list va) {
    // printf.cのsprintf_関数を呼び出す
    return sprintf_(buffer, format, va);
}

/**
 * @brief メモリ領域を動的に確保
 *
 * @param nmemb 要素数
 * @param size 要素サイズ
 * @return 確保されたメモリ領域へのポインタ、失敗時はNULL
 */
void* calloc(size_t nmemb, size_t size) {
    size_t total_size = nmemb * size;
    void* ptr = malloc(total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

/**
 * @brief 動的に確保されたメモリ領域を解放
 *
 * @param ptr 解放するメモリ領域へのポインタ
 */
void free(void* ptr) {
    // 簡易実装: SSOSメモリ管理システムを使用
    if (ptr) {
        // 実際のサイズがわからないため、ダミーサイズで解放
        // 本来はmallocで確保したサイズを記録すべき
        // ここでは警告として実装
        #ifdef DEBUG
        // デバッグ時は警告を表示
        #endif
        // freeは実際には何もしない（簡易実装）
    }
}

/**
 * @brief メモリ領域を動的に確保
 *
 * @param size 確保するサイズ
 * @return 確保されたメモリ領域へのポインタ、失敗時はNULL
 */
void* malloc(size_t size) {
    // SSOSメモリ管理システムを使用
    if (size == 0) {
        return NULL;
    }

    return (void*)ss_mem_alloc((uint32_t)size);
}

/**
 * @brief 整数平方根を計算（68000最適化版）
 *
 * @param x 平方根を求める値
 * @return 平方根の整数部分
 */
uint32_t isqrt(uint32_t x) {
    if (x == 0) return 0;
    if (x < 4) return 1; // x=1,2,3の場合

    // 初期値（ニュートン法の初期値）
    uint32_t result = x;
    uint32_t bit = 1UL << 30; // 最高位ビット

    // 結果が収まるビットを探索
    while (bit > x) bit >>= 2;

    // ニュートン法で平方根を計算
    while (bit) {
        if (result >= bit) {
            result ^= bit; // ビット反転で減算
            result ^= (bit >> 1); // 1ビット右シフト
        } else {
            result ^= (bit >> 1);
        }
        bit >>= 2;
    }

    return result;
}

/**
 * @brief 文字列を指定された長さで比較
 *
 * @param s1 比較元文字列
 * @param s2 比較先文字列
 * @param n 比較する最大文字数
 * @return 0: 等しい, 正: s1 > s2, 負: s1 < s2
 */
int strncmp(const char* s1, const char* s2, size_t n) {
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }

    if (n == 0) {
        return 0;
    }

    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

/**
 * @brief フォーマット文字列をバッファに書き込む（サイズ制限付き）
 *
 * @param buffer 出力バッファ
 * @param size バッファサイズ
 * @param format フォーマット文字列
 * @param ... 引数
 * @return 書き込んだ文字数（バッファサイズを超えた場合は負の値）
 */
int snprintf(char* buffer, size_t size, const char* format, ...) {
    va_list va;
    va_start(va, format);
    int result = vsnprintf(buffer, size, format, va);
    va_end(va);
    return result;
}

/**
 * @brief 可変引数版snprintf
 *
 * @param buffer 出力バッファ
 * @param size バッファサイズ
 * @param format フォーマット文字列
 * @param va 可変引数リスト
 * @return 書き込んだ文字数（バッファサイズを超えた場合は負の値）
 */
int vsnprintf(char* buffer, size_t size, const char* format, va_list va) {
    // 簡易実装: vsprintfと同様にprintf.cのsprintf_関数を使用
    // サイズ制限は実装されていない
    return vsprintf(buffer, format, va);
}

/**
 * @brief 浮動小数点平方根（整数平方根の簡易版）
 *
 * @param x 平方根を求める値
 * @return 平方根の近似値
 */
double sqrt(double x) {
    if (x < 0) return 0.0; // 負数の場合
    if (x == 0) return 0.0;

    // 整数平方根を使って近似値を計算
    uint32_t ix = (uint32_t)x;
    uint32_t root = isqrt(ix);

    // 浮動小数点に変換して微調整
    double result = (double)root;

    // より正確な値が必要な場合は線形補間
    if (ix > 0) {
        uint32_t next_root = root + 1;
        uint32_t root_sq = root * root;
        uint32_t next_sq = next_root * next_root;

        if (next_sq > root_sq) {
            double ratio = (double)(ix - root_sq) / (double)(next_sq - root_sq);
            result += ratio;
        }
    }

    return result;
}