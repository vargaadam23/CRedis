# CLAUDE.md

This is a learning project: a Redis server written in C (CodeCrafters "Build your own Redis" challenge). The user is writing the implementation themselves to learn C.

## Rules

- **Do not edit, change, or suggest changes to any file in `src/`** unless the user explicitly asks. This includes refactors, bug fixes, style tweaks, and "you might want to..." remarks about the implementation. Read the code freely for context, but leave it alone.
- If a test you write exposes a bug in `src/`, report the failing behavior (input, expected, actual) but do not propose or apply a fix unless asked.

## Your role

1. **Answer questions about the C language** (memory, pointers, strings, structs, the preprocessor, undefined behavior, build tooling, sockets/POSIX, etc.). Explain clearly and use small standalone examples; tie explanations to the user's code when relevant, without suggesting changes to it.
2. **Write unit and integration tests** that verify functionality.
   - Unit tests: exercise individual functions (e.g. the CRLF/RESP parsers) directly.
   - Integration tests: start the built `redis` server and talk to it over TCP (e.g. with `redis-cli` or a small client), checking RESP responses.

## Tests

- Tests live in `tests/` (e.g. `tests/test_clrf.c`) and are never placed in `src/`.
- Tests are registered in `CMakeLists.txt` under `if(BUILD_TESTING)` using `add_executable` + `add_test`. Test executables compile the needed `src/*.c` files with `target_include_directories(... PRIVATE src)`. Adding test targets to `CMakeLists.txt` is allowed; do not touch the `redis` target's sources.
- Keep tests plain C (no external framework unless the user asks); use `assert`-style checks or small helper macros, and print clear failure messages.
- Cover edge cases: empty input, malformed/truncated input, boundary lengths, and memory handling (check for leaks/invalid access when possible, e.g. with `-fsanitize=address,undefined` or valgrind).

## Build and run

- Language standard: C23 (`CMAKE_C_STANDARD 23`).
- Configure and build: `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build`
- Run tests: `ctest --test-dir build --output-on-failure`
- Run the server: `./your_program.sh`

## Layout

- `src/` – the user's implementation (hands off).
- `tests/` – tests (yours to write).
- `CMakeLists.txt` – build config; `your_program.sh` – CodeCrafters entrypoint.
