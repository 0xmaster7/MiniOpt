# Measured MiniOpt results

Source commit: `aebc0654ced0eeda4d37927ef93caed7cd008f1e`; date: 2026-10-20T09:03:36.855477+00:00
Environment: Linux-6.17.0-1022-azure-x86_64-with-glibc2.39; AMD EPYC 7763 64-Core Processor; g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0

In-process steady_clock: one warmup, 21 samples of 10 optimizations; microseconds per optimization. Synthetic fixtures. Labels excluded from instruction counts.

| Fixture | Instructions before / after | Reduction | Blocks before / after | Unreachable blocks removed | Optimization µs median (min–max) |
|---|---:|---:|---:|---:|---:|
| fixtures/branch.ir | 11 / 11 | 0.00% | 6 / 6 | 0 | 56.84 (50.64–77.00) |
| fixtures/constant.ir | 6 / 1 | 83.33% | 1 / 1 | 0 | 14.51 (13.32–19.79) |
| fixtures/dead.ir | 8 / 2 | 75.00% | 3 / 2 | 1 | 24.80 (22.68–34.19) |
| fixtures/loop.ir | 7 / 7 | 0.00% | 3 / 3 | 0 | 28.39 (26.36–34.42) |
| fixtures/mixed.ir | 14 / 11 | 21.43% | 5 / 5 | 0 | 140.26 (107.78–1383.66) |

Weighted aggregate reduction: **30.43%** (46 → 32). Arithmetic mean of per-fixture reductions: 35.95%.
Differential comparisons passed: 514; correctness failures: 0; execution-limit cases inconclusive: 1; malformed inputs rejected: 9.

## Limitations

- No claim of native program runtime speedup.
- Execution-limit outcomes are inconclusive; optimization can change executed instruction counts.
- Source commit identifies code under measurement; results are added in a subsequent commit.
- unreachable_blocks_removed counts blocks eliminated over the fixed-point pipeline, including after branch folding; initial_unreachable_blocks is the initial CFG count.
