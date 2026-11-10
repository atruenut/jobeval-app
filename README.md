# jobeval

Job opportunity evaluator: decide whether a job (or *not* working) is worth pursuing.
It turns wage, hours, commute, vehicle costs, benefits, schedule fit and career value
into take-home pay, a real hourly rate that counts the hours a job really consumes, and a
comparison across opportunities. Offline desktop app; region-specific rules (tax,
contributions, statutory minimums, benefits) come from data packs published by
[jobeval-data](https://github.com/atruenut/jobeval-data) and downloaded on demand.

## Status

Milestones 1 and 2 are in place:

- `evalcore`: GUI-free C++17 engine. Exact decimal arithmetic, the pack rule language
  (parser, evaluator with explainable traces), pack loading with layering and validation,
  and a golden-test runner.
- Pack handling (M2): minisign signature verification (Ed25519 via vendored Monocypher,
  SHA-512/SHA-256 in-house), catalog parsing, install planning along `extends` chains, and
  `PackStore`, an offline cache that only accepts signed, hash-checked, validated packs,
  refuses rollback to an older catalog, keeps the previous revision, and re-verifies cached
  files on every load. It does no networking itself: callers supply a `Fetcher`.
- `eval-cli`: `validate`, `eval`, `test`, `verify-catalog`, `install`, used by the data
  repo's CI and by the live-catalog check here.

Not built yet: HTTP fetcher and the Qt interface, workspace file, benefit and
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
| `evalcore/tests` | Unit tests; `fixtures/` holds signed test catalogs made by `make_fixtures.py` with a throwaway TEST key |
| `evalcore/third_party/monocypher` | Vendored Monocypher 4.0.3 (BSD-2-Clause OR CC0-1.0) |
| `docs/` | Design notes |

The pack format and rule language are specified in the data repo
(`docs/PACK_SPEC.md`).
