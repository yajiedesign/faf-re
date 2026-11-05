# Performance sampling tools

Reusable runner and log analyzer for engine performance runs. The current
analyzer understands the temporary Lua UI GC diagnostics (`[UIGC-P]` and
`[UIGC-T]`); additional event families can be added to `analyze_perf.py`
without changing the launch side.

## Files

- `run_perf.py` — launches one fixed-duration run, samples process CPU on
  Windows, captures its log, runs the analyzer and writes one JSON file.
- `analyze_perf.py` — parses the temporary frame/full/slice/cycle/heap logs
  into JSON. It can also be used standalone on an existing log.

The runner is path-agnostic: pass the executable, init script, replay, log and
output paths explicitly. It does not contain machine-specific paths.

## Example

```bash
python tools/perf/run_perf.py \
  --exe <build>/main_x64.exe \
  --init <init.lua> \
  --replay <replay.scfareplay> \
  --log <run.log> \
  --out <run.json> \
  --switch 1 \
  --seconds 120 \
  --simworkers 8
```

Run the same command with `--switch 0` as the baseline. The engine must contain
the temporary diagnostics used by `analyze_perf.py`.

## Output

The JSON contains:

- `frame`: all/steady frame-time summaries and threshold counts
- `full`: full UI collection summaries, including pre/post first frame
- `slices`: all/active/no-op slice summaries and budgets
- `cycles`: started/completed/aborted counts, slices and wall spans
- `heap`: heap samples and `nblocks`/threshold endpoints
- `cpu`: process user+kernel seconds and CPU per game minute
- `run` / `input` / `activity`: run metadata and sample window

Startup/session-load data is kept separate from steady-state frame data where
the log line order allows it. A full collection before the first frame sample is
reported in `full.before_first_frame`.

## Notes

- The current per-frame and per-slice diagnostics are temporary engine probes;
  remove them before merging the engine change.
- `analyze_perf.py` does not require the game to be running.
- Keep the launch command and binary identity in the JSON when comparing runs.
- CPU values require permission to query the process; they are `null` when the
  platform or access rights do not allow `GetProcessTimes`.
