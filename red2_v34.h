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
#ifndef RED2_V34_H
#define RED2_V34_H
#include "red2_v33.h"
typedef struct {
    size_t direct_rows, scheduled_rows, calendar_bytes_per_worker;
    uint64_t direct_row_visits, scheduled_row_visits;
    uint32_t cutoff, max_cofactor_gap, ring_slots;
    unsigned schedule_factor;
    int memory_fallback;
} red2_v34_stats;
/* schedule_factor 0 disables calendars; 1..64 selects cutoff=factor*segment bytes.
   The v0.33 API below uses factor 2. All modes produce full sieve bitmaps. */
red2_v33_result red2_v34_sieve(uint64_t,int,size_t,int,uint32_t,int,int,unsigned,
    red2_v33_segment_fn,void*,unsigned,red2_v34_stats*);
#endif
