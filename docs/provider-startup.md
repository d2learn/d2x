# Provider startup and transport contract

Since 2026.10.20.2, initial `describe` uses an absolute startup deadline (900s).
`exercises` and `check` use the existing idle limit (120s). The three Provider
verbs and protocol version 1 are unchanged. A valid describe must include
`protocol: 1` and a nonempty name. Old project configs need no changes.

Project `.d2x.json` example:

```json
{"buildtools":"my-provider","provider_startup_timeout":900,"provider_idle_timeout":120}
```

The startup field is a positive integer in seconds. Environment
`D2X_PROVIDER_STARTUP_TIMEOUT` overrides project, global and default values;
invalid effective settings fail before launching. `d2x config` preserves both
existing timeout fields when editing other settings.

Frontend additions (unknown events should be ignored by older clients):

```json
{"event":"preparing","phase":"describe","elapsed":0,"timeout":900}
{"event":"error","code":"startup_timeout","phase":"describe","message":"..."}
```

Preparation begins before session discovery, repeats every 10s, and does not
claim the child is making progress. It never refreshes the absolute deadline.
Only stdout is parsed as Provider NDJSON. Stderr cannot spoof protocol events.
Both streams drain independently, including large output without newlines.
Protocol lines above 1 MiB are discarded from parsing but retained in raw logs.
Unknown stdout lines are retained in logs; diagnostics never contaminate the
frontend stdout stream. Failure reports include bounded 8 KiB tails per stream,
command, cwd, elapsed time and child exit code when the command fails.

Error codes: `configuration`, `instance_lock`, `startup_timeout`, `idle_timeout`,
`provider_exit`, `provider_protocol`, `empty_course`, `cancelled`. Phases are
`configuration`, `describe`, `exercises`, `check`. Infrastructure errors return 1;
Ctrl+C during an active Provider call returns 130. A Provider exiting nonzero or
reporting error after partial success never advances learning progress.
Empty enumeration is a distinct fatal empty-course result. Normal completion
returns 0; an exercise verdict fail/blocked remains an interactive learning state.

Raw logs are overwritten when a new checker invocation begins; all three verbs
append to that invocation's logs. Timeouts/cancellation kill the process group on
POSIX and the Job Object on Windows, then reap the direct child. Startup fails
closed on Windows if process-tree containment cannot be established.

Run portable failure injection with `D2X=/absolute/path/to/d2x python tests/startup_test.py`.
`tests/e2e.sh` additionally covers save/advance, locking and print output flushing.
CI stages candidate binaries with `provenance.json` containing repository, checkout
SHA, binary version and SHA256. These are test artifacts, not stable releases.
