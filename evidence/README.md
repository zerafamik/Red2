# Package evidence guide

`full_10t_oracle.txt` is a complete range traversal with independent sampled bitmap and full segment coverage checks. `oracle_windows.jsonl` and `sanitizer_10t_tail.txt` check every byte of the specified short windows. `validation_v35.txt`, `validation_portable.txt` and `validation_fallback.txt` check full small-range bitmaps.

`final_window_pairs.jsonl`, `full_1t_pairs.jsonl` and `local_environment.json` record the final source and binary builds measured sequentially after other computation ended. `final_summary.json` derives medians from measured pairs only. `runner_smoke` records the end-to-end script check at 100 million; its source-check manifest is the package snapshot at that time, before final evidence was added.

`prototypes` contains exploratory sources and preliminary windows. These are not the final release. The target profile review is inherited from v0.34 and is input evidence, not a v0.35 performance result.

The two `reproduce_local_*.py` scripts document Linux evidence generation and write fresh logs when rerun. They are not prerequisites for the MSYS2 runners. Source files at the package root and `SHA256SUMS.txt` define the delivered snapshot. Build executables locally with `build_high.sh`.
