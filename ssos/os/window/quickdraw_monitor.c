#include "quickdraw_monitor.h"

#include "quickdraw.h"
#include "memory.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define QD_MONITOR_MAX_LINES       32
#define QD_MONITOR_TEXT_CAPACITY   256
#define QD_MONITOR_MAX_BLOCK_LINES 10

#define QD_MONITOR_HEADER_COLOR      QD_COLOR_BLUE
#define QD_MONITOR_HEADER_TEXT_COLOR QD_COLOR_BRIGHT_WHITE
#define QD_MONITOR_TEXT_COLOR        QD_COLOR_BLACK
#define QD_MONITOR_BORDER_COLOR      QD_COLOR_BLACK

typedef struct {
    char cached_text[QD_MONITOR_TEXT_CAPACITY];
} QD_MonitorLineCache;

typedef struct {
    bool initialized;
    QD_Rect bounds;
    int16_t text_left;
    int16_t text_top;
    uint16_t text_width;
    uint16_t line_height;
    uint16_t prev_line_count;
    QD_MonitorLineCache lines[QD_MONITOR_MAX_LINES];
} QD_MonitorPanelState;

static QD_MonitorPanelState s_monitor_panel = {
    .initialized = false,
    .bounds = {QD_MONITOR_PANEL_LEFT, QD_MONITOR_PANEL_TOP, QD_MONITOR_PANEL_WIDTH, QD_MONITOR_PANEL_HEIGHT},
};

extern const volatile uint32_t ss_timera_counter;
extern const volatile uint32_t ss_timerd_counter;
extern const volatile uint32_t ss_context_switch_counter;
extern const volatile uint32_t ss_save_data_base;
extern uint32_t global_counter;

static bool qd_monitor_panel_ready(void) {
    return s_monitor_panel.initialized && qd_is_initialized();
}

static inline uint16_t qd_monitor_line_height(void) {
    uint16_t height = qd_get_font_height();
    if (height == 0) {
        height = 16;
    }
    return height;
}

#ifdef NATIVE_TEST
static uint32_t qd_monitor_read_ssp(void) {
    return 0;
}

static uint32_t qd_monitor_read_pc(void) {
    return 0;
}

static uint16_t qd_monitor_read_sr(void) {
    return 0;
}
#else
static uint32_t qd_monitor_read_ssp(void) {
    uint32_t value = 0;
    asm volatile("move.l %%sp, %0" : "=r"(value));
    return value;
}

static uint32_t qd_monitor_read_pc(void) {
    uint32_t value = 0;
    asm volatile("bsr 1f\n\t"
                 "1: move.l (%%sp)+, %0"
                 : "=r"(value));
    return value;
}

static uint16_t qd_monitor_read_sr(void) {
    uint16_t value = 0;
    asm volatile("move.w %%sr, %0" : "=d"(value));
    return value;
}
#endif

static bool qd_monitor_text_equals(const char* lhs, const char* rhs) {
    if (!lhs || !rhs) {
        return false;
    }
    size_t lhs_len = strlen(lhs);
    size_t rhs_len = strlen(rhs);
    if (lhs_len != rhs_len) {
        return false;
    }
    if (lhs_len >= QD_MONITOR_TEXT_CAPACITY) {
        lhs_len = QD_MONITOR_TEXT_CAPACITY - 1;
    }
    return strncmp(lhs, rhs, lhs_len + 1) == 0;
}

static int16_t qd_monitor_line_y(uint16_t index) {
    return (int16_t)(s_monitor_panel.text_top + (int16_t)index * (int16_t)s_monitor_panel.line_height);
}

static void qd_monitor_clear_line(uint16_t index) {
    if (index >= QD_MONITOR_MAX_LINES) {
        return;
    }

    int16_t line_y = qd_monitor_line_y(index);
    qd_fill_rect(s_monitor_panel.text_left, line_y, s_monitor_panel.text_width,
                 s_monitor_panel.line_height, QD_MONITOR_BODY_COLOR);
    s_monitor_panel.lines[index].cached_text[0] = '\0';
}

