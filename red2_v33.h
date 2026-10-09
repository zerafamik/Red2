/* =========================================================================
   PROJECT: ASTER MOVIE SIEVE (v0.33 Product Completion Full Sieve)
   COPYRIGHT (C) 2026 [YOUR LEGAL NAME]. ALL RIGHTS RESERVED.

   PROPRIETARY & CONFIDENTIAL MATERIAL.
   This software source code, architectural design, data structures, and
   underlying mathematical kinematics concepts are the sole intellectual
   property of the author.

   Strictly prohibited: Unauthorized copying, modification, distribution,
   compilation, reverse-engineering, or commercial use of this file,
   in whole or in part, via any medium without explicit written consent.
   ========================================================================= */
#ifndef RED2_V33_H
#define RED2_V33_H
#include <stddef.h>
#include <stdint.h>

/* Audited range extension; the v0.33 function ABI is retained. */
#define RED2_RANGE_MAX_N UINT64_C(10000000000000)

typedef struct {
    uint64_t limit, prime_count, segments;
    int threads_requested, threads_used;
    size_t segment_bytes, estimated_peak_bytes;
    int presieve_max;
    size_t presieve_period_bytes;
    int layer_max;
    size_t layer_count, layer_mask_bytes;
    int plate_batch;
    uint32_t dense_cutoff;
    size_t dense_prime_count, sparse_prime_count;
    size_t resident_sparse_prime_count, bucket_sparse_prime_count;
    size_t bucket_page_capacity_per_thread;

    /* A full bitmap is produced in both modes. The factor-pair path uses
       marking_frontier>=floor(cuberoot(limit)); all residual products are
       explicitly marked. residual_semiprime_count is an audit count only.
       estimated_peak_bytes estimates engine allocations, not process RSS. */
    int product_completion_used;
    uint32_t marking_frontier;
    uint32_t aux_limit;
    size_t aux_prime_count;
    size_t aux_peak_bytes;
    uint64_t bitmap_survivor_count;
    uint64_t residual_semiprime_count;
    double completion_prepare_seconds;

    double prepare_seconds, work_seconds;
    size_t dense_tile_bytes;
    int task_count;
} red2_v33_result;

red2_v33_result red2_v33_count(
    uint64_t limit, int threads, size_t segment_bytes, int presieve_max,
    uint32_t dense_cutoff, int layer_max, int plate_batch);
int red2_v33_max_threads(void);
double red2_v33_now_seconds(void);
const char *red2_v33_timer_name(void);
/* Callback: fully sieved W30 bytes for blocks [start,start+size).
   Calls can be concurrent and unordered. Copy data before callback returns.
   Bit lanes: {1,7,11,13,17,19,23,29}. Handle primes 2,3,5 separately.
   Use one thread if ordered streaming is required. No callback for N<2.
   frontier_multiplier is clamped to 1..64, and y is capped at floor(sqrt(N)). */
typedef void (*red2_v33_segment_fn)(uint64_t,const uint8_t*,size_t,void*);
red2_v33_result red2_v33_sieve(uint64_t,int,size_t,int,uint32_t,int,int,unsigned,red2_v33_segment_fn,void*);
#endif
