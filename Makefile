CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
HEADERS := $(wildcard library/*.hpp)
PRACTICE_SOLVERS := $(wildcard practice/ahc*/main.cpp)

.PHONY: verify verify-practice clean

verify: verify-practice
	mkdir -p build
	for header in $(HEADERS); do $(CXX) $(CXXFLAGS) -x c++ -fsyntax-only $$header || exit 1; done
	$(CXX) $(CXXFLAGS) -I. tests/parts_test.cpp -o build/parts_test
	./build/parts_test
	$(CXX) $(CXXFLAGS) template/main.cpp -o build/template

verify-practice:
	for solver in $(PRACTICE_SOLVERS); do \
		echo "checking $$solver"; \
		$(CXX) $(CXXFLAGS) -fsyntax-only $$solver || exit 1; \
	done

clean:
	rm -rf build
