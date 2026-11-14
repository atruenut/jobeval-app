# Changelog

## 0.8.0
- App icon (Windows executable and window), a fuller About box with licences and engine version, Help menu: Check for app updates (asks GitHub only when you click) and a link to the online user guide.

## 0.7.1
- Fix: the job input pane was clipped on the right on Windows (display scaling made the form wider than its fixed width). It now sizes itself to the form, and long labels wrap above their box.

## 0.7.0
- Compare: type exchange rates (or press "Fetch current rates") to rank jobs paid in different currencies; "Save table as CSV…".
- Data packs added since 0.6.0 need no new app: all 50 US states and DC, Australia, Germany, Ireland and New Zealand (see jobeval-data `docs/COVERAGE.md`; several are marked "Rough estimate").
- Tagging `v*` now publishes a GitHub Release with the Windows zip.

## 0.6.0
- Compare: "Save table as CSV…" exports the table as shown (for spreadsheets).
- Compare: when jobs use more than one currency, a "Compare in" bar lets you type exchange rates (nothing is downloaded or guessed). With a rate set, those jobs are highlighted, scored and ranked against each other in your chosen currency; without one they stay separate. Saved in the project file. Optional "Fetch current rates" button (you press it; nothing is fetched automatically) gets European Central Bank reference rates from the open Frankfurter API; only currency codes are sent, and the source and date are shown. Typing a rate overrides it. Charts stay per-currency.
- Engine 1.1: packs can declare pick-one inputs (`type: choice` with numeric options); the app shows them as drop-downs in the Region-specific box. Documented in jobeval-data `docs/PACK_SPEC.md`.
- Data: United States (federal income tax, Social Security, Medicare, overtime) with a filing-status choice (single, married filing jointly, head of household), plus first state packs (nine no-wage-tax states, Pennsylvania, Illinois). Needs this version or newer for the US pack.

## 0.5.0
- Region list now covers ten more Canadian provinces and territories (data packs only; no engine change): BC, AB, SK, MB, ON (surtax, health premium), NS, NL, YT, NT, NU. Prince Edward Island was added afterwards as a data pack (no app release needed). Quebec is not included yet. See jobeval-data `docs/COVERAGE.md`.
- Region page lists provinces alphabetically and picks one by default instead of "whole country" (renamed "Federal rules only").

## 0.4.0
- Job list can be ordered: added, newest, name, pay, real hourly, take-home, hours, fit score, region (remembered; the project keeps its own order). Compare columns follow it.
- What if rebuilt: Scenario tab (pay change, raises over N years, inflation, hours, commute, remote days, bonus, benefits, costs, vacation, side by side with today), Break-even (match a job or your own figure, on top of the scenario), What matters most (one-at-a-time sensitivity), Pay curves.
- Fix: the unemployment benefit now ends after "Benefit lasts" weeks: year-one tax counts only what is received, and savings runway runs through benefit and post-benefit stages.
- Checks: hours per week cannot exceed 24 per day worked; a signing bonus needs an expected stay.
- Job page inputs regrouped: Pay and hours, Overtime, Bonuses and extras, Time off and working from home.

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
