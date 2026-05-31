# P0 Baseline Results

## Commands
- Build: `bazelisk build -c opt //library/tests:dtest`
- Test: `bazelisk test //...`
- Benchmark binary: `bazel-bin/library/tests/dtest.exe`

## Timing Table

| hands | threads | sys_time_ms | wall_time_ms | notes |
|-------|---------|-------------|--------------|-------|
| 100   | 1       | 657         | 675.22       | baseline |
| 100   | 2       | 659         | 674.42       | baseline |
| 100   | 4       | 644         | 659.96       | baseline |
| 100   | 8       | 668         | 685.12       | baseline |
| 1000  | 1       | 7760        | 7785.02      | baseline |
| 1000  | 4       | 8014        | 8041.19      | baseline |
| 1000  | 8       | 20914       | 20949.74     | baseline, severe slowdown |
| 1000  | 16      | 21862       | 21903.90     | baseline, severe slowdown |

## P1 Native Results (`--config=native`)

| hands | threads | sys_time_ms | wall_time_ms | notes |
|-------|---------|-------------|--------------|-------|
| 100   | 1       | 658         | 679.92       | native |
| 100   | 4       | 641         | 657.00       | native |
| 100   | 8       | 666         | 681.05       | native |
| 1000  | 4       | 7329        | 7357.21      | native |
| 1000  | 8       | 7366        | 7407.13      | native |

### Native Test Status
- `bazelisk test --config=native //...`: 39/39 passed.

## P2 oneTBB Results

| hands | threads | sys_time_ms | wall_time_ms | notes |
|-------|---------|-------------|--------------|-------|
| 100   | 4       | 618         | 635.22       | tbb |
| 100   | 8       | 669         | 685.35       | tbb |
| 1000  | 4       | 8099        | 8125.23      | tbb |
| 1000  | 8       | 7783        | 7808.98      | tbb |
| 1000  | 8       | 7994        | 8021.83      | tbb + native |

### oneTBB Test Status
- `bazelisk test //...`: 39/39 passed.
- `bazelisk test --config=native //...`: 39/39 passed.

### oneTBB Observation
- On this machine and workload, oneTBB produced stable behavior and correctness, but did not outperform the best P1 native result for `1000 x 8`.

## P3 SIMD + list10000 Results

| workload | config  | requested_threads | effective_threads | user_time_ms | sys_time_ms | wall_time_ms | notes |
|----------|---------|-------------------|-------------------|--------------|-------------|--------------|-------|
| list10000 | default | 8 | 1 | 96802 | 96827 | 96972.73 | output in perf/list10000_default_n8.txt |
| list10000 | native  | 8 | 1 | 79423 | 79447 | 79550.21 | output in perf/list10000_native_n8.txt |

### list10000 Consistency Check
- Version: default/native are both `3.0.0`.
- Number of hands: default/native are both `10000`.
- Effective threading mode and warning: identical (internal batch threading disabled, uses 1 thread).
- Ratio: default/native are both `1.00`.

## Observations
- Full test suite status: 39/39 passed.
- Parallel scaling is poor beyond 4 threads on this machine.
- Current scheduling path in `solve_all_boards_n()` likely incurs heavy per-board overhead and contention at higher thread counts.
