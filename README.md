# RED2 v0.34 Product Calendar — experimental full sieve

This candidate replaces repeated empty visits to sparse product rows with a
bounded bitset calendar. Busy product rows retain direct scanning. Every
composite is still explicitly cleared from the sieve bitmap; the prime count
comes from the surviving bits. There is no scalar prime-count correction or
probabilistic primality shortcut.

The unchanged RANGE_10T engine is included as `red2_original` after building.
The candidate is `red2_calendar`. This is an experiment to evaluate on more
systems, not a claim of universal superiority over RED2 or primesieve.

## Start with the range and thread matrix

Extract into a NEW folder and open MSYS2 UCRT64 inside
`RED2_v0.34_PRODUCT_CALENDAR`:

```bash
bash run_matrix.sh
```

The default matrix compares unchanged RED2 with the candidate at one billion,
ten billion and 100 billion, using several segment sizes and 1, approximately
half, and all reported logical processors. It includes a 512 KiB control case
at 100 billion where the calendar is not needed. Each configuration gets one
excluded warmup per engine and two alternating measured pairs. Allow roughly
5–15 minutes on a machine like the target i7; other machines will differ.

Each configuration compares both engines with identical parameters. The matrix
varies parameters to test sensitivity; it is not an automatic tuning procedure
and does not establish that every chosen segment size is optimal.

## Ten-trillion comparisons

To compare the candidate with unchanged RED2:

```bash
bash compare_calendar.sh
```

To compare the candidate with primesieve on Mik's Windows installation:

```bash
bash compare_calendar.sh "C:\Program Files\Primesieve\bin\primesieve.exe"
```

The default is **ten trillion, all reported logical processors, 512 KiB RED2
segments, one warmup per engine and two alternating measured pairs**. Primesieve
uses 128 KiB segments. Only the selected reference is run. The script builds
both RED2 versions, runs their bitmap tests, checks an independent oracle with
active scheduled rows at one billion, then starts timing. Every large count
must match the known checkpoint. Timed calls can remain quiet for several
minutes before printing their results.

Allow roughly 40–60 minutes for the ten-trillion comparison on the target PC,
depending on the selected reference and actual candidate runtime. This is a
planning estimate. Upload the automatically generated results ZIP.

For one trillion or an explicit thread count:

```bash
RED2_LIMIT=1000000000000 RED2_THREADS=12 bash compare_calendar.sh "C:\Program Files\Primesieve\bin\primesieve.exe"
```

`RED2_LIMIT` accepts powers of ten from 10^8 through 10^13. `RED2_THREADS` accepts
1..1024; the default is the OS-reported logical processor count, falling back
to one. `RED2_PAIRS` accepts even values 2..20. `RED2_SEGMENT_KIB` accepts 4..8192.
The comparison fixes schedule factor 2, frontier multiplier 1, plate layer 163,
plate batch 4 and dense tile size 16 KiB. Actual worker counts and calendar
geometry are retained in each raw JSON result.

## Portability and reproducibility

The scheduling rule depends on row/segment geometry, not a CPU model name.
No processor-specific source intrinsics were added beyond portable compiler
bit operations and the existing platform branches. A power-of-two segment
uses an integer shift; other sizes use exact division. A four-MiB-per-worker
calendar budget falls back to direct scanning if exceeded.

By default both engines are compiled locally with the same `-O3 -march=native`
flags. For a build without native CPU targeting:

```bash
RED2_BUILD_MODE=portable bash run_matrix.sh
```

This removes `-march=native`; it does not make one executable run across
different instruction-set architectures. Recompile on each target. GCC/OpenMP
is the supplied build path. Local checks ran in an environment reporting AMD
EPYC hardware. Intel, other AMD systems, ARM and other compilers still need
their own correctness and performance checks for this candidate before claims
are made about them.

`RED2_MATRIX_THREADS="1 4 8"` overrides the matrix team list.
`RED2_MATRIX_CASES="1000000000:4 100000000000:64"` overrides N:segment-KiB cases.
`CC` and `CFLAGS` can override the compiler and flags, applied to both engines.
Do not run other heavy work alongside a performance comparison.

## Individual commands and ablations

```bash
bash build_calendar.sh
./red2_calendar.exe --limit 10000000000000 --threads 12 --segment-kib 512
./red2_calendar.exe --limit 100000000000 --threads 12 --segment-kib 64 --schedule-factor 0
```

Factor 0 disables the calendar within the candidate. Factors 1..64 set the
direct/queued boundary to factor times segment bytes. These are experimental
controls, not automatic performance recommendations. The ordinary v0.33 API
uses factor 2; the new v0.34 API exposes the factor and scheduling statistics.
On Linux omit `.exe`. `--list` retains ordered one-thread enumeration.

See DESIGN.md for the coverage and ring-size argument, LOCAL_RESULTS.md for
completed evidence and its limits, and CALENDAR.diff for all engine changes.
Original proprietary/confidential copyright headers and the literal
`[YOUR LEGAL NAME]` placeholder are preserved. No new license is granted.
