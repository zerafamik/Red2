/* =========================================================================
   PROJECT: ASTER MOVIE SIEVE (v0.33 Tiled Product Full Sieve)
   COPYRIGHT (C) 2026 [YOUR LEGAL NAME]. ALL RIGHTS RESERVED.
   
   PROPRIETARY & CONFIDENTIAL MATERIAL.
   This software source code, architectural design, data structures, and 
   underlying mathematical kinematics concepts are the sole intellectual 
   property of the author. 
   
   Strictly prohibited: Unauthorized copying, modification, distribution, 
   compilation, reverse-engineering, or commercial use of this file, 
   in whole or in part, via any medium without explicit written consent.
   ========================================================================= */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "red2_v34.h"

_Static_assert(sizeof(size_t) >= 8, "RED2 range extension requires a 64-bit build");
_Static_assert(RED2_RANGE_MAX_N <= UINT64_C(10000000000000),
               "A larger limit requires a new numeric-bound audit");

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * RED2 v0.33 - Product Completion Full Sieve.
 * Derived from the retained v0.22 kernels and v0.30 threshold construction.
 * After ordinary marking to y>=floor(cuberoot(N)), explicitly strike every
 * residual prime product y<p<=q, p*q<=N. No scalar correction is subtracted.
 * q=30*u+r gives product block p*u+floor(p*r/30) and a fixed lane mask.
 * Every delivered segment is fully sieved and may be enumerated.
 * This uses classical factorization facts; mathematical novelty is unproved.
 */

/*
 * v0.14 extends the measured plate frontier beyond 83 while preserving the same layered method.
 * Mik's v0.13 sweep at 10^11 still improved at <=83, so this version adds
 * three more three-prime periodic plates and lets measurement find the true
 * crossover:
 *   <=101 {89,97,101}; <=109 {103,107,109}; <=131 {113,127,131}.
 * Existing layers remain separate; no fused plate trick is reintroduced.
 */
#include <time.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <malloc.h>
#endif

/*
 * ASTER Movie Sieve C v0.16 - batched plate passes
 *
 * Visual anchors:
 *   1 = grey stable reference, not prime
 *   2 = first red prime
 *
 * Survivors carried forward:
 *   W30 dense geometry; presieve <=19; merged W30 cycles; thread-owned
 *   territories; explicit dense_cutoff; W210 geometry for all sparse motion.
 *
 * v0.9 separated cache room size from motion scale. Mik's 1e11 measurements
 * then froze the current experimental point at roughly:
 *   segment = 128 KiB, dense_cutoff = 80000, threads = 12.
 *
 * v0.10 attacks only the sparse scheduler. A W210 prime whose maximum possible
 * strike gap is smaller than one segment cannot actually sleep across a whole
 * future segment. v0.9 still unlinked and relinked that prime through bucket
 * heads every frame. v0.10 keeps such primes in one contiguous resident array
 * and walks the active prefix directly (largest p first). Truly distant sparse
 * primes retain W210 motion but use contiguous bucket pages instead of one
 * linked node per event.
 *
 * Dense/sparse mathematics remains explicit:
 *   p < dense_cutoff  -> merged W30 cycles
 *   p >= dense_cutoff -> W210 sparse geometry
 *
 * Sparse scheduling becomes:
 *   gap < territory  -> resident contiguous W210
 *   otherwise        -> paged W210 buckets
 *
 * Keep what survives.
 */

static const uint8_t W30_RES[8] = {1, 7, 11, 13, 17, 19, 23, 29};
static const int8_t W30_LANE[30] = {
    -1, 0,-1,-1,-1,-1,-1, 1,-1,-1,
    -1, 2,-1, 3,-1,-1,-1, 4,-1, 5,
    -1,-1,-1, 6,-1,-1,-1,-1,-1, 7
};
static const uint32_t EXTRA_SMALL_PRIMES[] = {7u, 11u, 13u, 17u, 19u};

static const uint8_t W210_RES[48] = {
      1, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47,
     53, 59, 61, 67, 71, 73, 79, 83, 89, 97,101,103,
    107,109,113,121,127,131,137,139,143,149,151,157,
    163,167,169,173,179,181,187,191,193,197,199,209
};

typedef struct {
    uint8_t unset_mask;
    uint8_t next_factor; /* W210 multiplier gap: 2,4,6,8,10 */
    uint8_t correct;
    uint8_t _pad0;
    uint16_t next;
    uint16_t _pad1;
} wheel210_elem;

typedef struct {
    wheel210_elem e[8 * 48];
    int8_t phase_of_residue[210];
} wheel210_table;

static wheel210_table build_wheel210_table(void) {
    wheel210_table w;
    memset(&w, 0, sizeof(w));
    for (int i = 0; i < 210; ++i) w.phase_of_residue[i] = -1;
    for (int i = 0; i < 48; ++i) w.phase_of_residue[W210_RES[i]] = (int8_t)i;

    for (int ci = 0; ci < 8; ++ci) {
        uint32_t c = W30_RES[ci];
        for (int ph = 0; ph < 48; ++ph) {
            uint32_t s = W210_RES[ph];
            uint32_t ns = (ph == 47) ? 210u + W210_RES[0] : W210_RES[ph + 1];
            uint32_t gap = ns - s;
            uint32_t rem = (c * (s % 30u)) % 30u;
            int8_t lane = W30_LANE[rem];
            if (lane < 0 || (gap & 1u) || gap > 10u) {
                fprintf(stderr, "internal W210 transition invariant failed\n");
                exit(2);
            }
            wheel210_elem *x = &w.e[ci * 48 + ph];
            x->unset_mask = (uint8_t)~(uint8_t)(1u << (unsigned)lane);
            x->next_factor = (uint8_t)gap;
            x->correct = (uint8_t)((rem + c * gap) / 30u);
            x->next = (uint16_t)(ci * 48 + ((ph + 1) % 48));
        }
    }
    return w;
}

/* Prefer the native high-resolution Windows counter. */
double red2_v33_now_seconds(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq;
    static int initialized = 0;
    LARGE_INTEGER counter;
    if (!initialized) {
        QueryPerformanceFrequency(&freq);
        initialized = 1;
    }
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#elif defined(CLOCK_MONOTONIC_RAW)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
}

const char *red2_v33_timer_name(void) {
#ifdef _WIN32
    return "QueryPerformanceCounter";
#elif defined(CLOCK_MONOTONIC_RAW)
    return "CLOCK_MONOTONIC_RAW";
#else
    return "CLOCK_MONOTONIC";
#endif
}

int red2_v33_max_threads(void) {
#ifdef _OPENMP
    return omp_get_max_threads();
#else
    return 1;
#endif
}

static void *aligned_malloc64(size_t n) {
    if (n == 0) n = 64;
#ifdef _WIN32
    return _aligned_malloc(n, 64u);
#else
    void *p = NULL;
    if (posix_memalign(&p, 64u, n) != 0) return NULL;
    return p;
#endif
}

static void aligned_free64(void *p) {
#ifdef _WIN32
    _aligned_free(p);
#else
    free(p);
#endif
}

static uint64_t isqrt_u64(uint64_t n) {
    uint64_t x = (uint64_t)sqrt((double)n);
    while ((x + 1u) != 0u && (x + 1u) <= n / (x + 1u)) ++x;
    while (x > 0u && x > n / x) --x;
    return x;
}

typedef struct {
    uint32_t *data;
    size_t count;
    size_t peak_bytes; /* transient sieve bits + returned prime list */
} prime_list32;

static inline int bit_is_set(const uint8_t *bits, size_t i) {
    return (bits[i >> 3] >> (i & 7u)) & 1u;
}

static inline void bit_set(uint8_t *bits, size_t i) {
    bits[i >> 3] |= (uint8_t)(1u << (i & 7u));
}

/* Odd-only bit sieve.  v0.22's byte sieve was already tiny at sqrt(1e11),
   but v0.30 also needs an auxiliary prime list through about N^(2/3).
   Keeping one bit per odd number prevents the closure table from becoming
   the new memory bottleneck. */
static prime_list32 base_primes_upto(uint64_t limit) {
    prime_list32 out = {0};
    if (limit < 2u) return out;
    if (limit > UINT32_MAX) {
        fprintf(stderr, "base-prime limit too large for this prototype\n");
        exit(2);
    }

    size_t odd_count = limit >= 3u ? (size_t)((limit - 3u) / 2u + 1u) : 0u;
    size_t bit_bytes = (odd_count + 7u) >> 3;
    uint8_t *composite = bit_bytes ? (uint8_t *)calloc(bit_bytes, 1u) : NULL;
    if (bit_bytes && !composite) {
        fprintf(stderr, "allocation failed for odd base sieve (%zu bytes)\n", bit_bytes);
        exit(2);
    }

    uint64_t root = isqrt_u64(limit);
    for (uint64_t p0 = 3u; p0 <= root; p0 += 2u) {
        size_t idx = (size_t)((p0 - 3u) >> 1);
        if (bit_is_set(composite, idx)) continue;
        uint64_t step = p0 << 1;
        for (uint64_t m = p0 * p0; m <= limit; m += step) {
            bit_set(composite, (size_t)((m - 3u) >> 1));
        }
    }

    size_t count = 1u; /* prime 2 */
    for (size_t i = 0; i < odd_count; ++i) count += !bit_is_set(composite, i);

    uint32_t *pr = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!pr) {
        free(composite);
        fprintf(stderr, "allocation failed for base-prime list\n");
        exit(2);
    }

    size_t j = 0u;
    pr[j++] = 2u;
    for (size_t i = 0; i < odd_count; ++i) {
        if (!bit_is_set(composite, i)) pr[j++] = (uint32_t)(2u * (uint64_t)i + 3u);
    }
    if (j != count) {
        free(pr); free(composite);
        fprintf(stderr, "internal base-prime count mismatch\n");
        exit(2);
    }

    out.data = pr;
    out.count = count;
    out.peak_bytes = bit_bytes + count * sizeof(uint32_t);
    free(composite);
    return out;
}

