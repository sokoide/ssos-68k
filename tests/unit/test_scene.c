#include "ssos_test.h"
#include "scene.h"
#include "gfx.h"
#include "kernel.h"
#include "scheduler.h"
#include "win.h"

#define SCENE_TEST_W 512
#define SCENE_TEST_H 512

typedef struct {
    int mx, my, btn;
} SceneInput;

static const SceneInput inputs[] = {
    {50, 20, 0x0200},   /* activate Timer */
    {400, 300, 0x0200}, /* move it away from the old active Mouse */
    {400, 300, 0},      /* drop: old active title must become inactive */
    {200, 65, 0x0200},  /* activate Keyboard */
    {300, 150, 0x0200}, /* drop near overlapping Mouse */
    {300, 150, 0},
    {10, 400, 0},       /* ordinary dirty-text update after the drops */
    {290, 300, 0x0200}, /* drag Timer over Mouse */
    {98, 125, 0x0200},
    {98, 125, 0},       /* Mouse becomes fully occluded */
    {10, 400, 0},      /* update content while Mouse stays covered */
};

#define BASIC_INPUT_COUNT ((int)(sizeof(inputs) / sizeof(inputs[0])))
#define Z_RENUMBER_DRAGS 252
#define TOTAL_INPUT_COUNT (BASIC_INPUT_COUNT + 2 * Z_RENUMBER_DRAGS)

static SceneInput input_for_step(int step) {
    if (step < BASIC_INPUT_COUNT) return inputs[step];
    /* Repeatedly raise Keyboard past the z renumbering boundary. */
    SceneInput input = {400, 150, ((step - BASIC_INPUT_COUNT) & 1) ? 0 : 0x0200};
    return input;
}

typedef struct {
    int step;
    int mismatch_step;
    int mismatch_x;
    int mismatch_y;
    uint16_t incremental;
    uint16_t full;
} SceneTestState;

static uint16_t incremental_frame[SCENE_TEST_W * SCENE_TEST_H];

static void compare_with_full_repaint(SceneTestState* state, int frame_index) {
    SceneInput input = input_for_step(frame_index);
    volatile uint16_t* page = ss_draw_page;
    for (int i = 0; i < SCENE_TEST_W * SCENE_TEST_H; i++)
        incremental_frame[i] = page[i];

    ss_win_render_all();
    ss_gfx_xor_rect(input.mx, input.my, 6, 6);
    for (int y = 0; y < SCENE_TEST_H; y++) {
        for (int x = 0; x < SCENE_TEST_W; x++) {
            int i = y * SCENE_TEST_W + x;
            if (page[i] != incremental_frame[i]) {
                state->mismatch_step = frame_index + 1;
                state->mismatch_x = x;
                state->mismatch_y = y;
                state->incremental = incremental_frame[i];
                state->full = page[i];
                return;
            }
        }
    }
}

static int scene_test_wait(void* ctx) {
    SceneTestState* state = (SceneTestState*)ctx;
    /* Content updates are deliberately deferred while dragging, so only
     * compare after release and ordinary non-drag frames. */
    if (state->step > 0 &&
        (state->step == 3 || state->step == 6 || state->step == 7 ||
         state->step == 10 || state->step == 11 ||
         state->step == TOTAL_INPUT_COUNT))
        compare_with_full_repaint(state, state->step - 1);
    if (state->mismatch_step || state->step == TOTAL_INPUT_COUNT)
        return 1;
    SceneInput input = input_for_step(state->step);
    ss_scene_test_set_input(input.mx, input.my, input.btn);
    state->step++;
    ss_vsync_counter++;
    return 0;
}

TEST(scene_drag_and_dirty_text_match_full_repaint) {
    reset_test_state();
    ss_sched_init();
    ss_gfx_test_dma_status_mode(-1);
    ss_gfx_set_mode(SS_CRTMOD_8);
    ss_gfx_init();
    ss_win_init();
    SceneTestState state = {0};
    SSSceneHooks hooks = {scene_test_wait, NULL, &state};
    ss_scene_run(&hooks, NULL);
    if (state.mismatch_step != 0) {
        printf("FAIL\n  frame %d pixel (%d,%d): incremental=%04x full=%04x\n",
               state.mismatch_step, state.mismatch_x, state.mismatch_y,
               state.incremental, state.full);
        failed_tests++;
        return;
    }
    ASSERT_EQ(state.step, TOTAL_INPUT_COUNT);
    ASSERT_TRUE(ss_win_get_z(2) < 10); /* renumber actually happened */
}

void run_scene_tests(void) {
    RUN_TEST(scene_drag_and_dirty_text_match_full_repaint);
}
