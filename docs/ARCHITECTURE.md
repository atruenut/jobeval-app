# Architecture

```
jobeval-data (GitHub)                    jobeval-app (this repo)
  packs-src/*.yaml  --build.py-->  evalcore  (C++17, no Qt, no country knowledge)
  tests-src/*.yaml     compile      ├ decimal / json / value     exact fixed-point numbers
  CI: validate, golden tests        ├ expr / evaluator           rule language + traces
      sign index.json (minisign)    ├ pack                       load, layer (extends), validate
      deploy GitHub Pages           ├ hash / minisign            SHA-256/512, BLAKE2b, Ed25519
            │                       ├ catalog / packstore        verify, plan, install, cache
            │ HTTPS                 ├ takehome                   gross -> net using std slots
            ▼                       eval-cli                     validate / eval / test / install
   index.json + .minisig            app/ (Qt 6 Widgets)
   packs/<id>-r<rev>.<hash8>.json    ├ NetworkFetcher            blocking fetch, worker thread only
                                     ├ PackService               QtConcurrent refresh/install
                                     ├ RegionPage / EvaluatePage / MainWindow
```

## Key decisions

- **Engine knows nothing about places.** A *pack* is data: parameters (optionally
  effective-dated), inputs, rules in a small expression language, and named output *slots*.
  Standard names shared by app and packs: inputs `gross_annual`, `wage`; slots
  `income_tax`, `contributions`. Spec: jobeval-data `docs/PACK_SPEC.md`.
- **Decimal, not double.** int128 scaled by 1e9; numbers keep their source text in JSON.
  GCC/Clang only (MSVC unsupported by design).
- **Trust chain.** Signed catalog (minisign Ed25519, key embedded in `app/Config.hpp`) →
  per-pack SHA-256 + size → pack validation → whole-bundle validation → all-or-nothing
  install. Catalog rollback is refused via its `generated` timestamp; the previous revision
  is kept; cached files are re-hashed on every load; filenames must match
  `packs/[A-Za-z0-9._-]+.json`.
- **Offline first.** Everything works from the cache; the network is only used to refresh packs, and for the optional, button-triggered exchange-rate lookup in Compare (`app/FxRates.*`, ECB data via api.frankfurter.dev; v1 endpoint, which its maintainers mark deprecated but still working, so revisit if it stops).
- **Honesty about coverage.** Packs declare a tier (estimate / partial / full) and
  `not_modelled`; the UI shows both. Results are for comparing jobs, not payroll.
- **Qt rules.** The app is compiled with `QT_NO_KEYWORDS` (Qt's `slots` macro collides with
  engine member names): use `Q_SIGNALS` / `Q_EMIT`. Network calls happen only on worker
  threads.

## Where to add things

| Task | Place |
|---|---|
| New country/province/year | jobeval-data only (packs + tests) |
| New rule-language function | `expr.cpp` arity table, `evaluator.cpp`, tests, spec update, engine minor bump |
| New computed output (commute, benefits, real hourly…) | new engine module like `takehome.cpp`, then UI |
| Change signing key | `app/Config.hpp` + data repo secret (rotation plan not yet written) |
