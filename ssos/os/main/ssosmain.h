#pragma once

#include <stdbool.h>
#include <stdint.h>

// CLI コマンドプロセッサ
void ss_cli_processor(void);
bool ss_execute_command(const char* command);
void ss_cmd_echo(const char* args);
void ssos_main_cli_output_string(const char* str);
