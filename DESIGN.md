# Bounded product-row calendar

## Retained mathematics

With y=floor(cuberoot(N)), after marking primes up to y every surviving
composite is p*q with y<p<=q and p<=floor(sqrt(N)). The engine explicitly marks
these products in a W30 bitmap. The existing independent product audit H is
metadata only; H is not subtracted from a partial sieve count.

The prior engine revisits every active residual row in every segment after its
p^2 birth. At 512 KiB, that gives 3,342,769,123 row visits at one trillion and
97,407,590,242 at ten trillion. The target profile found 30.58% empty visits in
sampled ten-trillion segments. Skipping those visits is the design motivation.

## Direct and scheduled rows

For segment size B bytes and schedule factor F, rows p<=F*B retain the direct
loop. Larger rows use the calendar. F defaults to 2; this is a simple geometric
heuristic, not a proven optimal threshold. F=0 disables scheduling. The existing
cube-root frontier and dense marking are unchanged. At B=512 KiB and N<=10^12,
the default threshold exceeds sqrt(N), so every residual row remains direct.

The calendar has one bit per scheduled row per ring slot. A set bit means that
row's next unmarked product belongs to the segment represented by the slot.
Rows within a due slot are processed in increasing row order by scanning words
and extracting set bits. Empty words cost a word scan; empty row visits no
longer enter the product loop. This retains predictable row ordering without
allocating a linked-list node for each event.

Each due row has at least one mark to make. Its scalar loop processes products
until the next tag reaches the segment end, then reuses that tag to determine
the next visit. Busy direct rows retain the original four-product loop. The
scalar choice avoids a fourth-product lookahead for rows which usually make
only one or two marks per visit.

## Coverage invariant

1. Each task owns a disjoint contiguous territory. Its initial q cursor is the
   first prime q>=p whose product respects the territory's lower bound.
2. A scheduled row activates once when its p^2 enters the active prefix. If its
   next real product is inside the territory, exactly one calendar bit is set.
3. Processing that bit clears every product in the current segment and advances
   the cursor to the first product beyond the segment. A remaining product
   inside the territory is scheduled for its exact future segment.
4. The current slot is cleared before its set bits are processed. Reinsertions
   must go to a strictly later segment. The ring-size bound prevents a later
   rotation from colliding with the current slot.
5. No event is scheduled beyond the territory end or past the real cofactor
   array. The existing padded sentinel terminates the marking loop safely.

Thus each row still processes its products in increasing q order, with no
missing segment containing a product. The calendar changes when a row is
visited, not which composites are marked. Boundary bits above N remain cleared
by the existing endpoint handling. Dense marking and task ownership are retained.

## Ring horizon and memory bound

Let g be the maximum gap between consecutive real primes in the auxiliary
cofactor list; setup computes g exactly. A row p advances between consecutive
products by p*g at most. In segment coordinates the advance is bounded by

    floor(floor(sqrt(N))*g / (30*B)) + 1.

The calendar chooses the next power of two at least

    floor(floor(sqrt(N))*g / (30*B)) + 3,

leaving room for alignment and rounding. A row activated after its square has
the same gap bound on its first wake: its first cofactor is the first prime at
or beyond the lower bound. A row whose square lies in the current segment
starts at q=p. Events beyond the territory are discarded rather than inserted.

Insertion checks the forward distance against the ring horizon. Debug
validation also rejects a duplicate bit and verifies that every due row's
first product lies inside the segment. These extra checks are enabled in the
validation executables, not the performance executable.

Calendar storage is ring_slots * ceil(scheduled_rows/64) * 8 bytes per active
worker. Before allocation it is compared by division with a four-MiB budget.
If the bound would be exceeded or a representable ring cannot cover the
horizon, all rows use direct scanning. The fallback is reported in CLI JSON.
Allocation failures remain explicit. The previous N<=10^13 packed-arithmetic
guards are retained, and estimated engine storage now includes calendars.

Power-of-two segment sizes use shifts for segment coordinates; arbitrary
admitted byte sizes use exact division. The independent bitmap tests include
5000-byte segments. Calendar words and cursor arrays are private to each task,
and shared prime tables are read-only. Global counters use OpenMP reductions.

## What still needs measurement

Scheduling saves some row setup and probes but adds calendar reads, bit updates,
and a different memory pattern. A reduced number of row visits does not imply
the same percentage reduction in runtime. Dense marking remains a substantial
cost. The factor and memory budget are configurable controls, not CPU-specific
constants inferred from the i7 benchmark. Broader hardware measurements are
required before selecting a general default or claiming a speed advantage.
