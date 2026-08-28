CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic

.PHONY: verify clean

verify:
	mkdir -p build
	$(CXX) $(CXXFLAGS) -I. tests/library_test.cpp -o build/library_test
	./build/library_test
	python3 -m unittest tests/test_bundle.py

clean:
	rm -rf build submission.cpp