static inline int cube_leq_u64(uint64_t a, uint64_t n) {
    if (a == 0u) return 1;
    return a <= (n / a) / a;
}

static uint64_t icbrt_u64(uint64_t n) {
    uint64_t x = (uint64_t)cbrt((double)n);
    while (x < UINT64_MAX && cube_leq_u64(x + 1u, n)) ++x;
    while (!cube_leq_u64(x, n)) --x;
    return x;
}

static size_t upper_bound_prime32(const uint32_t *a, size_t n, uint64_t value) {
    size_t lo = 0u, hi = n;
    while (lo < hi) {
        size_t mid = lo + ((hi - lo) >> 1);
        if ((uint64_t)a[mid] <= value) lo = mid + 1u;
        else hi = mid;
    }
    return lo;
}

static inline int wheel_base_prime(uint32_t p) {
    return p == 2u || p == 3u || p == 5u;
}

static inline int is_presieved_prime(uint32_t p, int presieve_max) {
    return p >= 7u && p <= 19u && (int)p <= presieve_max;
}

typedef struct {
    uint8_t *mask;             /* one byte per W30 block */
    size_t period_blocks;
    uint32_t primes[5];
    size_t prime_count;
    int max_prime;
} presieve_plan;

static presieve_plan build_presieve(int requested_max) {
    presieve_plan ps = {0};
    ps.max_prime = requested_max;
    ps.period_blocks = 1u;

    for (size_t i = 0; i < sizeof(EXTRA_SMALL_PRIMES)/sizeof(EXTRA_SMALL_PRIMES[0]); ++i) {
        uint32_t p = EXTRA_SMALL_PRIMES[i];
        if ((int)p <= requested_max) {
            ps.primes[ps.prime_count++] = p;
            if (ps.period_blocks > SIZE_MAX / p) {
                fprintf(stderr, "presieve period too large\n");
                exit(2);
            }
            ps.period_blocks *= p;
        }
    }

    ps.mask = (uint8_t *)aligned_malloc64(ps.period_blocks);
    if (!ps.mask) {
        fprintf(stderr, "allocation failed for presieve mask (%zu bytes)\n", ps.period_blocks);
        exit(2);
    }
    memset(ps.mask, 0xFF, ps.period_blocks);

    /* Mass-print selected small-prime W30 trajectories into one periodic plate. */
    for (size_t pi = 0; pi < ps.prime_count; ++pi) {
        uint32_t p = ps.primes[pi];
        for (size_t si = 0; si < 8u; ++si) {
            uint32_t s = W30_RES[si];
            uint32_t prod = p * s;
            int8_t lane = W30_LANE[prod % 30u];
            size_t k0 = (size_t)(prod / 30u);
            uint8_t keep = (uint8_t)~(uint8_t)(1u << (unsigned)lane);
            for (size_t k = k0; k < ps.period_blocks; k += p) ps.mask[k] &= keep;
        }
    }

    return ps;
}

static void free_presieve(presieve_plan *ps) {
    if (!ps) return;
    aligned_free64(ps->mask);
    memset(ps, 0, sizeof(*ps));
}

typedef struct {
    uint8_t *mask;
    size_t period_blocks;
    uint32_t primes[3];
    size_t prime_count;
} presieve_layer;

#define MAX_PRESIEVE_LAYERS 12u

typedef struct {
    presieve_layer layer[MAX_PRESIEVE_LAYERS];
    size_t layer_count;
    int max_prime;
} layered_plan;

static const uint32_t LAYER_GROUPS[MAX_PRESIEVE_LAYERS][3] = {
    {23u, 29u, 31u},
    {37u, 41u, 43u},
    {47u, 53u, 59u},
    {61u, 67u, 71u},
    {73u, 79u, 83u},
    {89u, 97u, 101u},
    {103u, 107u, 109u},
    {113u, 127u, 0u},
    {131u, 137u, 0u},
    {139u, 149u, 0u},
    {151u, 157u, 0u},
    {163u, 0u, 0u}
};
static const uint8_t LAYER_COUNTS[MAX_PRESIEVE_LAYERS] = {3,3,3,3,3,3,3,2,2,2,2,1};
static const int LAYER_MAX_PRIME[MAX_PRESIEVE_LAYERS] = {31,43,59,71,83,101,109,127,137,149,157,163};

static presieve_layer build_layer(const uint32_t *pr, size_t cnt) {
    presieve_layer l = {0};
    l.period_blocks = 1u;
    l.prime_count = cnt;
    for (size_t i=0;i<cnt;i++) {
        l.primes[i]=pr[i];
        if (l.period_blocks > SIZE_MAX / pr[i]) { fprintf(stderr,"layer period too large\n"); exit(2); }
        l.period_blocks *= pr[i];
    }
    l.mask=(uint8_t*)aligned_malloc64(l.period_blocks);
    if(!l.mask){fprintf(stderr,"layer alloc failed\n");exit(2);}
    memset(l.mask,0xFF,l.period_blocks);
    for(size_t pi=0;pi<cnt;pi++){
        uint32_t pp=pr[pi];
        for(size_t si=0;si<8;si++){
            uint32_t ss=W30_RES[si];uint32_t prod=pp*ss;int8_t lane=W30_LANE[prod%30u];size_t k0=prod/30u;
            uint8_t keep=(uint8_t)~(uint8_t)(1u<<(unsigned)lane);
            for(size_t k=k0;k<l.period_blocks;k+=pp)l.mask[k]&=keep;
        }
    }
    return l;
}
static layered_plan build_layered(int maxp){
    layered_plan lp={0}; lp.max_prime=maxp;
    for(size_t gi=0;gi<MAX_PRESIEVE_LAYERS;gi++) if(maxp>=LAYER_MAX_PRIME[gi]) lp.layer[lp.layer_count++]=build_layer(LAYER_GROUPS[gi],LAYER_COUNTS[gi]);
    return lp;
}
static void free_layered(layered_plan *lp){for(size_t i=0;i<lp->layer_count;i++)aligned_free64(lp->layer[i].mask);memset(lp,0,sizeof(*lp));}
static inline void and_periodic_layer(uint8_t *dst,size_t n,uint64_t glo,const presieve_layer*l){size_t pos=(size_t)(glo%l->period_blocks);while(n){size_t chunk=l->period_blocks-pos;if(chunk>n)chunk=n;const uint8_t*src=l->mask+pos;size_t i=0;for(;i+8<=chunk;i+=8){uint64_t a,b;memcpy(&a,dst+i,8);memcpy(&b,src+i,8);a&=b;memcpy(dst+i,&a,8);}for(;i<chunk;i++)dst[i]&=src[i];dst+=chunk;n-=chunk;pos=0;}}

/* v0.16: batch two/three/four independent periodic plates into one destination
   pass.  Source masks keep their own periods; only the hot territory read/write
   is shared.  This attacks destination traffic without constructing a huge LCM
   fused mask. */
