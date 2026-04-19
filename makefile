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
debug: $(BIN_DIR)/eds-debug

$(BIN_DIR)/eds-debug: $(OBJS) | $(BIN_DIR)
	$(CXX) $(OBJS) -o $@ $(LINK_FLAGS)

# --- Release build (optimized) ---
release: CXXFLAGS := $(CXXSTD) $(WARN) $(RELEASE_FLAGS) $(INCLUDES)
release: LINK_FLAGS := $(LDFLAGS)
release: $(BIN_DIR)/eds

$(BIN_DIR)/eds: $(OBJS) | $(BIN_DIR)
	$(CXX) $(OBJS) -o $@ $(LINK_FLAGS)

# --- Compile each .cpp to .o with auto-dependency tracking ---
# -MMD generates .d files listing header dependencies
# -MP adds phony targets so deleted headers don't break the build
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# --- Create output directories ---
$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

# --- Clean all build artifacts ---
.PHONY: all debug release clean

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Pull in auto-generated dependency files (header tracking)
-include $(DEPS)
