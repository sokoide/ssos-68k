#ifndef S3_WIN_H
#define S3_WIN_H

#include <stdint.h>

#define S3_MAX_WINDOWS  32
#define S3_WIN_VISIBLE  0x01
#define S3_WIN_DIRTY    0x02

#define S3_BLOCK_SIZE   8
#define S3_ZMAP_W       (768 / S3_BLOCK_SIZE)   /* 96 */
#define S3_ZMAP_H       (512 / S3_BLOCK_SIZE)   /* 64 */

typedef struct S3Window S3Window;
struct S3Window {
    uint16_t x, y, w, h;
    uint16_t z;
    uint16_t flags;
    uint16_t dirty_x, dirty_y, dirty_w, dirty_h;
    void (*render)(S3Window* self);
    uint16_t id;
};

void     s3_win_init(void);
uint16_t s3_win_create(int x, int y, int w, int h, uint16_t z);
void     s3_win_destroy(uint16_t id);
void     s3_win_show(uint16_t id);
void     s3_win_hide(uint16_t id);
void     s3_win_damage(uint16_t id, int x, int y, int w, int h);
void     s3_win_render_all(void);
void     s3_win_move(uint16_t id, int x, int y);

#endif /* S3_WIN_H */
