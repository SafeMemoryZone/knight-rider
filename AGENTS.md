# Repository Guidelines

## Project Structure & Module Organization

Hemlok is a C++20 sampling profiler with a Dear ImGui interface backed by GLFW and OpenGL. Public headers live in `include/`, with implementations in matching `src/process/`, `src/profiler/`, and `src/ui/` directories. Include headers using project-relative paths, such as `<process/process.hpp>`.

`src/app.cc` is the executable entry point; reusable code builds into `hemlok_lib`. Tests mirror the modules under `tests/`, with a process fixture in `tests/process/fixtures/`. Formatting utilities and hooks live in `scripts/` and `.githooks/`. Keep generated files in the ignored `build/` directory.

## Build, Test, and Development Commands

Use CMake 3.20 or newer and a C++23 compiler. Configuration fetches GLFW, Dear ImGui, Boost, and Catch2; OpenGL must be available locally.

- `cmake -S . -B build`: configure the project with tests enabled.
- `cmake --build build`: compile the application, library, and tests.
- `./build/hemlok`: launch the graphical application.
- `ctest --test-dir build --output-on-failure`: run the test suite with failure details.
- `cmake -S . -B build -DHEMLOK_BUILD_TESTS=OFF`: configure without tests or Catch2.
- `./scripts/format.sh`: format tracked C/C++ files with `clang-format`.
- `git config core.hooksPath .githooks`: enable staged-file formatting checks for this clone.

## Coding Style & Naming Conventions

Follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html). Use the repository's `.clang-format` for formatting and retain `.cc` implementations and `.hpp` headers with include guards.

- Types (classes, structs, aliases, enums): `PascalCase`, e.g. `ProfilingTarget`.
- Functions and methods: `PascalCase`, e.g. `AddProfilingTarget()`.
- Variables and parameters: `snake_case`, e.g. `executable_path`.
- Class data members: trailing underscore, e.g. `next_target_id_`; struct members have no trailing underscore.
- Constants and enumerators: `kPascalCase`, e.g. `kNameCapacity`.
- Namespaces: lowercase with underscores where needed, e.g. `hemlok::profiler`.
- Filenames: lowercase with underscores, e.g. `profiling_target.hpp`; macros and include guards: `UPPER_SNAKE_CASE`.

## Data Records

Use structs only for data records, with public fields. Use classes for types that manage resources, implement behavior, or enforce invariants. Prefer immutable records after construction or publication. Enforce read-only access through const references or pointers at consumer boundaries; avoid `const` data members when they would prevent assignment or efficient movement.

## Change Scope

Keep changes focused on the requested task. Avoid unrelated refactoring unless explicitly requested.

## Error Handling

Return `std::expected` errors only for expected operational failures or invalid user input. Examples include a missing executable or permission denial in `Process::CreateFromExecutable()`, process status query failures in `Process::IsRunning()`, termination failures in `Process::Terminate()`, and blank names or empty/relative paths in `ProfilingTarget::Create()`.

Programming bugs and violated internal preconditions should be asserted, not represented as `std::expected` errors. For example, passing an unknown `TargetId` to `Profiler::StartRun()` or an unknown `RunId` to `Profiler::TerminateRun()` is a caller bug; assert that the ID exists before using it. Keep required operations outside assertions, since assertions may be disabled in release builds.

## Testing Guidelines

Tests use Catch2 v3 and CTest. Name test sources `*_test.cc`; CMake discovers these automatically. Use descriptive `TEST_CASE` names and module tags such as `[process]`. Add tests for changed behavior, including failure paths, and use the supplied process fixture for subprocess tests. No numerical coverage threshold is configured.

## Commit & Pull Request Guidelines

For commits on local branches, use a title only, with no body. Follow Conventional Commit prefixes such as `feat:`, `fix:`, `docs:`, `test:`, `refactor:`, or `chore:`, optionally scoped: `feat(ui): add executable path validation`.

For merge commits, keep Git's default title unchanged. After a blank line, add a descriptive paragraph explaining the merged work, followed by bullet points summarizing key changes and validation.

Keep commits focused and review formatting before staging. Pull requests should describe behavior changes, link relevant issues, and report build/test results.
