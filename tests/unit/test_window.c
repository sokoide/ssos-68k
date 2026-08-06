/* test_window.c - window manager logic tests.
 *
 * Learning objective: the window system's logic (slot allocation, z-order,
 * dirty regions, hit testing) is independent of the actual pixel output.
 * The native suite links the real drawing primitives against a RAM-backed
 * GVRAM seam, so rendering assertions can inspect final pixels. */

#include "ssos_test.h"
#include "win.h"
#include "gfx.h"

static int render_callback_calls;
static int render_callback_saw_null;
static SSGfxRect render_callback_clip;

static void record_render_clip(SSWindow* self, const SSGfxRect* clip) {
    (void)self;
    render_callback_calls++;
    if (clip == NULL) {
        render_callback_saw_null = 1;
    } else {
        render_callback_clip = *clip;
    }
}

/* ---- init / create ---- */

TEST(win_init_clears_slots) {
    ss_win_init();
    /* No slots allocated: hit_test finds nothing on an empty desktop. */
    ASSERT_EQ(ss_win_hit_test(10, 10), -1);
}

TEST(win_create_returns_ascending_ids) {
    ss_win_init();
    uint16_t a = ss_win_create(10, 20, 100, 50, 1);
    uint16_t b = ss_win_create(0, 0, 50, 50, 2);
    ASSERT_EQ(a, 1);
    ASSERT_EQ(b, 2);
    ASSERT_EQ(ss_win_get_x(a), 10);
    ASSERT_EQ(ss_win_get_y(a), 20);
    ASSERT_EQ(ss_win_get_w(a), 100);
    ASSERT_EQ(ss_win_get_h(a), 50);
    ASSERT_EQ(ss_win_get_z(a), 1);
}

TEST(win_create_sets_visible_and_dirty) {
    ss_win_init();
    uint16_t id = ss_win_create(0, 0, 40, 40, 0);
    SSWindow* w = ss_win_get_ptr(id);
    ASSERT_NEQ(w->flags & SS_WIN_VISIBLE, 0);
    ASSERT_NEQ(w->flags & SS_WIN_DIRTY, 0);
}

TEST(win_create_exhaustion) {
    ss_win_init();
    for (int i = 0; i < SS_MAX_WINDOWS; i++) {
        ASSERT_EQ(ss_win_create(0, 0, 8, 8, 0), (uint16_t)(i + 1));
    }
    /* Full -> 0. */
    ASSERT_EQ(ss_win_create(0, 0, 8, 8, 0), 0);
}

TEST(win_destroy_then_reuse_id) {
    ss_win_init();
    uint16_t id = ss_win_create(0, 0, 40, 40, 0);
    ss_win_destroy(id);
    /* Slot freed: the id field is cleared (get_ptr still returns the slot,
     * it doesn't check aliveness). */
    ASSERT_EQ(ss_win_get_ptr(id)->id, 0);
    /* Next create reuses the freed slot (lowest free id). */
    uint16_t id2 = ss_win_create(0, 0, 40, 40, 0);
    ASSERT_EQ(id2, id);
}

/* ---- visibility / geometry ---- */

TEST(win_hide_clears_visible) {
    ss_win_init();
    uint16_t id = ss_win_create(0, 0, 40, 40, 0);
    ss_win_hide(id);
    ASSERT_EQ(ss_win_get_ptr(id)->flags & SS_WIN_VISIBLE, 0);
    ss_win_show(id);
    ASSERT_NEQ(ss_win_get_ptr(id)->flags & SS_WIN_VISIBLE, 0);
}

TEST(win_move_updates_position_and_dirty) {
    ss_win_init();
    uint16_t id = ss_win_create(10, 10, 40, 40, 0);
    ss_win_move(id, 100, 200);
    ASSERT_EQ(ss_win_get_x(id), 100);
    ASSERT_EQ(ss_win_get_y(id), 200);
    SSWindow* w = ss_win_get_ptr(id);
    ASSERT_NEQ(w->flags & SS_WIN_DIRTY, 0);
    ASSERT_EQ(w->dirty_w, 40);   /* full-window dirty after move */
}

