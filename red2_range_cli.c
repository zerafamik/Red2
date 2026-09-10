/* =========================================================================
   PROJECT: ASTER MOVIE SIEVE (v0.33 Tagged Product Full Sieve)
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t number(const char *s,uint64_t max) {
    char *end=0;errno=0;
    if(!*s || *s=='-' || *s=='+'){fputs("invalid unsigned argument\n",stderr);exit(2);}
    unsigned long long n=strtoull(s,&end,10);
    if(errno || *end || n>max){fputs("argument outside supported range\n",stderr);exit(2);}
    return (uint64_t)n;
}
static void emit(uint64_t start,const uint8_t *buf,size_t size,void *ctx) {
    (void)ctx;static const unsigned residue[8]={1,7,11,13,17,19,23,29};
    for(size_t i=0;i<size;i++) for(unsigned r=0;r<8;r++) if(buf[i]&(1u<<r))
        if(printf("%" PRIu64 "\n",30u*(start+i)+residue[r])<0)exit(2);
}
int main(int argc,char **argv) {
    uint64_t n=UINT64_C(100000000000);int threads=12,list=0,layer=163,batch=4;
    size_t segment=512u*1024u;unsigned multiplier=1;
    for(int i=1;i<argc;i++) {
        const char *arg=argv[i];
        if(!strcmp(arg,"--version")){puts("RED2 v0.33 Range Extension to 10^13 (full sieve)");return 0;}
        if(!strcmp(arg,"--help")) {
            puts("RED2 full sieve: --limit N --threads T --segment-kib K --frontier-multiplier M --layer-max P --plate-batch B --list\n"
                 "Defaults: N=1e11, T=12, K=512, M=1, P=163, B=4. Range: 0<=N<=1e13 (64-bit build).\n"
                 "--list emits ordered primes, uses one thread, and reports timing to stderr.\n"
                 "For a controlled comparison run bash compare_10t.sh with the primesieve path.");return 0;
        }
        if(!strcmp(arg,"--list")){list=1;continue;}
        if(i+1==argc){fputs("missing argument value\n",stderr);return 2;}
        const char *value=argv[++i];
        if(!strcmp(arg,"--limit"))n=number(value,RED2_RANGE_MAX_N);
        else if(!strcmp(arg,"--threads"))threads=(int)number(value,1024);
        else if(!strcmp(arg,"--segment-kib"))segment=(size_t)number(value,8192)*1024u;
        else if(!strcmp(arg,"--layer-max"))layer=(int)number(value,163);
        else if(!strcmp(arg,"--plate-batch"))batch=(int)number(value,4);
        else if(!strcmp(arg,"--frontier-multiplier"))multiplier=(unsigned)number(value,64);
        else {fprintf(stderr,"unknown option: %s\n",arg);return 2;}
    }
    if(threads<1 || segment<4096 || multiplier<1 || batch<1)return 2;
    if(layer!=109 && layer!=127 && layer!=137 && layer!=149 && layer!=157 && layer!=163)return 2;
    if(list)threads=1;
    double start=red2_v33_now_seconds();
    if(list){if(n>=2)puts("2");if(n>=3)puts("3");if(n>=5)puts("5");}
    red2_v33_result r=red2_v33_sieve(n,threads,segment,19,70000,layer,batch,multiplier,list?emit:NULL,NULL);
    if(list && fflush(stdout))return 2;
    double total=red2_v33_now_seconds()-start;
    uint64_t expected=UINT64_MAX;
    switch(n){case 0:case 1:expected=0;break;case 2:expected=1;break;case 10:expected=4;break;
      case 100:expected=25;break;case 1000:expected=168;break;case 1000000:expected=78498;break;
      case 100000000:expected=5761455;break;case 1000000000:expected=50847534;break;
      case 10000000000ULL:expected=455052511;break;case 100000000000ULL:expected=4118054813ULL;break;
      case 1000000000000ULL:expected=37607912018ULL;break;
      case 10000000000000ULL:expected=346065536839ULL;break;}
    if(expected!=UINT64_MAX && r.prime_count!=expected){fputs("FAIL known prime-count checkpoint\n",stderr);return 1;}
    FILE *f=list?stderr:stdout;
    fprintf(f,"{\"engine\":\"RED2_v0.33_range_10t_full_sieve\",\"limit\":%" PRIu64 ",\"prime_count\":%" PRIu64
       ",\"threads\":%d,\"segment_bytes\":%zu,\"marking_frontier\":%u,\"aux_limit\":%u,"
       "\"residual_semiprimes_marked\":%" PRIu64 ",\"estimated_peak_bytes\":%zu,\"total_seconds\":%.9f,"
       "\"layer_max\":%d,\"plate_batch\":%d,\"dense_tile_bytes\":%zu,\"tasks\":%d,\"prepare_seconds\":%.9f,\"work_seconds\":%.9f,\"checkpoint\":\"%s\"}\n",
       n,r.prime_count,r.threads_used,r.segment_bytes,r.marking_frontier,r.aux_limit,r.residual_semiprime_count,
       r.estimated_peak_bytes,total,r.layer_max,r.plate_batch,r.dense_tile_bytes,r.task_count,r.prepare_seconds,r.work_seconds,expected==UINT64_MAX?"not_available":"pass");
    return ferror(f)?2:0;
}
