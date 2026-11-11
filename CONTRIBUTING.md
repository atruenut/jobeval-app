# Contributing

Thanks for helping. Read `CLAUDE.md` (the project rules) and `docs/ARCHITECTURE.md` first;
`docs/DEVELOPMENT.md` is the working log and roadmap.

## The rules that matter

1. **`evalcore/` is jurisdiction-blind and GUI-free.** No country, tax or benefit figure in
   C++ and no Qt include in the engine. Rules live in data packs (jobeval-data).
2. **Money is `Decimal`**, never `double`. `to_double()` is for display only.
3. **Never invent figures.** Packs need a `source` for every parameter; unverified values
   stay out and are listed in `not_modelled`.
4. **Yearly changes are parameter edits.** New logic means a new engine primitive here,
   a spec update in jobeval-data, and an engine minor-version bump.
5. **Trust chain is sacred.** Nothing is installed unless the catalog signature, every
   pack hash and the whole-bundle validation pass. Do not weaken it to make something work.

## Workflow

```
cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
cmake -S . -B build-app -DJOBEVAL_BUILD_APP=ON      # needs Qt 6 (apt: qt6-base-dev)
```

- Branch from `main`, open a PR; CI builds Linux, macOS and Windows (MinGW) and runs the
  tests plus a check of the live published catalog. Keep it green.
- Add tests with every behaviour change (`evalcore/tests/test_main.cpp`).
- Windows packaging problems only show up on Windows: the CI "smoke test with a clean
  PATH" step exists for that. Do not remove it.
- Add a line to `CHANGELOG.md` and, for design decisions, to `docs/DEVELOPMENT.md`.

## Running the app without the real signing key

`tools/dev_site.py` makes a locally signed catalog from the data repo's `dist/`; see its
header for the environment variables. Headless screenshots:
`QT_QPA_PLATFORM=offscreen build-app/app/jobeval --demo CA-NB-2026 --screenshot out.png`.

## Data contributions

Pack changes belong in jobeval-data (see its `CONTRIBUTING`/`PACK_SPEC.md`). Golden tests
marked `official` must come from an official calculator run with inputs and date recorded.
