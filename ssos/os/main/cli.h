#pragma once

#include <stdbool.h>

// CLIコマンドプロセッサ
void ss_cli_processor(void);
bool ss_execute_command(const char* command);
void ss_cmd_echo(const char* args);

// キーボードバッファ関数
void ss_kb_init();
int ss_kb_read();
bool ss_kb_is_empty();

// 出力関数
void ssos_main_cli_output_string(const char* str);
void ssos_main_cli_output_char(char c);
void ssos_main_cli_output_hex(int val);
