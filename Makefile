CXX ?= g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic -Icore/include
CORE_SRC := core/src/core.cpp core/src/device_identity.cpp

.PHONY: all test clean
all: build/lumaforge-engine build/core-tests

build:
	mkdir -p build

build/lumaforge-engine: build $(CORE_SRC) core/src/engine_main.cpp core/include/lumaforge/core.hpp core/include/lumaforge/device_identity.hpp
	$(CXX) $(CXXFLAGS) $(CORE_SRC) core/src/engine_main.cpp -o $@

build/core-tests: build $(CORE_SRC) core/tests/core_tests.cpp core/include/lumaforge/core.hpp core/include/lumaforge/device_identity.hpp
	$(CXX) $(CXXFLAGS) $(CORE_SRC) core/tests/core_tests.cpp -o $@

test: build/core-tests
	./build/core-tests

clean:
	rm -f build/lumaforge-engine build/core-tests
