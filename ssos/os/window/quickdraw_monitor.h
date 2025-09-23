#pragma once

#include <stdbool.h>
#include <stdint.h>

#define QD_MONITOR_PANEL_LEFT   16
#define QD_MONITOR_PANEL_TOP    80
#define QD_MONITOR_PANEL_WIDTH  512
#define QD_MONITOR_PANEL_HEIGHT 288

#define QD_MONITOR_PANEL_HEADER_HEIGHT 24
#define QD_MONITOR_PANEL_TEXT_PADDING_X 8
#define QD_MONITOR_PANEL_TEXT_TOP_OFFSET 6

#define QD_MONITOR_MAX_LINES       32
#define QD_MONITOR_TEXT_CAPACITY   256
#define QD_MONITOR_MAX_BLOCK_LINES 10

#define QD_MONITOR_HEADER_COLOR      QD_COLOR_BLUE
#define QD_MONITOR_HEADER_TEXT_COLOR QD_COLOR_BRIGHT_WHITE
#define QD_MONITOR_BODY_COLOR        QD_COLOR_BRIGHT_WHITE
#define QD_MONITOR_TEXT_COLOR        QD_COLOR_BLACK
#define QD_MONITOR_BORDER_COLOR      QD_COLOR_BLACK

void qd_monitor_panel_init(void);
bool qd_monitor_panel_tick(void);

#ifdef TESTING
const char* qd_monitor_panel_get_cached_line(uint16_t index);
uint16_t qd_monitor_panel_get_cached_line_count(void);
#endif
