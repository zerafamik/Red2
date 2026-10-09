# Exact representations and high-range work

The creative search produced three useful implementation ideas: share the high part of nearby cofactor values, release redundant representations after construction, and bound the calendar using products that can actually occur. None requires assuming a newly discovered law of prime gaps.

## Full-sieve coverage

Let y = floor(cube_root(N)). Ordinary marking through at least y removes composites with a small prime factor. A surviving composite cannot have three prime factors counted with multiplicity: each would be at least y+1, making its value at least (y+1)^3 > N. Therefore every surviving composite is p*q with prime y < p <= q and p <= sqrt(N). The product stage enumerates and clears these products. This argument is elementary factorization; the optimization changes storage and scheduling, not the set of products marked.

## Blocked cofactors

For q coprime to 30, store its exact wheel coordinate

    t(q) = 8 * floor(q / 30) + lane(q mod 30).

For a consecutive index block starting at j0, store one 32-bit base `b=t(q[j0])` and one unsigned byte `d[j]=t(q[j])-b` per element. Decode with

    t(q[j]) = base[j >> shift] + delta[j].

Sorted wheel coordinates make the last-minus-first difference an upper bound on every delta in a block. Setup starts with 32-element blocks, tests all blocks including eight lookahead sentinels, and halves the block size until every delta fits in 0..255. Each encoded value is then individually checked for exact reconstruction. This finite check remains enabled in release builds. No gap-size assertion is trusted without checking the actual table. At ten trillion the selected block size is 16.

Original storage costs four bytes per cofactor. With 16-element blocks it costs 1 + 4/16 = 1.25 bytes per cofactor, apart from final rounding: a 68.75% reduction. This trades an extra base load and addition for a smaller table. Whether it saves time depends on the memory hierarchy and access pattern.

Prime values can be reconstructed from these wheel coordinates:

    q = 30 * (t >> 3) + residues[t & 7].

Keep the raw prime prefix through sqrt(N), because product rows need p directly. After encoding, release the larger raw list. Binary searches beyond that prefix reconstruct q as needed. The special primes 2, 3 and 5 remain in the raw prefix, so they never depend on the wheel encoding's special entries. No binary search includes the padded sentinels.

## A tighter calendar horizon

Let E = 30 * (floor(N/30)+1) - 1, the inclusive endpoint of the allocated bitmap bytes. Using E rather than N accounts for temporary marks in the partial final byte; clipping removes out-of-range bits afterward.

For consecutive prime cofactors a < b, a row that can make both successive products satisfies

    p <= a, p*b <= E, p <= sqrt(N).

Thus p is at most `min(a, floor(E/b), floor(sqrt(N)))`. Choose the largest actual scheduled row prime below that cap and bound its product displacement by p*(b-a). Taking the maximum over all admissible consecutive cofactor pairs bounds every rescheduling step that remains inside the allocated bitmap. The largest-row pointer decreases monotonically once the cofactors exceed sqrt(N).

The ring size is a power of two at least `floor(max_step/(30*segment_bytes))+3`. Forward-motion and horizon checks remain enabled; diagnostic builds also check due/duplicate scheduling. The existing four-MiB per-worker calendar budget and direct-row fallback are preserved. The original sqrt(N)*maximum_gap bound is an explicit control option.

## Dense tile geometry

The dense loop initializes or checks marking state for each tile. At high limits its cube-root frontier crosses the original 16 KiB tile scale. A larger tile can reduce repeated row/tile overhead, while increasing the working set competing for cache space. A 32 KiB automatic choice and explicit 16/32/64 KiB comparisons expose this tradeoff. Increasing the frontier or the whole segment is a different change and was not silently bundled into this version.

## Rejected direction and limits

The identity `floor(p*q/30)=p*u+a*r+floor(c*r/30)` for p=30a+c and q=30u+r can replace row offsets with a very small table. An initial prototype added multiplication in the inner loop and did not show a clear gain, so it was set aside.

The supported ceiling remains 10^13. Packed offsets, cursor indices, sentinels and intermediate products retain setup guards for that ceiling. Estimated peak memory counts setup and runtime allocation estimates; it is not measured resident memory. Compression does not change the asymptotic number of marked products, and no result here proves a universal advantage over another sieve.
