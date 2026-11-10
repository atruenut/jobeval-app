# jobeval-app

Offline desktop job-opportunity evaluator. Qt 6 Widgets UI planned (MinGW/GCC on Windows);
the engine is plain C++17 and must never depend on Qt or any UI code.

- `evalcore/` is jurisdiction-blind. No country, tax or benefit rule may be hard-coded;
  all of it comes from packs in jobeval-data (spec: that repo's `docs/PACK_SPEC.md`).
- Money is `Decimal` (fixed point, 9 places), never `double`. `to_double()` is for
  display and plotting only.
- Evaluation errors throw `EvalError`; pack problems are reported through `Diagnostics`.
  New rule-language functions go in `expr.cpp` (arity table) and `evaluator.cpp`, plus
  tests, plus a spec update in jobeval-data and an engine minor-version bump.
- Build and test: `cmake -S . -B build && cmake --build build && ctest --test-dir build`.
- The user does not compile locally: GitHub Actions builds the downloadable artifacts.