TEST(win_damage_sets_region) {
    ss_win_init();
    uint16_t id = ss_win_create(0, 0, 100, 100, 0);
    ss_win_damage(id, 5, 6, 20, 30);
    SSWindow* w = ss_win_get_ptr(id);
    ASSERT_EQ(w->dirty_x, 5);
    ASSERT_EQ(w->dirty_y, 6);
    ASSERT_EQ(w->dirty_w, 20);
    ASSERT_EQ(w->dirty_h, 30);
}

TEST(win_set_z_updates) {
    ss_win_init();
    uint16_t id = ss_win_create(0, 0, 40, 40, 1);
    ss_win_set_z(id, 7);
    ASSERT_EQ(ss_win_get_z(id), 7);
}

/* ---- title / content ---- */

TEST(win_set_title_truncates_to_19) {
    ss_win_init();
    uint16_t id = ss_win_create(0, 0, 40, 40, 0);
    ss_win_set_title(id, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
    SSWindow* w = ss_win_get_ptr(id);
    /* title[20], strncpy(...,19) + NUL. */
    ASSERT_EQ(w->title[19], '\0');
    ASSERT_EQ(w->title[0], 'A');
}

TEST(win_set_content_line_pads_to_width) {
    ss_win_init();
    uint16_t id = ss_win_create(0, 0, 40, 40, 0);
    ss_win_set_content_line(id, 0, "hi");
    SSWindow* w = ss_win_get_ptr(id);
    ASSERT_EQ(w->content[0][0], 'h');
    ASSERT_EQ(w->content[0][1], 'i');
    /* remainder is space-padded up to cap-1 (29). */
    ASSERT_EQ(w->content[0][29], '\0');
    ASSERT_EQ(w->content[0][3], ' ');
}

TEST(win_set_content_line_ignores_out_of_range) {
    ss_win_init();
    uint16_t id = ss_win_create(0, 0, 40, 40, 0);
    /* line 5 is out of [0,3) — must be a safe no-op. */
    ss_win_set_content_line(id, 5, "ignored");
    /* No crash; content untouched (still zeroed from create). */
    ASSERT_EQ(ss_win_get_ptr(id)->content[0][0], '\0');
}

/* ---- hit testing ---- */

TEST(hit_test_picks_topmost_at_point) {
    ss_win_init();
    uint16_t a = ss_win_create(0, 0, 100, 100, 1);
    uint16_t b = ss_win_create(0, 0, 100, 100, 5);   /* same rect, higher z */
    /* Point inside both -> higher z wins. */
    ASSERT_EQ(ss_win_hit_test(50, 50), (int)b);
    /* Point outside all -> -1. */
    ASSERT_EQ(ss_win_hit_test(500, 500), -1);
    (void)a;
}

TEST(hit_test_skips_hidden) {
    ss_win_init();
    uint16_t a = ss_win_create(0, 0, 100, 100, 1);
    uint16_t b = ss_win_create(0, 0, 100, 100, 9);
    ss_win_hide(b);
    /* b hidden, so a (lower z) is picked. */
    ASSERT_EQ(ss_win_hit_test(50, 50), (int)a);
}

/* ---- rendering (stub call counts) ---- */

TEST(render_all_paints_visible_window) {
    ss_gfx_set_mode(SS_CRTMOD_16);
    ss_gfx_init();
    ss_win_init();
    uint16_t id = ss_win_create(0, 0, 40, 40, 1);   /* no render cb -> draw_frame */
    (void)id;
    ss_win_render_all();
    /* The frame's top-left border is black; background stipple is not. */
    ASSERT_EQ(ss_draw_page[0], 0);
    /* active_z reflects the highest visible window. */
    ASSERT_EQ(ss_win_active_z, 1);
}

TEST(render_all_skips_hidden) {
    ss_gfx_set_mode(SS_CRTMOD_16);
    ss_gfx_init();
    ss_win_init();
    ss_win_render_all();
    uint16_t background = ss_draw_page[0];
    uint16_t a = ss_win_create(0, 0, 40, 40, 1);
    ss_win_hide(a);
    ss_win_render_all();
    ASSERT_EQ(ss_draw_page[0], background);
}

TEST(render_region_clips_standard_frame_only) {
    ss_gfx_set_mode(SS_CRTMOD_16);
    ss_gfx_init();
    ss_win_init();
    uint16_t id = ss_win_create(10, 10, 40, 40, 1);
    (void)id;
    ss_win_render_region(15, 20, 5, 6);
    /* The dirty region itself is filled, while an outside pixel is untouched. */
    uint32_t stride = (uint32_t)ss_current_mode->bytes_per_line / 2;
    ASSERT_NEQ(ss_draw_page[20 * stride + 15], 0);
    ASSERT_EQ(ss_draw_page[19 * stride + 15], 0);
}

TEST(render_region_keeps_exposed_part_of_same_zmap_block) {
    ss_win_init();
    render_callback_calls = 0;
    render_callback_saw_null = 0;

    /* The higher window covers only the left half of the lower window, but
     * both fit in the same 8x8 z-map block.  The region is the exposed right
     * half, so the lower callback must still be rendered. */
    uint16_t lower = ss_win_create(0, 0, 4, 8, 1);
    uint16_t higher = ss_win_create(0, 0, 3, 8, 2);
    ss_win_set_render(lower, record_render_clip);
    ss_win_set_render(higher, record_render_clip);

    ss_win_render_region(2, 0, 2, 8);

    ASSERT_EQ(render_callback_calls, 2);
    ASSERT_EQ(render_callback_saw_null, 0);
}

TEST(render_callback_receives_explicit_region_clip) {
    ss_win_init();
    render_callback_calls = 0;
    render_callback_saw_null = 0;
    uint16_t id = ss_win_create(10, 10, 40, 40, 1);
    ss_win_set_render(id, record_render_clip);

    ss_win_render_all();
    ASSERT_EQ(render_callback_calls, 1);
    ASSERT_EQ(render_callback_saw_null, 1);

    ss_win_render_region(15, 20, 5, 6);
    ASSERT_EQ(render_callback_calls, 2);
    ASSERT_EQ(render_callback_clip.x, 15);
    ASSERT_EQ(render_callback_clip.y, 20);
    ASSERT_EQ(render_callback_clip.w, 5);
    ASSERT_EQ(render_callback_clip.h, 6);
}

/* ---- invalid ids ---- */

TEST(getters_return_zero_for_invalid_id) {
    ss_win_init();
    ASSERT_EQ(ss_win_get_x(0), 0);
    ASSERT_EQ(ss_win_get_x(SS_MAX_WINDOWS + 1), 0);
    ASSERT_NULL(ss_win_get_ptr(0));
    ASSERT_NULL(ss_win_get_ptr(SS_MAX_WINDOWS + 1));
}

void run_window_tests(void) {
    RUN_TEST(win_init_clears_slots);
    RUN_TEST(win_create_returns_ascending_ids);
    RUN_TEST(win_create_sets_visible_and_dirty);
    RUN_TEST(win_create_exhaustion);
    RUN_TEST(win_destroy_then_reuse_id);
    RUN_TEST(win_hide_clears_visible);
    RUN_TEST(win_move_updates_position_and_dirty);
    RUN_TEST(win_damage_sets_region);
    RUN_TEST(win_set_z_updates);
    RUN_TEST(win_set_title_truncates_to_19);
    RUN_TEST(win_set_content_line_pads_to_width);
    RUN_TEST(win_set_content_line_ignores_out_of_range);
    RUN_TEST(hit_test_picks_topmost_at_point);
    RUN_TEST(hit_test_skips_hidden);
    RUN_TEST(render_all_paints_visible_window);
    RUN_TEST(render_all_skips_hidden);
    RUN_TEST(render_region_clips_standard_frame_only);
    RUN_TEST(render_region_keeps_exposed_part_of_same_zmap_block);
    RUN_TEST(render_callback_receives_explicit_region_clip);
    RUN_TEST(getters_return_zero_for_invalid_id);
}
