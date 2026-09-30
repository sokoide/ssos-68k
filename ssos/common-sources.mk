# Paths relative to ssos/os. Host-only sources remain in each consumer.
SS_CORE_SRCS := kernel/scheduler.c kernel/sched_profile.c kernel/runtime_guard.c kernel/task_probe.c kernel/$(SCHED)/wakeups.c \
    util/numfmt.c mem/buddy.c mem/slab.c gfx/profile.c gfx/vram.c \
    win/window.c app/scene.c app/scene_replay.c
SS_TARGET_SRCS := kernel/main_task.c gfx/palette.c
SS_XDF_SRCS := kernel/work_queue.c ipc/message.c
