CXX       := g++
CXXSTD    := -std=c++17
WARN      := -Wall -Wextra
INCLUDES  := -Iinclude
LDFLAGS   := -pthread

DEBUG_FLAGS   := -O0 -g -fsanitize=address,undefined
RELEASE_FLAGS := -O2 -DNDEBUG

SRC_DIR   := src
OBJ_DIR   := build
BIN_DIR   := bin

SRCS  := $(wildcard $(SRC_DIR)/*.cpp)
OBJS  := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS  := $(OBJS:.o=.d)

# Default target
all: release

# --- Debug build (with AddressSanitizer + UBSan) ---
debug: CXXFLAGS := $(CXXSTD) $(WARN) $(DEBUG_FLAGS) $(INCLUDES)
debug: LINK_FLAGS := $(LDFLAGS) -fsanitize=address,undefined
debug: $(BIN_DIR)/shellx-debug

$(BIN_DIR)/shellx-debug: $(OBJS) | $(BIN_DIR)
	$(CXX) $(OBJS) -o $@ $(LINK_FLAGS)

# --- Release build (optimized) ---
release: CXXFLAGS := $(CXXSTD) $(WARN) $(RELEASE_FLAGS) $(INCLUDES)
release: LINK_FLAGS := $(LDFLAGS)
release: $(BIN_DIR)/shellx

$(BIN_DIR)/shellx: $(OBJS) | $(BIN_DIR)
	$(CXX) $(OBJS) -o $@ $(LINK_FLAGS)

# --- Compile each .cpp to .o with auto-dependency tracking ---
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# --- Create output directories ---
$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

# --- Smoke tests: built-ins, pipes, external commands, redirects ---
test: debug
	@echo "=== Test 1: ls built-in ==="
	@printf 'ls\nexit\n' | $(BIN_DIR)/shellx-debug
	@echo ""
	@echo "=== Test 2: cd + ls ==="
	@printf 'cd ~\nls\nexit\n' | $(BIN_DIR)/shellx-debug
	@echo ""
	@echo "=== Test 3: external command (echo) ==="
	@printf 'echo hello world\nexit\n' | $(BIN_DIR)/shellx-debug
	@echo ""
	@echo "=== Test 4: pipe (ls | wc -l) ==="
	@printf 'ls | wc -l\nexit\n' | $(BIN_DIR)/shellx-debug
	@echo ""
	@echo "=== Test 5: multi-pipe (ls | sort | head -n 3) ==="
	@printf 'ls | sort | head -n 3\nexit\n' | $(BIN_DIR)/shellx-debug
	@echo ""
	@echo "=== Test 6: redirect (echo test > /tmp/shellx_test.txt) ==="
	@printf 'echo redirect_works > /tmp/shellx_test.txt\nexit\n' | $(BIN_DIR)/shellx-debug
	@cat /tmp/shellx_test.txt
	@rm -f /tmp/shellx_test.txt
	@echo ""
	@echo "=== All smoke tests passed ==="

# --- Unit tests (tokenizer) ---
test-unit: debug
	$(CXX) $(CXXSTD) $(WARN) $(INCLUDES) -o $(BIN_DIR)/test_tokenizer tests/test_tokenizer.cpp src/Tokenizer.cpp
	$(BIN_DIR)/test_tokenizer

# --- Clean all build artifacts ---
.PHONY: all debug release test test-unit clean

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Pull in auto-generated dependency files (header tracking)
-include $(DEPS)
