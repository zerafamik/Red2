# Local results — final v0.35 candidate

These are measurements on an AMD EPYC 9V74 host with an eight-CPU cgroup budget, Linux and GCC 13.3.0. They are separate from Mik's Windows PC results. The user's newly reported one-trillion win over primesieve was not independently retimed here. No new primesieve comparison was performed on this host.

Both versions used identical native build flags, 512 KiB bitmap segments, frontier multiplier 1, presieve 19, layer 163, plate batch 4 and schedule factor 2. OpenMP dynamic adjustment and binding were disabled, with OMP_PLACES unset. Full-range v0.34 control files are byte-for-byte unchanged. The benchmark-only control adapter is documented by `evidence/control_window_adapter.diff`.

All final timed runs were sequential after compilation and correctness jobs had ended. Each case used one warmup per engine and two measured pairs in opposite orders. Warmups were excluded, every measured run retained, and each result written immediately. Two pairs on one shared host do not establish a confidence interval, universal speedup, or hardware portability.

## Short windows

These timings measure only window traversal (`work_seconds`), not a complete sieve to the global N. Global N still determines the auxiliary table, marking frontier and product rows. Each segment is 524,288 W30 bytes, representing 15,728,640 integer positions before final clipping. All paired survivor counts matched. Independent oracle checks were run separately with instrumentation, not timed as these measurements.

| Global N | Threads | First segment | Segments | v0.34 median s | v0.35 median s | Paired time reduction | Wins |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1,000,000,000,000 | 8 | 60,000 | 2,048 | 0.970853 | 0.951225 | +1.99% | 1/2 |
| 3,000,000,000,000 | 8 | 180,000 | 2,048 | 1.323028 | 1.244771 | +5.93% | 2/2 |
| 10,000,000,000,000 | 1 | 300,000 | 256 | 1.410764 | 1.123807 | +20.29% | 2/2 |
| 10,000,000,000,000 | 4 | 150,000 | 1,024 | 1.328270 | 1.084904 | +18.19% | 2/2 |
| 10,000,000,000,000 | 8 | 630,000 | 2,048 | 1.918490 | 1.644596 | +14.19% | 2/2 |

The final candidate used automatic cofactor and tile choices. At 10T that means blocked cofactors and a 32 KiB dense tile; at 3T it means blocked cofactors and a 16 KiB tile; at 1T it means plain cofactors and a 16 KiB tile. The raw JSON preserves initialization and total times as well. Do not extrapolate a window percentage directly into a full-range time.

## Complete one-trillion regression check

Eight threads, 512 KiB segments; one warmup per engine plus two measured alternating pairs. Every run returned 37,607,912,018.

- v0.34 median total: **26.279602368 s**.
- v0.35 median total: **25.962955104 s**.
- Median paired v0.35/v0.34 ratio: **0.989538679**; time reduction **+1.05%**; wins **1/2**.

These use each RED2 CLI's total internal time, including preparation. They compare RED2 versions, not primesieve.

## Correctness evidence

- 181 full-bitmap cases through 100,000,007 passed in native and generic builds. They cover small-number edges, non-power-of-two segment sizes, calendar reuse, explicit plain/blocked storage, both calendar bounds, tile sizes and threads.
- A diagnostic 64-byte calendar budget exercised eight memory-fallback cases; all 181 bitmap cases passed.
- AddressSanitizer and UndefinedBehaviorSanitizer reported no error on a four-segment ten-trillion tail window with three workers, forced blocked storage and a 64 KiB tile. Leak detection was disabled for this run. This is a bounded sanitizer check, not exhaustive memory-safety proof.
- Eighteen additional independently checked windows cover 1T, 3T, the 16,385-cubed dense-tile boundary, 10T and a nearby non-round endpoint. They check 36,619,336 W30 bytes (292,954,688 represented positions), with zero mismatches.
- A complete ten-trillion traversal passed the known count and exactly-once segment-coverage checks. Its ordinary oracle independently checked 103 sample windows, not every high-range bitmap byte. This diagnostic traversal is not used as a performance comparison.

```
PASS: 103 independently checked sample windows at N=10000000000000; W30_bytes=53937494; represented_candidates=431499952; count=346065536839; oracle_root=3162277; count_checkpoint=pass; callback_segments=635783; bad_bytes=0; missing_or_repeated=0; engine_estimated_peak_bytes=133474717; actual_threads=8; tasks=32
```

The full comparison runner also passed an end-to-end 100-million smoke run against the frozen control. Its very short timings are not used as performance evidence. Windows/MSYS2 execution of the new package remains for the target run; native compilation, generic compilation, shell syntax, argument parsing and the runner were checked on Linux.

## Footprint at ten trillion

| Structure | v0.34 bytes | v0.35 bytes |
| --- | ---: | ---: |
| Packed cofactor representation | 98,249,536 | 30,702,980 |
| Raw prime values retained during traversal | 98,249,504 | 910,588 |
| Calendar per worker, 512 KiB segment | 1,165,312 | 582,656 |

These are exact allocation sizes for these structures. They do not imply a corresponding percentage reduction in elapsed time. Setup retains larger temporary allocations; the engine's peak-memory value is an estimate rather than measured RSS.

## Remaining decision

The high-range window results support trying this candidate, and complete-range correctness passed through 10T. They do not establish a full-10T speedup or a primesieve win. Run `screen_high.sh` on the target hardware first, then use `compare_high.sh` for a complete matched comparison. The 64 KiB tile remains an explicit alternative because exploratory prototypes showed mixed tile-size rankings.

Source, binary and environment hashes are in `evidence/local_environment.json`. Raw final measurements are `final_window_pairs.jsonl` and `full_1t_pairs.jsonl`. The `prototypes` folder records earlier experiments and is explicitly separate from final-release evidence. The Linux reproduction scripts require Python only for local evidence replay; the supplied target runners require Bash, GCC/OpenMP and standard MSYS2 tools.