static bool qd_monitor_draw_line(uint16_t index, const char* text) {
    if (index >= QD_MONITOR_MAX_LINES) {
        return false;
    }

    if (!text) {
        text = "";
    }

    QD_MonitorLineCache* cache = &s_monitor_panel.lines[index];
    if (cache->cached_text[0] != '\0' && qd_monitor_text_equals(cache->cached_text, text)) {
        return false;
    }

    int16_t line_y = qd_monitor_line_y(index);
    qd_fill_rect(s_monitor_panel.text_left, line_y, s_monitor_panel.text_width,
                 s_monitor_panel.line_height, QD_MONITOR_BODY_COLOR);

    snprintf(cache->cached_text, sizeof(cache->cached_text), "%s", text);

    if (cache->cached_text[0] != '\0') {
        qd_draw_text(s_monitor_panel.text_left, line_y, cache->cached_text,
                     QD_MONITOR_TEXT_COLOR, QD_MONITOR_BODY_COLOR, true);
    }

    return true;
}

void qd_monitor_panel_init(void) {
    if (!qd_is_initialized()) {
        return;
    }

    s_monitor_panel.bounds.x = QD_MONITOR_PANEL_LEFT;
    s_monitor_panel.bounds.y = QD_MONITOR_PANEL_TOP;
    s_monitor_panel.bounds.width = QD_MONITOR_PANEL_WIDTH;
    s_monitor_panel.bounds.height = QD_MONITOR_PANEL_HEIGHT;
    s_monitor_panel.text_left = (int16_t)(s_monitor_panel.bounds.x + QD_MONITOR_PANEL_TEXT_PADDING_X);
    s_monitor_panel.line_height = qd_monitor_line_height();
    s_monitor_panel.text_top = (int16_t)(s_monitor_panel.bounds.y + QD_MONITOR_PANEL_HEADER_HEIGHT + QD_MONITOR_PANEL_TEXT_TOP_OFFSET);
    s_monitor_panel.text_width = (uint16_t)(s_monitor_panel.bounds.width - (QD_MONITOR_PANEL_TEXT_PADDING_X * 2));
    s_monitor_panel.prev_line_count = 0;

    for (uint16_t i = 0; i < QD_MONITOR_MAX_LINES; i++) {
        s_monitor_panel.lines[i].cached_text[0] = '\0';
    }

    const QD_Rect* previous_clip = qd_get_clip_rect();
    QD_Rect saved_clip = *previous_clip;

    qd_set_clip_rect(s_monitor_panel.bounds.x, s_monitor_panel.bounds.y,
                     s_monitor_panel.bounds.width, s_monitor_panel.bounds.height);

#ifdef TESTING
    printf("DEBUG: About to fill rect at (%d,%d) size %dx%d with color %d\n",
           s_monitor_panel.bounds.x, s_monitor_panel.bounds.y,
           s_monitor_panel.bounds.width, s_monitor_panel.bounds.height,
           QD_MONITOR_BODY_COLOR);
#endif
    qd_fill_rect(s_monitor_panel.bounds.x, s_monitor_panel.bounds.y,
                  s_monitor_panel.bounds.width, s_monitor_panel.bounds.height,
                  QD_MONITOR_BODY_COLOR);
#ifdef TESTING
    printf("DEBUG: Fill rect completed\n");
#endif
    qd_fill_rect(s_monitor_panel.bounds.x, s_monitor_panel.bounds.y,
                 s_monitor_panel.bounds.width, QD_MONITOR_PANEL_HEADER_HEIGHT,
                 QD_MONITOR_HEADER_COLOR);
    qd_draw_rect(s_monitor_panel.bounds.x, s_monitor_panel.bounds.y,
                 s_monitor_panel.bounds.width, s_monitor_panel.bounds.height,
                 QD_MONITOR_BORDER_COLOR);

    qd_draw_text(s_monitor_panel.text_left,
                 (int16_t)(s_monitor_panel.bounds.y + 4),
                 "Every Second: Timer", QD_MONITOR_HEADER_TEXT_COLOR,
                 QD_MONITOR_HEADER_COLOR, true);

    qd_set_clip_rect(saved_clip.x, saved_clip.y, saved_clip.width, saved_clip.height);

    s_monitor_panel.initialized = true;
}

