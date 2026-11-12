# Changelog

## 0.3.0
- Not-working situations (unemployment, student, savings) as first-class entries: benefit from the region pack, runway, time used.
- Many more job variables: remote days, signing bonus, expected stay, yearly raise, and 0-10 ratings (security, growth, interest, flexibility, culture, stress, learning, location).
- Salary and hourly pay convert into each other automatically when you switch.
- Compare rebuilt: overview with choosable rows and a "vs not working" baseline, charts (real hourly, where money and time go, time vs money, radar), weighted fit score from your priorities, what-if (break-even pay, pay curves, pay in N years).
- Project files gain optional `priorities` and `baseline` (additive; version stays 1).
- Engine: other income, base-pay override, CEA on gross (pack revision 3 in jobeval-data).

## 0.2.0
- Projects: several jobs, compare view, save/open/autosave, import/export selected jobs (`docs/FILE_FORMAT.md`).
- Company overtime policy on top of the legal minimum; salary basis, bonus, shift premium, paid days off, benefit premiums.
- Units shown on every input.

## 0.1.1
- Real hourly: commute, vehicle, unpaid time, benefits, work costs, paid vacation.
- Statutory overtime from packs (`overtime_rate`, `overtime_threshold_hours` slots).
- UI redesign with money and time breakdown tables.
- Fix: Windows bundle now includes all runtime DLLs; CI smoke-tests it on a clean PATH.
- Docs: ARCHITECTURE, DEVELOPMENT log, CONTRIBUTING, `tools/dev_site.py`.

## 0.1.0
- Engine, pack store, `eval-cli`, Qt desktop app (region picker, take-home evaluator).
