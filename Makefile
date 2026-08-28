CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic

.PHONY: verify clean

verify:
	mkdir -p build
	$(CXX) $(CXXFLAGS) -x c++ -fsyntax-only library/timer.hpp
	$(CXX) $(CXXFLAGS) -x c++ -fsyntax-only library/random.hpp
	$(CXX) $(CXXFLAGS) -x c++ -fsyntax-only library/simulated-annealing.hpp
	$(CXX) $(CXXFLAGS) -I. tests/parts_test.cpp -o build/parts_test
	./build/parts_test
	$(CXX) $(CXXFLAGS) template/main.cpp -o build/template

clean:
	rm -rf build
