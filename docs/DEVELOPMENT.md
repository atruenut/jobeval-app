# Development log and roadmap

Newest first. Keep this current: one entry per merged milestone or notable decision.

## Roadmap

- [x] M1 engine, CLI, CI
- [x] M2 signed catalog, pack store
- [x] M3 desktop app: region picker, take-home evaluator, Windows zip
- [x] M4 real hourly: commute time/distance, vehicle costs, unpaid time, benefits value, statutory overtime
- [ ] M5 comparison of opportunities (saved jobs, side by side, verdicts)
- [ ] M6 not-working modes: unemployment programs, student, savings runway
- [ ] M7 pack expansion: other provinces, US, UK, 2027 draft pack; official CRA PDOC runs
- [ ] Installer / code signing, auto-update of the app itself, key-rotation plan
- [ ] Decide data licence (MIT now; CC BY 4.0 suggested)

## Log

### 2026-11-11 — M4 real hourly; tax audit
- Audited the CA/NB tax maths against CRA T4127 (2026). Brackets, BPA, CPP, CPP2 and EI were
  right; the pack was missing the Canada employment amount ($1,501), the CPP base-portion
  and EI credits, and the enhanced-CPP deduction, which overstated tax by a few hundred
  dollars a year. Added them (revision 2) with an independent reference implementation
  (`jobeval-data/tools/reference_ca_nb.py`) feeding the `derived` golden tests; the app's
  output matches it to the cent.
- Statutory overtime: packs expose slots `overtime_rate` and `overtime_threshold_hours`;
  `takehome` pays hours beyond the threshold at max(rate, wage). Toggle for salaried work.
- New engine module `realhourly`: real hourly = (take-home + benefits - commute and work
  costs) / (hours worked + commute + unpaid time). Paid vacation raises the figure because
  it is paid but has no commute. 295 unit checks, hand-verified expectations.
- UI: grouped inputs (job, getting there, benefits and costs), money and time tables,
  headline real-hourly card.
- Found: CPP rate cut to 9.5% reportedly from 2027 (unverified); noted in jobeval-data
  `CONTRIBUTING.md`.

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
- Windows zip is large (~dozens of DLLs: Qt is dynamically linked); could trim plugins or move to an installer.
- Distance unit only relabels fields; no currency/locale-specific number formatting yet.
- Taxes assume this is the only income for a full year; part-year and second-job cases are not modelled.
- Tax lines are simplified (taxable income = gross, most credits unmodelled).
- No installer; the Windows build is an unzipped folder, unsigned (SmartScreen may warn).
- Only Canada/NB packs exist.
