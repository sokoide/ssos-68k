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

void qd_monitor_panel_init(void);
bool qd_monitor_panel_tick(void);

#ifdef TESTING
const char* qd_monitor_panel_get_cached_line(uint16_t index);
uint16_t qd_monitor_panel_get_cached_line_count(void);
#endif

