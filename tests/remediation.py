#!/usr/bin/env python3
"""M-1 regressions: literal expectations, including the original crashing inputs."""
import json, pathlib, subprocess, sys, tempfile

def validate(binary):
    long_name='a'*40000
    valid={
        'leading_spaces':(' '*40000+'return 0\n',0),
        'long_identifier':(f'{long_name} = 17\nreturn {long_name}\n',17),
        'long_label':(f'goto {long_name}\nreturn 8\n{long_name}:\nreturn 3\n',3),
        'long_zero_literal':('return '+'0'*40000+'\n',0),
        'mixed_whitespace':('\n \t\r\n\t x \t=\t -7\n\treturn\t x\t# comment\n',-7),
        'compact_minus':('x=1--2\nreturn x\n',3),
        'keyword_variable':('return = -5\nreturn return\n',-5),
        'branch_tabs':('if\t-1\tgoto\tend\nreturn 4\nend:\nreturn 9\n',9),
    }
    # No length cutoff: check around common allocation/power-of-two boundaries.
    for length in [32767,32768,65535,65536]:
        valid[f'boundary_{length}']=(' '*(length-len('return 1'))+'return 1\n',1)
    invalid={
        'huge_literal':'return '+'9'*40000+'\n',
        'huge_negative':'return -'+'9'*40000+'\n',
        'long_malformed':'x = '+'1+'*40000+'?\n',
        'long_name_malformed':long_name+' ???\n',
        'empty':'', 'whitespace_only':'\n\t \r\n',
        'space_before_colon':'name :\nreturn 0\n',
        'missing_goto_space':'if 1 gotoend\nend:\nreturn 0\n',
        'missing_condition_space':'if 1goto end\nend:\nreturn 0\n',
        'plus_literal':'return +1\n', 'split_negative':'return - 1\n',
        'trailing_atom':'return 1 extra\n', 'bad_binary':'x = 1 ** 2\n',
        'joined_keyword':'gotoend\nend:\nreturn 0\n',
    }
    with tempfile.TemporaryDirectory() as temp:
        path=pathlib.Path(temp)/'case.ir'; optimized=pathlib.Path(temp)/'optimized.ir'
        def invoke(cmd,p):
            result=subprocess.run([binary,cmd,str(p)],text=True,capture_output=True,timeout=20)
            assert 'AddressSanitizer' not in result.stderr and 'runtime error:' not in result.stderr,result.stderr
            return result
        for name,(source,value) in valid.items():
            path.write_text(source)
            for p in [path,optimized]:
                if p==optimized:
                    result=invoke('optimize',path);assert result.returncode==0,(name,result.stderr);optimized.write_text(result.stdout)
                result=invoke('run',p);assert result.returncode==0,(name,result.stderr)
                actual=json.loads(result.stdout);assert (actual['status'],actual['value'])==('return',value),(name,actual)
        for name,source in invalid.items():
            path.write_text(source);result=invoke('optimize',path)
            assert result.returncode==1 and result.stderr.startswith('miniopt:'),(name,result.returncode,result.stderr)
    return {'valid_long_line_and_grammar_cases':len(valid),'controlled_rejections':len(invalid)}

if __name__=='__main__':print(json.dumps(validate(str(pathlib.Path(sys.argv[1]).resolve())),indent=2))
