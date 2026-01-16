# Development Workflow

## 1. Preparation
- **Understand the Goal:** Before writing any code, clearly understand the `WHY`, `WHAT`, and `HOW` of the task.
- **Reference Context:** Always check `conductor/product.md`, `conductor/product-guidelines.md`, and `conductor/tech-stack.md` for alignment.

## 2. Test-Driven Development (TDD)
- **Red:** Write a failing test that defines the expected behavior.
- **Green:** Write the minimum amount of code to make the test pass.
- **Refactor:** Improve the code while ensuring tests remain green.
- **Coverage Goal:** Maintain test coverage above **80%**.

## 3. Implementation Cycle
1.  **Select Task:** Pick the next incomplete task from the current Phase in `plan.md`.
2.  **Implement:** Follow the TDD cycle to complete the task.
3.  **Verify:** Run all relevant tests to ensure no regressions.
4.  **Mark Complete:** Update `plan.md` by marking the task as `[x]`.

## 4. Phase Completion & Checkpointing
- **Commit Strategy:** You have chosen to commit **After each phase**.
- **Phase Completion Protocol:**
    1.  Upon completing ALL tasks in a Phase:
    2.  Run the full test suite.
    3.  **Verify User Manual:** If the phase introduced user-facing changes, verify and update the User Manual/Documentation.
    4.  **Create Checkpoint:**
        -   Stage all changes: `git add .`
        -   Commit with a descriptive message: `git commit -m "feat: Complete Phase X - <Phase Name>"`
        -   **Record Summary:** Use Git Notes to attach a detailed summary of the work done in this phase (key decisions, challenges, etc.).
            -   Command: `git notes add -m "Phase Summary: ..."`

## 5. Code Review & Quality
-   **Self-Review:** specific attention to:
    -   Readability and Naming
    -   Error Handling
    -   Performance (No premature optimization, but avoid obvious bottlenecks)
    -   Security best practices
-   **Style Check:** Ensure code matches the project's selected style guides in `conductor/code_styleguides/`.