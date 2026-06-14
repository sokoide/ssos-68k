#ifndef SS_WIN_H
#define SS_WIN_H

#include <stdint.h>

#define SS_MAX_WINDOWS  32
#define SS_WIN_VISIBLE  0x01
#define SS_WIN_DIRTY    0x02

#define SS_BLOCK_SIZE   8
#define SS_ZMAP_W       (768 / SS_BLOCK_SIZE)   /* 96 */
#define SS_ZMAP_H       (512 / SS_BLOCK_SIZE)   /* 64 */

typedef struct SSWindow SSWindow;
struct SSWindow {
    uint16_t x, y, w, h;
    uint16_t z;
    uint16_t flags;
    uint16_t dirty_x, dirty_y, dirty_w, dirty_h;
    void (*render)(SSWindow* self);
    uint16_t id;
};

void     ss_win_init(void);
uint16_t ss_win_create(int x, int y, int w, int h, uint16_t z);
void     ss_win_destroy(uint16_t id);
void     ss_win_show(uint16_t id);
void     ss_win_hide(uint16_t id);
void     ss_win_damage(uint16_t id, int x, int y, int w, int h);
void     ss_win_render_all(void);
void     ss_win_render_region(int rx, int ry, int rw, int rh);
void     ss_win_move(uint16_t id, int x, int y);
int      ss_win_hit_test(int mx, int my);
int      ss_win_get_x(uint16_t id);
int      ss_win_get_y(uint16_t id);
int      ss_win_get_w(uint16_t id);
int      ss_win_get_h(uint16_t id);
int      ss_win_get_z(uint16_t id);
void     ss_win_set_render(uint16_t id, void (*render)(SSWindow*));
void     ss_win_set_z(uint16_t id, uint16_t z);
void     ss_win_mark_dirty(uint16_t id);

extern uint16_t ss_win_active_z;   /* highest visible z, set by render_all */

#endif /* SS_WIN_H */