static inline void and_periodic_pair(uint8_t *dst,size_t n,uint64_t glo,const presieve_layer*a,const presieve_layer*b){
    size_t pa=(size_t)(glo%a->period_blocks), pb=(size_t)(glo%b->period_blocks);
    while(n){
        size_t chunk=a->period_blocks-pa, cb=b->period_blocks-pb; if(cb<chunk)chunk=cb; if(chunk>n)chunk=n;
        const uint8_t*sa=a->mask+pa; const uint8_t*sb=b->mask+pb; size_t i=0;
        for(;i+8<=chunk;i+=8){uint64_t v,x,y;memcpy(&v,dst+i,8);memcpy(&x,sa+i,8);memcpy(&y,sb+i,8);v&=x&y;memcpy(dst+i,&v,8);} 
        for(;i<chunk;i++)dst[i]&=(uint8_t)(sa[i]&sb[i]);
        dst+=chunk;n-=chunk;pa+=chunk;pb+=chunk;if(pa==a->period_blocks)pa=0;if(pb==b->period_blocks)pb=0;
    }
}
static inline void and_periodic_triple(uint8_t *dst,size_t n,uint64_t glo,const presieve_layer*a,const presieve_layer*b,const presieve_layer*c0){
    size_t pa=(size_t)(glo%a->period_blocks),pb=(size_t)(glo%b->period_blocks),pc=(size_t)(glo%c0->period_blocks);
    while(n){
        size_t chunk=a->period_blocks-pa, q=b->period_blocks-pb;if(q<chunk)chunk=q;q=c0->period_blocks-pc;if(q<chunk)chunk=q;if(chunk>n)chunk=n;
        const uint8_t*sa=a->mask+pa,*sb=b->mask+pb,*sc=c0->mask+pc;size_t i=0;
        for(;i+8<=chunk;i+=8){uint64_t v,x,y,z;memcpy(&v,dst+i,8);memcpy(&x,sa+i,8);memcpy(&y,sb+i,8);memcpy(&z,sc+i,8);v&=x&y&z;memcpy(dst+i,&v,8);} 
        for(;i<chunk;i++)dst[i]&=(uint8_t)(sa[i]&sb[i]&sc[i]);
        dst+=chunk;n-=chunk;pa+=chunk;pb+=chunk;pc+=chunk;if(pa==a->period_blocks)pa=0;if(pb==b->period_blocks)pb=0;if(pc==c0->period_blocks)pc=0;
    }
}
static inline void and_periodic_quad(uint8_t *dst,size_t n,uint64_t glo,const presieve_layer*a,const presieve_layer*b,const presieve_layer*c0,const presieve_layer*d){
    size_t pa=(size_t)(glo%a->period_blocks),pb=(size_t)(glo%b->period_blocks),pc=(size_t)(glo%c0->period_blocks),pd=(size_t)(glo%d->period_blocks);
    while(n){
        size_t chunk=a->period_blocks-pa,q=b->period_blocks-pb;if(q<chunk)chunk=q;q=c0->period_blocks-pc;if(q<chunk)chunk=q;q=d->period_blocks-pd;if(q<chunk)chunk=q;if(chunk>n)chunk=n;
        const uint8_t*sa=a->mask+pa,*sb=b->mask+pb,*sc=c0->mask+pc,*sd=d->mask+pd;size_t i=0;
        for(;i+8<=chunk;i+=8){uint64_t v,x,y,z,w;memcpy(&v,dst+i,8);memcpy(&x,sa+i,8);memcpy(&y,sb+i,8);memcpy(&z,sc+i,8);memcpy(&w,sd+i,8);v&=x&y&z&w;memcpy(dst+i,&v,8);} 
        for(;i<chunk;i++)dst[i]&=(uint8_t)(sa[i]&sb[i]&sc[i]&sd[i]);
        dst+=chunk;n-=chunk;pa+=chunk;pb+=chunk;pc+=chunk;pd+=chunk;if(pa==a->period_blocks)pa=0;if(pb==b->period_blocks)pb=0;if(pc==c0->period_blocks)pc=0;if(pd==d->period_blocks)pd=0;
    }
}
static inline void apply_layer_batches(uint8_t *dst,size_t n,uint64_t glo,const layered_plan*lp,int batch){
    size_t i=0;if(batch<1)batch=1;if(batch>4)batch=4;
    if(batch==4){for(;i+4<=lp->layer_count;i+=4)and_periodic_quad(dst,n,glo,&lp->layer[i],&lp->layer[i+1],&lp->layer[i+2],&lp->layer[i+3]);}
    else if(batch==3){for(;i+3<=lp->layer_count;i+=3)and_periodic_triple(dst,n,glo,&lp->layer[i],&lp->layer[i+1],&lp->layer[i+2]);}
    else if(batch==2){for(;i+2<=lp->layer_count;i+=2)and_periodic_pair(dst,n,glo,&lp->layer[i],&lp->layer[i+1]);}
    for(;i<lp->layer_count;i++)and_periodic_layer(dst,n,glo,&lp->layer[i]);
}
static inline int is_layered_prime(uint32_t p,int maxp){
    for(size_t gi=0;gi<MAX_PRESIEVE_LAYERS;gi++){
        if(maxp<LAYER_MAX_PRIME[gi]) break;
        for(size_t j=0;j<LAYER_COUNTS[gi];++j) if(p==LAYER_GROUPS[gi][j]) return 1;
    }
    return 0;
}

static inline void copy_periodic_mask(
    uint8_t *dst,
    size_t n,
    uint64_t global_block_lo,
    const presieve_plan *ps
) {
    if (ps->period_blocks == 1u) {
        memset(dst, ps->mask[0], n);
        return;
    }

    size_t pos = (size_t)(global_block_lo % ps->period_blocks);
    while (n) {
        size_t chunk = ps->period_blocks - pos;
        if (chunk > n) chunk = n;
        memcpy(dst, ps->mask + pos, chunk);
        dst += chunk;
        n -= chunk;
        pos = 0u;
    }
}

/*
 * One prime's eight independent v0.5 paths, merged into one periodic cycle.
 * offset[0] is always 0. The same 8-strike pattern repeats at base += p.
 */
typedef struct {
    uint64_t cycle_base0;
    uint32_t p;
    uint32_t offset[8];
    uint8_t clear_mask[8];
} prime_cycle;

typedef struct {
    prime_cycle *data;
    size_t count;
    uint32_t max_p;
} cycle_table;

typedef struct {
    uint64_t block;
    uint8_t mask;
} hit_pair;

static inline void sort8_hits(hit_pair x[8]) {
    /* Insertion sort is tiny and only runs during setup. */
    for (int i = 1; i < 8; ++i) {
        hit_pair v = x[i];
        int j = i - 1;
        while (j >= 0 && x[j].block > v.block) {
            x[j + 1] = x[j];
            --j;
        }
        x[j + 1] = v;
    }
}

static cycle_table build_cycles(uint64_t limit, int presieve_max, int layer_max, uint32_t max_sieving_prime) {
    cycle_table out = {0};
    uint64_t root = isqrt_u64(limit);
    if (root > (uint64_t)max_sieving_prime) root = (uint64_t)max_sieving_prime;
    prime_list32 base = base_primes_upto(root);

    size_t active = 0;
    for (size_t i = 0; i < base.count; ++i) {
        uint32_t p = base.data[i];
        if (wheel_base_prime(p) || is_presieved_prime(p, presieve_max) || is_layered_prime(p, layer_max)) continue;
        ++active;
    }

    prime_cycle *g = active ? (prime_cycle *)aligned_malloc64(active * sizeof(prime_cycle)) : NULL;
    if (active && !g) {
        free(base.data);
        fprintf(stderr, "allocation failed for prime-cycle table\n");
        exit(2);
    }

    size_t q = 0;
    uint32_t max_p = 0;
    for (size_t pi = 0; pi < base.count; ++pi) {
        uint32_t p = base.data[pi];
        if (wheel_base_prime(p) || is_presieved_prime(p, presieve_max) || is_layered_prime(p, layer_max)) continue;

        prime_cycle *x = &g[q++];
        x->p = p;
        if (p > max_p) max_p = p;

        uint32_t c = p % 30u;
        uint64_t a = ((uint64_t)p - c) / 30u;
        hit_pair hit[8];

        for (size_t i = 0; i < 8u; ++i) {
            uint32_t s = W30_RES[i];
            uint64_t t = a + (c > s ? 1u : 0u);
            uint32_t prod = c * s;
            int8_t lane = W30_LANE[prod % 30u];
            uint64_t q0 = prod / 30u;
            uint64_t k = (uint64_t)p * t + a * s + q0;
            hit[i].block = k;
            hit[i].mask = (uint8_t)~(uint8_t)(1u << (unsigned)lane);
        }

        sort8_hits(hit);
        x->cycle_base0 = hit[0].block;
        for (size_t i = 0; i < 8u; ++i) {
            uint64_t d = hit[i].block - hit[0].block;
            if (d > UINT32_MAX) {
                fprintf(stderr, "cycle offset overflow for p=%u\n", p);
                exit(2);
            }
            x->offset[i] = (uint32_t)d;
            x->clear_mask[i] = hit[i].mask;
        }

        /* The first hit must be p^2, enforcing the local-square rule. */
        if (x->cycle_base0 != ((uint64_t)p * (uint64_t)p) / 30u) {
            fprintf(stderr, "internal cycle invariant failed for p=%u\n", p);
            exit(2);
        }
    }

    free(base.data);
    out.data = g;
    out.count = q;
    out.max_p = max_p;
    return out;
}

static void free_cycles(cycle_table *g) {
    if (!g) return;
    aligned_free64(g->data);
    memset(g, 0, sizeof(*g));
}


typedef struct {
    uint64_t residual_semiprimes;
    uint32_t aux_limit;
    size_t aux_prime_count;
    size_t aux_peak_bytes;
    double seconds;
    uint32_t *primes;
    uint32_t *packed;
    uint32_t (*entries)[8];
    size_t first, end;
} closure_plan;

/* After every prime <= y=floor(cuberoot(N)) has marked from p^2, any
   surviving composite <=N has no small factor.  Three factors >y would
   exceed N, so every residual composite is exactly p*q with y<p<=q.

   Compute an audit count of the products to be marked (not a correction):
       H = sum_{y<p<=sqrt(N)} [pi(floor(N/p)) - pi(p) + 1].
   The largest pi() query is <= floor(N/(y+1)), so one compact auxiliary
   prime list is sufficient. */
