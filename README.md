# MiniOpt

A C++17 optimizer, control-flow analyzer, and interpreter for a small integer three-address IR. The project explores the core correctness problems in compiler passes: merges, loop backedges, liveness, and preserving traps. It is deliberately bounded to this IR.

## Build and use

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
build/miniopt optimize fixtures/constant.ir
build/miniopt run fixtures/loop.ir
build/miniopt analyze fixtures/branch.ir
build/miniopt stats fixtures/mixed.ir
python3 benchmarks/benchmark.py build/miniopt
```

For ASan/UBSan: configure a separate directory with `-DENABLE_SANITIZERS=ON`. GCC/Clang on Linux are supported; CMake, Python 3, and a C++17 compiler are required. No external runtime dependencies.

## Language and semantics

One instruction per line; `#` starts a comment. Names match `[A-Za-z_][A-Za-z_0-9]*`. Supported statements:

```text
entry:
  x = -7
  y = x / 3
  c = y < 0
  if c goto negative
  goto done
negative:
  y = y * -1
done:
  return y
```

Assignments accept a literal, a variable, or two operands with `+ - * / == != < <= > >=`. Conditions branch on nonzero. Labels must be unique and branch targets must exist. Literals must fit signed 32-bit integers. Arithmetic uses 32-bit two's-complement modular wraparound; comparisons are signed. Division truncates toward zero, zero divisors trap, and `INT32_MIN / -1` wraps to `INT32_MIN`. Reading an undefined variable traps, including in a dead assignment. Operands are evaluated left to right. Falling off the end produces a distinct `fell_off` outcome.

`run` emits JSON with status, value (meaningful for return), and executed instruction count. Its optional fourth argument sets the instruction limit (default 100000). Labels do not consume steps. Execution-limit comparisons are **inconclusive**: optimization changes step counts, and a limit is not a proof of nontermination.

## Architecture and convergence

- `include/miniopt.hpp`: typed operands, instruction variants, CFG blocks, and public interfaces.
- `src/miniopt.cpp`: grammar-based line parser, interpreter, CFG, and optimization pipeline.
- `src/main.cpp`: CLI and in-process timing harness.

Basic-block leaders are entry, labels, and statements after terminators. Blocks track predecessors, successors, and entry reachability. Constant propagation uses a forward fixed-point framework: bottom means no incoming path yet; unknown values (including undefined values) are top; joins retain only identical constants on every incoming reachable path. Entry always includes an unknown initial environment, including when it has a backedge. Transfers substitute and fold only proven constants. Conditional branches then simplify and unreachable blocks are removed.

Backward liveness drives dead-assignment elimination. A separate forward must-defined analysis ensures removing an instruction cannot hide an undefined-variable read. Division is retained unless its divisor is proven nonzero and all operands are definitely defined. Passes repeat until serialized IR stops changing; each change removes instructions or replaces variable operands/operators with literals. No expansion or speculative transformations occur. Labels remain stable for branch targets; an empty program retains an inert label to preserve falling-off behavior.

## Testing and measurements

Unit tests exercise parser rejection, CFG edges, folding, wraparound, and trap preservation. CLI regression tests exercise control-flow joins, loops, unreachable traps, undefined reads, malformed input, negative division, and nontermination limits. Deterministic generated programs include conditional arithmetic updates within loop SCCs and path-dependent division traps. They are checked against independently computed Python arithmetic and branch outcomes; original and optimized IR are also interpreted and compared. All tests execute production code.

Recorded results and timing methodology: [benchmarks/results.md](benchmarks/results.md), with raw repeated samples in [results.json](benchmarks/results.json). Five committed synthetic workloads cover constant, dead-code, branch, loop, and mixed cases. Instruction counts exclude labels. Weighted aggregate reductions and arithmetic means are reported separately. Counts are not native runtime speedups.

Example captured from `fixtures/constant.ir`:

```text
entry:
  return 60
```

`build/miniopt run fixtures/loop.ir` returns `{"status":"return","value":4950,"steps":403}`.

## Scope and references

There are no functions, input parameters, I/O, memory, SSA, native code generation, or C/C++ frontend. The interpreter is intentionally simple; the bounded generator supplements but cannot prove general equivalence. Constant propagation is nonrelational and conservatively loses values at conflicting joins. CFG reachability is structural until constant branches simplify.

References: Aho et al., *Compilers: Principles, Techniques, and Tools* (data-flow analysis); [LLVM language reference](https://llvm.org/docs/LangRef.html) (IR design and semantic precision); [LLVM pass documentation](https://llvm.org/docs/Passes.html). MiniOpt's integer/trap semantics are its own and are not LLVM poison/undefined-behavior semantics.
