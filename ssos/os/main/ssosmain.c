#include "ssosmain.h"
#include "kernel.h"
#include "memory.h"
#include "vram.h"
#include "ss_perf.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <x68k/iocs.h>

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
    _iocs_crtmod(16); // 768x512 dots, 16 colors, 1 screen
    _iocs_g_clr_on(); // clear gvram, reset palette, access page 0
    _iocs_b_curoff(); // stop cursor

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

    // プロンプトを表示
    ssos_main_cli_output_string(prompt);

    while (1) {
        // コマンドを入力（簡易的な入力処理）
        // 実際のX68000ではIOCSコールを適切に使用する必要があります
        // ここでは簡易的な実装とします

        // コマンド入力待ち（実際の実装ではキー入力処理が必要）
        // ここではテスト用にechoコマンドを直接実行
        ss_execute_command("echo Hello, SSOS CLI!");

        // dummy
        while (1) ;
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
    if (str == NULL) return;
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
