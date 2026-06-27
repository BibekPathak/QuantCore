#!/bin/bash
# run_all_benchmarks.sh — Compile and run all HFT benchmarks
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

echo "=== HFT Trading System — Full Benchmark Suite ==="
echo ""

# Compile benchmarks
echo "--- Compiling ---"
g++ -std=c++20 -O3 -march=native -I include -pthread -o benchmarks/benchmark benchmarks/benchmark.cpp
echo "Compilation OK"
echo ""

# Run benchmarks
echo "--- Running Benchmarks ---"
./benchmarks/benchmark
echo ""

# Compile and run SIMD benchmark if it exists
if [ -f benchmarks/simd_benchmark.cpp ]; then
    echo "--- Compiling SIMD Benchmark ---"
    g++ -std=c++20 -O3 -march=native -I include -o benchmarks/simd_benchmark benchmarks/simd_benchmark.cpp
    echo "--- Running SIMD Benchmark ---"
    ./benchmarks/simd_benchmark
fi

echo ""
echo "=== All benchmarks complete ==="