static closure_plan build_closure_plan(uint64_t limit, uint32_t mark_max) {
    closure_plan cp = {0};
    double t0 = red2_v33_now_seconds();
    uint64_t aux64 = limit / ((uint64_t)mark_max + 1u);
    if (aux64 > UINT32_MAX) {
        fprintf(stderr, "closure auxiliary limit exceeds 32-bit prototype range\n");
        exit(2);
    }
    uint64_t root = isqrt_u64(limit);
    /* Setup-only checks protect the 24-bit row offsets, packed cofactors,
       padded sentinel, and all lookahead products. No hot-loop widening. */
    if ((root*29u)/30u > UINT32_C(0x00ffffff) ||
        ((aux64/30u)+2u)*8u > UINT32_MAX ||
        root > (UINT64_MAX/256u)/(aux64+60u)) {
        fputs("range extension: packed product bound exceeded\n",stderr);
        exit(2);
    }
    cp.aux_limit = (uint32_t)aux64;
    prime_list32 pr = base_primes_upto(aux64);
    if (pr.count > (size_t)UINT32_MAX-8u) {
        fputs("range extension: cofactor cursor bound exceeded\n",stderr);
        exit(2);
    }
    cp.aux_prime_count = pr.count;
    cp.aux_peak_bytes = pr.peak_bytes;

    size_t first = upper_bound_prime32(pr.data, pr.count, mark_max);
    size_t end = upper_bound_prime32(pr.data, pr.count, root);
    uint64_t h = 0u;
    for (size_t i = first; i < end; ++i) {
        uint32_t p0 = pr.data[i];
        uint64_t qmax = limit / (uint64_t)p0;
        size_t qcount = upper_bound_prime32(pr.data, pr.count, qmax);
        if (qcount <= i) {
            free(pr.data);
            fprintf(stderr, "closure semiprime invariant failed for p=%u\n", p0);
            exit(2);
        }
        h += (uint64_t)(qcount - i); /* q indexes i..qcount-1 */
    }
    cp.packed = malloc((pr.count+8u) * sizeof(uint32_t));
    if(pr.count && !cp.packed) { fputs("packed prime allocation failed\n",stderr); exit(2); }
    for(size_t i=0;i<pr.count;i++) {
        uint32_t q=pr.data[i];
        int lane=W30_LANE[q%30u];
        cp.packed[i]=lane<0 ? 0u : (q/30u)*8u+(unsigned)lane;
    }
    /* Sentinel q is a W30 candidate above every represented cofactor.
       p>=y+1 implies p*q>N; padding allows safe four/eight-way lookahead. */
    for(size_t i=pr.count;i<pr.count+8u;i++)
        cp.packed[i]=((cp.aux_limit/30u)+2u)*8u;
    /* p <= sqrt(1e13) < 2^24: floor(p*r/30) fits 24 bits.
       A row stores eight (offset<<8)|mask entries in 32 bytes. */
    cp.entries=aligned_malloc64((end-first)*sizeof(*cp.entries));
    if(end>first && !cp.entries){fputs("entry allocation failed\n",stderr);exit(2);}
    for(size_t i=first;i<end;i++)for(unsigned lane=0;lane<8;lane++) {
        uint64_t v=(uint64_t)pr.data[i]*W30_RES[lane];
        uint8_t mask=(uint8_t)~(1u<<(unsigned)W30_LANE[v%30u]);
        cp.entries[i-first][lane]=((uint32_t)(v/30u)<<8)|mask;
    }
    cp.primes = pr.data;
    cp.first = first;
    cp.end = end;
    cp.residual_semiprimes = h;
    cp.seconds = red2_v33_now_seconds() - t0;
    return cp;
}

static inline unsigned popcount64(uint64_t x) {
#ifdef _MSC_VER
    return (unsigned)__popcnt64(x);
#else
    return (unsigned)__builtin_popcountll(x);
#endif
}

static inline uint64_t popcount_bytes(const uint8_t *buf, size_t n) {
    uint64_t total = 0;
    size_t words = n >> 3;
    const uint64_t *w = (const uint64_t *)(const void *)buf;
    for (size_t i = 0; i < words; ++i) total += popcount64(w[i]);
    size_t off = words << 3;
    for (size_t i = off; i < n; ++i) {
#ifdef _MSC_VER
        total += (uint64_t)__popcnt((unsigned)buf[i]);
#else
        total += (uint64_t)__builtin_popcount((unsigned)buf[i]);
#endif
    }
    return total;
}

static inline void restore_presieve_primes(
    uint8_t *buf,
    uint64_t block_lo,
    uint64_t block_hi,
    uint64_t limit,
    const presieve_plan *ps
) {
    for (size_t i = 0; i < ps->prime_count; ++i) {
        uint32_t p = ps->primes[i];
        if ((uint64_t)p > limit) continue;
        uint64_t b = p / 30u;
        if (b >= block_lo && b < block_hi) {
            int8_t lane = W30_LANE[p % 30u];
            buf[b - block_lo] |= (uint8_t)(1u << (unsigned)lane);
        }
    }
}

static inline void clear_special_edges(
    uint8_t *buf,
    uint64_t block_lo,
    uint64_t block_hi,
    uint64_t total_blocks,
    uint64_t limit,
    const presieve_plan *ps
) {
    if (block_lo == 0u) buf[0] &= (uint8_t)~1u; /* 1 is grey, not prime. */
    restore_presieve_primes(buf, block_lo, block_hi, limit, ps);

    if (block_hi == total_blocks) {
        uint64_t final_block = total_blocks - 1u;
        if (final_block >= block_lo) {
            size_t local = (size_t)(final_block - block_lo);
            uint8_t b = buf[local];
            for (size_t lane = 0; lane < 8u; ++lane) {
                uint64_t n = final_block * 30u + W30_RES[lane];
                if (n > limit) b &= (uint8_t)~(uint8_t)(1u << lane);
            }
            buf[local] = b;
        }
    }
}

/*
 * v0.10 sparse scheduling.
 *
 * v0.9 represented every sparse prime as a linked bucket event even when its
 * W210 strike gap was smaller than one cache territory. In that regime, once a
 * prime wakes it must strike every subsequent segment, so repeatedly unlinking
 * and relinking it is pure bookkeeping.
 *
 * v0.10 therefore keeps all sparse motion in W210 geometry but uses two
 * scheduling representations:
 *
 *   resident W210  - a contiguous active frontier for primes whose maximum
 *                    strike gap is guaranteed smaller than one segment;
 *   far W210       - contiguous bucket pages for primes that can truly sleep
 *                    across one or more whole segments.
 *
 * The dense/sparse mathematical cutoff remains independently controlled by
 * dense_cutoff. The resident/far split is only a scheduler safety property.
 */
typedef struct {
    uint32_t sieving_prime;  /* p / 30 */
    uint32_t multiple_index; /* relative to current/activation segment */
    uint32_t start_seg;      /* local segment where this event first wakes */
    uint8_t phase;           /* 0..47 inside one W210 class row */
    uint8_t class_idx;       /* 0..7, fixed for the lifetime of this prime */
    uint16_t _pad;
} resident210_event;

typedef struct {
    uint32_t sieving_prime;  /* p / 30 */
    uint32_t multiple_index; /* relative to scheduled segment */
    uint8_t phase;
    uint8_t class_idx;
    uint16_t _pad;
} far210_event;

typedef struct {
    uint64_t block;
    uint16_t wheel_index;
} big210_init;

#define NIL_EVENT UINT32_MAX
#define BUCKET_PAGE_CAP 16u

typedef struct {
    uint32_t next_page;
    uint16_t count;
    uint16_t _pad;
    uint32_t event_idx[BUCKET_PAGE_CAP];
} bucket_page;

typedef struct {
    uint32_t head;
    uint32_t tail;
} bucket_span;

typedef struct {
    bucket_page *pages;
    uint32_t capacity;
    uint32_t next_unused;
    uint32_t free_head;
} bucket_pool;

static inline uint32_t bucket_page_alloc(bucket_pool *bp) {
    uint32_t idx;
    if (bp->free_head != NIL_EVENT) {
        idx = bp->free_head;
        bp->free_head = bp->pages[idx].next_page;
    } else {
        if (bp->next_unused >= bp->capacity) return NIL_EVENT;
        idx = bp->next_unused++;
    }
    bp->pages[idx].next_page = NIL_EVENT;
    bp->pages[idx].count = 0u;
    return idx;
}

static inline void bucket_page_recycle(bucket_pool *bp, uint32_t idx) {
    bp->pages[idx].next_page = bp->free_head;
    bp->free_head = idx;
}

static inline int bucket_append(
    bucket_span *buckets,
    uint32_t bucket_idx,
    bucket_pool *bp,
    uint32_t event_idx
) {
    bucket_span *b = &buckets[bucket_idx];
    uint32_t pg = b->tail;
    if (pg == NIL_EVENT || bp->pages[pg].count == BUCKET_PAGE_CAP) {
        uint32_t np = bucket_page_alloc(bp);
        if (np == NIL_EVENT) return 0;
        if (pg == NIL_EVENT) b->head = np;
        else bp->pages[pg].next_page = np;
        b->tail = np;
        pg = np;
    }
    bucket_page *p = &bp->pages[pg];
    p->event_idx[p->count++] = event_idx;
    return 1;
}

/* Conservative monotone test: every W210 step is <= 10*(p/30)+10 blocks.
   If that bound is smaller than one segment, once the prime wakes it cannot
   skip any future full segment and can live in the contiguous resident set. */
static inline int w210_is_resident(uint32_t p, uint64_t segment_blocks) {
    uint64_t max_gap_bound = 10u * (uint64_t)(p / 30u) + 10u;
    return max_gap_bound < segment_blocks;
}

static int resident_start_cmp(const void *a, const void *b) {
    const resident210_event *x = (const resident210_event *)a;
    const resident210_event *y = (const resident210_event *)b;
    if (x->start_seg < y->start_seg) return -1;
    if (x->start_seg > y->start_seg) return 1;
    if (x->sieving_prime < y->sieving_prime) return -1;
    if (x->sieving_prime > y->sieving_prime) return 1;
    return 0;
}

typedef struct {
    uint64_t base;
    uint8_t phase;
    uint8_t _pad[7];
} cycle_state;

