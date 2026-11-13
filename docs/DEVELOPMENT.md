# Development log and roadmap

Newest first. Keep this current: one entry per merged milestone or notable decision.

## Roadmap

- [x] M1 engine, CLI, CI
- [x] M2 signed catalog, pack store
- [x] M3 desktop app: region picker, take-home evaluator, Windows zip
- [x] M4 real hourly: commute time/distance, vehicle costs, unpaid time, benefits value, statutory overtime
- [x] M5 projects: several jobs per project, side-by-side comparison, save/open/autosave, import/export of selected jobs, company overtime and more pay types
- [x] M5b/M6 comparison and not-working modes: charts, fit score, sorting, what-if scenario, break-even, sensitivity, benefit duration and runway (0.3.0-0.4.0)
- [x] M7 packs: all Canadian provinces and territories (Quebec as an estimate), UK incl. Scotland, US federal + 15 states, filing-status choice (0.5.0-0.6.0)
- [ ] M7b still open: 2027 pack (needs CRA's January tables), more US states (NY, OR, IN, NC, ... need official sources), other countries, official calculator runs (`docs/OFFICIAL_RUNS.md`), Quebec to `partial` (needs TP-1015.F-V)
- [ ] Installer / code signing, auto-update of the app itself, key-rotation plan
- [x] Data licence decided: CC BY 4.0 for data and docs, MIT for tools (jobeval-data `LICENSE`; the full legal text is not yet in the repo, add it via GitHub's licence picker or copy from creativecommons.org)

## Log

### 2026-11-13 — sorting under test
- The job-list ordering moved out of `MainWindow` into `sortOrder()` in `JobResult.cpp`, so it has a unit test
  (`sortOrderModes`): all eleven modes, missing values last, jobs never reordered. Behaviour unchanged.
- Test names use one letter case on purpose: `localeAwareCompare` is codepoint-based in the C locale.

### 2026-11-12 (late night) — choice inputs, US filing status (0.6.0)
- Engine 1.1: `InputDef.options`, `type: choice` validated in pack.cpp (needs options, default must be
  one of them, options only on choice). Values stay numbers (`Value::num`), so job JSON, extras
  (`x.<name>`) and the not-working path needed no change. App: `EvaluatePage::extraChoices_` combo boxes.
- US-2026 revision 2 uses it for single / married filing jointly / head of household (standard
  deductions 16,100 / 32,200 / 24,150; MFJ brackets from the IRS release; HoH brackets from Tax
  Foundation citing Rev. Proc. 2025-32, flagged third_party). MFJ means "this job's pay only": spouse
  income is not modelled.
- Tests: new engine choice test; 21 derived US cases (3 statuses x 7 incomes) from tools/reference_us.py.

### 2026-11-12 (night) — US federal pack
- US-2026 added in jobeval-data (PR #6): single filer, standard deduction 16,100, seven brackets
  (IRS Rev. Proc. 2025-32 release), Social Security 6.2% to 184,500, Medicare 1.45%, Additional
  Medicare 0.9% over 200,000 (IRS Topic 751), FLSA overtime after 40 h at 1.5x (DOL Fact Sheet 23).
  No engine change. `taxable_income` is federal taxable income, the hook state packs will use.
- Not modelled: other filing statuses, credits, itemizing, the 2025-2028 overtime/tips deductions,
  all state/local tax, state unemployment benefits. Next: state packs (verify each from the state's
  revenue department; no-tax states are the easy start), then a filing-status input.

### 2026-11-12 (night) — PEI pack
- CA-PE-2026 added in jobeval-data (PR #5). The "footnote" worry was unfounded: CRA T4032PE and EY
  agree on 142,250 / KP 6,460 (the PEI finance page prints 142,520, apparently a typo; both values
  are continuity-consistent, so the check could not decide, the two agreeing sources did).
- New in 2026: a 20% bracket over 200,000, effective for all of 2026 (first published as 2027, then
  corrected). The PEI page calls it proposed (Bill 23); payroll applies it from July. Modelled with
  KP 8,460 by continuity; "enactment unconfirmed" is in not_modelled. Overtime: 48 h at 1.5x.
- Not modelled: PEI low-income tax reduction. Tests: 8 derived (incl. a 250,000 case).
- Next: 2027 pack when CRA publishes, then US.

### 2026-11-12 (evening) — M7 provinces
- Ten provincial/territorial packs added in jobeval-data (PR #4). Tax from CRA T4127 ed.122 chapter 8
  and T4032ON; overtime from each government's page where one was retrieved and read. NL (minimum
  overtime rate, multiplier unconfirmed) and YT (no source) have no statutory overtime on purpose.
  Quebec needs its own design (QPP, QPIP, abatement).
- Tests: derived from `tools/reference_ca_provinces.py`, which uses CRA's V x A - K form with the
  exact constants; it asserts the published rounded K values agree with the thresholds (within $1),
  which is what catches transcription typos.
- No engine change was needed. Open: official calculator runs for any province; 2027 pack (wait for
  the 2027 T4127 and CPP/EI announcements); US and UK.

### 2026-11-13 — Manual currency conversion in Compare
- Why: CA, US and GB jobs could not be ranked together. Rates are typed by the person (offline app, no invented figures); `Project.homeCurrency`/`rates` are stored under `currencies` in the project file.
- How: `applyRates()` sets each result's `fx` and `group` (its own currency, or the home currency if a rate exists); `scoreResults` scales money by `fx` and compares within `group`. Overview highlights, summary and baseline deltas use group values. Charts and the pay-curve view remain per-currency.
- Live rates: `fx::latestUrl/parseLatest` (pure, tested) + a button in the Compare FX bar. Asks for `base=<home>&symbols=<others>` and inverts, so the stored convention stays 1 <other> = x <home>. Never automatic; failures leave typed rates untouched. Changing the comparison currency clears rates (they were quoted in the old one). Could not call the API from the build sandbox (not allowlisted), so the reply shape was taken from the Frankfurter docs and is parsed defensively; confirm with a real click.
- Tests: JobModel currencies round trip, `ratesLetCurrenciesCompete`.

### 2026-11-12 (later) — M7 consistency audit, sorting, what-if controls
- Audit of the money/time chain found: (1) `benefit_weeks` was collected but never used, so a 26-week
  benefit was taxed and budgeted as a full year. `evaluate_not_working` now taxes year one on what is
  actually received and runs the runway in two stages (benefit, then other income only).
  (2) hours above 24 per working day and a signing bonus with no expected stay were silently accepted;
  both are now input errors. Confirmed consistent: hours "per week" are averaged over paid weeks
  everywhere; paid vacation adds pay but no hours; commute scales with days actually commuted.
- Baseline comparison uses year-one net income of the not-working entry (living costs are common to both
  sides and are left out).
- Job list sorting is a view (`MainWindow::order_`); the project order never changes. Pay sorts by
  hourly-equivalent wage; not-working entries sort last for pay/hourly keys.
- What if: `applyScenario()` in ComparePage.cpp applies the changes to `JobInputs`; break-even solves
  for pay on top of them. Dev flag: `--scenario key=value`.

### 2026-11-12 — M6 not working, more variables, advanced compare
- Engine: `notworking` (benefit from the pack slot `unemployment_weekly_benefit`, runway), `scoring`
  (weighted fit 0-100, 0-10 scales), overtime tiers, remote days, signing bonus. 372 checks.
- App: `JobResult` evaluates a job or not-working entry once for all views; `ChartWidget` paints
  every chart by hand (no Qt Charts dependency, keeps the Windows bundle small); `ComparePage` has
  Overview / Charts / Fit score / What if. Money is only ranked within one currency.
- Fit score: money and real hourly are relative to the best in the same currency; free time is
  absolute (0 h = 10, 60 h = 0); subjective criteria come from the job's own sliders.
- Break-even uses `solve_pay_for_real_hourly` (bisection). "Pay in N years" holds brackets fixed.
- Testing: `--subtab N --chart N` dev flags screenshot each view. A scratch cache needs
  `installed.json` + packs (copy from the dev site build) or jobs show "rules not downloaded".
- Needs jobeval-data PR (pack revision 3, EI benefit) merged to get benefits in not-working mode.

### 2026-11-11 (later) — M5 projects, company policy, units
- Engine: `jobcalc` evaluates a whole job. Overtime now has tiers: the region's legal
  minimum plus optional company policy (any multiplier, own threshold); each hour earns the
  highest applicable rate and never below the legal one. Added salary basis, bonus, shift
  premium, paid days off, benefit premiums. 324 engine checks, hand-computed expectations.
- App: project = list of jobs, each with its own region; compare view ranks by real hourly
  within a currency only; jobs list; autosave and restore; File menu (new/open/recent/save/
  import/export); `docs/FILE_FORMAT.md`; `jobmodel_test` (Qt Test) covers the file format.
- Every field now shows its unit (CAD/day, CAD/month, h, %, km...) plus a tooltip, and each
  group has a plain-language description.

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
- Break-even and pay-in-N-years hold tax brackets and costs fixed.
