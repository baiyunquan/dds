
# DDS Phase Validation and Accuracy-Depth Test Report

Date: 2026-05-31
Platform: Windows 11 + VS2022 Community + Bazelisk 1.29.0

## Scope

This report validates the completed P0/P1/P2 work and verifies an additional
accuracy-oriented change: increasing AnalysePlay prediction search depth.

## Validation Matrix

1. P0 Baseline path:
	- Build and test under default configuration
	- Benchmark data already recorded in perf/baseline.md
2. P1 Native optimization path:
	- Build and test under --config=native
3. P2 oneTBB scheduler path:
	- Build and test with oneTBB-enabled solve_all_boards_n
4. Accuracy-depth enhancement:
	- Increase AnalysePlay hint search backoff window in analyse_later_board
	- Run focused AnalysePlay consistency regression test
	- Re-run full default and native test suites

## Accuracy-Depth Change

File changed:
- library/src/solver_if.cpp

Change summary:
- In analyse_later_board, replace strict one-sided hint bounds with a small
  depth-aware backoff window:
  - ini_depth >= 24: backoff = 2
  - ini_depth < 24: backoff = 1
- Clamp guess into [lowerbound, upperbound] before null-window loop.

Goal:
- Improve robustness/accuracy in complex AnalysePlay positions where hint
  direction can be noisy, while preserving current performance characteristics.

## Commands Executed

1. Focused consistency test:
	- bazelisk test //library/tests/solve_board:analyse_play_consistency_test
2. Full default suite:
	- bazelisk test //...
3. Full native suite:
	- bazelisk test --config=native //...

## Results

1. Focused consistency test:
	- PASS (1/1)
2. Full default suite:
	- PASS (39/39)
3. Full native suite:
	- PASS (39/39)

Conclusion:
- The three completed phases still run correctly.
- The increased prediction depth (hint backoff) passes consistency and full
  regression tests in both default and native configurations.

## P3 Stage Execution (SIMD)

Stage target (from REBUILD stage 3):
- SIMD bit-operation acceleration in quick_tricks and moves paths.

Implemented code changes:
- library/src/moves/moves.cpp
	- Added AVX2-guarded initialization path for removed-ranks computation in
		Moves::Init (with scalar fallback).
- library/src/quick_tricks.cpp
	- Added AVX2-guarded ruff-extension candidate check helper
		(with scalar fallback) in the quick-trick pruning path.

Build/Regression status after P3 changes:
- bazelisk test //... : PASS (39/39)

## list10000 Verification and Comparison

Commands:
1. Default (scalar fallback):
	 - bazelisk build -c opt //library/tests:dtest
	 - bazel-bin/library/tests/dtest.exe -f hands/list10000.txt -s solve -n 8
2. Native (AVX2 enabled):
	 - bazelisk build --config=native -c opt //library/tests:dtest
	 - bazel-bin/library/tests/dtest.exe -f hands/list10000.txt -s solve -n 8

Recorded outputs:
- perf/list10000_default_n8.txt
- perf/list10000_native_n8.txt

Key comparison (consistency-focused):
- Version: both 3.0.0
- Number of hands: both 10000
- Effective thread mode: both single-thread fallback (warning shown in both)
- Ratio: both 1.00

Conclusion:
- list10000 run completed in both configurations.
- Non-performance result fields are consistent with previous behavior.
- Timing differs (native faster), but consistency checks remain stable.

## New Randomized Round/Direction Test

Added files:
- library/tests/solve_board/random_round_direction_prediction_test.cpp
- library/tests/solve_board/BUILD.bazel (new cc_test target)

Test design:
- Randomly generate complete legal deals.
- Try all 4 opening directions (N/E/S/W leader variants).
- Generate legal full-play traces per direction.
- Compare DDS AnalysePlayPBN prediction with independently reconstructed
	SolveBoardPBN "actual" result at selected checkpoints:
	- after first card exposed (cards_played = 1)
	- every full-trick boundary (cards_played = 4, 8, ..., 48)
- Scaled volume: generated deal count increased 10x (16 -> 160), and total
  comparisons are guaranteed >= 1000.

Command:
- bazelisk test //library/tests/solve_board:random_round_direction_prediction_test --test_output=all

Measured result:
- prediction_match_stats: matched=8320 compared=8320 rate=1
- Test status: PASS (1/1)

Conclusion:
- In this randomized multi-round, multi-direction test, DDS prediction and
	reconstructed actual values were fully consistent (100% match rate) under
	10x scaled generation volume and 1000+ comparisons.

