# Development log and roadmap

Newest first. Keep this current: one entry per merged milestone or notable decision.

## Roadmap

- [x] M1 engine, CLI, CI
- [x] M2 signed catalog, pack store
- [x] M3 desktop app: region picker, take-home evaluator, Windows zip
- [ ] M4 real hourly: commute time/distance, vehicle costs, unpaid time, benefits value
- [ ] M5 comparison of opportunities (saved jobs, side by side, verdicts)
- [ ] M6 not-working modes: unemployment programs, student, savings runway
- [ ] M7 pack expansion: other provinces, US, UK, 2027 draft pack; official CRA PDOC runs
- [ ] Installer / code signing, auto-update of the app itself, key-rotation plan
- [ ] Decide data licence (MIT now; CC BY 4.0 suggested)

## Log

### 2026-11-10 — M3 desktop app; Windows bundle fix
- Qt 6 Widgets app: region page (from signed catalog), evaluate page with live take-home,
  breakdown table, coverage notice and a "how this was calculated" trace.
- `takehome` engine module; 247 unit checks.
- First Windows zip failed to start (missing icu / pcre2 / double-conversion DLLs):
  `windeployqt` under MSYS2 does not copy transitive DLLs. CI now copies every `/ucrt64`
  dependency found by `ldd` and smoke-tests the zip with a clean PATH.
- Testing recipe: `tools/dev_site.py` + `QT_QPA_PLATFORM=offscreen --screenshot`.

### 2026-11 — M2 pack store
- Windows CI crash traced to CRLF conversion of signed fixtures; fixed with `.gitattributes`.
- GitHub Pages was redirecting to an expired custom domain; removed in repo settings.

### 2026-11 — M1 engine
- Canadian federal + New Brunswick 2026 packs (tier `partial`; many credits in
  `not_modelled`; income-tax golden tests stay `pending` until official CRA PDOC runs are
  recorded in jobeval-data `docs/OFFICIAL_RUNS.md`).

## Known gaps
- Tax lines are simplified (taxable income = gross, most credits unmodelled).
- No installer; the Windows build is an unzipped folder, unsigned (SmartScreen may warn).
- Only Canada/NB packs exist.
