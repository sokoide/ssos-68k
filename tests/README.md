# SSOS Tests

The unit test suite that previously lived here has been removed.

It referenced source files that no longer exist (e.g., `os/kernel/memory.c`,
`os/kernel/task_manager.c`) and tested APIs that no longer match the current
codebase (e.g., `ss_mem_alloc()`, `TaskControlBlock`).

Only `framework/ssos_test.h` remains as a starting point for a future rewrite.

Current test coverage: **0%** (test suite removed — was testing old API, needs
rewrite).

Verify changes by building both `SCHED=cooperative` and `SCHED=preemptive`
targets and running them under an emulator or on real hardware. Restore
`make test` only after the suite is rewritten against the current kernel and
window APIs.
