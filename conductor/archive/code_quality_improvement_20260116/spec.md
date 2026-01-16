# Specification: Improve Code Quality and Naming Conventions

## 1. Overview
Review and improve the naming conventions and overall code quality of the SSOS kernel, focusing on clarity, consistency, and alignment with the "Literate Programming" philosophy. This track addresses inconsistencies introduced during previous refactorings and aims to establish a solid foundation for future development.

## 2. Goals
-   **Consistency:** Unify naming conventions across the kernel (e.g., `free_block_count` vs `num_free_blocks`).
-   **Clarity:** Use descriptive names for all public and internal symbols.
-   **Robustness:** Improve error handling and input validation in critical paths.
-   **Maintainability:** Reduce global state visibility and improve encapsulation where appropriate.

## 3. Scope
-   **Primary Focus:** `ssos/os/kernel/memory.c` and `memory.h`.
-   **Secondary Focus:** `ssos/os/main/ssoswindows.c` (to fix regressions from previous naming changes).
-   **Verification:** Existing unit tests in `tests/unit/test_memory.c` must pass.

## 4. Requirements
-   Unify `num_free_blocks` to `free_block_count` in all files.
-   Unify `addr` to `start_address` and `sz` to `size_in_bytes` in memory-related structures.
-   Ensure all public APIs in `memory.h` have consistent prefixes and descriptive names.
-   Improve documentation in `memory.c` to explain "why" optimizations are used.

## 5. Non-Goals
-   Rewriting the memory allocator algorithm.
-   Significant architectural changes to the graphics or task management systems.
