import subprocess,json,os,time
from pathlib import Path
p=Path(__file__).resolve().parents[1]
env=dict(os.environ,OMP_DYNAMIC='false',OMP_PROC_BIND='false',ASAN_OPTIONS='detect_leaks=0');env.pop('OMP_PLACES',None)
def run(args,name):
 with (p/'evidence'/name).open('w') as f:
  r=subprocess.run(args,cwd=p,env=env,stdout=f,stderr=subprocess.STDOUT)
 if r.returncode:raise RuntimeError((args,name,r.returncode))
 print('PASS',name,flush=True)
flags=['gcc','-O2','-std=c11','-Wall','-Wextra','-Wpedantic','-fopenmp','-DRED2_CALENDAR_CHECKS=1']
run(flags+['validate_block_cases.c','red2_block.c','-lm','-o','validate_portable'],'build_portable.txt')
run(['./validate_portable'],'validation_portable.txt')
run(flags+['-DRED2_CALENDAR_MAX_BYTES=64','validate_block_cases.c','red2_block.c','-lm','-o','validate_fallback'],'build_fallback.txt')
run(['./validate_fallback'],'validation_fallback.txt')
run(flags+['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','screen_window.c','-lm','-o','window_sanitize'],'build_sanitize.txt')
run(['./window_sanitize','10000000000000','3','512','635779','4','2','64','1','1'],'sanitizer_10t_tail.txt')
cases=[]
for n in [10**12,3*10**12,16385**3-1,16385**3,10**13,9999999999987]:
 segs=(n//30+1+524287)//524288
 for first in [0,segs//2,segs-4]:
  cases.append((n,first))
with (p/'evidence/oracle_windows.jsonl').open('w') as f:
 for i,(n,first) in enumerate(cases):
  # Final auto geometry plus explicit decoding at low N and tile64 coverage.
  mode=2 if i%3==0 else 0;tile=64 if i%3==0 else 0
  args=['./window_v35',str(n),'4','512',str(first),'4',str(mode),str(tile),'1','1']
  r=subprocess.run(args,cwd=p,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,check=True)
  v=json.loads(r.stdout);f.write(json.dumps(v)+'\n');f.flush()
print('PASS 18 independent high-range windows',flush=True)
run(['./validate_range_v35','10000000000000','8','512','1'],'full_10t_oracle.txt')
print('ALL RELEASE CHECKS COMPLETE',flush=True)
