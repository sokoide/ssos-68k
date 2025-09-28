#include "ssosmain.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x68k/iocs.h>

#include "kernel.h"
#include "memory.h"
#include "ss_perf.h"
#include "vram.h"

// 関数の前方宣言
static void ssos_main_cli_output_char(char c);

// IOCSコールの宣言
int _iocs_b_keyinp(void);
int _iocs_b_keysns(void);

// IOCSコールの実装（インラインアセンブリ）
int _iocs_b_keyinp(void) {
    int result;
    register int d0 asm("d0");
    d0 = 0x00;  // _B_KEYINP
    asm volatile("trap #15\n" : "=d"(d0) : "0"(d0));
    result = d0;
    return result;
}

int _iocs_b_keysns(void) {
    int result;
    register int d0 asm("d0");
    d0 = 0x01;  // _B_KEYSNS
    asm volatile("trap #15\n" : "=d"(d0) : "0"(d0));
    result = d0;
    return result;
}

// X68000キーコードをASCIIコードに変換
int x68k_keycode_to_ascii(int keycode) {
    // キーコードの下位8ビットがキー、上位8ビットが修飾キー
    int key = keycode & 0xFF;
    int mod = (keycode >> 8) & 0xFF;

    // 修飾キーが押されている場合は、シフトされた文字を返す
    int shifted = (mod & 0x01) || (mod & 0x02);  // ShiftキーまたはCtrlキー

    // キーコードをASCIIコードに変換
    switch (key) {
    // 数字キー
    case 0x0B:
        return shifted ? '!' : '1';
    case 0x02:
        return shifted ? '"' : '2';
    case 0x03:
        return shifted ? '#' : '3';
    case 0x04:
        return shifted ? '$' : '4';
    case 0x05:
        return shifted ? '%' : '5';
    case 0x06:
        return shifted ? '&' : '6';
    case 0x07:
        return shifted ? '\'' : '7';
    case 0x08:
        return shifted ? '(' : '8';
    case 0x09:
        return shifted ? ')' : '9';
    case 0x0A:
        return shifted ? '*' : '0';

    // アルファベットキー (QWERTY配列)
    case 0x14:
        return shifted ? 'Q' : 'q';
    case 0x1A:
        return shifted ? 'W' : 'w';
    case 0x15:
        return shifted ? 'E' : 'e';
    case 0x17:
        return shifted ? 'R' : 'r';
    case 0x1C:
        return shifted ? 'T' : 't';
    case 0x18:
        return shifted ? 'Y' : 'y';
    case 0x0C:
        return shifted ? 'U' : 'u';
    case 0x12:
        return shifted ? 'I' : 'i';
    case 0x13:
        return shifted ? 'O' : 'o';
    case 0x1F:
        return shifted ? 'P' : 'p';
    case 0x1D:
        return shifted ? 'A' : 'a';
    case 0x1B:
        return shifted ? 'S' : 's';
    case 0x16:
        return shifted ? 'D' : 'd';
    case 0x1E:
        return shifted ? 'F' : 'f';
    case 0x11:
        return shifted ? 'G' : 'g';
    case 0x10:
        return shifted ? 'H' : 'h';
    case 0x19:
        return shifted ? 'J' : 'j';
    case 0x28:
        return shifted ? 'K' : 'k';
    case 0x20:
        return shifted ? 'L' : 'l';
    case 0x21:
        return shifted ? 'Z' : 'z';
    case 0x22:
        return shifted ? 'X' : 'x';
    case 0x23:
        return shifted ? 'C' : 'c';
    case 0x24:
        return shifted ? 'V' : 'v';
    case 0x25:
        return shifted ? 'B' : 'b';
    case 0x26:
        return shifted ? 'N' : 'n';
    case 0x27:
        return shifted ? 'M' : 'm';

    // 特殊キー
    case 0x01:
        return '\r';  // Enter
    case 0x0E:
        return '\b';  // Backspace
    case 0x0F:
        return '\t';  // Tab
    case 0x39:
        return ' ';  // Space

    // 記号キー
    case 0x2A:
        return shifted ? '|' : '\\';
    case 0x2B:
        return shifted ? '@' : '`';
    case 0x2C:
        return shifted ? '{' : '[';
    case 0x2D:
        return shifted ? '}' : ']';
    case 0x2E:
        return shifted ? '+' : ';';
    case 0x2F:
        return shifted ? '*' : ':';
    case 0x30:
        return shifted ? '<' : ',';
    case 0x31:
        return shifted ? '>' : '.';
    case 0x32:
        return shifted ? '?' : '/';

    // デフォルト
    default:
        return 0;  // 不明なキーコード
    }
}

