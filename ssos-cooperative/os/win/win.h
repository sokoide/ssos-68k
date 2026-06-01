#ifndef S4_WIN_H
#define S4_WIN_H

#include <stdint.h>

#define S4_MAX_WINDOWS  32
#define S4_WIN_VISIBLE  0x01
#define S4_WIN_DIRTY    0x02

#define S4_BLOCK_SIZE   8
#define S4_ZMAP_W       (768 / S4_BLOCK_SIZE)   /* 96 */
#define S4_ZMAP_H       (512 / S4_BLOCK_SIZE)   /* 64 */

typedef struct S4Window S4Window;
struct S4Window {
    uint16_t x, y, w, h;
    uint16_t z;
    uint16_t flags;
    uint16_t dirty_x, dirty_y, dirty_w, dirty_h;
    void (*render)(S4Window* self);
    uint16_t id;
};

void     s4_win_init(void);
uint16_t s4_win_create(int x, int y, int w, int h, uint16_t z);
void     s4_win_destroy(uint16_t id);
void     s4_win_show(uint16_t id);
void     s4_win_hide(uint16_t id);
void     s4_win_damage(uint16_t id, int x, int y, int w, int h);
void     s4_win_render_all(void);
void     s4_win_move(uint16_t id, int x, int y);

#endif /* S4_WIN_H */