/* Align one merged 8-strike cycle to the first unprocessed hit >= target. */
static inline cycle_state init_cycle_state(const prime_cycle *g, uint64_t target) {
    cycle_state st;
    uint64_t base = g->cycle_base0;
    uint32_t p = g->p;

    if (target > base) {
        uint64_t q = (target - base) / p;
        base += q * (uint64_t)p;
        while (base + g->offset[7] < target) base += p;
    }

    uint8_t phase = 0;
    while (phase < 8u && base + g->offset[phase] < target) ++phase;
    if (phase == 8u) {
        base += p;
        phase = 0;
    }

    st.base = base;
    st.phase = phase;
    memset(st._pad, 0, sizeof(st._pad));
    return st;
}

static inline big210_init init_big210(
    uint32_t p,
    uint64_t target,
    const wheel210_table *w
) {
    big210_init st;
    int8_t ci = W30_LANE[p % 30u];
    int8_t ph = w->phase_of_residue[p % 210u];
    if (ci < 0 || ph < 0) {
        fprintf(stderr, "internal W210 class invariant failed for p=%u\n", p);
        exit(2);
    }

    uint64_t block = ((uint64_t)p * (uint64_t)p) / 30u;
    uint16_t wi = (uint16_t)((uint16_t)ci * 48u + (uint16_t)ph);
    uint32_t sp = p / 30u;

    if (target > block) {
        /* One complete W210 wheel revolution advances +210*p numbers = +7*p blocks. */
        uint64_t super = 7u * (uint64_t)p;
        uint64_t q = (target - block) / super;
        block += q * super;

        while (block < target) {
            const wheel210_elem *x = &w->e[wi];
            block += (uint64_t)x->next_factor * sp + x->correct;
            wi = x->next;
        }
    }

    st.block = block;
    st.wheel_index = wi;
    return st;
}

/*
 * Dense hot path.  The middle loop crosses off eight multiples with eight ANDs,
 * one base increment and one loop test. Boundary fragments are handled at most
 * once at each side of a segment.
 */
static inline void mark_cycle_segment(
    uint8_t *buf,
    uint64_t block_lo,
    uint64_t block_hi,
    const prime_cycle *g,
    cycle_state *st
) {
    uint64_t base = st->base;
    uint8_t phase = st->phase;
    const uint32_t p = g->p;

    /* Finish a partial cycle carried from the previous segment. */
    while (phase != 0u) {
        uint64_t k = base + g->offset[phase];
        if (k >= block_hi) {
            st->base = base;
            st->phase = phase;
            return;
        }
        if (k >= block_lo) buf[(size_t)(k - block_lo)] &= g->clear_mask[phase];
        ++phase;
        if (phase == 8u) {
            phase = 0u;
            base += p;
        }
    }

    /* A defensive catch-up; normally carried state already satisfies base>=lo. */
    if (base < block_lo) {
        uint64_t q = (block_lo - base) / p;
        base += q * (uint64_t)p;
        while (base + g->offset[7] < block_lo) base += p;
        while (phase < 8u && base + g->offset[phase] < block_lo) ++phase;
        if (phase == 8u) {
            phase = 0u;
            base += p;
        }
        if (phase != 0u) {
            while (phase != 0u) {
                uint64_t k = base + g->offset[phase];
                if (k >= block_hi) {
                    st->base = base;
                    st->phase = phase;
                    return;
                }
                if (k >= block_lo) buf[(size_t)(k - block_lo)] &= g->clear_mask[phase];
                ++phase;
                if (phase == 8u) { phase = 0u; base += p; }
            }
        }
    }

    /* Eight-strike unrolled cycles. */
    const uint32_t o1 = g->offset[1], o2 = g->offset[2], o3 = g->offset[3];
    const uint32_t o4 = g->offset[4], o5 = g->offset[5], o6 = g->offset[6], o7 = g->offset[7];
    const uint8_t m0 = g->clear_mask[0], m1 = g->clear_mask[1], m2 = g->clear_mask[2], m3 = g->clear_mask[3];
    const uint8_t m4 = g->clear_mask[4], m5 = g->clear_mask[5], m6 = g->clear_mask[6], m7 = g->clear_mask[7];

    while (base < block_hi && o7 < block_hi - base) {
        size_t x = (size_t)(base - block_lo);
        buf[x]      &= m0;
        buf[x + o1] &= m1;
        buf[x + o2] &= m2;
        buf[x + o3] &= m3;
        buf[x + o4] &= m4;
        buf[x + o5] &= m5;
        buf[x + o6] &= m6;
        buf[x + o7] &= m7;
        base += p;
    }

    /* Tail fragment: remember the exact phase for the next segment. */
    phase = 0u;
    while (phase < 8u) {
        uint64_t k = base + g->offset[phase];
        if (k >= block_hi) break;
        if (k >= block_lo) buf[(size_t)(k - block_lo)] &= g->clear_mask[phase];
        ++phase;
    }
    if (phase == 8u) {
        phase = 0u;
        base += p;
    }

    st->base = base;
    st->phase = phase;
}


/* Bounded calendar memory is a resource limit, not a CPU cache-size guess. */
#ifndef RED2_CALENDAR_MAX_BYTES
#define RED2_CALENDAR_MAX_BYTES (4u*1024u*1024u)
#endif
#ifndef RED2_CALENDAR_CHECKS
#define RED2_CALENDAR_CHECKS 0
#endif
static inline unsigned calendar_first_bit(uint64_t bits) {
#ifdef _MSC_VER
    unsigned long index;_BitScanForward64(&index,bits);return (unsigned)index;
#else
    return (unsigned)__builtin_ctzll(bits);
#endif
}
static inline uint64_t calendar_product_block(const closure_plan *cp,size_t row,uint32_t cursor) {
    uint32_t q=cp->packed[cursor];
    uint64_t tag=((uint64_t)cp->primes[row]<<8)*(uint64_t)(q>>3)+cp->entries[row-cp->first][q&7u];
    return tag>>8;
}
static inline uint64_t calendar_segment(uint64_t block,uint64_t segment_blocks,unsigned shift) {
    return shift<64u?block>>shift:block/segment_blocks;
}
static inline void calendar_insert(uint64_t *calendar,size_t words,uint32_t slots,
    uint64_t current_seg,uint64_t next_seg,size_t row) {
    if(next_seg<current_seg || next_seg-current_seg>=slots) {
        fputs("calendar horizon invariant failed\n",stderr);exit(2);
    }
    size_t cell=(size_t)(next_seg&(slots-1u))*words+row/64u;
    uint64_t bit=UINT64_C(1)<<(row%64u);
    if(RED2_CALENDAR_CHECKS && (calendar[cell]&bit)) {
        fputs("duplicate calendar row\n",stderr);exit(2);
    }
    calendar[cell]|=bit;
}

