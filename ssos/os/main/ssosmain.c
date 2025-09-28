#include "ssosmain.h"

#include <x68k/iocs.h>

#include "cli.h"
#include "kernel.h"
#include "memory.h"
#include "ss_perf.h"
#include "vram.h"

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

// CLIコマンドプロセッサ（実装は cli.c に移動）
