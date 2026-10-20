#!/usr/bin/env python3
import argparse, datetime, json, pathlib, platform, subprocess, sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tests'))
from test import validate
from remediation import validate as validate_regressions

def output(*args): return subprocess.check_output(args,text=True,cwd=ROOT).strip()
def main():
    parser=argparse.ArgumentParser();parser.add_argument('binary');parser.add_argument('--smoke',action='store_true');args=parser.parse_args()
    binary=str(pathlib.Path(args.binary).resolve()); correctness=validate(binary,args.smoke); regressions=validate_regressions(binary)
    rows=[]
    for path in sorted((ROOT/'fixtures').glob('*.ir')):
        row=json.loads(output(binary,'stats',str(path)));row['fixture']=str(path.relative_to(ROOT))
        row['instruction_reduction_pct']=100*(row['original_instructions']-row['optimized_instructions'])/row['original_instructions']
        rows.append(row)
    if args.smoke:
        assert len(rows)==5 and all(r['optimized_instructions']<=r['original_instructions'] for r in rows)
        print('benchmark smoke passed');return
    original=sum(r['original_instructions'] for r in rows);optimized=sum(r['optimized_instructions'] for r in rows)
    data={'source_commit':output('git','rev-parse','HEAD'),'date_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
          'os':platform.platform(),'cpu':next((l.split(':',1)[1].strip() for l in pathlib.Path('/proc/cpuinfo').read_text().splitlines() if l.startswith('model name')),'unknown'),
          'compiler':output('g++','--version').splitlines()[0],'build':'CMake Release; GCC; -O3 -DNDEBUG',
          'method':'In-process steady_clock: one warmup, 21 samples of 10 optimizations; microseconds per optimization. Synthetic fixtures. Labels excluded from instruction counts.',
          'audit_regressions':regressions,'correctness':correctness,'workloads':rows,'aggregate':{'original_instructions':original,'optimized_instructions':optimized,'weighted_reduction_pct':100*(original-optimized)/original,'arithmetic_mean_reduction_pct':sum(r['instruction_reduction_pct'] for r in rows)/len(rows)},
          'limitations':['No claim of native program runtime speedup.','Execution-limit outcomes are inconclusive; optimization can change executed instruction counts.','Source commit identifies code under measurement; results are added in a subsequent commit.','unreachable_blocks_removed counts blocks eliminated over the fixed-point pipeline, including after branch folding; initial_unreachable_blocks is the initial CFG count.']}
    (ROOT/'benchmarks/results.json').write_text(json.dumps(data,indent=2)+'\n')
    lines=['# Measured MiniOpt results','',f"Source commit: `{data['source_commit']}`; date: {data['date_utc']}",f"Environment: {data['os']}; {data['cpu']}; {data['compiler']}",'',data['method'],'','| Fixture | Instructions before / after | Reduction | Blocks before / after | Unreachable blocks removed | Optimization µs median (min–max) |','|---|---:|---:|---:|---:|---:|']
    for r in rows: lines.append(f"| {r['fixture']} | {r['original_instructions']} / {r['optimized_instructions']} | {r['instruction_reduction_pct']:.2f}% | {r['original_blocks']} / {r['optimized_blocks']} | {r['unreachable_blocks_removed']} | {r['optimization_us_median']:.2f} ({r['optimization_us_min']:.2f}–{r['optimization_us_max']:.2f}) |")
    lines+=['',f"Weighted aggregate reduction: **{data['aggregate']['weighted_reduction_pct']:.2f}%** ({original} → {optimized}). Arithmetic mean of per-fixture reductions: {data['aggregate']['arithmetic_mean_reduction_pct']:.2f}%.",f"Differential comparisons passed: {correctness['differential_passed']}; correctness failures: {correctness['correctness_failures']}; execution-limit cases inconclusive: {correctness['inconclusive_execution_limits']}; malformed inputs rejected: {correctness['invalid_inputs_rejected']}.",'','## Limitations','']+['- '+s for s in data['limitations']]
    (ROOT/'benchmarks/results.md').write_text('\n'.join(lines)+'\n')
    print(json.dumps(data['aggregate']));print(json.dumps(correctness))
if __name__=='__main__':main()
