# Job Evaluator: user guide

Job Evaluator answers one question: **is this job worth it, compared with other jobs and with not working?**
It works offline. Region rules (tax, contributions, overtime) are small data packs downloaded the first time you
pick a region and cached on your computer afterwards.

## Quick start
1. **Region**: pick a country, then a province, state or nation. A badge shows how complete the rules are:
   *Full*, *Partial* (some items listed as not modelled) or *Rough estimate* (a large item is missing, so treat the result as approximate).
2. **Evaluate**: enter pay and hours. Pay can be hourly or salary (the other is worked out). Add commute, vehicle costs, benefits,
   unpaid time and company overtime policy. Every box shows its unit.
3. **Results** show take-home per month, the **real hourly** (what you keep per hour of your time, including commute and unpaid time),
   and where the tax went.
4. Add more jobs (File menu or the job list) and open **Compare**.

## Not working
Add a "not working" entry (unemployment benefit, studying, living on savings). Pick it as the **baseline** in Compare to see what each job
gives you *over* staying home, per month and per hour, including how long savings last.

## Compare
- Side-by-side table with the best value in each row highlighted, a fit score from the priorities you set, charts, a what-if scenario
  (raises, hours, commute, remote days), break-even, and what matters most.
- **Different currencies**: a "Compare in" bar appears. Type a rate for each currency or press **Fetch current rates** (ECB rates, needs internet).
  Jobs without a rate are shown but not ranked against the others.
- **Save table as CSV…** exports what you see.

## Files
Projects are saved as `.jobeval` (JSON, see `FILE_FORMAT.md`). Export/Import moves selected jobs between projects.

## Limits to keep in mind
Taxes are computed as if this job were your only income, single filing unless the region offers a choice. Credits, local taxes and some
deductions are not modelled; each pack lists them under "not modelled". Figures from third-party sources are labelled in the data repo
and worth checking against the official page before a big decision.
