CXX       := g++
CXXFLAGS  := -std=c++20 -O3 -march=native -I include
LDFLAGS   := -pthread
BENCH_DIR := benchmarks

.PHONY: all clean run run_all perf flamegraph

all: $(BENCH_DIR)/benchmark

$(BENCH_DIR)/benchmark: $(BENCH_DIR)/benchmark.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $<

$(BENCH_DIR)/simd_benchmark: $(BENCH_DIR)/simd_benchmark.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

run: $(BENCH_DIR)/benchmark
	./$(BENCH_DIR)/benchmark

run_all: $(BENCH_DIR)/benchmark
	./$(BENCH_DIR)/benchmark

# perf profiling — requires linux-perf (sudo pacman -S perf)
perf: $(BENCH_DIR)/benchmark
	@echo "=== perf record (call-graph dwarf) ==="
	perf record -F 1000 --call-graph dwarf -o perf.data ./$(BENCH_DIR)/benchmark
	@echo ""
	@echo "=== perf report ==="
	perf report -i perf.data --stdio | head -60

# flamegraph generation — requires FlameGraph from https://github.com/brendangregg/FlameGraph
flamegraph: perf.data
	@echo "=== Generating flamegraph ==="
	perf script -i perf.data > out.perf
	./FlameGraph/stackcollapse-perf.pl out.perf > out.folded
	./FlameGraph/flamegraph.pl out.folded > flamegraph.svg
	@echo "Flamegraph: flamegraph.svg"

clean:
	rm -f $(BENCH_DIR)/benchmark $(BENCH_DIR)/simd_benchmark
	rm -f perf.data out.perf out.folded flamegraph.svg
