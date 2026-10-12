#define _POSIX_C_SOURCE 200809L
#include "clock.h"
#include "scene_replay.h"
#include "gfx.h"
#include "profile.h"
#include "memory.h"
#include "scheduler.h"
#include "work_queue.h"
#include "win.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static uint8_t arena[1024 * 1024] __attribute__((aligned(16)));
static uint64_t samples[4096], frame_start;
static unsigned nsamples;
static uint64_t now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000u + t.tv_nsec;
}
static int cmp(const void* a, const void* b) {
    uint64_t x = *(const uint64_t*)a, y = *(const uint64_t*)b;
    return (x > y) - (x < y);
}
static void report(const char* phase, unsigned scale) {
    qsort(samples, nsamples, sizeof(samples[0]), cmp);
    printf("{\"phase\":\"%s\",\"profile\":%d,\"scale\":%u,\"samples\":%u,\"median_ns\":%llu,\"p95_ns\":%llu,\"max_ns\":%llu}\n",
           phase, SS_PROFILE_GFX, scale, nsamples,
           (unsigned long long)samples[nsamples / 2],
           (unsigned long long)samples[(nsamples - 1) * 95 / 100],
           (unsigned long long)samples[nsamples - 1]);
    nsamples = 0;
}
static int wait_frame(void* ctx) { (void)ctx; ss_vsync_counter++; return 0; }
static int input(void* ctx, uint32_t frame, int* x, int* y, int* buttons, int* key) {
    int result = ss_scene_replay_input(ctx, frame, x, y, buttons, key);
    frame_start = now();
    return result;
}
static void after(void* ctx, uint32_t frame, int dragging) {
    (void)ctx; (void)frame; (void)dragging;
    samples[nsamples++] = now() - frame_start;
}
static void* entry(void* arg) { return arg; }
static void handler(void* arg) { (*(unsigned*)arg)++; }
int main(int argc, char** argv) {
    int mode = argc > 1 && atoi(argv[1]) == 16 ? SS_CRTMOD_16 : SS_CRTMOD_8;
    struct timespec resolution;
    clock_getres(CLOCK_MONOTONIC, &resolution);
    uint64_t observed = UINT64_MAX, previous = now();
    for (unsigned i = 0; i < 10000; i++) {
        uint64_t current = now(), delta = current - previous;
        if (delta && delta < observed) observed = delta;
        previous = current;
    }
    printf("{\"phase\":\"clock\",\"reported_resolution_ns\":%llu,\"observed_min_step_ns\":%llu}\n",
           (unsigned long long)((uint64_t)resolution.tv_sec * 1000000000u + resolution.tv_nsec),
           (unsigned long long)observed);
    /* Warm up, then five independent samples. Host ns measure C logic/RAM;
     * they are never used as X68000 timing or a DMA threshold. */
    for (int run = 0; run < 6; run++) {
        ss_sched_init(); ss_gfx_test_dma_status_mode(-1);
        ss_gfx_set_mode(mode); ss_gfx_init(); ss_win_init();
        SSSceneHooks hooks = {.wait_vsync = wait_frame, .read_input = input, .after_frame = after, .collect_pixel_checksum = 1};
        SSSceneStats stats;
        ss_gfx_profile_reset();
        ss_scene_run(&hooks, &stats);
        if (run) {
            report("scene-frame-body-host", run);
            SSGfxProfile p;
            ss_gfx_profile_snapshot(&p);
            printf("{\"phase\":\"scene-pixels\",\"profile\":%d,\"run\":%d,\"frames\":%u,\"input\":%u,\"pixels\":%u,\"planned_writes\":%u,\"glyph_fast\":%u,\"glyph_clip\":%u}\n",
                   SS_PROFILE_GFX, run, stats.frames, stats.input_checksum, stats.pixel_checksum,
                   p.gvram_words_written, p.glyph_fast, p.glyph_clip);
        } else nsamples = 0;
    }
    static const unsigned counts[] = {1, 2, 8, 32};
    for (unsigned phase = 0; phase < 4; phase++) {
        ss_sched_init();
        for (unsigned i = 0; i < counts[phase]; i++) {
            SSTaskInfo info = {.entry = entry, .pri = 8};
            uint16_t id = ss_task_create(&info);
            if (ss_task_start(id) != 0) return 1;
        }
        for (unsigned i = 0; i < 2000; i++) {
            uint64_t start = now();
            for (unsigned batch = 0; batch < 100; batch++) ss_task_yield();
            samples[nsamples++] = now() - start;
        }
        report("scheduler-logic-host", counts[phase]);
    }
    ss_mem_init(arena, sizeof(arena));
    uint32_t baseline = ss_mem_free_bytes(), largest = ss_mem_largest_free();
    void* blocks[1024] = {0};
    unsigned count = 0;
    while (count < 1024 && (blocks[count] = ss_alloc(1000)) != NULL) count++;
    for (unsigned i = 0; i < count; i += 2) { ss_free(blocks[i]); blocks[i] = NULL; }
    uint32_t fragmented = ss_mem_largest_free();
    for (unsigned i = 0; i < 2000; i++) {
        uint64_t start = now();
        for (unsigned batch = 0; batch < 100; batch++) {
            void* p = ss_alloc((i % 4 + 1) * 100);
            if (p == NULL) return 1;
            ss_free(p);
        }
        samples[nsamples++] = now() - start;
    }
    report("allocator-fragmented-host", count);
    for (unsigned i = 0; i < count; i++) ss_free(blocks[i]);
    if (ss_mem_free_bytes() != baseline || ss_mem_largest_free() != largest) return 1;
    printf("{\"phase\":\"allocator-capacity\",\"baseline\":%u,\"largest\":%u,\"fragmented_largest\":%u,\"recovered\":%u}\n",
           baseline, largest, fragmented, ss_mem_free_bytes());
    SSWorkQueue q; ss_work_init(&q); unsigned done = 0;
    for (unsigned i = 0; i < 2000; i++) {
        uint64_t start = now();
        for (unsigned batch = 0; batch < 100; batch++) {
            ss_work_enqueue(&q, handler, &done);
            ss_work_drain_budget(&q, 1);
        }
        samples[nsamples++] = now() - start;
    }
    report("work-drain-host", 1);
    return done != 200000;
}
