# AGENTS.md – Guidance for AI Agents (OpenAI Codex, ChatGPT, etc.)

> **Purpose** This document explains how automated AI agents should **navigate, modify, test, and contribute** to this repository. Follow these rules **strictly** whenever you generate code, write documentation, or open a pull‑request (PR).

---

## 1 Repository Layout

| Path            | Description                                                               |
| --------------- | ------------------------------------------------------------------------- |
| `rkcfgtool.cpp` | Main CLI tool (C++17) for reading/writing Rockchip binary CFG files.      |
| `test.sh`       | Shell script that builds `rkcfgtool` and executes basic regression tests. |
| `cfg`           | RKDevTool configuration file and screenshot of the configuration                  |
| `AGENTS.md`     | *← you are here* — workflow guide for AI agents.                          |

## Tool Overview & Principles

`rkcfgtool` is a **single‑binary CLI utility** that lets you *inspect* and *modify* Rockchip‑style binary CFG files used by upgrade tools and flashing utilities.

---

## 2 Build & Test Matrix

| Platform | Compiler         | Command                                         |
| -------- | ---------------- | ----------------------------------------------- |
| Linux    | GCC 10 +         | `make` |

### 2.1 Automated Tests

Run **all** tests exactly like this:

```bash
./test.sh

# or
make test
```

The script must exit with status 0. If you need extra test cases, append them **inside** `test.sh` and keep the script idempotent.

---

## 3 Coding Conventions

* **Language:** C++17 or newer (no compiler‑specific extensions unless guarded).
* **Comments & Identifiers:** English only. Prefer full sentences for function‑level comments.
* **Formatting:** `clang-format` profile `LLVM` (2‑space indents). If you add new files, run `clang-format -i *.cpp *.h`.
* **Naming:**
  * Functions: `camelCase()`
  * Types / structs: `PascalCase`
  * Constants/macros: `UPPER_SNAKE_CASE`
* **Headers:** Use standard headers first, then project headers; **no** `using namespace std;` in headers.
* **Error Handling:** Return explicit error codes or throw (but never mix both in the same call stack).
* **Dependencies:** The repo is intentionally self‑contained; do **not** add external libraries without explicit human approval.

---

## 4 Pull‑Request (PR) Checklist

When Codex proposes a PR, it **must**:

1. **Describe the change** – What, why, and how. Reference any related GitHub issues.
2. **Follow scope‑one rule** – Address **one logical concern** per PR.
3. \*\*Pass \*\*\`\` on the target branch (CI will enforce this).
4. **Include screenshots** if you modified CLI output formatting or any UI artifacts.
5. **Update docs** (`README.md`, `AGENTS.md`, or in‑code comments) whenever behavior or public API changes.
6. **Respect Conventional Commits** for both commit messages and PR title, e.g. `feat(cfg): add --rename option`.

---

## 6 Security & Privacy Rules

* No hard‑coded absolute paths, credentials, or identifying information.
* Any sample CFG files added for testing **must not** contain sensitive production data.

---

## 7 Acknowledging Limitations

Codex should:

* Flag uncertainty (e.g., when modifying binary format parsing) and request human review.
* Never invent undocumented command‑line arguments.

---

## 8 Failure Scenarios & Recovery

| Situation             | Required Codex Action                                                   |
| --------------------- | ----------------------------------------------------------------------- |
| Tests fail            | Attempt to reproduce locally; propose a fix or revert your last change. |
| Undefined behavior    | Add regression test before submitting a fix.                            |
| Large refactor needed | Open an issue first and wait for maintainer approval.                   |

---

## 10 Contact & Maintainer Notes

For decisions that exceed this guide, Codex should leave a PR comment marked \`\` describing the ambiguity.

Maintainers: update this file whenever workflows change so AI agents stay in sync.

---
