CXX = g++
DEBUG_FLAGS = -g -Wall -std=c++17 -pthread
OPTIMIZED_FLAGS = -O2 -std=c++17 -pthread

SRC = mthreading.cpp
DEBUG_OUT = debug
OPTIMIZED_OUT = optimized

all: debug optimized

debug: $(SRC)
	$(CXX) $(DEBUG_FLAGS) -o $(DEBUG_OUT) $(SRC)

optimized: $(SRC)
	$(CXX) $(OPTIMIZED_FLAGS) -o $(OPTIMIZED_OUT) $(SRC)

.PHONY: clean

clean:
	rm -f $(DEBUG_OUT) $(OPTIMIZED_OUT)