# Specification: Refactor Memory Manager

## 1. Overview
Refactor the existing memory manager (`ssos/os/kernel/memory.c` and `.h`) to align with the project's new "Literate Programming" and educational goals. The primary objective is not to change the external behavior or APIs, but to make the internal logic (allocation, freeing, coalescing, alignment) crystal clear for students.

## 2. Goals
-   **Educational Value:** Transform the source code into a primary learning resource with extensive "why" and "how" comments.
-   **Simplification:** If the current coalescing or alignment logic is overly clever or optimized, simplify it to the most readable, correct implementation.
-   **Maintain Quality:** Ensure 100% of existing tests in `tests/unit/test_memory.c` continue to pass.
-   **Style Compliance:** Enforce the new modular C style and strict naming conventions.

## 3. Scope
-   **Target Files:**
    -   `ssos/os/kernel/memory.c`
    -   `ssos/os/kernel/memory.h`
-   **Verification:**
    -   `tests/unit/test_memory.c` (Must pass without modification if possible, or be updated only if internal structures change significantly).

## 4. Requirements
-   **Comments:** Add a file header explaining the Memory Management strategy (e.g., Free List, Bitmap, etc.). Add block comments before every function. Add inline comments for complex pointer arithmetic.
-   **Naming:** Rename any variables like `p`, `ptr`, `curr`, `next` to more descriptive names like `current_block`, `next_free_block`, etc., if they are ambiguous.
-   **Logic:** Ensure the `malloc` and `free` algorithms are standard and recognizable (e.g., First-Fit).

## 5. Non-Goals
-   Implementing a new allocator algorithm (e.g., Slab, Buddy) unless the current one is broken.
-   Optimizing for speed.
