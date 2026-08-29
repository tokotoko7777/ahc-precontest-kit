CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
HEADERS := $(wildcard library/*.hpp)

.PHONY: verify clean

verify:
	mkdir -p build
	for header in $(HEADERS); do $(CXX) $(CXXFLAGS) -x c++ -fsyntax-only $$header || exit 1; done
	$(CXX) $(CXXFLAGS) -I. tests/parts_test.cpp -o build/parts_test
	./build/parts_test
	$(CXX) $(CXXFLAGS) template/main.cpp -o build/template

clean:
	rm -rf build
