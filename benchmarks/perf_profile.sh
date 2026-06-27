#!/bin/bash
# perf_profile.sh — Profile HFT benchmarks with Linux perf
#
# Usage: ./benchmarks/perf_profile.sh [benchmark_binary]
#
# Requires: perf (linux-tools)
# Install:  sudo pacman -S perf   (Arch)
#           sudo apt install linux-tools-common linux-tools-generic  (Debian/Ubuntu)
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BENCH="${1:-$SCRIPT_DIR/benchmark}"

if ! command -v perf &>/dev/null; then
    echo "ERROR: 'perf' not found. Install it:"
    echo "  Arch:    sudo pacman -S perf"
    echo "  Ubuntu:  sudo apt install linux-tools-common linux-tools-generic"
    exit 1
fi

if [ ! -f "$BENCH" ]; then
    echo "ERROR: benchmark binary not found at $BENCH"
    echo "  Compile first: cd $PROJECT_DIR && g++ -std=c++20 -O3 -march=native -I include -pthread -o benchmarks/benchmark benchmarks/benchmark.cpp"
    exit 1
fi

cd "$PROJECT_DIR"

echo "=== perf record (call-graph dwarf) ==="
echo "Binary: $BENCH"
echo ""

# Check if running as root (required for some perf features)
if [ "$EUID" -ne 0 ]; then
    echo "NOTE: Running without root. Some events (e.g., cache-misses) may not be available."
    echo "      Use: sudo ./benchmarks/perf_profile.sh"
    echo ""
    PERF_EVENTS="cycles,instructions,branch-misses"
else
    PERF_EVENTS="cycles,instructions,branch-misses,cache-misses,cache-references"
fi

perf record -F 1000 --call-graph dwarf -e "$PERF_EVENTS" -o perf.data "$BENCH"

echo ""
echo "=== perf report (top functions) ==="
perf report -i perf.data --stdio -n --sort symbol | head -40

echo ""
echo "=== perf stat summary ==="
perf stat -r 3 "$BENCH" 2>&1 | tail -20

echo ""
echo "Profile data saved to: perf.data"
echo "View interactively:    perf report -i perf.data"
