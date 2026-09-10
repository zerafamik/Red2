# Local results — 2026-09-10

These are local Linux/GCC 13.3.0 results. The captured environment reports an
AMD EPYC 9V74 80-Core Processor, with an eight-CPU cgroup budget; the advertised
model name is not the number of cores allocated to this run. These are not
results from Mik's PC and do not establish performance on other AMD processors,
Intel or ARM. Both engines used identical
-O3 -march=native C11/OpenMP flags.

## Sequential range and thread screen

Each row contains two paired comparisons in opposite orders, with all runs
retained. There were no excluded warmups for this local screen. These are
small exploratory samples, not confidence intervals. The supplied target
runners add an excluded warmup for each engine and configuration.

| N | KiB | Threads | Original median s | Candidate median s | Paired time reduction | Wins | Calendar active |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | :--- |
| 1,000,000,000 | 4 | 1 | 0.286811 | 0.203690 | 28.96% | 2/2 | yes |
| 1,000,000,000 | 4 | 4 | 0.077615 | 0.062074 | 19.98% | 2/2 | yes |
| 1,000,000,000 | 4 | 8 | 0.049269 | 0.035547 | 27.60% | 2/2 | yes |
| 10,000,000,000 | 16 | 1 | 2.313084 | 1.703761 | 26.34% | 2/2 | yes |
| 10,000,000,000 | 16 | 4 | 0.625823 | 0.489627 | 21.61% | 2/2 | yes |
| 10,000,000,000 | 16 | 8 | 0.351138 | 0.266085 | 24.17% | 2/2 | yes |
| 100,000,000,000 | 64 | 1 | 21.265803 | 17.758190 | 16.49% | 2/2 | yes |
| 100,000,000,000 | 64 | 4 | 5.739054 | 4.659641 | 18.80% | 2/2 | yes |
| 100,000,000,000 | 64 | 8 | 3.040932 | 2.473654 | 18.59% | 2/2 | yes |
| 100,000,000,000 | 512 | 1 | 11.576855 | 10.939109 | 5.51% | 2/2 | no |
| 100,000,000,000 | 512 | 4 | 3.134631 | 2.872821 | 8.35% | 2/2 | no |
| 100,000,000,000 | 512 | 8 | 1.690187 | 1.606481 | 4.91% | 2/2 | no |
| 1,000,000,000,000 | 512 | 8 | 25.734690 | 24.134284 | 6.21% | 2/2 | no |

The candidate won 26 of 26 pairs across these 13 configurations in this local
session. The nine configurations with active scheduling showed paired time
reductions between 16.49% and 28.96%. The segment sizes are explicitly listed:
these are not all default-size cases, and the matrix does not establish optimal
parameters for any machine.

Some improvement also appears when no row is scheduled. It must not all be
attributed to skipped visits: changes in compiler code generation, layout and
run conditions can affect the full candidate. This motivated the separate
same-binary ablation below.

## Same-binary scheduling ablation

At N=100 billion, 64 KiB segments and eight threads, two alternating pairs
compared factor 2 with factor 0 in the SAME candidate binary. Enabling scheduling
reduced paired runtime by **7.728466%**, winning both pairs. The exact row visits
fell from 922,332,561 to 782,627,517, including 224,666,276 scheduled visits.
This supports a scheduling benefit in this configuration; it is not a general
prediction of the ten-trillion speedup.

## Full ten-trillion correctness

The invariant-enabled candidate traversed every segment through 10^13 and
returned **346,065,536,839** primes. Every one of 635,783 segment callbacks
occurred once. All **103 independent sample windows** matched, totaling
53,937,494 W30 bytes or 431,499,952 represented candidate positions. This is
independent sampled comparison and full callback coverage, not an independent
comparison of every bit of the entire ten-trillion bitmap.

The run used eight workers and 32 tasks. Reported estimated engine storage was
228,346,717 bytes, including 1,165,312 calendar bytes per active worker. Oracle
memory is additional. The estimate is not measured process RSS. Small builds
and functional/sanitizer checks overlapped part of this correctness traversal;
its elapsed time is not used as a performance comparison.

## Other completed checks

- The original 133 full-bitmap cases passed on the candidate.
- The extended validator passed those 133 plus 24 scheduling-specific cases
  against an independent ordinary sieve through 100,000,007. Eight additional
  cases had scheduled rows. Tests include non-power-of-two 5000-byte segments,
  multiple factors, single-thread and multithread operation, and ring reuse.
- The 157-case validator also passed in a generic build without -march=native.
- AddressSanitizer and UndefinedBehaviorSanitizer passed a one-billion traversal
  with active scheduled rows and an independent sampled oracle, using 4 KiB
  segments. Leak detection was disabled.
- A forced 64-byte calendar budget exercised eight direct-scan fallback cases
  and passed all 157 bitmap cases.
- Seven requested workers with the runtime limited to two passed the sanitized
  oracle with active scheduling.
- Ordered enumeration through one million matched the original exactly.
- Complete original-reference, primesieve-reference and matrix shell workflows
  passed on small local intervals, including parsing, raw logs, checksums and
  result ZIP creation. These functional-test timings overlapped correctness work
  and are not performance evidence.
- The summary parser passed CRLF equivalence and rejected missing or duplicate
  rows and incorrect pair order.
- Frozen original source/header files match the delivered RANGE_10T package.

Windows native execution and PowerShell ZIP creation await the target run.
The same platform and path conversion patterns were used in earlier successful
packages. Further machines must build and test the source before performance
claims are extended to them.

## Final ten-trillion timing screen

The release candidate ran at ten trillion, eight threads and 512 KiB
segments after other heavy local work had stopped:

| Engine | Total seconds | Prime count |
| --- | ---: | ---: |
| Calendar candidate (first) | 453.898617437 | 346,065,536,839 |
| Unchanged original (second) | No completed result saved | — |

The benchmark session became inaccessible before the original control left a
saved result. Its process was no longer present; the precise cause is unknown.
**This ten-trillion timing comparison is incomplete, so no runtime ratio or
speedup is reported.** There was no excluded warmup for this particular timing
screen. The candidate time must not be compared directly with elapsed times
from Mik's different machine. The supplied runner can repeat a complete matched
comparison; it also supports primesieve as the reference.

The candidate visited 73,237,843,449 product rows: 48,808,312,611
direct and 24,429,530,838 scheduled. Against the original's
97,407,590,242 visits at the same N and segment size, this avoided
24,169,746,793 visits (24.81%). Calendar geometry was
145,622 scheduled rows, 64 ring slots and
1,165,312 bytes per worker. The measured maximum auxiliary
prime gap was 282; no memory fallback was used.

Raw results are in evidence/large_10t_screen.jsonl and the numerical review is
in evidence/large_10t_review.json.

Raw logs, per-run counters, source hashes and independent reviews are in
evidence/. The initial variable-division/four-way-loop prototype is preserved
separately as initial_prototype.zip; its timings are not final-release results.
