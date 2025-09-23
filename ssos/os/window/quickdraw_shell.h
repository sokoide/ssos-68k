#pragma once

#include <stdbool.h>

void qd_shell_draw_desktop_background(void);
void qd_shell_draw_taskbar(void);
void qd_shell_draw_title_bar(void);
void qd_shell_draw_desktop_chrome(void);

void qd_shell_init_info_panel(void);
void qd_shell_update_info_panel(bool force_full_refresh);
