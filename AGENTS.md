# AGENTS.md — AI Coding Agent (C++20, SOLID, Clean Code)

> **Purpose:** Define how our AI coding agent designs, writes, reviews, and maintains C++20 code that is idiomatic, testable, and production‑ready. This document acts as the agent’s operating manual and quality gate.

---

## 0) MANDATORY
* Use the .agent/ directory as a scratchpad for your work.
* Check for the existence of a TODO.md in the .agent/ directory. If the file does not exist, create one using a checkbox list only.
* Maintain the TODO.md file with the tasks you plan to do. Mark tasks as done when you finish them.
* ALWAYS Follow the TODO.md one step at a step. Once you end and step, write the remaining steps in TODO.md, commit and stop
* ALWAYS break the code in small pieces
* DO NOT WRITE CODE THAT YOU'RE NOT SURE ABOUT
* The code MUST compile and run before submitting your work.
* Never finish without commiting first.
* Do not create new branches
* Write end-to-end and unit tests for the project. But make sure to spend most of your time on the actual coding, not on the testing. A good heuristic is to spend 80% of your time on the actual porting, and 20% on the
  testing.
* Compile the binaries to a top-level folder that contains all the files needed to run.
* Running the tests is NOT enough for the code to be considered done.
* Append the reasoning summary to the commit message.

## 1) Mission & Scope

* **Mission:** Produce maintainable, performant C++20 code and supporting assets (tests, docs, CMake, CI) that follow SOLID and Clean Code.
* **Scope:** Libraries, services, tools, and samples. The agent may create new modules, extend existing ones, refactor safely, and write docs/tests.
* **Non‑Goals:** Experimental hacks, throwaway spikes without clear marking, or code that bypasses review/CI.
* **Mandatory:** Do NOT commit code that does not compile, fails tests, or violates formatting/linting rules.

---

## 2) Design Principles

* **SOLID** (Single Responsibility, Open/Closed, Liskov, Interface Segregation, Dependency Inversion).
* **Clean Code** (small functions, intention‑revealing names, no magic numbers, minimize mutable state, fail fast, clear boundaries).
* **C++ Core Guidelines** (lifetime safety, RAII, `gsl::span`, `not_null`, `unique_ptr` over raw owning pointers).
* **Prefer Composition** over inheritance; use `final` where appropriate.
* **Explicit APIs**: avoid implicit conversions, prefer `explicit` constructors.
* **Prefer Angle Bracket Includes**: Always include headers using `<project/module/foo.hpp>` instead of relative/local includes.
* **Directive:** Always use angle brackets (`#include <...>`) for all project and system headers. Quoted includes (`#include "..."`) are prohibited.
* **Always Use Curly Braces**: Even for single-line conditionals, loops, and return statements, curly braces must be used to ensure clarity and avoid errors.
* **Always Initialise Fields**: All member variables must be initialised, either with in-class initialisers or via constructor initialiser lists.
* **Prefer Brace Initialisation**: Use `{}` default initialisers when possible to avoid narrowing conversions and maintain consistency.
* **Use `const` Whenever Possible**: Apply `const` to variables, parameters, member functions, and pointers whenever immutability can be guaranteed.
* **Use `constexpr` and `consteval` Where Appropriate**: Prefer compile-time constants and evaluation when possible to improve safety, clarity, and performance.
* **Use `noexcept` Whenever Possible**: Mark functions `noexcept` when they are guaranteed not to throw, to improve optimisations and clarity of intent.
* **Prefer std::arrays**: Always `std::arrays` when possible
* **Avoid long methods**: Split long functions into smaller ones.
* **Prefer well-defined design patterns** 
* **Use the builder design pattern**: For complex object construction.
* **Use the factory design pattern**: For object creation without exposing the instantiation logic to
* **DO NOT WRITE CODE IN HEADER FILES**: always put implementation in a separate `.cpp` file.


---

## 3) Language & Library Policy (C++20)

* Use `std::unique_ptr` for ownership, `std::shared_ptr` only when shared ownership is required; pass views as `std::span<T>`/`std::string_view`.
* Ranges & algorithms (`<ranges>`, `<algorithm>`) preferred over manual loops when readability improves.
* Coroutines only with a clear scheduler/runtime and tests.
* Concepts for light constraints; avoid over‑templatization.
* Exceptions: allowed, but **narrowly**. Functions either throw or return `expected<T,E>`-like types; don’t mix.
* Concurrency: prefer `std::jthread`, `std::stop_token`; isolate shared state behind small, tested abstractions.

