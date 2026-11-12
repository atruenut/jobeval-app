# Project and job files (`.jobeval`)

JSON, UTF-8. One format for whole projects and for exported jobs; importing lets the user
pick which jobs to take, so a job file is just a project file with `"kind": "jobs"`.

```json
{
  "format": "jobeval",
  "version": 1,
  "kind": "project",          // "project" or "jobs"; both are read the same way
  "name": "My jobs",
  "priorities": { "money": 8, "stress": 4 },  // optional: 0-10 weight per criterion (keys in app/JobResult.cpp)
  "baseline": "uuid",         // optional: id of a not-working entry used as the comparison baseline
  "jobs": [
    {
      "id": "uuid",           // regenerated on import if it collides
      "name": "Warehouse (nights)",
      "notes": "free text",
      "region": "CA-NB-2026", // pack id; the rules come from the user's installed packs
      "inputs": { "kind": 0,      // 0 job, 1 not working (then the nw_* keys apply)
                  "pay": 19.5, "hours": 44, "asof": "2026-11-10", "x.<packInput>": 0 }
    }
  ]
}
```

## Rules

- `inputs` keys are listed in `app/JobModel.cpp` (`fieldSpecs()`), with defaults, ranges and
  decimals. Missing keys take their default; out-of-range numbers are clamped; unknown keys
  are ignored; `x.<name>` keys carry a region pack's own inputs.
- Only inputs are stored, never results: results are always recomputed from the current
  rules, so a saved job re-evaluates with updated tax rules and a pack revision cannot make a
  file stale.
- Jobs reference a region by pack id. If the pack is not installed the job still loads and
  shows "rules not downloaded" until the user downloads that region.
- `version` is the file format version. A file with a higher version is refused with a
  message asking the user to update the app; the reader ignores unknown fields so additive
  changes need no bump. Bump `kFileVersion` only for changes an old reader would misread, and
  keep reading older versions.
- The app also keeps an autosave of the working project in its data folder
  (`session.jobeval`, same format) and restores it on start.
- Writes go through a temporary file and a rename, so an interrupted save cannot corrupt an
  existing file.
