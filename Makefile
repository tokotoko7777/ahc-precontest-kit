CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
HEADERS := $(wildcard library/*.hpp)
PRACTICE_SOLVERS := $(wildcard practice/ahc*/main.cpp)
SEARCH_EXAMPLES := $(wildcard examples/search/*.cpp)
UPGRADE_TESTS := $(wildcard tests/*_upgrades_test.cpp)

.PHONY: verify verify-practice benchmark-search clean

verify: verify-practice
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
	$(CXX) $(CXXFLAGS) -I. -DVARIABLE_COST_BEAM_SELF_TEST \
		examples/search/variable_cost_beam.cpp -o build/variable_cost_beam_test
	./build/variable_cost_beam_test
	$(CXX) $(CXXFLAGS) template/main.cpp -o build/template

verify-practice:
	for solver in $(PRACTICE_SOLVERS); do \
		echo "checking $$solver"; \
		$(CXX) $(CXXFLAGS) -fsyntax-only $$solver || exit 1; \
	done

benchmark-search:
	mkdir -p build
	$(CXX) $(CXXFLAGS) -O3 -DNDEBUG -I. \
		benchmarks/search_core_benchmark.cpp -o build/search_core_benchmark
	./build/search_core_benchmark

clean:
	rm -rf build
