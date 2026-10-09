# Exploratory prototypes, before the final release

These sources and raw short-window screens document the search. They do not contain the final raw-prime-list trimming or geometry-based automatic selection. `bench.c` includes the selected source through `RED2_VARIANT`, e.g.:

```bash
gcc -O3 -march=native -std=c11 -DNDEBUG -Wall -Wextra -Wpedantic -fopenmp -DRED2_VARIANT=\"compressed_tight_tile32.c\" bench.c -lm -o bench
./bench 10000000000000 8 512 600000 2048 2
```

Run this from the prototypes folder. Its survivor count describes only the selected bitmap window, not pi(N). The window count checks in these screens only compare engines; independent byte checking was added for the final release. Initial screens used reverse-order repetitions without a formal warmup protocol, and shared-host timing drift was material. Treat their results as exploratory. `LOCAL_RESULTS.md` at the package root reports final-release checks and measurements separately.
