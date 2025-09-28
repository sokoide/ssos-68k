#include "cli.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../kernel/input.h"
#include "../kernel/ss_config.h"
#include "../util/string.h"

static void ss_cli_output_hex8(int value) {
    ssos_main_cli_output_hex(value & 0xFF);
}

static void ss_cli_output_hex16(int value) {
    ssos_main_cli_output_hex((value >> 8) & 0xFF);
    ssos_main_cli_output_hex(value & 0xFF);
}

static void ss_cli_debug_print_key(int keycode, int ascii) {
    ssos_main_cli_output_string("KEY 0x");
    ss_cli_output_hex16(keycode & 0xFFFF);
    ssos_main_cli_output_string(" ASCII 0x");
    ss_cli_output_hex8(ascii);
    ssos_main_cli_output_string(" ");

    if (ascii >= 0x20 && ascii <= 0x7E) {
        ssos_main_cli_output_char((char)0x27);
        ssos_main_cli_output_char((char)ascii);
        ssos_main_cli_output_char((char)0x27);
    } else if (ascii == 0x1B) {
        ssos_main_cli_output_string("ESC");
    } else {
        switch (ascii) {
        case '\r':
            ssos_main_cli_output_string("\\r");
            break;
        case '\n':
            ssos_main_cli_output_string("\\n");
            break;
        case '\t':
            ssos_main_cli_output_string("\\t");
            break;
        case '\b':
            ssos_main_cli_output_string("\\b");
            break;
        case 0:
            ssos_main_cli_output_string("null");
            break;
        default:
            ssos_main_cli_output_string("0x");
            ss_cli_output_hex8(ascii);
            break;
        }
    }

    ssos_main_cli_output_string("\n");
}

// CLIコマンドプロセッサ
void ss_cli_processor(void) {
    char command[256];
    char prompt[] = "SSOS> ";
    int i;

    while (1) {
        ssos_main_cli_output_string(prompt);

        i = 0;
        while (i < (int)sizeof(command) - 1) {
            int keycode = _iocs_b_keyinp();
            int c = x68k_keycode_to_ascii(keycode);

            ss_cli_debug_print_key(keycode, c);

            if ((keycode & 0xFFFF) == ESC_SCANCODE || c == 0x1B) {
                ssos_main_cli_output_string("\n");
                return;
            }

            if (c == 0) {
                continue;
            }

            if (c == '\r' || c == '\n') {
                command[i] = '\0';
                ssos_main_cli_output_string("\n");
                break;
            }

            if (c == '\b') {
                if (i > 0) {
                    i--;
                    ssos_main_cli_output_string("\b \b");
                }
                continue;
            }

            if (c >= 0x20 && c <= 0x7E) {
                command[i++] = (char)c;
                ssos_main_cli_output_char((char)c);
            }
        }

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
