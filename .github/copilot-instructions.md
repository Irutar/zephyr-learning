# Copilot Agent Instructions

## Environment
- **WSL**: We are running inside Windows Subsystem for Linux (WSL2, Ubuntu 22.04).
- **WSL user**: `irutar`. Windows host user: `krystian`.
- **Zephyr base**: `/home/irutar/zephyrproject/zephyr`
- **West workspace**: `/home/irutar/zephyrproject`
- **Python venv**: `/home/irutar/zephyrproject/.venv`
- **SDK**: `/home/irutar/zephyr-sdk-1.0.1`

## Communication
- **Chat with the user**: always respond in **Polish**.
- **Code, comments, identifiers**: always in **English** (see Coding Standards below).
- **Unclear tasks — ask, don't guess.** If the engineer's request is
  ambiguous, incomplete, or could be interpreted in multiple ways,
  **ask for clarification**. Do not attempt to guess what was meant.
  A quick clarifying question saves more time than implementing the
  wrong thing.

## Coding Standards

### 1. Language — English Only
- **All** comments, variable names, function names, log messages, Kconfig help
  strings, documentation, and any other human-readable text **must** be in
  **English**.
- If you encounter any Polish text in the codebase, translate it to English.
  The only exception is the Git trigger phrases listed below.

### 2. No Abbreviations — Full Names Only
- Variables, functions, structs, macros, Kconfig symbols, devicetree node
  labels, and every other identifier **must** use **full, unabbreviated names**.
- **Never** use shortcuts like `tmr`, `cfg`, `buf`, `ctx`, `dev`, `err`, `ret`,
  `cnt`, `cb`, `init`, `reg`, `msg`, `pkt`, `addr`, etc.
- Acceptable examples: `timer`, `configuration`, `buffer`, `context`, `device`,
  `error`, `return_value`, `count`, `callback`, `initialize`, `register`,
  `message`, `packet`, `address`.
- Yes, the code will be longer. That is intentional — readability is the
  priority.
- Existing abbreviations in the codebase (e.g., `vm`, `cfg`, `dev`) are
  technical debt — do **not** introduce new ones. When touching a file that
  contains abbreviations, refactor them to full names as part of your change.

### 3. Comments — Absolute Minimum
- Keep comments to the **absolute minimum**. Most code should be
  self-documenting through clear variable and function names.
- Add comments **only** where the mechanism is non-obvious or genuinely
  complex — a hardware workaround, a subtle protocol requirement, an
  algorithm whose intent is not clear from the code alone.
- **Never** comment things that are obvious from reading the code.
  Examples of **banned** comments:
  - `/* Initialize the variable */`
  - `/* Set pin high */`
  - `/* Check if device is ready */`
  - `/* Main loop */`
  - `/* Increment counter */`
- A file with zero comments is perfectly acceptable if the code is clear.
  Only add a comment when you genuinely cannot make the code itself tell
  the story.

### 4. Variable Declarations — Top of Scope Only
- **All** local variables must be declared at the **top** of their block
  (function, loop body, or `{ }` compound statement).
- **Never** interleave declarations with code (C99 mixed declarations).
  Example of **banned**:
  ```c
  int compute(void) {
      do_something();
      int result = 42;   /* ← NOT at top */
      return result;
  }
  ```
- Acceptable:
  ```c
  int compute(void) {
      int result;
      do_something();
      result = 42;
      return result;
  }
  ```

### 5. Explicit Boolean Comparisons — No Implicit Truthiness
- **Never** use a variable or expression directly as a boolean condition.
- Always compare against an explicit value: `NULL`, `0`, `false`, etc.
  - Banned: `if (pointer)`, `if (!error)`, `if (count)`, `while (remaining)`
  - Required: `if (pointer != NULL)`, `if (error == 0)`, `if (count > 0)`,
    `while (remaining > 0)`

### 6. Yoda Notation — Constant on the Left
- In all equality and relational comparisons, put the **constant or macro
  on the left** side.
  - Banned: `if (result == 0)`, `if (error == -ENODEV)`, `if (count > 5)`
  - Required: `if (0 == result)`, `if (-ENODEV == error)`, `if (5 < count)`
- This catches accidental assignment (`=` instead of `==`) at compile time
  because you cannot assign to a constant.

## Git Rules
- **NEVER** run `git add`, `git commit`, or `git push` unless the user
  explicitly says something like "zrób commit", "commitnij", "wypchnij", etc.
- `git status`, `git diff`, `git log` are fine anytime.
- Before `git push`, **review `README.md`** and update the
  **Implemented Features** section with any new functionality that was added
  since the last push. Ask the user if unsure whether something warrants a
  README update.

## Agent Memory Rules
- **All** project-specific instructions, rules, and context stay in **this file**
  (`.github/copilot-instructions.md`) or in the repo.
- Do **NOT** store repo-specific info in `/memories/` or any external location.
- When working on a different project, that project will have its own instructions.
- `/memories/` is only for global/personal preferences, not project rules.

## Project Philosophy — Learning Over Polish

This project exists to **learn Zephyr RTOS** in depth. The engineer may need
Zephyr knowledge in professional work soon, so understanding the OS, its
subsystems, APIs, and build system is the primary goal.

- **Bugs are expected and acceptable.** The priority is exploring Zephyr's
  capabilities and understanding how things work, not handling every edge case
  or writing production-grade code.
- **Corner cases are deliberately ignored** unless they teach something
  valuable about Zephyr. Do not spend time on defensive programming, error
  recovery for unlikely scenarios, or exhaustive input validation.
- **Depth over breadth.** When faced with a choice between "make it work
  quickly" and "understand why it works", always choose understanding. Read
  the Zephyr source, trace the Kconfig dependencies, inspect the generated
  devicetree — that is the whole point.

## Target Hardware
- **Board**: ESP32-DevKitC V4 (ESP32-WROVER-IE)
- **SoC**: ESP32 (Xtensa LX6, single core — PROCPU variant)
- **Flash**: 4 MB / **PSRAM**: 8 MB
- **Zephyr build target**: `esp32_devkitc/esp32/procpu`
- **Console UART**: UART0 (GPIO1 TX, GPIO3 RX, via onboard CP2102 → microUSB)
- **UART1**: GPIO10 TX, GPIO9 RX (available on goldpins)
- **UART2**: GPIO17 TX, GPIO16 RX (available on goldpins)
- **Toolchain**: Zephyr SDK 1.0.1

## Known Hardware Issues — DO NOT DEBUG

### Wi-Fi "not found" / zsock_sendto failures
- **Symptom**: `SSID 'domek_galazka' not found in scan results`, `zsock_sendto failed: -1`
- **Root cause**: ESP32 weak antenna. Router is physically distant. Signal unreliable.
- **Agent action**: **IGNORE**. Do NOT attempt to debug Wi-Fi connectivity, reconnect logic, DHCP, sockets, or suggest changes to `wifi_log.c` related to RF range. This is a known hardware limitation, not a software bug.