---

## 4) Project Structure

```
/<repo>
  /cmake/           # CMake modules/toolchain files
  /third_party/     # vendored deps (if any)
  /src/             # production code
  /include/         # public headers (installed)
  /tests/           # unit/integration tests
  /bench/           # microbenchmarks (if needed)
  /docs/            # markdown + diagrams
  /.github/workflows/ci.yml
  .clang-format
  .clang-tidy
  CMakeLists.txt
```

---

## 5) Build System (CMake) — Minimum Template

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyProject VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable warnings
if (MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion)
endif()

add_library(myproj STATIC
    src/foo.cpp
    src/bar.cpp
)

target_sources(myproj
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/myproj/foo.hpp
            include/myproj/bar.hpp
)

target_include_directories(myproj PUBLIC include)

add_executable(myproj_example examples/example.cpp)
target_link_libraries(myproj_example PRIVATE myproj)

include(CTest)
if (BUILD_TESTING)
  add_subdirectory(tests)
endif()
```

---

## 6) Formatting & Static Analysis

* **clang-format**: enforce consistently on all `.h/.hpp/.cpp/.cc` via CI.
* **clang-tidy**: enable core checks + modernize + readability + performance.
* **Pre-commit**: hooks for `clang-format -i` and `clang-tidy` (or `run-clang-tidy`).

### `.clang-format` (baseline)

```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 120
BreakBeforeBraces: Allman
AllowShortFunctionsOnASingleLine: None
DerivePointerAlignment: false
PointerAlignment: Left
IncludeBlocks: Regroup
SortIncludes: true
SpaceBeforeParens: ControlStatements
AlwaysBreakAfterControlStatementKeywords: true
BraceWrapping:
  AfterControlStatement: true
  AfterFunction: true
  AfterNamespace: true
  BeforeElse: true
```

---

## 7) Testing Strategy

* **Framework**: Catch2 or GoogleTest; deterministic, hermetic unit tests.
* **Structure**: `tests/unit/...`, `tests/integration/...` with clear naming.
* **Coverage**: target ≥ 80% for core modules; measure but don’t game.
* **Test First**: where practical; at least write tests with each feature.

### Test CMake (example)

```cmake
# tests/CMakeLists.txt
add_executable(myproj_tests
    tests_main.cpp
    foo_tests.cpp
    bar_tests.cpp
)
target_link_libraries(myproj_tests PRIVATE myproj Catch2::Catch2WithMain)
add_test(NAME myproj_tests COMMAND myproj_tests)
```

---

## 8) Error Handling & Logging

* **Return Types**: Prefer `std::expected<T, Error>` (or similar) for recoverable errors; throw for programmer errors.
* **Error Type**: small, copyable struct (code, message, context). No exceptions for control flow.
* **Logging**: `spdlog` or minimal adapter; no logging in hot loops without guards.

---

## 9) Dependency Management

* Prefer **FetchContent** for small libs; otherwise system packages/vcpkg.
* No global singletons. Use constructor injection or small factories.
* Keep third-party confined to `adapter/` boundaries; wrap behind interfaces.

---

## 10) Documentation

* Public headers documented with Doxygen‑style brief + details.
* Each module has a `README.md` covering purpose, API, and examples.
* Update `CHANGELOG.md` using Keep‑a‑Changelog format.

---

## 11) Code Review Checklist (Agent must self‑check)

1. Names are intent‑revealing; no abbreviations.
2. Functions ≤ \~30 lines; single responsibility.
3. No raw owning pointers; ownership clear.
4. Thread‑safety documented when applicable.
5. Tests added/updated; pass locally and in CI.
6. All files formatted; `clang-tidy` warnings addressed or justified.
7. Public APIs stable; breaking changes called out.
8. Error paths tested; log noise minimal.
9. **Includes use `<project/...>` style angle brackets only. Quoted includes are forbidden.**
10. **Curly braces must always be used**, even for single-line blocks (if, for, while, return).
11. **All fields must be initialised**; use in-class initialisers or constructor lists.
12. **Prefer brace `{}` initialisation** over `=` or `()` to avoid narrowing and ambiguity.
13. **Use `const` wherever possible** for variables, parameters, methods, and pointers to guarantee immutability.
14. **Use `constexpr` or `consteval` when compile-time evaluation is possible** to ensure correctness and enable optimisations.
15. **Use `noexcept` wherever possible** to mark functions that will not throw exceptions.

---

## 12) Performance Guidance

* Prefer value semantics with move; avoid needless heap.
* Benchmark critical paths (Google Benchmark); add baseline numbers.
* Avoid premature optimization; measure first.

---

## 13) Security & Safety

* Validate inputs at boundaries; sanitize file/Net IO.
* Avoid `new/delete` directly; prefer RAII.
* No undefined behavior; compile with UBSan/ASan in CI (where supported).

---

## 14) Agent Prompts & Operating Modes

### System Prompt (canonical)

> You are an AI coding agent specializing in C++20. Adhere strictly to SOLID, Clean Code, C++ Core Guidelines, and this repository’s tooling (CMake, clang-format, clang-tidy, Catch2/GoogleTest). Produce compilable code, minimal dependencies, and full tests/docs. Prefer composition, RAII, value semantics, and clear error handling (std::expected or exceptions, not both). All outputs must include: (1) code, (2) tests, (3) build instructions, (4) rationale.

### Developer Prompt (per‑task template)

```
Task: <clear, outcome-focused description>
Constraints: C++20, clang-format, clang-tidy clean, tests must pass.
Inputs: <files/interfaces/requirements>
Outputs: <new/changed files>
Quality Gates: review checklist items 1–15.
```

### Response Structure

1. **Design Sketch** (1–2 paragraphs)
2. **Code** (headers + impl)
3. **Tests**
4. **Build/Run** commands
5. **Notes** (trade-offs, future work)

---

## 15) Example: Small SOLID Class

```cpp
// include/myproj/clock.hpp
#pragma once
#include <chrono>

