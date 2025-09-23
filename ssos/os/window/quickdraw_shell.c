#include "quickdraw_shell.h"

#include "quickdraw.h"
#include "kernel.h"
#include "memory.h"

#include <stdio.h>
#include <string.h>

#define QD_SHELL_TASKBAR_HEIGHT 32
#define QD_SHELL_INFO_LEFT 16
#define QD_SHELL_INFO_TOP 80
#define QD_SHELL_INFO_WIDTH 512
#define QD_SHELL_INFO_HEIGHT 288
#define QD_SHELL_INFO_HEADER_HEIGHT 24
#define QD_SHELL_INFO_CONTENT_MARGIN_X 8
#define QD_SHELL_INFO_CONTENT_TOP_OFFSET 30
#define QD_SHELL_INFO_MAX_BLOCKS 10
#define QD_SHELL_INFO_LINE_CAP 128

typedef struct {
    bool cache_initialized;
    char panel_label[QD_SHELL_INFO_LINE_CAP];
    char timer_vdisp[QD_SHELL_INFO_LINE_CAP];
    char timer_1khz[QD_SHELL_INFO_LINE_CAP];
    char timer_global[QD_SHELL_INFO_LINE_CAP];
    char context_switch[QD_SHELL_INFO_LINE_CAP];
    char registers[QD_SHELL_INFO_LINE_CAP];
    char text_line[QD_SHELL_INFO_LINE_CAP];
    char data_line[QD_SHELL_INFO_LINE_CAP];
    char bss_line[QD_SHELL_INFO_LINE_CAP];
    char ram_line[QD_SHELL_INFO_LINE_CAP];
    char timer_addr_line[QD_SHELL_INFO_LINE_CAP];
    char save_addr_line[QD_SHELL_INFO_LINE_CAP];
    char memory_summary_line[QD_SHELL_INFO_LINE_CAP];
    char memory_blocks[QD_SHELL_INFO_MAX_BLOCKS][QD_SHELL_INFO_LINE_CAP];
} QD_InfoPanelCache;

static QD_InfoPanelCache s_info_panel_cache;
static bool s_info_panel_ready = false;

static bool qd_shell_ready(void) {
    return qd_is_initialized();
}

static void qd_shell_reset_info_cache(void) {
    memset(&s_info_panel_cache, 0, sizeof(s_info_panel_cache));
    s_info_panel_cache.cache_initialized = true;
}

static bool qd_shell_draw_info_line(int16_t left, int16_t top, uint16_t width,
                                    uint16_t line_height, int line_index,
                                    const char* text, char* cache,
                                    size_t cache_size, bool force_refresh) {
    if (!cache || cache_size == 0) {
        return false;
    }

    const char* safe_text = text ? text : "";
    if (!force_refresh && strncmp(cache, safe_text, cache_size) == 0) {
        return false;
    }

    int16_t line_y = (int16_t)(top + (int16_t)(line_index * (int)line_height));
    qd_fill_rect(left, line_y, width, line_height, QD_COLOR_BRIGHT_WHITE);
    if (safe_text[0] != '\0') {
        qd_draw_text(left, line_y, safe_text, QD_COLOR_BLACK,
                     QD_COLOR_BRIGHT_WHITE, true);
    }

    strncpy(cache, safe_text, cache_size - 1);
    cache[cache_size - 1] = '\0';
    return true;
}

static void qd_shell_read_registers(uint32_t* ssp, uint32_t* pc, uint16_t* sr) {
#if defined(__m68k__) && !defined(NATIVE_TEST)
    if (ssp) {
        asm volatile("move.l %%sp, %0" : "=r"(*ssp));
    }
    if (pc) {
        asm volatile("bsr 1f\n\t"
                     "1: move.l (%%sp)+, %0"
                     : "=r"(*pc));
    }
    if (sr) {
        asm volatile("move.w %%sr, %0" : "=d"(*sr));
    }
#else
    if (ssp) {
        *ssp = 0;
    }
    if (pc) {
        *pc = 0;
    }
    if (sr) {
        *sr = 0;
    }
#endif
}

void qd_shell_draw_desktop_background(void) {
    if (!qd_shell_ready()) {
        return;
    }

    qd_clear_screen(QD_COLOR_LTGREEN);

    const int16_t highlight_y = (int16_t)(QD_SCREEN_HEIGHT - QD_SHELL_TASKBAR_HEIGHT - 1);
    qd_fill_rect(0, highlight_y, QD_SCREEN_WIDTH, 1, QD_COLOR_BRIGHT_WHITE);
}

