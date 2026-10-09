# RED2 v0.35 — Blocked Cofactors

Experimental full prime sieve for 0 <= N <= 10,000,000,000,000, built on the frozen v0.34 Product Calendar control included here. The objective is to reduce the memory traffic and repeated dense-loop work that grow at high limits. This package does not establish a win over primesieve on every machine or limit.

At ten trillion, exact cofactor compression reduces the packed table from 98,249,536 to 30,702,980 bytes. The duplicate auxiliary prime list is replaced after setup by a 910,588-byte row-prime prefix. The tighter calendar bound reduces its allocation from 1,165,312 to 582,656 bytes per worker at a 512 KiB segment size. These are allocation measurements, not proportional speedup claims.

## Start here: short high-range screen

Extract this ZIP into a new folder, open MSYS2 UCRT64 in the folder containing the scripts, and run:

```bash
RED2_THREADS=12 bash screen_high.sh
```

This builds both versions, checks correctness, and compares selected windows at global limits of one, three and ten trillion. It tests v0.35 automatic tiles and an explicit 64 KiB tile against v0.34. It performs one warmup and two alternating measured pairs for each comparison. Upload the generated results ZIP.

The screen times the window traversal (`work_seconds`). A window's `bitmap_survivors` is the number of represented prime candidates in that window, **not pi(N)**. Setup and total times are retained separately. Short windows are a screening tool; they do not establish complete-range speed.

To check different thread counts, repeat with `RED2_THREADS=1` or `RED2_THREADS=6`. Run one benchmark at a time, with Blue3 and other CPU-intensive jobs stopped.

## Complete ten-trillion comparison with primesieve

```bash
RED2_LIMIT=10000000000000 RED2_THREADS=12 RED2_PAIRS=2 bash compare_high.sh "C:\Program Files\Primesieve\bin\primesieve.exe"
```

This includes one full warmup per engine and two measured pairs, with reversed execution order. It therefore runs **six complete sieves** and will take substantially longer than the screen. Every complete count must equal 346,065,536,839. Each engine reports its own internal elapsed time; process startup boundaries can differ. RED2 includes its internal preparation in `total_seconds`.

For a complete one-trillion regression comparison:

```bash
RED2_LIMIT=1000000000000 RED2_THREADS=12 RED2_PAIRS=2 bash compare_high.sh "C:\Program Files\Primesieve\bin\primesieve.exe"
```

Omit the primesieve argument to compare with the frozen v0.34 control. `RED2_PAIRS` accepts even values from 2 through 20. The comparison runner accepts powers of ten from 10^8 through 10^13, where it checks an independent known count.

## Controls

| Variable | Default | Meaning |
| --- | --- | --- |
| `RED2_THREADS` | OS logical processor count | Requested OpenMP workers |
| `RED2_SEGMENT_KIB` | 512 | Bitmap segment size in the full comparison |
| `RED2_COFACTOR_MODE` | `auto` | `auto`, `plain`, or `blocked` in the full comparison |
| `RED2_DENSE_TILE_KIB` | 0 | Automatic tile, or explicit 4 through 64 KiB |
| `RED2_CALENDAR_BOUND` | `tight` | `tight` or `original` |
| `RED2_BUILD_MODE` | `native` | Build for this host; use `portable` to omit `-march=native` |

The screen intentionally fixes segment size to 512 KiB and compares the automatic and 64 KiB tile choices. The other tuning variables above apply to the full comparison, not to its fixed screen matrix. Source code uses no CPU-model selection or newly required vector instruction set. Performance portability remains an empirical question; compiler, cache, memory bandwidth and thread count matter.

Automatic blocked storage activates when `floor(sqrt(N)) > 2 * segment_bytes`. In that geometry, the automatic dense tile becomes 32 KiB when the marking frontier exceeds 16,384; otherwise it remains 16 KiB. At one trillion and 512 KiB segments the automatic cofactor representation and tile remain plain and 16 KiB, respectively. These are explicit, testable heuristics, not proven optimal settings.

Examples after building:

```bash
./red2_v35.exe --limit 10000000000000 --threads 12 --segment-kib 512
./red2_v35.exe --limit 10000000000000 --threads 12 --segment-kib 512 --dense-tile-kib 64
./red2_v35.exe --limit 100 --list
```

On Linux, omit `.exe`. `--list` emits ordered primes and uses one worker. Counting still traverses the actual composite-marked bitmap; known counts are checked afterward, never used to compute the answer.

## Full-sieve status and validation

RED2 retains ordinary small-prime marking through at least the cube-root frontier, then explicitly marks the remaining prime products p*q. Every composite is covered, and every surviving represented bit identifies a prime. This is a full segmented sieve using two marking stages. It does not switch to an analytic prime-counting algorithm or probabilistic primality tests.

`validate_v35` compares full bitmaps through 100,000,007 in 181 cases, including both cofactor representations, calendar reuse/fallback, thread counts, tile sizes and segment boundaries. `window_v35` independently checks every byte of selected short windows using an ordinary segmented sieve. `validate_range_v35` traverses the complete range, checks every segment is delivered once, independently checks sampled windows, and verifies the known total count. Sampled high-range checking is explicitly distinguished from checking every high-range bit independently.

For a separate full traversal with sample verification:

```bash
./validate_range_v35.exe 10000000000000 12 512 1
```

See `DESIGN.md`, `LOCAL_RESULTS.md`, and the raw evidence. Source hashes identify the exact tested candidate and frozen control. The benchmark-only control adapter differs only in window selection; the complete-range control sources are unchanged.

The inherited proprietary source notices and `[YOUR LEGAL NAME]` placeholders are preserved. No license grant or novelty certification is added by this package.
