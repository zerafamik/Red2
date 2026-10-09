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

/* Benchmark-only window driver. Its count is NOT pi(N).
   check=1 verifies every selected bitmap byte with an ordinary sieve. */
#define RED2_BENCH_WINDOW 1
#ifdef RED2_WINDOW_CONTROL
#include "red2_v34_window.c"
#else
#include "red2_block.c"
#endif
#include <errno.h>
static const unsigned oracle_residues[8]={1,7,11,13,17,19,23,29};
typedef struct {
 uint64_t n,first,count,total_blocks,checked,bad,survivors;
 size_t bytes,pc;uint32_t *primes;unsigned *visits;
} window_oracle;
static uint64_t arg(const char *s,uint64_t cap) {
 char *end;errno=0;if(!*s||*s=='-'||*s=='+')exit(2);
 uint64_t n=strtoull(s,&end,10);if(errno||*end||n>cap)exit(2);return n;
}
static void oracle_check(uint64_t start,const uint8_t *buf,size_t size,void *ctx) {
 window_oracle *o=ctx;uint64_t seg=start/o->bytes;
 if(start%o->bytes||seg<o->first||seg>=o->first+o->count||
    size!=(o->total_blocks-start<o->bytes?o->total_blocks-start:o->bytes))exit(1);
 unsigned prior;
 #pragma omp atomic capture
 {prior=o->visits[seg-o->first];o->visits[seg-o->first]++;}
 if(prior)exit(1);
 uint64_t lo=30*start,hi=30*(start+size),bad=0,survivors=0;
 unsigned char *prime=malloc((size_t)(hi-lo));if(!prime)exit(2);
 memset(prime,1,(size_t)(hi-lo));
 for(size_t i=0;i<o->pc;i++) {
  uint64_t p=o->primes[i];if(p*p>=hi)break;
  uint64_t m=((lo+p-1)/p)*p;if(m<p*p)m=p*p;
  for(;m<hi;m+=p)prime[m-lo]=0;
 }
 if(!lo){prime[0]=0;prime[1]=0;}
 for(size_t i=0;i<size;i++) {
  unsigned want=0;
  for(unsigned j=0;j<8;j++) {
   uint64_t v=lo+30*i+oracle_residues[j];
   if(v<=o->n&&prime[30*i+oracle_residues[j]]){want|=1u<<j;survivors++;}
  }
  bad+=want!=buf[i];
 }
 free(prime);
 #pragma omp atomic update
 o->bad+=bad;
 #pragma omp atomic update
 o->checked+=size;
 #pragma omp atomic update
 o->survivors+=survivors;
}
int main(int argc,char **argv) {
 if(argc!=10){fputs("Usage: screen_window N threads segment_KiB first_segment segments mode(0/1/2) tile_KiB bound(0/1) check(0/1)\n",stderr);return 2;}
 uint64_t n=arg(argv[1],RED2_RANGE_MAX_N);int threads=(int)arg(argv[2],1024);
 size_t bytes=(size_t)arg(argv[3],8192)*1024u;
 bench_first_segment=arg(argv[4],UINT64_MAX);bench_segment_count=arg(argv[5],UINT32_MAX);
 unsigned mode=(unsigned)arg(argv[6],2);size_t tile=(size_t)arg(argv[7],64)*1024u;
 int bound=(int)arg(argv[8],1),check=(int)arg(argv[9],1);
 if(n<2||threads<1||bytes<4096||!bench_segment_count||(tile&&tile<4096))return 2;
 window_oracle o={0};o.n=n;o.bytes=bytes;o.first=bench_first_segment;o.total_blocks=n/30+1;
 uint64_t segments=(o.total_blocks+bytes-1)/bytes;
 if(o.first>=segments)return 2;
 o.count=bench_segment_count<segments-o.first?bench_segment_count:segments-o.first;
 if(check) {
  uint64_t root=(uint64_t)sqrt((double)n);while((root+1)*(root+1)<=n)root++;while(root*root>n)root--;
  uint8_t *prime=malloc((size_t)root+1);o.primes=malloc(((size_t)root+1)*sizeof(uint32_t));
  o.visits=calloc((size_t)o.count,sizeof(unsigned));if(!prime||!o.primes||!o.visits)return 2;
  memset(prime,1,(size_t)root+1);prime[0]=prime[1]=0;
  for(uint64_t p=2;p<=root/p;p++)if(prime[p])for(uint64_t m=p*p;m<=root;m+=p)prime[m]=0;
  for(uint64_t p=2;p<=root;p++)if(prime[p])o.primes[o.pc++]=(uint32_t)p;
  free(prime);
 }
 red2_v34_stats stats;double t=red2_v33_now_seconds();
#ifdef RED2_WINDOW_CONTROL
 (void)mode;(void)tile;(void)bound;
 const char *engine="v34";
 red2_v33_result r=red2_v34_sieve(n,threads,bytes,19,70000,163,4,1,check?oracle_check:NULL,&o,2,&stats);
 unsigned used=1,shift=0;size_t cofactor=0,rowbytes=0;
#else
 const char *engine="v35";red2_v35_stats extra;red2_v35_options options={mode,tile,bound};
 red2_v33_result r=red2_v35_sieve(n,threads,bytes,19,70000,163,4,1,check?oracle_check:NULL,&o,2,&stats,&options,&extra);
 unsigned used=extra.cofactor_mode_used,shift=extra.cofactor_block_shift;
 size_t cofactor=extra.cofactor_table_bytes,rowbytes=extra.row_prime_bytes;
#endif
 double total=red2_v33_now_seconds()-t;
 if(check) {
  for(uint64_t j=0;j<o.count;j++)if(o.visits[j]!=1)return 1;
  if(o.bad||o.survivors!=r.bitmap_survivor_count)return 1;
 }
 printf("{\"engine\":\"%s\",\"N\":%" PRIu64 ",\"first_segment\":%" PRIu64 ",\"segments\":%" PRIu64 ",\"segment_bytes\":%zu,\"threads\":%d,\"bitmap_survivors\":%" PRIu64 ",\"total_seconds\":%.9f,\"prepare_seconds\":%.9f,\"work_seconds\":%.9f,\"calendar_bytes\":%zu,\"slots\":%u,\"cofactor_mode\":%u,\"block_shift\":%u,\"cofactor_bytes\":%zu,\"row_prime_bytes\":%zu,\"dense_tile_bytes\":%zu,\"oracle_enabled\":%d,\"checked_bytes\":%" PRIu64 ",\"bad_bytes\":%" PRIu64 "}\n",
 engine,n,o.first,o.count,bytes,r.threads_used,r.bitmap_survivor_count,total,r.prepare_seconds,r.work_seconds,stats.calendar_bytes_per_worker,stats.ring_slots,used,shift,cofactor,rowbytes,r.dense_tile_bytes,check,o.checked,o.bad);
 free(o.primes);free(o.visits);return 0;
}
