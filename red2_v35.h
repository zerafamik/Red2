/* =========================================================================
   PROJECT: ASTER MOVIE SIEVE (v0.35 Blocked Cofactors Full Sieve)
   COPYRIGHT (C) 2026 [YOUR LEGAL NAME]. ALL RIGHTS RESERVED.
   
   PROPRIETARY & CONFIDENTIAL MATERIAL.
   This software source code, architectural design, data structures, and 
   underlying mathematical kinematics concepts are the sole intellectual 
   property of the author. 
   
   Strictly prohibited: Unauthorized copying, modification, distribution, 
   compilation, reverse-engineering, or commercial use of this file, 
   in whole or in part, via any medium without explicit written consent.
   ========================================================================= */

#ifndef RED2_V35_H
#define RED2_V35_H
#include "red2_v34.h"
typedef struct {
    unsigned cofactor_mode; /* 0=geometry auto, 1=plain, 2=blocked */
    size_t dense_tile_bytes; /* 0=auto, or 4096..65536 */
    int tight_calendar; /* 0=original bound, 1=admissible-product bound */
} red2_v35_options;
typedef struct {
    unsigned cofactor_mode_used,cofactor_block_shift;
    size_t cofactor_table_bytes,plain_cofactor_bytes,row_prime_bytes;
    uint64_t calendar_max_product_step;
    int high_range_geometry,tight_calendar;
} red2_v35_stats;
red2_v33_result red2_v35_sieve(uint64_t,int,size_t,int,uint32_t,int,int,unsigned,
    red2_v33_segment_fn,void*,unsigned,red2_v34_stats*,const red2_v35_options*,red2_v35_stats*);
#endif
