# jobeval

Job opportunity evaluator: decide whether a job (or *not* working) is worth pursuing.
It turns wage, hours, commute, vehicle costs, benefits, schedule fit and career value
into take-home pay, a real hourly rate that counts the hours a job really consumes, and a
comparison across opportunities. Offline desktop app; region-specific rules (tax,
contributions, statutory minimums, benefits) come from data packs published by
[jobeval-data](https://github.com/atruenut/jobeval-data) and downloaded on demand.

## Status

Milestones 1-3 are done: engine and rule language, signed pack catalog with an offline
cache, and a Qt desktop app (pick a region, enter wage and hours, see take-home with a full
breakdown and explanation). Only Canada / New Brunswick 2026 packs exist so far.
You can keep several jobs in a project, compare them side by side, and save, load, import and export them (`docs/FILE_FORMAT.md`). Roadmap and log: `docs/DEVELOPMENT.md`. Design: `docs/ARCHITECTURE.md`. Contributing:
`CONTRIBUTING.md`.

**Windows download:** GitHub → Actions → latest run on `main` → artifact `JobEvaluator-windows`.

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
| `evalcore/tests` | Unit tests; `fixtures/` holds signed test catalogs made by `make_fixtures.py` with a throwaway TEST key |
| `evalcore/third_party/monocypher` | Vendored Monocypher 4.0.3 (BSD-2-Clause OR CC0-1.0) |
| `app/` | Qt 6 desktop app |
| `tools/` | `dev_site.py`: locally signed dev catalog |
| `docs/` | ARCHITECTURE, DEVELOPMENT log |

The pack format and rule language are specified in the data repo
(`docs/PACK_SPEC.md`).
