# Product Guidelines

## Coding Style & Architecture
-   **Approach:** Modular & Functional.
    -   Adhere to strict C standards (C99/C11).
    -   Emphasize small, pure functions with single responsibilities.
    -   Design for clear data flow and minimize global state to enhance testability and reasoning.
-   **Assembly Usage:** Minimally Invasive.
    -   Prioritize C for all system logic.
    -   Restrict Assembly (`.s` files) strictly to hardware-specific necessities: boot sectors, context switching, and I/O instructions inaccessible from C.

## Documentation Standards
-   **Philosophy:** Literate Programming.
    -   Code should be written as an educational resource first.
    -   **Requirement:** Extensive comments must explain the *why* (architectural decisions) and *how* (algorithmic steps) of every major block.
    -   Assume the reader is a student learning OS concepts.

## Testing Strategy
-   **Unit Testing (Primary):**
    -   All platform-independent logic (scheduler algorithms, memory allocators, data structures) MUST be unit tested on the host machine.
    -   Use mocks to simulate hardware interfaces, ensuring tests are fast and runnable without emulation.
-   **Integration Testing:**
    -   Maintain a suite of bootable tests to verify end-to-end behavior on the actual emulator/hardware.

## Naming Conventions
-   **Standard C (Snake Case):**
    -   Functions: `function_name`
    -   Variables: `variable_name`
    -   Structs/Types: `struct_name_t`
    -   Macros/Constants: `MACRO_NAME`
