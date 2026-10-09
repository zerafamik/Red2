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

#define RED2_BENCH_WINDOW 1
#ifndef RED2_VARIANT
#define RED2_VARIANT "baseline.c"
#endif
#include RED2_VARIANT
int main(int argc,char **argv) {
    if(argc!=7)return 2;
    uint64_t n=strtoull(argv[1],NULL,10);
    int threads=atoi(argv[2]);size_t bytes=(size_t)strtoull(argv[3],NULL,10)*1024u;
    bench_first_segment=strtoull(argv[4],NULL,10);
    bench_segment_count=strtoull(argv[5],NULL,10);
    unsigned factor=(unsigned)atoi(argv[6]);
    double t=red2_v33_now_seconds();red2_v34_stats stats;
    red2_v33_result r=red2_v34_sieve(n,threads,bytes,19,70000,163,4,1,NULL,NULL,factor,&stats);
    printf("{\"variant\":\"%s\",\"N\":%" PRIu64 ",\"first_segment\":%" PRIu64 ",\"segments_requested\":%" PRIu64 ",\"segment_bytes\":%zu,\"threads\":%d,\"bitmap_survivors\":%" PRIu64 ",\"total_seconds\":%.9f,\"prepare_seconds\":%.9f,\"work_seconds\":%.9f,\"calendar_bytes\":%zu,\"slots\":%u,\"visits\":%" PRIu64 "}\n",
        RED2_VARIANT,n,bench_first_segment,bench_segment_count,r.segment_bytes,r.threads_used,r.bitmap_survivor_count,red2_v33_now_seconds()-t,r.prepare_seconds,r.work_seconds,stats.calendar_bytes_per_worker,stats.ring_slots,stats.direct_row_visits+stats.scheduled_row_visits);
    return 0;
}