red2_v33_result red2_v34_sieve(
    uint64_t limit, int threads, size_t segment_bytes, int presieve_max, uint32_t dense_cutoff, int layer_max, int plate_batch,
    unsigned frontier_multiplier, red2_v33_segment_fn consume, void *context,
    unsigned schedule_factor, red2_v34_stats *stats
) {
    red2_v33_result out = {0};
    if(stats)memset(stats,0,sizeof(*stats));
    if(schedule_factor>64u){fputs("schedule factor outside 0..64\n",stderr);exit(2);}
    if(limit>RED2_RANGE_MAX_N || segment_bytes>8192u*1024u || threads>1024) {
        fputs("RED2 v0.33 range extension: N<=1e13, segment<=8192 KiB, threads<=1024\n",stderr);
        exit(2);
    }
    out.limit = limit;
    if (plate_batch < 1) plate_batch = 1;
    if (plate_batch > 4) plate_batch = 4;
    out.plate_batch = plate_batch;
    out.threads_requested = threads;
    out.presieve_max = presieve_max;
    if (dense_cutoff < 23u) dense_cutoff = 23u;
    out.dense_cutoff = dense_cutoff;

    if (threads < 1) threads = 1;
#ifndef _OPENMP
    threads = 1;
#endif
    if (segment_bytes < 4096u) segment_bytes = 4096u;
    out.segment_bytes = segment_bytes;

    if (presieve_max < 5) presieve_max = 5;
    if (presieve_max > 19) presieve_max = 19;
    out.presieve_max = presieve_max;
    if (layer_max < 31) layer_max = 19;
    else if (layer_max < 43) layer_max = 31;
    else if (layer_max < 59) layer_max = 43;
    else if (layer_max < 71) layer_max = 59;
    else if (layer_max < 83) layer_max = 71;
    else if (layer_max < 101) layer_max = 83;
    else if (layer_max < 109) layer_max = 101;
    else if (layer_max < 127) layer_max = 109;
    else if (layer_max < 137) layer_max = 127;
    else if (layer_max < 149) layer_max = 137;
    else if (layer_max < 157) layer_max = 149;
    else if (layer_max < 163) layer_max = 157;
    else layer_max = 163;

    if (limit < 2u) {
        out.threads_used = 1;
        return out;
    }

    /* Mark through the cube-root frontier, then explicitly strike every
       residual prime product. Small inputs use the full sqrt-frontier fallback. */
    uint64_t sqrt_limit = isqrt_u64(limit);
    uint64_t cube_limit = icbrt_u64(limit);
    if (frontier_multiplier<1u) frontier_multiplier=1u;
    if (frontier_multiplier>64u) frontier_multiplier=64u;
    cube_limit *= frontier_multiplier;
    if(cube_limit>sqrt_limit) cube_limit=sqrt_limit;
    uint64_t closure_aux64 = limit / (cube_limit + 1u);
    int product_completion_used = cube_limit >= (uint64_t)presieve_max &&
        cube_limit >= (uint64_t)layer_max &&
        cube_limit <= UINT32_MAX && closure_aux64 <= UINT32_MAX;
    uint32_t mark_max = (uint32_t)(product_completion_used ? cube_limit : sqrt_limit);

    double prep0 = red2_v33_now_seconds();
    presieve_plan ps = build_presieve(presieve_max);
    layered_plan lp = build_layered(layer_max);
    cycle_table cycles = build_cycles(limit, presieve_max, layer_max, mark_max);
    wheel210_table wheel210 = build_wheel210_table();
    closure_plan closure = {0};
    if (product_completion_used) closure = build_closure_plan(limit, mark_max);

    out.product_completion_used = product_completion_used;
    out.marking_frontier = product_completion_used ? mark_max : 0u;
    out.aux_limit = closure.aux_limit;
    out.aux_prime_count = closure.aux_prime_count;
    out.aux_peak_bytes = closure.aux_peak_bytes;
    out.residual_semiprime_count = closure.residual_semiprimes;
    out.completion_prepare_seconds = closure.seconds;

    uint64_t total_blocks = limit / 30u + 1u;
    uint64_t segment_blocks = (uint64_t)segment_bytes; /* 1 W30 block == 1 byte */
    uint64_t total_segments = (total_blocks + segment_blocks - 1u) / segment_blocks;

    if ((uint64_t)threads > total_segments) threads = (int)total_segments;
    if (threads < 1) threads = 1;

    /* v0.9 split: explicit motion scale independent of room size. */
    size_t dense_count = 0;
    while (dense_count < cycles.count && cycles.data[dense_count].p < dense_cutoff) ++dense_count;
    size_t sparse_count = cycles.count - dense_count;

    /* v0.10 scheduler split inside the same W210 sparse geometry.
       The conservative resident test is monotone in p, so resident sparse
       primes form a contiguous prefix and truly-far sparse primes a suffix. */
    size_t resident_count = 0;
    while (dense_count + resident_count < cycles.count &&
           w210_is_resident(cycles.data[dense_count + resident_count].p, segment_blocks)) {
        ++resident_count;
    }
    size_t far_count = sparse_count - resident_count;

    out.threads_used = threads;
    out.segments = total_segments;
    out.presieve_period_bytes = ps.period_blocks;
    out.layer_max = layer_max;
    out.layer_count = lp.layer_count;
    size_t layer_mask_bytes_meta = 0u;
    for (size_t li = 0; li < lp.layer_count; ++li) layer_mask_bytes_meta += lp.layer[li].period_blocks;
    out.layer_mask_bytes = layer_mask_bytes_meta;
    out.dense_prime_count = dense_count;
    out.sparse_prime_count = sparse_count;
    out.resident_sparse_prime_count = resident_count;
    out.bucket_sparse_prime_count = far_count;

    uint64_t max_local_segments64 = (total_segments + (uint64_t)threads - 1u) / (uint64_t)threads;
    if (max_local_segments64 > UINT32_MAX) {
        free_cycles(&cycles);
        free_layered(&lp);
        free_presieve(&ps);
        fprintf(stderr, "v0.13 local segment index exceeds 32-bit prototype range\n");
        exit(2);
    }
    size_t max_local_segments = (size_t)max_local_segments64;

    size_t page_cap_max = 0u;
    if (far_count) {
        page_cap_max = (far_count + BUCKET_PAGE_CAP - 1u) / BUCKET_PAGE_CAP + max_local_segments + 64u;
        if (page_cap_max > UINT32_MAX) {
            free_cycles(&cycles);
            free_layered(&lp);
        free_presieve(&ps);
            fprintf(stderr, "v0.13 bucket page pool exceeds 32-bit prototype range\n");
            exit(2);
        }
    }
    out.bucket_page_capacity_per_thread = page_cap_max;

    size_t layer_bytes = out.layer_mask_bytes;
    size_t shared_bytes = cycles.count * sizeof(prime_cycle) + ps.period_blocks + layer_bytes + sizeof(wheel210);
    size_t state_bytes = dense_count * sizeof(cycle_state);
    size_t resident_bytes = resident_count * sizeof(resident210_event);
    size_t far_event_bytes = far_count * sizeof(far210_event);
    size_t bucket_span_bytes = far_count ? max_local_segments * sizeof(bucket_span) : 0u;
    size_t page_bytes = far_count ? page_cap_max * sizeof(bucket_page) : 0u;
    size_t residual_rows = closure.end - closure.first;
    size_t scheduled_first=closure.end;
    uint64_t cutoff64=(uint64_t)segment_bytes*schedule_factor;
    uint32_t calendar_cutoff=cutoff64>UINT32_MAX?UINT32_MAX:(uint32_t)cutoff64;
    if(schedule_factor && residual_rows) {
        scheduled_first=upper_bound_prime32(closure.primes,closure.end,calendar_cutoff);
        if(scheduled_first<closure.first)scheduled_first=closure.first;
    }
    size_t scheduled_rows=closure.end-scheduled_first;
    unsigned calendar_shift=(segment_blocks&(segment_blocks-1u))?64u:calendar_first_bit(segment_blocks);
    uint32_t max_cofactor_gap=0,calendar_slots=1;
    size_t calendar_words=(scheduled_rows+63u)/64u,calendar_bytes=0;
    int calendar_fallback=0;
    if(scheduled_rows) {
        for(size_t i=1;i<closure.aux_prime_count;i++) {
            uint32_t gap=closure.primes[i]-closure.primes[i-1u];
            if(gap>max_cofactor_gap)max_cofactor_gap=gap;
        }
        /* Consecutive real products advance by p*(next_q-q).
           +3 covers both segment alignment and integer division rounding. */
        uint64_t needed=(sqrt_limit*(uint64_t)max_cofactor_gap)/(30u*segment_blocks)+3u;
        while(calendar_slots<needed && calendar_slots<=UINT32_MAX/2u)calendar_slots*=2u;
        if(calendar_slots<needed || calendar_words>RED2_CALENDAR_MAX_BYTES/sizeof(uint64_t)/calendar_slots) {
            calendar_fallback=1;scheduled_first=closure.end;scheduled_rows=0;calendar_words=0;calendar_slots=1;
        } else calendar_bytes=(size_t)calendar_slots*calendar_words*sizeof(uint64_t);
    }
    if(stats) {
        stats->direct_rows=scheduled_first-closure.first;stats->scheduled_rows=scheduled_rows;
        stats->calendar_bytes_per_worker=calendar_bytes;stats->ring_slots=calendar_slots;
        stats->max_cofactor_gap=max_cofactor_gap;stats->cutoff=calendar_cutoff;
        stats->schedule_factor=schedule_factor;stats->memory_fallback=calendar_fallback;
    }
    size_t auxiliary_live = (2u * closure.aux_prime_count + 8u) * sizeof(uint32_t) +
        residual_rows * sizeof(*closure.entries);
    size_t runtime_peak_bytes = shared_bytes + auxiliary_live + (size_t)threads *
        (segment_bytes + state_bytes + resident_bytes + far_event_bytes + bucket_span_bytes + page_bytes +
         residual_rows * sizeof(uint32_t) + calendar_bytes);
    size_t closure_peak_bytes = shared_bytes + closure.aux_peak_bytes;
    if(shared_bytes + auxiliary_live > closure_peak_bytes) closure_peak_bytes=shared_bytes + auxiliary_live;
    out.estimated_peak_bytes = runtime_peak_bytes > closure_peak_bytes ? runtime_peak_bytes : closure_peak_bytes;

    double prep1 = red2_v33_now_seconds();
    out.prepare_seconds = prep1 - prep0;

    uint64_t candidate_count = 0;
    uint64_t direct_visits=0,scheduled_visits=0;
    int allocation_failed = 0;
    out.dense_tile_bytes=16384u;
    /* Four independently initialized, contiguous territories per requested
       worker. The OpenMP queue gives the next territory to a free worker.
       Each territory retains increasing segments and carried sieve cursors.
       One-thread enumeration remains one ordered territory. */
    int tasks = threads > 1 ? threads * 4 : 1;
    if ((uint64_t)tasks > total_segments) tasks = (int)total_segments;
    out.task_count=tasks;
    double work0 = red2_v33_now_seconds();

#ifdef _OPENMP
#pragma omp parallel for num_threads(threads) schedule(dynamic,1) reduction(+:candidate_count,direct_visits,scheduled_visits)
#endif
    for (int task = 0; task < tasks; ++task) {
#ifdef _OPENMP
        /* Only task zero writes this field; readers run after the join. */
        if (task == 0) out.threads_used = omp_get_num_threads();
#endif
        int thread_failed = 0;
        size_t residual_count = closure.end - closure.first;
        /* Auxiliary limit < 464187025 for N<=1e13; indices fit uint32_t. */
        uint32_t *q_cursor = residual_count ? malloc(residual_count * sizeof(uint32_t)) : NULL;
        if (residual_count && !q_cursor) { fputs("residual allocation failed\n",stderr); exit(2); }
        uint64_t seg_begin = total_segments * (uint64_t)task / (uint64_t)tasks;
        uint64_t seg_end   = total_segments * (uint64_t)(task + 1) / (uint64_t)tasks;
        uint64_t local_segments64 = seg_end - seg_begin;
        uint32_t local_segments = (uint32_t)local_segments64;
        uint64_t territory_block_begin = seg_begin * segment_blocks;
        uint64_t territory_block_end = seg_end * segment_blocks;
        if (territory_block_end > total_blocks) territory_block_end = total_blocks;

        for (size_t i=closure.first; i<closure.end; ++i) {
            uint64_t p0=closure.primes[i];
            uint64_t qmin=(territory_block_begin*30u+p0-1u)/p0;
            if(qmin<p0) qmin=p0;
            q_cursor[i-closure.first]=(uint32_t)upper_bound_prime32(closure.primes,closure.aux_prime_count,qmin-1u);
        }
        uint64_t *calendar=calendar_bytes?calloc(1,calendar_bytes):NULL;
        if(calendar_bytes&&!calendar){fputs("calendar allocation failed\n",stderr);exit(2);}
        uint8_t *buf = (uint8_t *)aligned_malloc64(segment_bytes);
        cycle_state *state = dense_count ?
            (cycle_state *)aligned_malloc64(dense_count * sizeof(cycle_state)) : NULL;
        resident210_event *resident = resident_count ?
            (resident210_event *)aligned_malloc64(resident_count * sizeof(resident210_event)) : NULL;
        far210_event *far_events = far_count ?
            (far210_event *)aligned_malloc64(far_count * sizeof(far210_event)) : NULL;
        bucket_span *buckets = (far_count && local_segments) ?
            (bucket_span *)aligned_malloc64((size_t)local_segments * sizeof(bucket_span)) : NULL;

        size_t local_page_cap_sz = 0u;
        bucket_page *pages = NULL;
        if (far_count) {
            local_page_cap_sz = (far_count + BUCKET_PAGE_CAP - 1u) / BUCKET_PAGE_CAP + (size_t)local_segments + 64u;
            pages = (bucket_page *)aligned_malloc64(local_page_cap_sz * sizeof(bucket_page));
        }

        if (!buf || (dense_count && !state) || (resident_count && !resident) ||
            (far_count && (!far_events || !buckets || !pages))) {
            thread_failed = 1;
        } else {
            /* Dense W30 states. */
            for (size_t gi = 0; gi < dense_count; ++gi) {
                state[gi] = init_cycle_state(&cycles.data[gi], territory_block_begin);
            }

            /* Resident W210 events are stored contiguously in wake order.
               Most already-active primes wake in local segment 0; unborn
               primes enter later according to their p^2 activation frontier. */
            size_t resident_used = 0u;
            for (size_t gi = dense_count; gi < dense_count + resident_count; ++gi) {
                const prime_cycle *g = &cycles.data[gi];
                if (g->cycle_base0 >= territory_block_end) break;

                big210_init bi = init_big210(g->p, territory_block_begin, &wheel210);
                if (bi.block >= territory_block_end) continue;

                uint64_t ls64 = (bi.block - territory_block_begin) / segment_blocks;
                if (ls64 >= local_segments64 || ls64 > UINT32_MAX) continue;
                uint64_t seg_block = territory_block_begin + ls64 * segment_blocks;

                resident210_event *e = &resident[resident_used++];
                e->sieving_prime = g->p / 30u;
                e->multiple_index = (uint32_t)(bi.block - seg_block);
                e->start_seg = (uint32_t)ls64;
                e->class_idx = (uint8_t)(bi.wheel_index / 48u);
                e->phase = (uint8_t)(bi.wheel_index % 48u);
                e->_pad = 0u;
            }

            /* p-order normally already gives monotone wake order. Verify it
               cheaply; sort only on the defensive slow path. */
            int resident_sorted = 1;
            for (size_t i = 1u; i < resident_used; ++i) {
                if (resident[i-1u].start_seg > resident[i].start_seg) {
                    resident_sorted = 0;
                    break;
                }
            }
            if (!resident_sorted) qsort(resident, resident_used, sizeof(*resident), resident_start_cmp);

            /* Far W210 page buckets. Pages and bucket spans are contiguous;
               a bucket consumes event indices linearly instead of pointer-
               chasing one event node at a time. */
            bucket_pool pool = {0};
            if (far_count) {
                pool.pages = pages;
                pool.capacity = (uint32_t)local_page_cap_sz;
                pool.next_unused = 0u;
                pool.free_head = NIL_EVENT;
                for (uint32_t i = 0u; i < local_segments; ++i) {
                    buckets[i].head = NIL_EVENT;
                    buckets[i].tail = NIL_EVENT;
                }
            }

            size_t far_used = 0u;
            if (far_count) {
                for (size_t gi = dense_count + resident_count; gi < cycles.count; ++gi) {
                    const prime_cycle *g = &cycles.data[gi];
                    if (g->cycle_base0 >= territory_block_end) break;

                    big210_init bi = init_big210(g->p, territory_block_begin, &wheel210);
                    if (bi.block >= territory_block_end) continue;

                    uint64_t ls64 = (bi.block - territory_block_begin) / segment_blocks;
                    if (ls64 >= local_segments64 || ls64 > UINT32_MAX) continue;
                    uint64_t seg_block = territory_block_begin + ls64 * segment_blocks;

                    far210_event *e = &far_events[far_used];
                    e->sieving_prime = g->p / 30u;
                    e->multiple_index = (uint32_t)(bi.block - seg_block);
                    e->class_idx = (uint8_t)(bi.wheel_index / 48u);
                    e->phase = (uint8_t)(bi.wheel_index % 48u);
                    e->_pad = 0u;

                    if (!bucket_append(buckets, (uint32_t)ls64, &pool, (uint32_t)far_used)) {
                        thread_failed = 1;
                        break;
                    }
                    ++far_used;
                }
            }

            size_t active_resident = 0u;
            size_t active_products = closure.first;
            size_t calendar_active=scheduled_first;

            for (uint64_t seg = seg_begin; seg < seg_end && !thread_failed; ++seg) {
                uint64_t block_lo = seg * segment_blocks;
                uint64_t block_hi = block_lo + segment_blocks;
                if (block_hi > total_blocks) block_hi = total_blocks;
                size_t blocks_here = (size_t)(block_hi - block_lo);
                uint32_t local_seg = (uint32_t)(seg - seg_begin);

                copy_periodic_mask(buf, blocks_here, block_lo, &ps);
                apply_layer_batches(buf, blocks_here, block_lo, &lp, plate_batch);
                clear_special_edges(buf, block_lo, block_hi, total_blocks, limit, &ps);
                for(size_t li=0;li<lp.layer_count;li++)for(size_t pj=0;pj<lp.layer[li].prime_count;pj++){uint32_t pp=lp.layer[li].primes[pj];uint64_t bb=pp/30u;if((uint64_t)pp<=limit&&bb>=block_lo&&bb<block_hi){int8_t lane=W30_LANE[pp%30u];buf[bb-block_lo]|=(uint8_t)(1u<<(unsigned)lane);}}

                /* Dense strikes use a small tile; product completion still
                   sees the whole segment and pays row setup once per segment. */
                for(uint64_t tile_lo=block_lo;tile_lo<block_hi;tile_lo+=16384u) {
                    uint64_t tile_hi=tile_lo+16384u;
                    if(tile_hi>block_hi)tile_hi=block_hi;
                    uint8_t *tile_buf=buf+(size_t)(tile_lo-block_lo);
                    for(size_t gi=0;gi<dense_count;gi++)
                        mark_cycle_segment(tile_buf,tile_lo,tile_hi,&cycles.data[gi],&state[gi]);
                }

                /* Wake newly-born resident sparse primes. Once active, every
                   resident W210 trajectory is guaranteed to hit each full
                   future segment, so the active set is one contiguous prefix. */
                while (active_resident < resident_used &&
                       resident[active_resident].start_seg <= local_seg) {
                    ++active_resident;
                }

                for (size_t rr = active_resident; rr != 0u; --rr) {
                    size_t ri = rr - 1u;
                    resident210_event *e = &resident[ri];
                    uint64_t idx = e->multiple_index;
                    uint8_t ph = e->phase;
                    const wheel210_elem *row = &wheel210.e[(unsigned)e->class_idx * 48u];

                    while (idx < (uint64_t)blocks_here) {
                        const wheel210_elem *x = &row[ph];
                        buf[(size_t)idx] &= x->unset_mask;
                        idx += (uint64_t)x->next_factor * (uint64_t)e->sieving_prime + x->correct;
                        if (++ph == 48u) ph = 0u;
                    }

                    /* For full segments, resident safety guarantees the next
                       hit is less than one further segment away. */
                    if (local_seg + 1u < local_segments) {
                        if (idx < segment_blocks) {
                            /* This can only occur when the current global
                               segment is partial; there is then no later local
                               full segment that requires this state. */
                            if (blocks_here == segment_bytes) {
                                thread_failed = 1;
                                break;
                            }
                        } else {
                            idx -= segment_blocks;
                            if (idx >= segment_blocks) {
                                thread_failed = 1;
                                break;
                            }
                        }
                    }

                    e->multiple_index = (uint32_t)idx;
                    e->phase = ph;
                }

                if (thread_failed) break;

                /* Truly-far W210 events: process one contiguous page at a time.
                   Recycle each page before rescheduling its copied entries so
                   the page pool can be reused immediately. */
                if (far_count) {
                    uint32_t pg = buckets[local_seg].head;
                    buckets[local_seg].head = NIL_EVENT;
                    buckets[local_seg].tail = NIL_EVENT;

                    while (pg != NIL_EVENT && !thread_failed) {
                        bucket_page *page = &pool.pages[pg];
                        uint32_t next_pg = page->next_page;
                        uint16_t cnt = page->count;
                        uint32_t local_idx[BUCKET_PAGE_CAP];
                        for (uint16_t j = 0u; j < cnt; ++j) local_idx[j] = page->event_idx[j];
                        bucket_page_recycle(&pool, pg);

                        for (uint16_t j = 0u; j < cnt; ++j) {
                            uint32_t eidx = local_idx[j];
                            far210_event *e = &far_events[eidx];
                            uint64_t idx = e->multiple_index;
                            uint8_t ph = e->phase;
                            const wheel210_elem *row = &wheel210.e[(unsigned)e->class_idx * 48u];
                            uint64_t delta_seg = 0u;

                            while (idx < (uint64_t)blocks_here) {
                                const wheel210_elem *x = &row[ph];
                                buf[(size_t)idx] &= x->unset_mask;
                                idx += (uint64_t)x->next_factor * (uint64_t)e->sieving_prime + x->correct;
                                if (++ph == 48u) ph = 0u;

                                if (idx >= segment_blocks) {
                                    do {
                                        idx -= segment_blocks;
                                        ++delta_seg;
                                    } while (idx >= segment_blocks);
                                    break;
                                }
                            }

                            e->multiple_index = (uint32_t)idx;
                            e->phase = ph;

                            if (delta_seg != 0u) {
                                uint64_t ns64 = (uint64_t)local_seg + delta_seg;
                                if (ns64 < local_segments64) {
                                    if (!bucket_append(buckets, (uint32_t)ns64, &pool, eidx)) {
                                        thread_failed = 1;
                                        break;
                                    }
                                }
                            }
                        }
                        pg = next_pg;
                    }
                }

                /* Sorted squares activate a monotone prefix, once per row
                   per territory, instead of testing every row every segment. */
                while(active_products<closure.end &&
                      (uint64_t)closure.primes[active_products]*closure.primes[active_products]<block_hi*30u)
                    ++active_products;
                size_t direct_end=active_products<scheduled_first?active_products:scheduled_first;
                direct_visits+=(uint64_t)(direct_end-closure.first);
                for (size_t i=closure.first; i<direct_end; ++i) {
                    uint8_t *restrict marks=buf;
                    const uint32_t *restrict cofactors=closure.packed;
                    uint64_t p0=closure.primes[i];
                    const uint32_t *restrict entries=closure.entries[i-closure.first];
                    /* For q=30*u+r, S=(p<<8)*u+entry[r].
                       S>>8=floor(p*q/30), (uint8_t)S=clearing mask.
                       Separate allocations justify these local restrict views. */
                    const uint64_t stride=p0<<8;
                    const uint64_t tagged_end=block_hi<<8;
                    size_t j=q_cursor[i-closure.first];
                    /* Ordered q implies ordered p*q. The fourth product
                       certifies the upper bound of the complete batch. */
                    for (;;) {
                        uint32_t qlast=cofactors[j+3u];
                        uint64_t blast=stride*(uint64_t)(qlast>>3)+entries[qlast&7u];
                        if(blast>=tagged_end) break;
                        uint32_t q0=cofactors[j+0u];
                        uint64_t b0=stride*(uint64_t)(q0>>3)+entries[q0&7u];
                        uint32_t q1=cofactors[j+1u];
                        uint64_t b1=stride*(uint64_t)(q1>>3)+entries[q1&7u];
                        uint32_t q2=cofactors[j+2u];
                        uint64_t b2=stride*(uint64_t)(q2>>3)+entries[q2&7u];
                        uint32_t q3=cofactors[j+3u];
                        uint64_t b3=stride*(uint64_t)(q3>>3)+entries[q3&7u];
                        marks[(b0>>8)-block_lo]&=(uint8_t)b0;
                        marks[(b1>>8)-block_lo]&=(uint8_t)b1;
                        marks[(b2>>8)-block_lo]&=(uint8_t)b2;
                        marks[(b3>>8)-block_lo]&=(uint8_t)b3;
                        j+=4u;
                    }
                    for (;;) {
                        uint32_t q=cofactors[j];
                        uint64_t b=stride*(uint64_t)(q>>3)+entries[q&7u];
                        if(b>=tagged_end) break;
                        /* Initialization and carried cursors imply (b>>8)>=block_lo. */
                        marks[(b>>8)-block_lo]&=(uint8_t)b;
                        ++j;
                    }
                    q_cursor[i-closure.first]=(uint32_t)j;
                }

                if(scheduled_rows) {
                    /* Activate each queued row once per territory. Its initial
                       cursor already respects both p^2 and the territory start. */
                    while(calendar_active<active_products) {
                        size_t row=calendar_active++;
                        uint32_t cursor=q_cursor[row-closure.first];
                        if(cursor>=closure.aux_prime_count)continue;
                        uint64_t next=calendar_product_block(&closure,row,cursor);
                        if(next<territory_block_end)
                            calendar_insert(calendar,calendar_words,calendar_slots,seg,calendar_segment(next,segment_blocks,calendar_shift),row-scheduled_first);
                    }
                    uint64_t *due=calendar+(size_t)(seg&(calendar_slots-1u))*calendar_words;
                    for(size_t word=0;word<calendar_words;word++) {
                        uint64_t bits=due[word];due[word]=0;
                        scheduled_visits+=popcount64(bits);
                        while(bits) {
                            unsigned bit=calendar_first_bit(bits);bits&=bits-1u;
                            size_t i=scheduled_first+word*64u+bit;
                            if(RED2_CALENDAR_CHECKS) {
                                uint64_t first=calendar_product_block(&closure,i,q_cursor[i-closure.first]);
                                if(i>=active_products || first<block_lo || first>=block_hi) {
                                    fputs("calendar row not due in segment\n",stderr);exit(2);
                                }
                            }
                            uint8_t *restrict marks=buf;
                            const uint32_t *restrict cofactors=closure.packed;
                            const uint32_t *restrict entries=closure.entries[i-closure.first];
                            const uint64_t stride=(uint64_t)closure.primes[i]<<8;
                            const uint64_t tagged_end=block_hi<<8;
                            size_t j=q_cursor[i-closure.first];
                            uint32_t q=cofactors[j];
                            uint64_t tag=stride*(uint64_t)(q>>3)+entries[q&7u];
                            /* A calendar event certifies at least one hit.
                               Sparse rows do not need a fourth-product probe. */
                            do {
                                marks[(tag>>8)-block_lo]&=(uint8_t)tag;
                                q=cofactors[++j];
                                tag=stride*(uint64_t)(q>>3)+entries[q&7u];
                            } while(tag<tagged_end);
                            q_cursor[i-closure.first]=(uint32_t)j;
                            if(j<closure.aux_prime_count) {
                                uint64_t next=tag>>8;
                                if(next<territory_block_end) {
                                    uint64_t next_seg=calendar_segment(next,segment_blocks,calendar_shift);
                                    if(next_seg<=seg){fputs("calendar failed to advance\n",stderr);exit(2);}
                                    calendar_insert(calendar,calendar_words,calendar_slots,seg,next_seg,i-scheduled_first);
                                }
                            }
                        }
                    }
                }
                if (consume) consume(block_lo,buf,blocks_here,context);
                candidate_count += popcount_bytes(buf, blocks_here);
            }
        }

        aligned_free64(pages);
        aligned_free64(buckets);
        aligned_free64(far_events);
        aligned_free64(resident);
        aligned_free64(state);
        aligned_free64(buf);
        free(q_cursor);
        free(calendar);

        if (thread_failed) {
#ifdef _OPENMP
#pragma omp atomic write
#endif
            allocation_failed = 1;
        }
    }

    double work1 = red2_v33_now_seconds();
    out.work_seconds = work1 - work0;
    if(stats){stats->direct_row_visits=direct_visits;stats->scheduled_row_visits=scheduled_visits;}

    if (allocation_failed) {
        free_cycles(&cycles);
        free_layered(&lp);
        free_presieve(&ps);
        fprintf(stderr, "v0.33 product-sieve scheduling failed\n");
        exit(2);
    }

    uint64_t wheel_primes = 0;
    if (limit >= 2u) ++wheel_primes;
    if (limit >= 3u) ++wheel_primes;
    if (limit >= 5u) ++wheel_primes;
    out.bitmap_survivor_count = candidate_count;
    uint64_t total_count = candidate_count + wheel_primes;
    out.prime_count = total_count;
    free(closure.primes);
    free(closure.packed);
    aligned_free64(closure.entries);

    free_cycles(&cycles);
    free_layered(&lp);
        free_presieve(&ps);
    return out;
}

red2_v33_result red2_v33_sieve(
    uint64_t n,int threads,size_t segment,int presieve,uint32_t dense,int layer,int batch,
    unsigned frontier,red2_v33_segment_fn consume,void *context) {
    return red2_v34_sieve(n,threads,segment,presieve,dense,layer,batch,frontier,consume,context,2,NULL);
}

red2_v33_result red2_v33_count(uint64_t limit,int threads,size_t segment_bytes,int presieve_max,uint32_t dense_cutoff,int layer_max,int plate_batch) {
    return red2_v33_sieve(limit,threads,segment_bytes,presieve_max,dense_cutoff,layer_max,plate_batch,1,NULL,NULL);
}
