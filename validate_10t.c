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

#include "red2_v33.h"
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SAMPLES 16384
static const unsigned residues[8] = {1,7,11,13,17,19,23,29};
typedef struct {
    uint64_t segment, bytes, bad;
    unsigned visits;
} sample;
typedef struct {
    uint64_t limit, blocks, segments, callback_count;
    size_t segment_bytes, prime_count, sample_count;
    uint32_t *primes;
    sample samples[MAX_SAMPLES];
    unsigned *segment_visits;
    int inject_error;
} oracle;

static uint64_t number(const char *s, uint64_t max) {
    char *end;
    errno = 0;
    if (!*s || *s == '-' || *s == '+') exit(2);
    unsigned long long v = strtoull(s, &end, 10);
    if (errno || *end || v > max) exit(2);
    return (uint64_t)v;
}
static void choose(oracle *o, uint64_t seg) {
    if (seg >= o->segments) return;
    for (size_t i=0; i<o->sample_count; ++i) if (o->samples[i].segment == seg) return;
    if (o->sample_count == MAX_SAMPLES) { fputs("too many sample windows\n", stderr); exit(2); }
    o->samples[o->sample_count++].segment = seg;
}
static void neighbors(oracle *o, uint64_t seg) {
    if (seg) choose(o, seg-1);
    choose(o, seg);
    choose(o, seg+1);
}
static int compare_sample(const void *a, const void *b) {
    uint64_t x=((const sample *)a)->segment, y=((const sample *)b)->segment;
    return (x>y)-(x<y);
}
static void verify(uint64_t start, const uint8_t *bits, size_t size, void *context) {
    oracle *o=context;
    if (start % o->segment_bytes || start+size > o->blocks ||
        size != (o->blocks-start < o->segment_bytes ? o->blocks-start : o->segment_bytes)) {
        fputs("invalid callback bounds\n", stderr); exit(1);
    }
    uint64_t completed;
    #pragma omp atomic capture
    completed=++o->callback_count;
    uint64_t stride=o->segments/10u;
    if (!stride) stride=1;
    if (completed%stride==0 || completed==o->segments) {
        fprintf(stderr,"CHECK_PROGRESS completed_segments=%" PRIu64 "/%" PRIu64 "\n",completed,o->segments);
    }
    uint64_t seg=start/o->segment_bytes;
    unsigned previous;
    #pragma omp atomic capture
    { previous=o->segment_visits[seg]; o->segment_visits[seg]++; }
    if (previous) { fputs("duplicate segment callback\n",stderr); exit(1); }
    size_t left=0, right=o->sample_count;
    while (left<right) { size_t mid=left+(right-left)/2;
        if (o->samples[mid].segment < seg) left=mid+1; else right=mid;
    }
    if (left==o->sample_count || o->samples[left].segment!=seg) return;
    sample *s=&o->samples[left];
    uint64_t lo=30u*start, hi=30u*(start+size);
    size_t len=(size_t)(hi-lo);
    unsigned char *prime=malloc(len);
    if (!prime) { fputs("oracle window allocation failed\n", stderr); exit(2); }
    memset(prime,1,len);
    for (size_t i=0; i<o->prime_count; ++i) {
        uint64_t p=o->primes[i];
        if (p*p>=hi) break;
        uint64_t first=((lo+p-1)/p)*p;
        if (first<p*p) first=p*p;
        for (uint64_t m=first; m<hi; m+=p) prime[m-lo]=0;
    }
    if (lo==0) { prime[0]=0; prime[1]=0; }
    for (size_t i=0; i<size; ++i) {
        unsigned want=0, got=bits[i];
        for (unsigned lane=0; lane<8; ++lane) {
            uint64_t value=lo+30u*i+residues[lane];
            if (value<=o->limit && prime[30u*i+residues[lane]]) want|=1u<<lane;
        }
        /* A negative control for this checker, never enabled by run_profile.sh. */
        if (o->inject_error && left==0 && i==0) got^=1u;
        s->bad+=(got!=want);
    }
    ++s->visits; s->bytes=size;
    free(prime);
}
int main(int argc, char **argv) {
    if (argc>6) return 2;
    uint64_t n=argc>1 ? number(argv[1],RED2_RANGE_MAX_N) : RED2_RANGE_MAX_N;
    int threads=argc>2 ? (int)number(argv[2],1024) : 12;
    size_t bytes=(argc>3 ? (size_t)number(argv[3],8192) : 512u)*1024u;
    unsigned multiplier=argc>4 ? (unsigned)number(argv[4],64) : 1;
    int inject=argc>5 && !strcmp(argv[5],"--inject-error");
    if (n<2 || threads<1 || bytes<4096 || multiplier<1 || (argc>5&&!inject)) return 2;
    oracle *o=calloc(1,sizeof(*o));
    if (!o) return 2;
    o->limit=n; o->blocks=n/30u+1u; o->segment_bytes=bytes;
    o->segments=(o->blocks+bytes-1)/bytes; o->inject_error=inject;
    uint32_t root=(uint32_t)sqrt((double)n);
    while ((uint64_t)(root+1)*(root+1)<=n) ++root;
    while ((uint64_t)root*root>n) --root;
    unsigned char *prime=malloc((size_t)root+1);
    o->primes=malloc(((size_t)root+1)*sizeof(*o->primes));
    if (!prime || !o->primes) return 2;
    memset(prime,1,(size_t)root+1); prime[0]=0;
    if (root>=1) prime[1]=0;
    for (uint32_t p=2; p<=root/p; ++p) if (prime[p])
        for (uint32_t m=p*p; m<=root; m+=p) prime[m]=0;
    for (uint32_t p=2; p<=root; ++p) if (prime[p]) o->primes[o->prime_count++]=p;
    free(prime);
    neighbors(o,0); neighbors(o,o->segments-1);
    for (uint64_t i=0; i<=32; ++i) choose(o, i*(o->segments-1)/32u);
    o->segment_visits=calloc((size_t)o->segments,sizeof(*o->segment_visits));
    if (!o->segment_visits) return 2;
    int effective_threads=threads;
    if ((uint64_t)effective_threads>o->segments) effective_threads=(int)o->segments;
    int territories=effective_threads>1 ? effective_threads*4 : 1;
    if ((uint64_t)territories>o->segments) territories=(int)o->segments;
    for (int i=1; i<territories; ++i) neighbors(o,o->segments*(uint64_t)i/(uint64_t)territories);
    uint64_t cube=(uint64_t)cbrt((double)n);
    while ((cube+1)*(cube+1)*(cube+1)<=n) ++cube;
    while (cube*cube*cube>n) --cube;
    cube*=multiplier;
    if (cube>root) cube=root;
    size_t first=0;
    while (first<o->prime_count && o->primes[first]<=cube) ++first;
    if (first<o->prime_count) {
        size_t ix[3]={first,first+(o->prime_count-first)/2,o->prime_count-1};
        for (unsigned i=0;i<3;i++) {
            uint64_t p=o->primes[ix[i]];
            neighbors(o,(p*p/30u)/bytes);
        }
    }
    qsort(o->samples,o->sample_count,sizeof(*o->samples),compare_sample);
    printf("VALIDATION N=%" PRIu64 " threads=%d segment_kib=%zu oracle_root=%u selected_windows=%zu frontier_multiplier=%u\n",
           n,threads,bytes/1024u,root,o->sample_count,multiplier);
    fflush(stdout);
    red2_v33_result r=red2_v33_sieve(n,threads,bytes,19,70000,163,4,multiplier,verify,o);
    uint64_t bad=0,checked=0;
    int missing=0;
    for (size_t i=0; i<o->sample_count; ++i) {
        sample *s=&o->samples[i];
        missing+=(s->visits!=1); bad+=s->bad; checked+=s->bytes;
        printf("WINDOW segment=%" PRIu64 " first_block=%" PRIu64 " bytes=%" PRIu64 " bad=%" PRIu64 " visits=%u\n",
               s->segment,s->segment*bytes,s->bytes,s->bad,s->visits);
    }
    uint64_t expected=UINT64_MAX;
    switch(n) {
        case 1000000: expected=78498; break;
        case 100000000: expected=5761455; break;
        case 1000000000: expected=50847534; break;
        case 10000000000ULL: expected=455052511; break;
        case 100000000000ULL: expected=4118054813ULL; break;
        case 1000000000000ULL: expected=37607912018ULL; break;
        case 10000000000000ULL: expected=346065536839ULL; break;
    }
    for (uint64_t seg=0;seg<o->segments;seg++) missing+=(o->segment_visits[seg]!=1);
    int failed=bad || missing || o->callback_count!=o->segments ||
        (expected!=UINT64_MAX && r.prime_count!=expected);
    printf("%s: %zu independently checked sample windows at N=%" PRIu64
           "; W30_bytes=%" PRIu64 "; represented_candidates=%" PRIu64
           "; count=%" PRIu64 "; oracle_root=%u; count_checkpoint=%s; callback_segments=%" PRIu64
           "; bad_bytes=%" PRIu64 "; missing_or_repeated=%d; engine_estimated_peak_bytes=%zu; actual_threads=%d; tasks=%d\n",
           failed?"FAIL":"PASS",o->sample_count,n,checked,checked*8u,r.prime_count,root,
           expected==UINT64_MAX?"unavailable":r.prime_count==expected?"pass":"fail",o->callback_count,bad,missing,r.estimated_peak_bytes,r.threads_used,r.task_count);
    free(o->segment_visits); free(o->primes); free(o);
    return failed?1:ferror(stdout)?2:0;
}
