import subprocess,json,os,time,statistics,hashlib,platform
from pathlib import Path
p=Path(__file__).resolve().parents[1]
env=dict(os.environ,OMP_DYNAMIC='false',OMP_PROC_BIND='false');env.pop('OMP_PLACES',None)
evidence=p/'evidence'
# This driver is started only after all correctness/build jobs have ended.
cpu=next((s for s in Path('/proc/cpuinfo').read_text().splitlines() if s.startswith('model name')),'unknown')
metadata={'platform':platform.platform(),'cpu':cpu,'cpu_max':Path('/sys/fs/cgroup/cpu.max').read_text().strip(),'compiler':subprocess.check_output(['gcc','--version'],text=True).splitlines()[0],'flags':'-O3 -march=native -std=c11 -DNDEBUG -Wall -Wextra -Wpedantic -fopenmp','OMP_DYNAMIC':'false','OMP_PROC_BIND':'false','OMP_PLACES':'unset','started_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime())}
metadata['hashes']={name:hashlib.sha256((p/name).read_bytes()).hexdigest() for name in ['red2_block.c','red2_block_cli.c','red2_v35.h','red2_calendar.c','red2_calendar_cli.c','red2_v34_window.c','screen_window.c','red2_v34','red2_v35','window_v34','window_v35']}
(evidence/'local_environment.json').write_text(json.dumps(metadata,indent=2)+'\n')
cases=[(10**12,8,60000,2048),(3*10**12,8,180000,2048),(10**13,1,300000,256),(10**13,4,150000,1024),(10**13,8,630000,2048)]
with (evidence/'final_window_pairs.jsonl').open('w') as out:
 for case,(n,t,first,count) in enumerate(cases):
  for pair in [-1,0,1]:
   engines=['v35','v34'] if pair<1 else ['v34','v35']
   counts=[]
   for pos,engine in enumerate(engines,1):
    args=[f'./window_{engine}',str(n),str(t),'512',str(first),str(count),'0','0','1','0']
    data=json.loads(subprocess.check_output(args,cwd=p,env=env,text=True))
    data.update(case=case,pair=pair,phase='warmup' if pair<0 else 'measured',position=pos)
    counts.append(data['bitmap_survivors']);out.write(json.dumps(data)+'\n');out.flush()
    print('WINDOW',case,pair,pos,engine,data['work_seconds'],flush=True)
   assert counts[0]==counts[1]
with (evidence/'full_1t_pairs.jsonl').open('w') as out:
 for pair in [-1,0,1]:
  for pos,engine in enumerate(['v35','v34'] if pair<1 else ['v34','v35'],1):
   data=json.loads(subprocess.check_output([f'./red2_{engine}','--limit','1000000000000','--threads','8','--segment-kib','512'],cwd=p,env=env,text=True))
   assert data['prime_count']==37607912018
   data.update(comparison_engine=engine,pair=pair,phase='warmup' if pair<0 else 'measured',position=pos)
   out.write(json.dumps(data)+'\n');out.flush();print('FULL_1T',pair,pos,engine,data['total_seconds'],flush=True)
print('ALL BENCHMARKS COMPLETE',flush=True)