static uintptr_t qd_monitor_section_end(uintptr_t base, uint32_t size) {
    return (size > 0) ? (base + (uintptr_t)size - 1) : base;
}

bool qd_monitor_panel_tick(void) {
    if (!qd_monitor_panel_ready()) {
        return false;
    }

    bool any_updates = false;
    uint16_t line_index = 0;
    char buffer[QD_MONITOR_TEXT_CAPACITY];

    const QD_Rect* previous_clip = qd_get_clip_rect();
    QD_Rect saved_clip = *previous_clip;

    qd_set_clip_rect(s_monitor_panel.bounds.x, s_monitor_panel.bounds.y,
                     s_monitor_panel.bounds.width, s_monitor_panel.bounds.height);

    if (line_index < QD_MONITOR_MAX_LINES) {
        any_updates |= qd_monitor_draw_line(line_index++, "layer id: QuickDraw");
    }

    uint32_t timera = ss_timera_counter;
    uint32_t timerd = ss_timerd_counter;
    uint32_t global = global_counter;
    uint32_t context_switches = ss_context_switch_counter;

    if (line_index < QD_MONITOR_MAX_LINES) {
        snprintf(buffer, sizeof(buffer), "A: V-DISP counter: %9lu (vsync count)",
                 (unsigned long)timera);
        any_updates |= qd_monitor_draw_line(line_index++, buffer);
    }

    if (line_index < QD_MONITOR_MAX_LINES) {
        snprintf(buffer, sizeof(buffer), "D: 1000Hz timer:   %9lu (every 1ms)",
                 (unsigned long)timerd);
        any_updates |= qd_monitor_draw_line(line_index++, buffer);
    }

    if (line_index < QD_MONITOR_MAX_LINES) {
        snprintf(buffer, sizeof(buffer), "global_counter:    %9lu (every 1ms)",
                 (unsigned long)global);
        any_updates |= qd_monitor_draw_line(line_index++, buffer);
    }

    if (line_index < QD_MONITOR_MAX_LINES) {
        snprintf(buffer, sizeof(buffer), "Context Switch:    %9lu (not implemented yet)",
                 (unsigned long)context_switches);
        any_updates |= qd_monitor_draw_line(line_index++, buffer);
    }

    uint32_t ssp = qd_monitor_read_ssp();
    uint32_t pc = qd_monitor_read_pc();
    uint16_t sr = qd_monitor_read_sr();

    if (line_index < QD_MONITOR_MAX_LINES) {
        snprintf(buffer, sizeof(buffer), "ssp: 0x%08lx, pc: 0x%08lx, sr: 0x%04x",
                 (unsigned long)ssp, (unsigned long)pc, (unsigned)sr);
        any_updates |= qd_monitor_draw_line(line_index++, buffer);
    }

    void* section_base = NULL;
    uint32_t section_size = 0;
    uintptr_t section_end = 0;

    ss_get_text(&section_base, &section_size);
    section_end = qd_monitor_section_end((uintptr_t)section_base, section_size);
    if (line_index < QD_MONITOR_MAX_LINES) {
        snprintf(buffer, sizeof(buffer), ".text   addr: 0x%08lx-0x%08lx, size: %lu",
                 (unsigned long)(uintptr_t)section_base,
                 (unsigned long)section_end,
                 (unsigned long)section_size);
        any_updates |= qd_monitor_draw_line(line_index++, buffer);
    }

    ss_get_data(&section_base, &section_size);
    section_end = qd_monitor_section_end((uintptr_t)section_base, section_size);
    if (line_index < QD_MONITOR_MAX_LINES) {
        snprintf(buffer, sizeof(buffer), ".data   addr: 0x%08lx-0x%08lx, size: %lu",
                 (unsigned long)(uintptr_t)section_base,
                 (unsigned long)section_end,
                 (unsigned long)section_size);
        any_updates |= qd_monitor_draw_line(line_index++, buffer);
    }

    ss_get_bss(&section_base, &section_size);
    section_end = qd_monitor_section_end((uintptr_t)section_base, section_size);
    if (line_index < QD_MONITOR_MAX_LINES) {
        snprintf(buffer, sizeof(buffer), ".bss    addr: 0x%08lx-0x%08lx, size: %lu",
                 (unsigned long)(uintptr_t)section_base,
                 (unsigned long)section_end,
                 (unsigned long)section_size);
        any_updates |= qd_monitor_draw_line(line_index++, buffer);
    }

#ifdef TESTING
    uintptr_t ram_base = (uintptr_t)ss_ssos_memory_base;
    uintptr_t ram_end = qd_monitor_section_end(ram_base, ss_ssos_memory_size);
    if (line_index < QD_MONITOR_MAX_LINES) {
        snprintf(buffer, sizeof(buffer), "RAM     addr: 0x%08lx-0x%08lx, size: %lu",
                  (unsigned long)ram_base, (unsigned long)ram_end,
                  (unsigned long)ss_ssos_memory_size);
        any_updates |= qd_monitor_draw_line(line_index++, buffer);
    }
#endif

    if (line_index < QD_MONITOR_MAX_LINES) {
        snprintf(buffer, sizeof(buffer), "ss_timer_counter_base addr: 0x%p",
                 (const void*)&ss_timera_counter);
        any_updates |= qd_monitor_draw_line(line_index++, buffer);
    }

    if (line_index < QD_MONITOR_MAX_LINES) {
        snprintf(buffer, sizeof(buffer), "ss_save_data_base addr: 0x%p",
                 (const void*)&ss_save_data_base);
        any_updates |= qd_monitor_draw_line(line_index++, buffer);
    }

    if (line_index < QD_MONITOR_MAX_LINES) {
        snprintf(buffer, sizeof(buffer), "memory total: %lu, free: %lu",
                 (unsigned long)ss_mem_total_bytes(),
                 (unsigned long)ss_mem_free_bytes());
        any_updates |= qd_monitor_draw_line(line_index++, buffer);
    }

    uint16_t remaining_lines = (line_index < QD_MONITOR_MAX_LINES)
                                    ? (uint16_t)(QD_MONITOR_MAX_LINES - line_index)
                                    : 0;
#ifdef TESTING
    uint16_t blocks_to_draw = (ss_mem_mgr.num_free_blocks < QD_MONITOR_MAX_BLOCK_LINES)
                                   ? (uint16_t)ss_mem_mgr.num_free_blocks
                                   : QD_MONITOR_MAX_BLOCK_LINES;
    if (blocks_to_draw > remaining_lines) {
        blocks_to_draw = remaining_lines;
    }

    for (uint16_t i = 0; i < blocks_to_draw && line_index < QD_MONITOR_MAX_LINES; i++) {
        snprintf(buffer, sizeof(buffer), "memory mgr: block: %d, addr: 0x%lx, sz:%lu",
                  (int)i,
                  (unsigned long)ss_mem_mgr.free_blocks[i].addr,
                  (unsigned long)ss_mem_mgr.free_blocks[i].sz);
        any_updates |= qd_monitor_draw_line(line_index++, buffer);
    }
#endif

    for (uint16_t i = line_index; i < s_monitor_panel.prev_line_count && i < QD_MONITOR_MAX_LINES; i++) {
        qd_monitor_clear_line(i);
        any_updates = true;
    }

    s_monitor_panel.prev_line_count = line_index;

    qd_set_clip_rect(saved_clip.x, saved_clip.y, saved_clip.width, saved_clip.height);

    return any_updates;
}

#ifdef TESTING
const char* qd_monitor_panel_get_cached_line(uint16_t index) {
    if (index >= QD_MONITOR_MAX_LINES) {
        return NULL;
    }
    return s_monitor_panel.lines[index].cached_text;
}

uint16_t qd_monitor_panel_get_cached_line_count(void) {
    return s_monitor_panel.prev_line_count;
}
#endif
