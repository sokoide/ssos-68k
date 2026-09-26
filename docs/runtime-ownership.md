# Runtime ownership and interrupt rules

Both scheduler variants run in Supervisor mode. Timer D can interrupt a task
in the preemptive build; a cooperative yield can switch tasks in either build.
The following rules apply to both `.x` and `.xdf`.

| State/API | Owner | Exclusion contract |
| --- | --- | --- |
| Scheduler ready/sleep queues, task TCB state | Scheduler | Mutate with SR interrupt mask 7. `ss_do_wakeups` requires the caller to hold it; Timer D ISR already does, and task-side wakeups run inside `ss_do_context_switch`. |
| IPC queues, work queue | Kernel APIs | Their operations save and restore the caller's SR. Message payloads are copied. Handlers run after the work-queue lock is released. |
| Buddy arena and slab caches | Initializing host, then one designated task | `ss_mem_init`/`ss_slab_init` run before task scheduling. Alloc/free operations have no internal lock. If ownership is transferred between tasks, the caller must hold `ss_irq_save` through the entire operation and restore the saved SR afterward. Never call them from an ISR. |
| Window model, scene state, GVRAM and graphics mode | UI task | Only the UI task calls `ss_win_*`/`ss_gfx_*` after initialization. A few window operations have local IRQ guards, but they do not make the whole renderer reentrant. Other tasks must post a request to the UI owner; the `.xdf` work queue is one available transport. |
| DMAC channel 2, fill buffer and transfer descriptor | UI task, through the graphics API | One synchronous transfer at a time. Do not start a second transfer or mutate the graphics page from another task or ISR. Do not mask interrupts over the whole DMA polling interval. After SAB, an uncleared `ACT` latches a process-lifetime failure: no CPU fallback, further DMA setup, or graphics-page writes through this API. `ss_gfx_dma_stop_unconfirmed()` exposes this state; there is no recovery or error return from the `void` drawing API. Investigate the hardware state before restarting. See [remaining work](maintenance-performance-review-2026-09-24.md). |

`ss_irq_save`/`ss_irq_restore` are Supervisor-only. Nesting is valid when every
level restores its own saved SR in reverse order. A task may yield or sleep
while a critical section is active: its yield frame preserves its SR, and the
resumed task restores its own frame. Keep such sections short so Timer D and
V-DISP are not delayed unnecessarily.

Task entry receives its `SSTaskInfo.arg`. Its return value is ignored. An
entry that returns is marked `SS_TS_TERMINATED`; its ID and stack remain
reserved because the current stack cannot be reclaimed during exit. There is
no join, cancellation, or slot reuse API yet. A custom stack is caller-owned
for the full lifetime of that task, including after termination. For a custom
descending stack, `SSTaskInfo.stack` points at the final aligned word of the
region, not its first byte; valid bytes are `[stack - stack_size + 4, stack + 4)`.