static void draw_left_task_button(int16_t taskbar_top) {
    const int16_t left = 3;
    const uint16_t width = 98;
    const int16_t top_line_y = (int16_t)(taskbar_top + 2);
    const uint16_t vertical_height = 27;

    qd_fill_rect(left, top_line_y, width, 1, QD_COLOR_BRIGHT_WHITE);
    qd_fill_rect(left, (int16_t)(taskbar_top + 3), 1, vertical_height, QD_COLOR_BRIGHT_WHITE);
    qd_fill_rect(left, (int16_t)(taskbar_top + 29), width, 1, QD_COLOR_BLUE);
    qd_fill_rect((int16_t)(left + width - 1), (int16_t)(taskbar_top + 3), 1,
                 (uint16_t)(vertical_height - 1), QD_COLOR_BLUE);
}

static void draw_right_task_button(int16_t taskbar_top) {
    const int16_t left = (int16_t)(QD_SCREEN_WIDTH - 100);
    const int16_t right = (int16_t)(QD_SCREEN_WIDTH - 4);
    const uint16_t width = (uint16_t)(right - left + 1);
    const int16_t top_line_y = (int16_t)(taskbar_top + 2);
    const uint16_t vertical_height = 27;

    qd_fill_rect(left, top_line_y, width, 1, QD_COLOR_BLUE);
    qd_fill_rect(left, (int16_t)(taskbar_top + 3), 1, vertical_height, QD_COLOR_BLUE);
    qd_fill_rect(left, (int16_t)(taskbar_top + 29), width, 1, QD_COLOR_BRIGHT_WHITE);
    qd_fill_rect((int16_t)(left + width - 1), (int16_t)(taskbar_top + 3), 1,
                 (uint16_t)(vertical_height - 1), QD_COLOR_BRIGHT_WHITE);
}

void qd_shell_draw_taskbar(void) {
    if (!qd_shell_ready()) {
        return;
    }

    const int16_t taskbar_top = (int16_t)(QD_SCREEN_HEIGHT - QD_SHELL_TASKBAR_HEIGHT);
    qd_fill_rect(0, taskbar_top, QD_SCREEN_WIDTH, QD_SHELL_TASKBAR_HEIGHT, QD_COLOR_YELLOW);

    draw_left_task_button(taskbar_top);
    draw_right_task_button(taskbar_top);

    qd_draw_text(16, (int16_t)(taskbar_top + 8), "Start", QD_COLOR_BLUE, QD_COLOR_YELLOW, true);
}

void qd_shell_draw_title_bar(void) {
    if (!qd_shell_ready()) {
        return;
    }

#ifdef LOCAL_MODE
    qd_draw_text(0, 0, "Scott & Sandy OS x68k, [ESC] to quit", QD_COLOR_MAGENTA,
                 QD_COLOR_BLACK, true);
#else
    qd_draw_text(0, 0, "Scott & Sandy OS x68k", QD_COLOR_MAGENTA, QD_COLOR_BLACK, true);
#endif
}

void qd_shell_draw_desktop_chrome(void) {
    if (!qd_shell_ready()) {
        return;
    }

    qd_shell_draw_desktop_background();
    qd_shell_draw_taskbar();
    qd_shell_draw_title_bar();
}

void qd_shell_init_info_panel(void) {
    if (!qd_shell_ready()) {
        return;
    }

    qd_shell_reset_info_cache();
    s_info_panel_ready = true;

    qd_fill_rect(QD_SHELL_INFO_LEFT, QD_SHELL_INFO_TOP, QD_SHELL_INFO_WIDTH,
                 QD_SHELL_INFO_HEIGHT, QD_COLOR_BRIGHT_WHITE);
    qd_draw_rect(QD_SHELL_INFO_LEFT, QD_SHELL_INFO_TOP, QD_SHELL_INFO_WIDTH,
                 QD_SHELL_INFO_HEIGHT, QD_COLOR_BLACK);

    if (QD_SHELL_INFO_WIDTH > 2) {
        qd_fill_rect((int16_t)(QD_SHELL_INFO_LEFT + 1),
                     (int16_t)(QD_SHELL_INFO_TOP + 1),
                     (uint16_t)(QD_SHELL_INFO_WIDTH - 2),
                     QD_SHELL_INFO_HEADER_HEIGHT, QD_COLOR_GREEN);
    }

    qd_draw_text((int16_t)(QD_SHELL_INFO_LEFT + QD_SHELL_INFO_CONTENT_MARGIN_X),
                 (int16_t)(QD_SHELL_INFO_TOP + 4), "Every Second: Timer",
                 QD_COLOR_BRIGHT_WHITE, QD_COLOR_GREEN, true);

    if (QD_SHELL_INFO_HEIGHT > QD_SHELL_INFO_HEADER_HEIGHT + 2) {
        qd_fill_rect((int16_t)(QD_SHELL_INFO_LEFT + 1),
                     (int16_t)(QD_SHELL_INFO_TOP + QD_SHELL_INFO_HEADER_HEIGHT + 1),
                     (uint16_t)(QD_SHELL_INFO_WIDTH - 2),
                     (uint16_t)(QD_SHELL_INFO_HEIGHT - QD_SHELL_INFO_HEADER_HEIGHT - 2),
                     QD_COLOR_BRIGHT_WHITE);
    }

    qd_shell_update_info_panel(true);
}