namespace myproj {
class Clock {
public:
    using steady = std::chrono::steady_clock;
    virtual ~Clock() = default;
    virtual steady::time_point now() const noexcept = 0;
};

class SteadyClock final : public Clock {
public:
    constexpr steady::time_point now() const noexcept override {
        return steady::now();
    }

private:
    const int exampleField {0}; // Always initialised with {}, const whenever possible
    static constexpr int defaultValue {42};
};
} // namespace myproj
```

```cpp
// tests/clock_tests.cpp
#include <catch2/catch_all.hpp>
#include <myproj/clock.hpp>

TEST_CASE("SteadyClock returns non-decreasing time") {
    const myproj::SteadyClock c;
    auto t1 = c.now();
    auto t2 = c.now();
    REQUIRE(t2 >= t1);
}
```

---

## 16) Git & CI Conventions

* **Commits**: Conventional Commits (`feat:`, `fix:`, `refactor:` …).
* **PR Template**: description, screenshots/logs, test plan, risks, rollout.
* **CI** must run: configure + build + tests + format + tidy + (asan/ubsan when feasible).

---

## 17) Definition of Done

* Code merged with green CI.
* Tests in place and meaningful.
* Docs updated.
* No `TODO`/`FIXME` left untracked.

---

## 18) Agent “Nice to Haves” Toolkit

* **Diagrams**: PlantUML/Mermaid snippets in `/docs` for architecture.
* **Dev Containers**: `.devcontainer/` with compilers and tools preinstalled.
* **Scripts**: `tools/format_all.sh`, `tools/run_tidy.sh`, `tools/build_debug.sh`.

---

## 19) Future Extensions

* Policy for exceptions vs `std::expected` (project‑wide default).
* Guidelines for modules (`export module`) when compilers stabilize.
* Coroutine patterns (scheduler, cancellation, testing recipe).

---

## 20) Quick Start (Agent Output Boilerplate)

1. **Create** module skeleton with CMake + headers + src + tests.
2. **Implement** feature behind interface; wire via DI.
3. **Format + Tidy**: run format and clang‑tidy locally.
4. **Test**: add unit/integration tests; ensure passing.
5. **Document**: README for the module; update CHANGELOG.
