#!/bin/bash
# flamegraph_gen.sh — Generate flamegraph SVG from perf.data
#
# Usage: ./benchmarks/flamegraph_gen.sh [perf.data file]
#
# Requires:
#   1. perf (linux-tools) — for perf script
#   2. FlameGraph — https://github.com/brendangregg/FlameGraph
#
# Setup FlameGraph:
#   git clone https://github.com/brendangregg/FlameGraph.git
#   export FLAMEGRAPH_DIR="$PWD/FlameGraph"
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
PERF_DATA="${1:-$PROJECT_DIR/perf.data}"

# Find FlameGraph scripts
FLAMEGRAPH_DIR="${FLAMEGRAPH_DIR:-}"
if [ -z "$FLAMEGRAPH_DIR" ]; then
    for dir in "$PROJECT_DIR/FlameGraph" "$HOME/FlameGraph" "/opt/FlameGraph"; do
        if [ -f "$dir/stackcollapse-perf.pl" ]; then
            FLAMEGRAPH_DIR="$dir"
            break
        fi
    done
fi

if [ ! -f "$PERF_DATA" ]; then
    echo "ERROR: perf.data not found at $PERF_DATA"
    echo "  Run ./benchmarks/perf_profile.sh first to generate it."
    exit 1
fi

if [ -z "$FLAMEGRAPH_DIR" ] || [ ! -f "$FLAMEGRAPH_DIR/stackcollapse-perf.pl" ]; then
    echo "ERROR: FlameGraph scripts not found."
    echo "  Clone: git clone https://github.com/brendangregg/FlameGraph.git"
    echo "  Then:  export FLAMEGRAPH_DIR=\"\$PWD/FlameGraph\""
    exit 1
fi

cd "$PROJECT_DIR"

echo "=== Generating FlameGraph ==="
echo "Perf data:  $PERF_DATA"
echo "FlameGraph: $FLAMEGRAPH_DIR"
echo ""

perf script -i "$PERF_DATA" > out.perf
"$FLAMEGRAPH_DIR/stackcollapse-perf.pl" out.perf > out.folded
"$FLAMEGRAPH_DIR/flamegraph.pl" out.folded > flamegraph.svg

echo "Flamegraph generated: flamegraph.svg"
echo "Open in browser:      firefox flamegraph.svg"
