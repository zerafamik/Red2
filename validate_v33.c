/* =========================================================================
   PROJECT: ASTER MOVIE SIEVE (v0.33 Independent Full-Bitmap Validation)
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

static const unsigned R[8]={1,7,11,13,17,19,23,29};
typedef struct {uint64_t limit;const unsigned char *prime;unsigned long long bad,blocks;} check;
static void verify_segment(uint64_t start,const uint8_t *buf,size_t size,void *context) {
    check *c=context;unsigned long long bad=0;
    for(size_t i=0;i<size;i++) {
        unsigned byte=0;
        for(unsigned r=0;r<8;r++) {
            uint64_t n=30u*(start+i)+R[r];
            if(n<=c->limit && c->prime[n]) byte|=1u<<r;
        }
        bad += byte!=buf[i];
    }
    #pragma omp atomic update
    c->bad+=bad;
    #pragma omp atomic update
    c->blocks+=size;
}
int main(void) {
    const uint64_t bound=100000007;
    unsigned char *prime=malloc((size_t)bound+1);
    uint32_t *prefix=malloc(((size_t)bound+1)*sizeof(uint32_t));
    if(!prime || !prefix)return 2;
    memset(prime,1,(size_t)bound+1);prime[0]=prime[1]=0;
    for(uint64_t p=2;p<=bound/p;p++) if(prime[p])
        for(uint64_t m=p*p;m<=bound;m+=p)prime[m]=0;
    uint32_t count=0;
    for(uint64_t n=0;n<=bound;n++){count+=prime[n];prefix[n]=count;}
    uint64_t limits[200];unsigned size=0;
    for(unsigned n=0;n<35;n++)limits[size++]=n;
    uint64_t anchors[]={4096*30,32768*30,131072*30,137*137*137,163*163*163,1009*1009,19*19*19,5000000};
    for(unsigned a=0;a<sizeof(anchors)/sizeof(*anchors);a++) for(int d=-2;d<=2;d++)limits[size++]=anchors[a]+d;
    uint64_t seed=UINT64_C(0x9e3779b97f4a7c15);
    for(unsigned i=0;i<45;i++){seed=seed*UINT64_C(6364136223846793005)+1;limits[size++]=seed%10000000;}
    limits[size++]=bound;
    const unsigned configured_cases_begin=size;
    for(unsigned extra=0;extra<12;extra++)limits[size++]=bound-extra;
    for(unsigned i=0;i<size;i++) {
        uint64_t n=limits[i];int threads=i%3?1:3;
        size_t segment=(i%3==0?4096u:i%3==1?32768u:131072u);
        int layer=i%2?19:137,batch=3;
        unsigned multiplier=i>=configured_cases_begin?1u:1u<<(i%6);
        if(i>=configured_cases_begin){layer=163;batch=4;threads=i%2?8:1;segment=i%3==0?5000u:i%3==1?100003u:131072u;}
        uint32_t dense=i%4?70000:23;
        check c={n,prime,0,0};
        red2_v33_result r=red2_v33_sieve(n,threads,segment,19,dense,layer,batch,multiplier,verify_segment,&c);
        uint64_t expected_blocks=n<2?0:n/30u+1u;
        if(c.bad || c.blocks!=expected_blocks || r.prime_count!=prefix[n]) {
            fprintf(stderr,"FAIL product N=%" PRIu64 " bad=%llu blocks=%llu expected=%" PRIu64 " got=%" PRIu64 " want=%u\n",n,c.bad,c.blocks,expected_blocks,r.prime_count,prefix[n]);return 1;
        }

    }
    printf("PASS: %u full-bitmap product cases; independent ordinary sieve through %" PRIu64 ".\n",size,bound);
    free(prefix);free(prime);return 0;
}