// 簡易的なstrtokの実装
static char* strtok_ptr = NULL;

char* strtok(char* str, const char* delim) {
    if (str != NULL) {
        strtok_ptr = str;
    } else if (strtok_ptr == NULL) {
        return NULL;
    }

    // デリミタをスキップ
    while (*strtok_ptr && strchr(delim, *strtok_ptr)) {
        strtok_ptr++;
    }

    if (*strtok_ptr == '\0') {
        strtok_ptr = NULL;
        return NULL;
    }

    char* token = strtok_ptr;

    // 次のデリミタを探す
    while (*strtok_ptr && !strchr(delim, *strtok_ptr)) {
        strtok_ptr++;
    }

    if (*strtok_ptr) {
        *strtok_ptr = '\0';
        strtok_ptr++;
    } else {
        strtok_ptr = NULL;
    }

    return token;
}

void ssosmain() {
    _iocs_crtmod(16);  // 768x512 dots, 16 colors, 1 screen
    _iocs_g_clr_on();  // clear gvram, reset palette, access page 0
    _iocs_b_curoff();  // stop cursor

    _iocs_ms_init();
    _iocs_skey_mod(0, 0, 0);
    _iocs_ms_curon();

    ss_clear_vram_fast();
    ss_wait_for_clear_vram_completion();

    ss_init_memory_info();
    ss_mem_init();

    // Initialize performance monitoring system
    ss_perf_init();

    // CLIコマンドプロセッサを起動
    ss_cli_processor();
}

// CLIコマンドプロセッサ
void ss_cli_processor(void) {
    char command[256];
    char prompt[] = "SSOS> ";
    int i;

    while (1) {
        // プロンプトを表示
        ssos_main_cli_output_string(prompt);

        // コマンド入力
        i = 0;
        while (i < sizeof(command) - 1) {
            // キー入力待ち
            int keycode = _iocs_b_keyinp();
            // キーコードをASCIIコードに変換
            int c = x68k_keycode_to_ascii(keycode);

            // Enterキーが押されたらコマンドを実行
            if (c == '\r') {
                command[i] = '\0';
                ssos_main_cli_output_string("\n");
                break;
            }

            // Backspaceキーが押されたら1文字削除
            if (c == '\b') {  // Backspace
                if (i > 0) {
                    i--;
                    // カーソルを1文字戻してスペースを出力し、再度カーソルを戻す
                    ssos_main_cli_output_string("\b \b");
                }
                continue;
            }

            // 印字可能文字のみを処理
            if (c >= 0x20 && c <= 0x7E) {
                command[i++] = c;
                // 入力した文字を表示
                ssos_main_cli_output_char(c);
            }
        }

        // コマンドを実行
        if (i > 0) {
            ss_execute_command(command);
        }
    }
}

// コマンド実行
bool ss_execute_command(const char* command) {
    char cmd_copy[256];
    strcpy(cmd_copy, command);

    // コマンドの先頭の空白をスキップ
    char* token = strtok(cmd_copy, " \t");

    if (token == NULL) {
        return false;
    }

    // echoコマンドの処理
    if (strcmp(token, "echo") == 0) {
        char* args = strtok(NULL, "");
        if (args != NULL) {
            // 引数の先頭の空白をスキップ
            while (*args == ' ' || *args == '\t') args++;
            ss_cmd_echo(args);
        } else {
            ss_cmd_echo("");
        }
        return true;
    }

    // 不明なコマンド
    ssos_main_cli_output_string("Unknown command: ");
    ssos_main_cli_output_string(token);
    ssos_main_cli_output_string("\n");
    return false;
}

// X68000用の文字出力関数
static void ssos_main_cli_output_char(char c) {
    _iocs_b_putc(c);
}

// X68000用の文字列出力関数
void ssos_main_cli_output_string(const char* str) {
    if (str == NULL)
        return;
    while (*str) {
        ssos_main_cli_output_char(*str++);
    }
}

// echoコマンドの実装
void ss_cmd_echo(const char* args) {
    if (args && args[0] != '\0') {
        ssos_main_cli_output_string(args);
        ssos_main_cli_output_string("\n");
    } else {
        ssos_main_cli_output_string("\n");
    }
}
