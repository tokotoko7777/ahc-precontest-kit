CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
HEADERS := $(wildcard library/*.hpp)
PRACTICE_SOLVERS := $(wildcard practice/ahc*/main.cpp)
SEARCH_EXAMPLES := $(wildcard examples/search/*.cpp)
SEARCH_STARTERS := $(wildcard template/search/*.cpp)
SCORE_BENCHMARKS := $(wildcard benchmarks/*_score_benchmark.cpp)
UPGRADE_TESTS := $(wildcard tests/*_upgrades_test.cpp)
SANITIZER_TESTS := tests/parts_test.cpp tests/search_engines_test.cpp $(UPGRADE_TESTS)
SANITIZER_FLAGS := -std=c++17 -O1 -g -Wall -Wextra -pedantic \
	-fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: verify verify-practice verify-copy verify-debug verify-sanitize \
	benchmark-search benchmark-search-speed \
	benchmark-sa benchmark-tree-beam benchmark-monte-carlo \
	benchmark-real-search clean

verify: verify-practice verify-copy verify-debug
	mkdir -p build
	for header in $(HEADERS); do $(CXX) $(CXXFLAGS) -x c++ -fsyntax-only $$header || exit 1; done
	$(CXX) $(CXXFLAGS) -I. tests/parts_test.cpp -o build/parts_test
	./build/parts_test
	$(CXX) $(CXXFLAGS) -I. tests/search_engines_test.cpp -o build/search_engines_test
	./build/search_engines_test
	for test in $(UPGRADE_TESTS); do \
		name=$$(basename $$test .cpp); \
		$(CXX) $(CXXFLAGS) -I. $$test -o build/$$name || exit 1; \
		./build/$$name || exit 1; \
	done
	for solver in $(SEARCH_EXAMPLES); do \
		echo "checking $$solver"; \
		$(CXX) $(CXXFLAGS) -I. -fsyntax-only $$solver || exit 1; \
	done
	for starter in $(SEARCH_STARTERS); do \
		echo "checking $$starter"; \
		$(CXX) $(CXXFLAGS) -I. -fsyntax-only $$starter || exit 1; \
	done
	for benchmark in $(SCORE_BENCHMARKS); do \
		echo "checking $$benchmark"; \
		$(CXX) $(CXXFLAGS) -I. -fsyntax-only $$benchmark || exit 1; \
	done
	$(CXX) $(CXXFLAGS) -I. -DVARIABLE_COST_BEAM_SELF_TEST \
		examples/search/variable_cost_beam.cpp -o build/variable_cost_beam_test
	./build/variable_cost_beam_test
	$(CXX) $(CXXFLAGS) template/main.cpp -o build/template

verify-copy:
	mkdir -p build
	python3 tests/copy_part_test.py
	python3 tests/source_url_test.py
	python3 tests/standalone_search_test.py
	python3 tests/ahc001_gap_test.py
	python3 tests/beam_score_benchmark_test.py
	python3 tests/ahc071_key_benchmark_test.py
	python3 tests/tree_score_benchmark_test.py
	python3 tests/repository_bundle_test.py
	python3 tools/copy_part.py --ref HEAD \
		--main tests/fixtures/copied_parts_main.cpp \
		-o build/copied_parts_smoke.cpp \
		library/timer.hpp library/random.hpp library/simulated-annealing.hpp
	$(CXX) $(CXXFLAGS) -Werror build/copied_parts_smoke.cpp \
		-o build/copied_parts_smoke
	./build/copied_parts_smoke

verify-debug:
	mkdir -p build
	$(CXX) $(CXXFLAGS) -Werror -Wshadow -I. \
		examples/debug/route_swap_debug.cpp -o build/route_swap_debug
	./build/route_swap_debug
	$(CXX) $(CXXFLAGS) -Werror -Wshadow -I. \
		examples/debug/grid_update_debug.cpp -o build/grid_update_debug
	./build/grid_update_debug
	$(CXX) $(CXXFLAGS) -Werror -Wshadow -DNDEBUG -I. \
		examples/debug/route_swap_debug.cpp -o build/route_swap_release
	./build/route_swap_release
	$(CXX) $(CXXFLAGS) -Werror -Wshadow -DNDEBUG -I. \
		examples/debug/grid_update_debug.cpp -o build/grid_update_release
	./build/grid_update_release
	$(CXX) $(CXXFLAGS) -Werror -Wshadow -I. \
		-DAHC_DEBUG_INJECT_SCORE_BUG examples/debug/route_swap_debug.cpp \
		-o build/route_score_bug
	python3 tests/debug_fixture_test.py build/route_score_bug score
	$(CXX) $(CXXFLAGS) -Werror -Wshadow -I. \
		-DAHC_DEBUG_INJECT_HASH_BUG examples/debug/route_swap_debug.cpp \
		-o build/route_hash_bug
	python3 tests/debug_fixture_test.py build/route_hash_bug hash
	$(CXX) $(CXXFLAGS) -Werror -Wshadow -I. \
		-DAHC_DEBUG_INJECT_REVERT_BUG examples/debug/route_swap_debug.cpp \
		-o build/route_revert_bug
	python3 tests/debug_fixture_test.py build/route_revert_bug position_cache

verify-sanitize:
	mkdir -p build/sanitize
	for test in $(SANITIZER_TESTS); do \
		name=$$(basename $$test .cpp); \
		$(CXX) $(SANITIZER_FLAGS) -I. $$test \
			-o build/sanitize/$$name || exit 1; \
		ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
			./build/sanitize/$$name || exit 1; \
	done
	for example in examples/debug/route_swap_debug.cpp \
	               examples/debug/grid_update_debug.cpp; do \
		name=$$(basename $$example .cpp); \
		$(CXX) $(SANITIZER_FLAGS) -I. $$example \
			-o build/sanitize/$$name || exit 1; \
		ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
			./build/sanitize/$$name || exit 1; \
	done

verify-practice:
	for solver in $(PRACTICE_SOLVERS); do \
		echo "checking $$solver"; \
		$(CXX) $(CXXFLAGS) -fsyntax-only $$solver || exit 1; \
	done

benchmark-search:
	mkdir -p build
	$(CXX) $(CXXFLAGS) -O3 -DNDEBUG -I. \
		benchmarks/ahc032_score_benchmark.cpp -o build/ahc032_score_benchmark
	./build/ahc032_score_benchmark

benchmark-search-speed:
	mkdir -p build
	$(CXX) $(CXXFLAGS) -O3 -DNDEBUG -I. \
		benchmarks/search_core_benchmark.cpp -o build/search_core_benchmark
	./build/search_core_benchmark

benchmark-sa:
	mkdir -p build
	$(CXX) $(CXXFLAGS) -O3 -DNDEBUG -I. \
		benchmarks/ahc001_annealing_score_benchmark.cpp \
		-o build/ahc001_annealing_score_benchmark
	./build/ahc001_annealing_score_benchmark

benchmark-tree-beam:
	mkdir -p build
	$(CXX) $(CXXFLAGS) -O3 -DNDEBUG -I. \
		benchmarks/ahc021_tree_beam_score_benchmark.cpp \
		-o build/ahc021_tree_beam_score_benchmark
	./build/ahc021_tree_beam_score_benchmark

benchmark-monte-carlo:
	mkdir -p build
	$(CXX) $(CXXFLAGS) -O3 -DNDEBUG -I. \
		benchmarks/ahc015_monte_carlo_score_benchmark.cpp \
		-o build/ahc015_monte_carlo_score_benchmark
	./build/ahc015_monte_carlo_score_benchmark

benchmark-real-search: benchmark-sa benchmark-tree-beam \
	benchmark-monte-carlo benchmark-search

clean:
	rm -rf build
