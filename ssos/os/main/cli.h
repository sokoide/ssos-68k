#pragma once

#include <stdbool.h>

// CLIコマンドプロセッサ
void ss_cli_processor(void);
bool ss_execute_command(const char* command);
void ss_cmd_echo(const char* args);

// 出力関数
void ssos_main_cli_output_string(const char* str);
void ssos_main_cli_output_char(char c);