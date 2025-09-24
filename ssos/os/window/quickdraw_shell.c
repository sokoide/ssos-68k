#include "quickdraw_shell.h"

#include "quickdraw.h"

static bool qd_shell_ready(void) {
    return qd_is_initialized();
}

void qd_shell_draw_desktop_background(void) {
    if (!qd_shell_ready()) {
        return;
    }

    qd_clear_screen(QD_COLOR_GREEN);

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

void qd_shell_update_taskbar(void) {
    if (!qd_shell_ready()) {
        return;
    }

    qd_shell_draw_taskbar();
}

void qd_shell_update_desktop_chrome(void) {
    if (!qd_shell_ready()) {
        return;
    }

    qd_shell_draw_desktop_background();
    qd_shell_draw_taskbar();
    qd_shell_draw_title_bar();
}
