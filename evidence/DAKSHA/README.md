# Daksha SG test evidence

This evidence records a fresh run on 2026-11-15 at commit
`09757541b9cbe3a79d30255fa0bd18f1a88a5d5e`. It is not a run from Nov 2–6.
The build and verbose CTest outputs are under `../automated_tests/`.

The 17 functional/security executable runs each exited 0 and each has a raw
`run.log`, exact `command.txt`, and timestamp/commit/exit record in
`execution_metadata.txt`. The executables emit suite-level success messages,
not case-specific assertion names or UI/API measurements. These cases are
therefore marked Partial. The three performance cases remain Deferred. No
per-case manual screenshots or timestamped videos were captured for this run.
