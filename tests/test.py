#!/usr/bin/env python3
"""Production CLI differential tests plus independently computed 32-bit outcomes."""
import argparse, json, pathlib, random, subprocess, tempfile

def validate(binary, smoke=False):
    cases = [
        ('constants','a = 10\nb = 20\nc = a + b\nd = c * 2\nunused = 999\nreturn d\n',('return',60)),
        ('negative','a = -7 / 3\nreturn a\n',('return',-2)),
        ('wrap','a = 2147483647 + 1\nb = a / -1\nreturn b\n',('return',-2147483648)),
        ('mul_wrap','a = 2147483647 * 2147483647\nreturn a\n',('return',1)),
        ('zero_trap','unused = 5 / 0\nreturn 9\n',('division_by_zero',0)),
        ('undefined_dead','unused = x + 1\nreturn 0\n',('undefined_variable:x',0)),
        ('undefined_self','x = x + 1\nreturn x\n',('undefined_variable:x',0)),
        ('unreachable','goto end\nbad:\nx = 1 / 0\nend:\nreturn 7\n',('return',7)),
        ('join','flag = 0\nif flag goto left\nx = 2\ngoto join\nleft:\nx = 3\njoin:\nreturn x\n',('return',2)),
        ('backedge','i = 0\ns = 0\nloop:\ns = s + i\ni = i + 1\nc = i < 20\nif c goto loop\nreturn s\n',('return',190)),
        ('join_undefined','i = 0\nloop:\nif i goto exit\ni = 1\ngoto loop\nexit:\nreturn z\n',('undefined_variable:z',0)),
        ('partial_definition','i = 0\nloop:\nc = i == 1\nif c goto exit\nx = 8\ni = i + 1\ngoto loop\nexit:\nreturn x\n',('return',8)),
        ('nonterminating','loop:\ngoto loop\n',('limit',0)),
        ('falls_off','a = 1\n',('fell_off',0)),
        ('branch_trap','if z goto end\nreturn 1\nend:\nreturn 0\n',('undefined_variable:z',0)),
    ]
    rng=random.Random(73519)
    def wrap(x): return (x+2**31)%2**32-2**31
    for n in range(30 if smoke else 300):
        # Reference expectation is computed independently in Python, not by MiniOpt.
        a,b=rng.randint(-100000,100000),rng.randint(-100000,100000)
        op=rng.choice(['+','-','*']); value=wrap({'+':lambda:a+b,'-':lambda:a-b,'*':lambda:a*b}[op]())
        bound=rng.randrange(1,12); delta=rng.randrange(-9,10)
        expected=wrap(value+bound*delta)
        program=f'a = {a}\nb = {b}\nx = a {op} b\ni = 0\nloop:\nx = x + {delta}\ni = i + 1\nc = i < {bound}\nif c goto loop\n'
        if rng.choice([True,False]):
            program+=f'c = x < 0\nif c goto neg\ny = x + 3\ngoto join\nneg:\ny = x - 5\njoin:\nreturn y\n'
            expected=wrap(expected+3 if expected>=0 else expected-5)
        else: program+='return x\n'
        cases.append((f'random_{n}',program,('return',expected)))
    passed=0; inconclusive=0
    with tempfile.TemporaryDirectory() as tmp:
        original=pathlib.Path(tmp)/'original.ir'; optimized=pathlib.Path(tmp)/'optimized.ir'
        for name,source,expected in cases:
            original.write_text(source)
            result=subprocess.run([binary,'optimize',str(original)],capture_output=True,text=True,check=True)
            optimized.write_text(result.stdout)
            outcomes=[json.loads(subprocess.check_output([binary,'run',str(p),'5000'],text=True)) for p in [original,optimized]]
            obs=[(o['status'],o['value']) for o in outcomes]
            if any(o['status']=='limit' for o in outcomes):
                assert expected[0]=='limit', (name,obs)
                inconclusive+=1; continue
            assert obs[0]==obs[1]==expected, (name,obs,expected,source,result.stdout)
            passed+=1
        invalid=['','return 2147483648','x = -2147483649','goto absent','x = 2 ** 3','a:\na:\nreturn 0','if x goto','return 1 extra','x = +1']
        for source in invalid:
            original.write_text(source)
            assert subprocess.run([binary,'optimize',str(original)],capture_output=True).returncode!=0,source
    return {'differential_passed':passed,'inconclusive_execution_limits':inconclusive,'invalid_inputs_rejected':len(invalid),'correctness_failures':0,'seed':73519}
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('binary');p.add_argument('--smoke',action='store_true');args=p.parse_args()
    print(json.dumps(validate(args.binary,args.smoke),indent=2))
