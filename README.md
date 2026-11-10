# jobeval

Job opportunity evaluator: decide whether a job (or *not* working) is worth pursuing.
It turns wage, hours, commute, vehicle costs, benefits, schedule fit and career value
into take-home pay, a real hourly rate that counts the hours a job really consumes, and a
comparison across opportunities. Offline desktop app; region-specific rules (tax,
contributions, statutory minimums, benefits) come from data packs published by
[jobeval-data](https://github.com/atruenut/jobeval-data) and downloaded on demand.

## Status

Milestone 1 (foundation) is in place:

- `evalcore`: GUI-free C++17 engine. Exact decimal arithmetic, the pack rule language
  (parser, evaluator with explainable traces), pack loading with layering and validation,
  and a golden-test runner.
- `eval-cli`: `validate`, `eval` and `test` commands, used by the data repo's CI.

Not built yet: pack download/verification, workspace file, the Qt interface, benefit and
unemployment programs, the timeline simulator.

## Build

Requires CMake 3.21+ and GCC (including MinGW-w64) or Clang. MSVC is not supported (the
decimal type uses a 128-bit integer).

```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Layout

| Path | Contents |
|---|---|
| `evalcore/include`, `evalcore/src` | Engine library |
| `evalcore/cli` | `eval-cli` |
| `evalcore/tests` | Unit tests |
| `docs/` | Design notes |

The pack format and rule language are specified in the data repo
(`docs/PACK_SPEC.md`).
