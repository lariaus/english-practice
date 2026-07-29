# Migration to Offline App — scratchpad

**Temporary.** This directory is a working scratchpad for planning and
tracking the big rewrite toward a native, offline-first, cross-device-synced
app. It exists to hold context across many work sessions so it doesn't need
to be re-derived from scratch each time. Once the migration is done, this
whole directory goes away — anything still true/useful at that point gets
folded into the permanent docs (`app-overview.md`, per-tool specs,
`common-design-philosophy.md`) and this directory is deleted.

Not a spec for the current app's behavior (that's what the rest of `docs/`
is for) — this is a plan/log for getting from here to there.

## Files

- **`A-vision-and-goals.md`** - the destination: what "done" looks like,
  the ordered checklist of big steps, and the guiding principles clarified
  so far (offline scope, storage-tier split, testing-from-day-one).
- **`B-current-architecture.md`** - a snapshot of how the app works today
  (as of starting this migration), captured so later steps have a fixed
  "before" picture to diff against instead of re-reading the whole codebase
  each time.
- **`C-migration-steps.md`** - the literal step-by-step checklist, tracked
  and updated as work happens (status, notes, decisions made per step).
- **`D-open-questions.md`** - things not yet resolved; check here before
  assuming a decision has been made.

These four use letters (not numbers) deliberately, to keep `step-1.md`,
`step-2.md`, etc. free for per-step task docs added as each step in
`C-migration-steps.md` actually starts.

## How to use this while working

- Update `C-migration-steps.md`'s status as steps start/complete.
- When a design decision gets made mid-step, record it in that step's notes
  (or in `D-open-questions.md` if it's still unresolved) rather than only
  in conversation - conversations don't persist, these files do.
- Keep `B-current-architecture.md` accurate only up to "when this migration
  started" - don't try to keep it live-updated as the rewrite progresses;
  once pieces move, that's what the steps doc and eventually the permanent
  docs are for.
