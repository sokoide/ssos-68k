#include "cli.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../kernel/input.h"
#include "../util/string.h"

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
            // while (_iocs_b_keysns() == 0) {
            //     // ここで idle thread に渡すとか、hlt とかで待機させる
            // }
            // int keycode = _iocs_b_keyinp();
            // // キーコードをASCIIコードに変換
            // int c = x68k_keycode_to_ascii(keycode);
            int c = _iocs_b_coninp();
            ssos_main_cli_output_hex(c);
            ssos_main_cli_output_string("\n");

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
                // Note: 実際の文字出力はエコーバックしない
                // ユーザーが入力した文字は画面に表示されない
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
void ssos_main_cli_output_char(char c) {
    // 直接IOCSコールを使用
    register int d0 asm("d0");
    d0 = 0x20;  // _B_PUTC
    register int d1 asm("d1");
    d1 = c;
    asm volatile("trap #15" : : "d"(d0), "d"(d1));
}

void ssos_main_cli_output_hex(int val) {
    unsigned char v = (unsigned char)val;
    const char* hex = "0123456789ABCDEF";
    ssos_main_cli_output_char(hex[(v >> 4) & 0x0F]);
    ssos_main_cli_output_char(hex[v & 0x0F]);
}

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