void qd_shell_update_info_panel(bool force_full_refresh) {
    if (!qd_shell_ready() || !s_info_panel_ready) {
        return;
    }

    if (!s_info_panel_cache.cache_initialized) {
        qd_shell_reset_info_cache();
        force_full_refresh = true;
    }

    const uint16_t line_height = qd_get_font_height();
    if (line_height == 0) {
        return;
    }

    const int16_t content_left =
        (int16_t)(QD_SHELL_INFO_LEFT + QD_SHELL_INFO_CONTENT_MARGIN_X);
    const int16_t content_top =
        (int16_t)(QD_SHELL_INFO_TOP + QD_SHELL_INFO_CONTENT_TOP_OFFSET);
    const uint16_t content_width =
        (uint16_t)(QD_SHELL_INFO_WIDTH - 2 * QD_SHELL_INFO_CONTENT_MARGIN_X);

    char buffer[QD_SHELL_INFO_LINE_CAP];
    int line_index = 0;

    snprintf(buffer, sizeof(buffer), "QuickDraw monitor");
    qd_shell_draw_info_line(content_left, content_top, content_width,
                            line_height, line_index++, buffer,
                            s_info_panel_cache.panel_label,
                            sizeof(s_info_panel_cache.panel_label),
                            force_full_refresh);

    snprintf(buffer, sizeof(buffer),
             "A: V-DISP counter: %9lu (vsync count)",
             (unsigned long)ss_timera_counter);
    qd_shell_draw_info_line(content_left, content_top, content_width,
                            line_height, line_index++, buffer,
                            s_info_panel_cache.timer_vdisp,
                            sizeof(s_info_panel_cache.timer_vdisp),
                            force_full_refresh);

    snprintf(buffer, sizeof(buffer),
             "D: 1000Hz timer:   %9lu (every 1ms)",
             (unsigned long)ss_timerd_counter);
    qd_shell_draw_info_line(content_left, content_top, content_width,
                            line_height, line_index++, buffer,
                            s_info_panel_cache.timer_1khz,
                            sizeof(s_info_panel_cache.timer_1khz),
                            force_full_refresh);

    snprintf(buffer, sizeof(buffer),
             "global_counter:    %9lu (every 1ms)",
             (unsigned long)global_counter);
    qd_shell_draw_info_line(content_left, content_top, content_width,
                            line_height, line_index++, buffer,
                            s_info_panel_cache.timer_global,
                            sizeof(s_info_panel_cache.timer_global),
                            force_full_refresh);

    snprintf(buffer, sizeof(buffer),
             "Context Switch:    %9lu (not implemented yet)",
             (unsigned long)ss_context_switch_counter);
    qd_shell_draw_info_line(content_left, content_top, content_width,
                            line_height, line_index++, buffer,
                            s_info_panel_cache.context_switch,
                            sizeof(s_info_panel_cache.context_switch),
                            force_full_refresh);

    uint32_t ssp = 0;
    uint32_t pc = 0;
    uint16_t sr = 0;
    qd_shell_read_registers(&ssp, &pc, &sr);
    snprintf(buffer, sizeof(buffer), "ssp: 0x%08lX, pc: 0x%08lX, sr: 0x%04X",
             (unsigned long)ssp, (unsigned long)pc, (unsigned int)sr);
    qd_shell_draw_info_line(content_left, content_top, content_width,
                            line_height, line_index++, buffer,
                            s_info_panel_cache.registers,
                            sizeof(s_info_panel_cache.registers),
                            force_full_refresh);

    void* base = NULL;
    uint32_t size = 0;
    ss_get_text(&base, &size);
    unsigned long text_start = (unsigned long)(uintptr_t)base;
    unsigned long text_end = text_start + (size > 0 ? (unsigned long)size - 1UL : 0UL);
    snprintf(buffer, sizeof(buffer),
             ".text   addr: 0x%08lX-0x%08lX, size: %u", text_start, text_end,
             (unsigned int)size);
    qd_shell_draw_info_line(content_left, content_top, content_width,
                            line_height, line_index++, buffer,
                            s_info_panel_cache.text_line,
                            sizeof(s_info_panel_cache.text_line),
                            force_full_refresh);

    ss_get_data(&base, &size);
    unsigned long data_start = (unsigned long)(uintptr_t)base;
    unsigned long data_end = data_start + (size > 0 ? (unsigned long)size - 1UL : 0UL);
    snprintf(buffer, sizeof(buffer),
             ".data   addr: 0x%08lX-0x%08lX, size: %u", data_start, data_end,
             (unsigned int)size);
    qd_shell_draw_info_line(content_left, content_top, content_width,
                            line_height, line_index++, buffer,
                            s_info_panel_cache.data_line,
                            sizeof(s_info_panel_cache.data_line),
                            force_full_refresh);

    ss_get_bss(&base, &size);
    unsigned long bss_start = (unsigned long)(uintptr_t)base;
    unsigned long bss_end = bss_start + (size > 0 ? (unsigned long)size - 1UL : 0UL);
    snprintf(buffer, sizeof(buffer),
             ".bss    addr: 0x%08lX-0x%08lX, size: %u", bss_start, bss_end,
             (unsigned int)size);
    qd_shell_draw_info_line(content_left, content_top, content_width,
                            line_height, line_index++, buffer,
                            s_info_panel_cache.bss_line,
                            sizeof(s_info_panel_cache.bss_line),
                            force_full_refresh);

    unsigned long ram_start =
        (unsigned long)(uintptr_t)ss_ssos_memory_base;
    unsigned long ram_end =
        ram_start + (ss_ssos_memory_size > 0 ?
                        (unsigned long)ss_ssos_memory_size - 1UL : 0UL);
    snprintf(buffer, sizeof(buffer),
             "RAM     addr: 0x%08lX-0x%08lX, size: %u", ram_start, ram_end,
             (unsigned int)ss_ssos_memory_size);
    qd_shell_draw_info_line(content_left, content_top, content_width,
                            line_height, line_index++, buffer,
                            s_info_panel_cache.ram_line,
                            sizeof(s_info_panel_cache.ram_line),
                            force_full_refresh);

    snprintf(buffer, sizeof(buffer), "ss_timer_counter_base addr: 0x%p",
             (void*)&ss_timera_counter);
    qd_shell_draw_info_line(content_left, content_top, content_width,
                            line_height, line_index++, buffer,
                            s_info_panel_cache.timer_addr_line,
                            sizeof(s_info_panel_cache.timer_addr_line),
                            force_full_refresh);

    snprintf(buffer, sizeof(buffer), "ss_save_data_base addr: 0x%p",
             (void*)&ss_save_data_base);
    qd_shell_draw_info_line(content_left, content_top, content_width,
                            line_height, line_index++, buffer,
                            s_info_panel_cache.save_addr_line,
                            sizeof(s_info_panel_cache.save_addr_line),
                            force_full_refresh);

    uint32_t total_bytes = ss_mem_total_bytes();
    uint32_t free_bytes = ss_mem_free_bytes();
    snprintf(buffer, sizeof(buffer), "memory total: %u, free: %u", total_bytes,
             free_bytes);
    qd_shell_draw_info_line(content_left, content_top, content_width,
                            line_height, line_index++, buffer,
                            s_info_panel_cache.memory_summary_line,
                            sizeof(s_info_panel_cache.memory_summary_line),
                            force_full_refresh);

    int max_blocks = ss_mem_mgr.num_free_blocks;
    if (max_blocks > QD_SHELL_INFO_MAX_BLOCKS) {
        max_blocks = QD_SHELL_INFO_MAX_BLOCKS;
    }

    for (int i = 0; i < max_blocks; ++i) {
        const SsMemFreeBlock* block = &ss_mem_mgr.free_blocks[i];
        snprintf(buffer, sizeof(buffer),
                 "memory mgr: block: %d, addr: 0x%08X, sz:%u", i, block->addr,
                 block->sz);
        qd_shell_draw_info_line(content_left, content_top, content_width,
                                line_height, line_index++, buffer,
                                s_info_panel_cache.memory_blocks[i],
                                sizeof(s_info_panel_cache.memory_blocks[i]),
                                force_full_refresh);
    }

    for (int i = max_blocks; i < QD_SHELL_INFO_MAX_BLOCKS; ++i) {
        qd_shell_draw_info_line(content_left, content_top, content_width,
                                line_height, line_index++, "",
                                s_info_panel_cache.memory_blocks[i],
                                sizeof(s_info_panel_cache.memory_blocks[i]),
                                force_full_refresh);
    }
}
