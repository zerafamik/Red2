# RED2 v0.34 Product Calendar

**Exact full prime sieve with bounded sparse-row scheduling.**

RED2 v0.34 is an experimental C/OpenMP full prime sieve using W30 segmented bitmaps, cube-root residual-product closure, and a bounded product-row calendar. The calendar changes **when** sparse residual rows are revisited; it does not change **which** composites are marked.

## Release evidence

- Correct full count through `10^13`: `346,065,536,839` primes.
- 157 bitmap validation cases passed.
- 103 independent sample windows at `10^13` matched, covering 431,499,952 represented candidate positions.
- Every one of 635,783 segment callbacks in the 10T invariant-enabled run occurred exactly once.
- Ordered enumeration through `10^6` matched exactly: 78,498 primes.
- ASan/UBSan one-billion traversal passed (leak detection disabled).
- Forced calendar-memory fallback and reduced-runtime-team tests passed.

## Performance

In the supplied local Linux/GCC 13.3.0 matrix, v0.34 won **26/26 measured pairs** against unchanged RED2 across 13 configurations from `10^9` through `10^12`. When the calendar was active, paired time reductions ranged from **16.49% to 28.96%**.

A same-binary ablation at `10^11`, 64 KiB segments and 8 threads measured a **7.728%** paired runtime reduction with scheduling enabled.

At `10^13`, the candidate reduced exact residual row visits by **24.81%** versus the independently reviewed baseline. The archive does **not** contain a completed matched 10T original timing, so no 10T speedup percentage is claimed.

At `10^12` / 512 KiB / 8 threads, v0.34 measured **24.134 s** vs **25.735 s** for unchanged RED2 (**6.21%** paired reduction). The calendar is inactive at this configuration, so that difference must not be attributed solely to scheduling.

## External comparator status

Historical target evidence for RED2 v0.33 reports **20/20 measured pair wins** over the installed `primesieve` executable at `10^11` on an Intel Core i7-8700K, with paired reductions of about **6.31%–6.91%** across two sessions.

**A direct RED2 v0.34 vs primesieve comparison at `10^12` is still pending.** This repository therefore does **not** claim “fastest in the world up to one trillion.” That would require broader comparable benchmarking and independent reproduction.

## Reproduce the 1T comparison

From MSYS2 UCRT64 on Windows:

```bash
RED2_LIMIT=1000000000000 RED2_THREADS=12 RED2_PAIRS=10 \
  bash compare_calendar.sh "C:\Program Files\Primesieve\bin\primesieve.exe"
```

Run with other heavy workloads stopped and retain the generated results ZIP.

## Benchmark claim boundary

A successful same-machine 1T run would support wording such as:

> RED2 v0.34 beat the installed primesieve build at N=10^12 on the tested i7-8700K configuration.

It would **not by itself** establish a global fastest-sieve ranking. Specialist `primecount` algorithms are also a different comparator class: they can count `pi(x)` without constructing a full prime bitmap.

## Sources

Release evidence is under `evidence/`, with integrity hashes in `SHA256SUMS.txt`. Public comparator documentation:

- https://github.com/kimwalisch/primesieve
- https://github.com/kimwalisch/primecount

## License note

The supplied release retains proprietary/confidential source headers and states that no new license is granted. Choose the repository license intentionally before describing the source as open source.
