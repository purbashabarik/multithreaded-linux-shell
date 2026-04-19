# Shell Project — Upgrade Roadmap

A step-by-step engineering plan to evolve `mthreading.cpp` from a classroom exercise into a portfolio-grade, interview-ready C++ shell.

---

## Table of Contents

1. [Overview & Ordering Rationale](#overview--ordering-rationale)
2. [Phase 0 — Baseline Hygiene](#phase-0--baseline-hygiene)
3. [Phase 1 — Correctness Fixes](#phase-1--correctness-fixes)
   - [1.1 Memory safety with `std::unique_ptr`](#11-memory-safety-with-stdunique_ptr)
   - [1.2 Thread correctness in `RmCommand`](#12-thread-correctness-in-rmcommand)
   - [1.3 Exception safety in `recursiveMove`](#13-exception-safety-in-recursivemove)
4. [Phase 2 — Parsing](#phase-2--parsing)
   - [2.1 Robust tokenizer with quotes & escapes](#21-robust-tokenizer-with-quotes--escapes)
5. [Phase 3 — Project Structure](#phase-3--project-structure)
   - [3.1 Split into headers and sources](#31-split-into-headers-and-sources)
   - [3.2 Upgraded Makefile](#32-upgraded-makefile)
   - [3.3 CMake build system](#33-cmake-build-system)
6. [Phase 4 — Real Shell Features](#phase-4--real-shell-features)
   - [4.1 Pipes between commands](#41-pipes-between-commands)
7. [Phase 5 — User Experience](#phase-5--user-experience)
   - [5.1 Tab completion (Level 1)](#51-tab-completion-level-1)
   - [5.2 Command-aware completion (Level 2)](#52-command-aware-completion-level-2)
8. [Phase 6 — GUI Frontend](#phase-6--gui-frontend)
   - [6.1 Qt desktop application](#61-qt-desktop-application)
9. [Master Timeline](#master-timeline)
10. [Definition of Done](#definition-of-done)
11. [Risks & Pitfalls](#risks--pitfalls)

---

## Overview & Ordering Rationale

The upgrades are ordered by **dependency** and **risk**, not by the order you listed them. A bug you fix first cannot break a feature you haven't written yet. Structure comes before new features, because restructuring later is expensive. UX and GUI come last, because they sit on top of a working core.

```
Phase 0 → 1 → 2 → 3 → 4 → 5 → 6
hygiene  fix   parse struct feats UX   GUI
```

Estimated total effort: **~60–90 hours of focused work** spread over 4–6 weeks part-time.

---

## Phase 0 — Baseline Hygiene

Before any refactor, make sure you have a safety net.

### Steps

1. **Initialize Git** and commit the current working code as the baseline.
   ```bash
   cd "/Users/devkuls/Downloads/OOPD Project/purbasha oopd"
   git init
   git add .
   git commit -m "baseline: working threaded shell"
   ```
2. Create a **feature branch** for each upgrade (`feat/unique-ptr`, `feat/tokenizer`, etc.) and merge to `main` when green.
3. Add a `.gitignore`:
   ```
   debug
   optimized
   build/
   *.o
   *.d
   .vscode/
   .DS_Store
   ```
4. Verify the current build still works: `make clean && make && ./debug`.

**Acceptance:** Project is in Git, current build green, each phase below becomes a PR.

---

## Phase 1 — Correctness Fixes

These are small, local, zero-risk improvements. Do them **first** because they show engineering discipline and unblock later refactors.

**Estimated effort:** 2–3 hours total.

### 1.1 Memory safety with `std::unique_ptr`

**Goal:** Replace raw `new`/`delete` in `main()` with `std::unique_ptr<Command>`.

**Why:** Prevents leaks if an exception is thrown inside `execute()`. Demonstrates modern C++.

**Prerequisites:** None.

**Step-by-step:**

1. At the top of `mthreading.cpp`, add:
   ```cpp
   #include <memory>
   ```
2. In `main()`, change the dispatch block from raw pointers to `std::unique_ptr`:
   ```cpp
   std::unique_ptr<Command> command;
   if (tokens[0] == "cd")         command = std::make_unique<CdCommand>();
   else if (tokens[0] == "ls")    command = std::make_unique<LsCommand>();
   else if (tokens[0] == "mv")    command = std::make_unique<MvCommand>();
   else if (tokens[0] == "cp")    command = std::make_unique<CpCommand>();
   else if (tokens[0] == "rm")    command = std::make_unique<RmCommand>();
   else if (tokens[0] == "exit")  break;
   else {
       std::cout << "Invalid command\n";
       continue;
   }

   command->execute(tokens);
   // no delete — unique_ptr destroys on scope exit
   ```
3. Delete the `delete command;` line.
4. Run a leak check with AddressSanitizer:
   ```bash
   g++ -std=c++17 -pthread -g -fsanitize=address,undefined -o debug mthreading.cpp
   ./debug
   ```

**Acceptance:**
- Compiles clean with `-Wall -Wextra`.
- `grep -n "new " mthreading.cpp | grep -v "new line"` returns nothing.
- ASan reports no leaks on exit.

**Pitfalls:**
- Don't `return` or `break` between `make_unique` and `execute()` without letting the unique_ptr destruct.
- Don't store the unique_ptr in a container that requires copyability.

---


### 1.2 Thread correctness in `RmCommand`

**Goal:** Fix the silently-serial `std::async` calls in `RmCommand::execute()` (line 492).

**Why:** A future returned by `std::async(std::launch::async, …)` blocks in its destructor. Discarding it makes the loop appear parallel but actually runs **sequentially**. Interviewers love spotting this.

**Step-by-step:**

1. Add a vector to collect futures:
   ```cpp
   std::vector<std::future<void>> futures;
   ```
2. Replace the discarded `std::async` call:
   ```cpp
   futures.push_back(
       std::async(std::launch::async,
                  &RmCommand::removeFileOrDirectory,
                  this, arg, recursive, force, verbose)
   );
   ```
3. After the `for` loop ends, drain the futures:
   ```cpp
   for (auto& f : futures) {
       try { f.get(); }
       catch (const std::exception& e) {
           std::cerr << "rm task failed: " << e.what() << "\n";
       }
   }
   ```
4. Mark the worker `noexcept(false)` if it throws, and let `f.get()` propagate.

**Acceptance:**
- Running `rm -r big_dir small_dir another_dir` truly overlaps. Measure with `time` — wall time should be roughly `max(t_i)`, not `sum(t_i)`.
- `-Wunused-result` warning is gone.

**Pitfalls:**
- If the member function throws, only the **first** `f.get()` will see it unless you wrap each in try/catch.
- Don't reuse the same `std::future` after `get()`.

---

### 1.3 Exception safety in `recursiveMove`

**Goal:** Guarantee the caller's `future.wait()` never deadlocks on error.

**Why:** Right now, if `fs::copy` throws, the catch logs the error but **never** calls `promise.set_value()`. The caller's `future.wait()` then blocks forever.

**Step-by-step:**

1. In `MvCommand::recursiveMove`, change the `catch` block:
   ```cpp
   catch (...) {
       try {
           promise.set_exception(std::current_exception());
       } catch (...) { /* set_exception itself may throw */ }
   }
   ```
2. In `performMove`, change `future.wait()` to `future.get()` so exceptions propagate to the user.
3. Wrap `future.get()` in its own try/catch and log the failure.
4. Apply the same fix to `CpCommand::recursiveCopy`.

**Acceptance:**
- Simulate a failure (e.g., move into a read-only destination) → program prints the error and returns to prompt, does not hang.
- Unit test: feed an invalid path, assert the shell returns to prompt in under 1 s.

**Pitfalls:**
- A promise can only be satisfied **once**. Don't call both `set_value` and `set_exception`.
- If the thread terminates via `std::terminate` (e.g. from a noexcept violation), the promise is never satisfied — keep the exception handlers broad (`catch (...)`).

---

## Phase 2 — Parsing

### 2.1 Robust tokenizer with quotes & escapes

**Goal:** Replace the naive `input.find(' ')` loop with a real tokenizer that handles:
- Single quotes `'hello world'` (literal)
- Double quotes `"hello $USER"` (future: variable expansion)
- Backslash escapes `\ `, `\"`, `\\`
- Consecutive spaces / tabs as one separator
- Trailing/leading whitespace

**Why:** Any shell that can't handle `cd "My Documents"` isn't a shell.

**Prerequisites:** Phase 1 done.

**Step-by-step:**

1. Extract tokenizer into its own function:
   ```cpp
   std::vector<std::string> tokenize(const std::string& input);
   ```
2. Implement a state machine:
   - States: `NORMAL`, `IN_SINGLE_QUOTE`, `IN_DOUBLE_QUOTE`, `ESCAPED`
   - Accumulate characters into a `current` buffer. On whitespace in `NORMAL`, push `current` if non-empty and reset.
   - In `IN_SINGLE_QUOTE`: everything is literal until the next `'`.
   - In `IN_DOUBLE_QUOTE`: backslash escapes `"` and `\`; everything else is literal.
   - In `ESCAPED`: add the next char literally and return to previous state.
3. On unterminated quote, either throw `std::invalid_argument("unterminated quote")` or print an error and return empty tokens.
4. Update `main()` to call `tokenize(input)` and bail out on empty result.
5. Add a small test harness (can be conditional `#ifdef TEST_TOKENIZER`) with inputs like:
   - `ls -la "My Documents"` → `[ls, -la, My Documents]`
   - `echo "hi   there"` → `[echo, hi   there]`
   - `cp a\ b.txt c\ d.txt` → `[cp, a b.txt, c d.txt]`
   - `echo 'it\'s ok'` → tokenizer error (or support with escape rules)

**Acceptance:**
- All the above test cases pass.
- `cd "My Files"` works.
- ASan/UBSan clean.

**Pitfalls:**
- Don't forget to flush `current` at the end of the loop.
- Tabs are whitespace too; use `std::isspace(static_cast<unsigned char>(c))`.
- Windows line endings (`\r\n`) — strip `\r`.

---

## Phase 3 — Project Structure

### 3.1 Split into headers and sources

**Goal:** One class = one `.h` + one `.cpp`. Declarations in headers, definitions in sources.

**Why:** Faster incremental builds, clearer ownership, enables unit testing, standard industry layout.

**Prerequisites:** Phases 1 & 2 done (easier to split correct, tested code).

**Target directory layout:**
```
purbasha oopd/
├── include/
│   ├── Command.h
│   ├── ModifyCommand.h
│   ├── NoModifyCommand.h
│   ├── CdCommand.h
│   ├── LsCommand.h
│   ├── MvCommand.h
│   ├── CpCommand.h
│   ├── RmCommand.h
│   ├── Tokenizer.h
│   └── CommandFactory.h
├── src/
│   ├── Command.cpp              (if any non-inline methods)
│   ├── CdCommand.cpp
│   ├── LsCommand.cpp
│   ├── MvCommand.cpp
│   ├── CpCommand.cpp
│   ├── RmCommand.cpp
│   ├── Tokenizer.cpp
│   ├── CommandFactory.cpp
│   └── main.cpp
├── tests/
│   └── test_tokenizer.cpp
├── Makefile
├── CMakeLists.txt
└── UPGRADE_ROADMAP.md
```

**Step-by-step:**

1. Create the directories.
2. For each class `Foo`:
   - Create `include/Foo.h` with include guards:
     ```cpp
     #ifndef EDS_FOO_H
     #define EDS_FOO_H
     #include <vector>
     #include <string>
     class Foo : public Bar {
     public:
         void execute(const std::vector<std::string>& args) override;
     private:
         void displayHelp() const;
     };
     #endif
     ```
   - Create `src/Foo.cpp` with the method bodies.
3. Add a `CommandFactory` class that centralizes the string → `unique_ptr<Command>` dispatch in `main()`.
   ```cpp
   class CommandFactory {
   public:
       static std::unique_ptr<Command> create(const std::string& name);
   };
   ```
4. `main.cpp` becomes ~25 lines: read → tokenize → factory → execute.
5. Update the build system (next section).

**Acceptance:**
- Each `.cpp` compiles independently.
- Editing one class's body recompiles only that TU + the final link.
- `main.cpp` has no `#include` for command internals, only `CommandFactory.h` and `Tokenizer.h`.

**Pitfalls:**
- Forgetting `override` or `virtual` in derived class headers.
- Circular `#include`s — use forward declarations where possible.
- Definition in header instead of source (ODR violations on link).

---

### 3.2 Upgraded Makefile

**Goal:** A Makefile that supports `make`, `make debug`, `make release`, `make test`, `make clean`, with proper dependency tracking.

**Step-by-step:**

1. Replace the current Makefile with:
   ```makefile
   CXX       := g++
   CXXSTD    := -std=c++17
   WARN      := -Wall -Wextra -Wpedantic
   INCLUDES  := -Iinclude
   LDFLAGS   := -pthread

   DEBUG_FLAGS   := -O0 -g -fsanitize=address,undefined
   RELEASE_FLAGS := -O2 -DNDEBUG

   SRC_DIR   := src
   OBJ_DIR   := build
   BIN_DIR   := bin
   TEST_DIR  := tests

   SRCS  := $(wildcard $(SRC_DIR)/*.cpp)
   OBJS  := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
   DEPS  := $(OBJS:.o=.d)

   TARGET_DEBUG   := $(BIN_DIR)/eds-debug
   TARGET_RELEASE := $(BIN_DIR)/eds

   .PHONY: all debug release test clean

   all: release

   debug:   CXXFLAGS := $(CXXSTD) $(WARN) $(DEBUG_FLAGS) $(INCLUDES) -MMD -MP
   debug:   $(TARGET_DEBUG)

   release: CXXFLAGS := $(CXXSTD) $(WARN) $(RELEASE_FLAGS) $(INCLUDES) -MMD -MP
   release: $(TARGET_RELEASE)

   $(TARGET_DEBUG) $(TARGET_RELEASE): $(OBJS) | $(BIN_DIR)
       $(CXX) $(OBJS) -o $@ $(LDFLAGS)

   $(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
       $(CXX) $(CXXFLAGS) -c $< -o $@

   $(OBJ_DIR) $(BIN_DIR):
       mkdir -p $@

   test: debug
       # wire in GoogleTest later; for now just run the binary smoke test
       echo "exit" | $(TARGET_DEBUG)

   clean:
       rm -rf $(OBJ_DIR) $(BIN_DIR)

   -include $(DEPS)
   ```
2. What the flags do:
   - `-MMD -MP` — generate `.d` files with header dependencies; `-MP` adds phony targets to avoid "missing header" errors after rename.
   - `-include $(DEPS)` — pull those dep files in so changing a header only rebuilds affected TUs.
   - `-fsanitize=address,undefined` — ASan + UBSan in debug builds.
3. Run: `make debug`, `make release`, `make clean`.

**Acceptance:**
- Edit one header → only dependent `.o`s rebuild.
- `make debug` runs under ASan cleanly.
- `make release` produces an optimized binary.

**Pitfalls:**
- Mixing tabs/spaces in Makefile — **must** be real tabs.
- Forgetting `mkdir -p` on out-dirs (the `| $(OBJ_DIR)` order-only prerequisite handles this).
- Shell sanitizers not supported by clang on some older macOS versions — fall back to `-fsanitize=address` alone.

---

### 3.3 CMake build system

**Goal:** A `CMakeLists.txt` that builds the same targets, so any IDE (CLion, VS Code, Qt Creator) can open the project.

**Prerequisites:** 3.1 done.

**Step-by-step:**

1. Create `CMakeLists.txt` at the project root:
   ```cmake
   cmake_minimum_required(VERSION 3.16)
   project(eds LANGUAGES CXX)

   set(CMAKE_CXX_STANDARD 17)
   set(CMAKE_CXX_STANDARD_REQUIRED ON)
   set(CMAKE_CXX_EXTENSIONS OFF)
   set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # for clangd/IDEs

   if(NOT CMAKE_BUILD_TYPE)
       set(CMAKE_BUILD_TYPE Release)
   endif()

   add_compile_options(-Wall -Wextra -Wpedantic)

   file(GLOB_RECURSE EDS_SRC CONFIGURE_DEPENDS src/*.cpp)

   add_library(eds_core STATIC ${EDS_SRC})
   target_include_directories(eds_core PUBLIC include)
   target_link_libraries(eds_core PUBLIC pthread)

   add_executable(eds src/main.cpp)
   target_link_libraries(eds PRIVATE eds_core)

   # Sanitizer-enabled debug target
   if(CMAKE_BUILD_TYPE STREQUAL "Debug")
       target_compile_options(eds_core PRIVATE -fsanitize=address,undefined -O0 -g)
       target_link_options(eds_core    PUBLIC  -fsanitize=address,undefined)
   endif()

   enable_testing()
   add_subdirectory(tests)
   ```
2. `tests/CMakeLists.txt`:
   ```cmake
   add_executable(test_tokenizer test_tokenizer.cpp)
   target_link_libraries(test_tokenizer PRIVATE eds_core)
   add_test(NAME tokenizer COMMAND test_tokenizer)
   ```
3. Build:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build -j
   ctest --test-dir build
   ```

**Acceptance:**
- `compile_commands.json` appears in `build/`, clangd/IDE intellisense works.
- `ctest` runs and passes.
- CLion/VS Code/Qt Creator opens the project without manual setup.

**Pitfalls:**
- Don't mix Makefile and CMake build outputs in the same directory — CMake uses `build/`, Makefile uses `build/` too. Either rename one, or keep `build/cmake/` vs `build/make/`.
- `file(GLOB)` doesn't re-detect new files across builds unless `CONFIGURE_DEPENDS` is set.

---

## Phase 4 — Real Shell Features

### 4.1 Pipes between commands

**Goal:** Support `cmd1 | cmd2` where `cmd1`'s stdout feeds `cmd2`'s stdin.

**Why:** This is the single most important feature of any Unix shell. Until you have pipes, you're writing a command dispatcher, not a shell.

**Prerequisites:** Phases 1–3.

**Key design question:** does `cmd1` or `cmd2` have to be an **external program**, or are they your built-in `Command` classes?

There are two paths. Do both, in order:

#### Path A — External program pipes (most useful, most classic)

Your shell currently runs 5 built-ins. For pipes to shine you also need to fork/exec real programs (`grep`, `wc`, `sort`, `head`, …).

**Step-by-step:**

1. Teach the tokenizer to split on `|`: output a `vector<vector<string>>` — one inner vector per pipeline stage.
2. Add a new `Pipeline` executor:
   ```cpp
   int executePipeline(const std::vector<std::vector<std::string>>& stages);
   ```
3. For N stages, create N-1 pipes using `pipe(int[2])`.
4. For each stage, `fork()`, then in the child:
   - `dup2` the appropriate pipe end to stdin (if not first stage) and stdout (if not last stage).
   - Close all other pipe fds.
   - If the command matches a built-in name, call the built-in's `execute()` and `_exit(0)`.
   - Else call `execvp(argv[0], argv)` — this looks up `$PATH` and runs external programs.
5. In the parent, close all pipe fds, then `waitpid` for every child.
6. Return the exit code of the **last** stage (Bash's convention).

**Code sketch:**
```cpp
int run_stage(const std::vector<std::string>& argv, int in_fd, int out_fd) {
    pid_t pid = fork();
    if (pid == 0) {
        if (in_fd  != STDIN_FILENO)  { dup2(in_fd,  STDIN_FILENO);  close(in_fd); }
        if (out_fd != STDOUT_FILENO) { dup2(out_fd, STDOUT_FILENO); close(out_fd); }
        std::vector<char*> c_argv;
        for (auto& s : argv) c_argv.push_back(const_cast<char*>(s.c_str()));
        c_argv.push_back(nullptr);
        execvp(c_argv[0], c_argv.data());
        std::perror("execvp");
        _exit(127);
    }
    return pid;
}
```

7. Test with:
   ```
   ls | wc -l
   ls | grep .cpp | sort
   ```

#### Path B — Built-in command pipes

If you also want pipes between your built-ins (`ls -r | rm`), you need to redirect the built-in's `std::cout` to a pipe's write end and `std::cin` to the read end. Easiest approach: run each built-in in its own forked child so `dup2` works on stdout/stdin just like external programs.

**Acceptance:**
- `ls | wc -l` returns the correct file count.
- `ls | grep .cpp | sort | head -n 3` works.
- Pressing Ctrl+C during a long pipeline terminates **all** stages.
- No zombie processes after pipelines complete.

**Pitfalls:**
- **Leaking pipe fds:** every pipe fd must be closed in every process that isn't using it. Common bug: forgetting to close in the parent → reader never sees EOF → pipeline hangs.
- **SIGPIPE:** if the reader exits early, the writer gets SIGPIPE. Either ignore it (`signal(SIGPIPE, SIG_IGN)` + handle EPIPE) or let the default kill the child.
- **Process groups and terminal control:** for proper Ctrl+C handling, put the whole pipeline in one process group via `setpgid`, then `tcsetpgrp` to give it the terminal. This is surprisingly deep; look up "shell job control" in the glibc manual.
- **Zombies:** always `waitpid` every child you fork.

---

## Phase 5 — User Experience

### 5.1 Tab completion (Level 1)

**Goal:** Pressing `Tab` completes filenames in the current directory. Pressing `Tab` twice lists candidates.

**Prerequisites:** Phase 3 done (clean source layout). Decide between **GNU Readline** (powerful, GPL) and **linenoise** (small, BSD). For portfolio purposes, Readline is fine and ubiquitous.

**Step-by-step:**

1. Install Readline:
   - macOS: `brew install readline` — on macOS the system provides `libedit` which has a Readline-compatible header; prefer the Homebrew one.
   - Ubuntu: `sudo apt install libreadline-dev`
2. Link it in the Makefile/CMake:
   - Makefile: `LDFLAGS += -lreadline`
   - CMake: `target_link_libraries(eds PRIVATE readline)` (plus `find_library` if on macOS brew).
3. Replace the `std::getline(std::cin, input)` loop with Readline:
   ```cpp
   #include <readline/readline.h>
   #include <readline/history.h>

   char* line = readline(">> ");
   if (!line) break;            // EOF (Ctrl+D)
   if (*line) add_history(line);
   std::string input(line);
   free(line);
   ```
4. Register a completion function:
   ```cpp
   static char** eds_completion(const char* text, int start, int end);
   static char*  eds_path_generator(const char* text, int state);
   ...
   rl_attempted_completion_function = eds_completion;
   ```
5. Implementation sketch:
   ```cpp
   static char* eds_path_generator(const char* text, int state) {
       static std::vector<std::string> matches;
       static size_t idx;
       if (state == 0) {
           matches.clear(); idx = 0;
           std::string prefix(text);
           namespace fs = std::filesystem;
           fs::path dir = prefix.empty() ? "." : fs::path(prefix).parent_path();
           if (dir.empty()) dir = ".";
           std::string stem = fs::path(prefix).filename().string();
           for (auto& e : fs::directory_iterator(dir)) {
               auto name = e.path().filename().string();
               if (name.rfind(stem, 0) == 0) {
                   matches.push_back((dir / name).string());
               }
           }
       }
       if (idx >= matches.size()) return nullptr;
       return strdup(matches[idx++].c_str());   // readline frees it
   }
   ```
6. Save/load history across sessions:
   ```cpp
   read_history("~/.eds_history");
   ...
   write_history("~/.eds_history");
   ```

**Acceptance:**
- Arrow-up recalls previous commands.
- Tab completes filenames; double-tab lists candidates.
- Persists history across shell restarts.

**Pitfalls:**
- Readline uses `malloc`/`free` — you **must** return `strdup`'d strings from generator, Readline frees them.
- On macOS, `libedit` has subtle API differences; pinning to Homebrew's GNU Readline is safer.
- Interference between Readline's terminal handling and your SIGINT handler; install Readline's signal-handling callbacks.

---

### 5.2 Command-aware completion (Level 2)

**Goal:** After `cd `, only suggest directories. After `rm `, suggest files. After a command name with no leading context, suggest built-in command names.

**Prerequisites:** 5.1 done.

**Design:** Extend every `Command` subclass with a virtual completion hook:

```cpp
class Command {
public:
    virtual std::vector<std::string>
    complete(const std::vector<std::string>& tokens,
             size_t tokenIdx,
             const std::string& prefix) const {
        return defaultPathComplete(prefix);   // fallback
    }
    ...
};
```

**Step-by-step:**

1. Add `complete()` to `Command` with a default implementation that returns filesystem paths matching `prefix`.
2. Override in:
   - `CdCommand::complete` — filter to directories only.
   - `RmCommand::complete` — filter to files (or files+dirs if `-r` flag is seen).
   - `MvCommand::complete` / `CpCommand::complete` — two-path completion: first arg = source (file), second arg = dest (file or dir).
   - `LsCommand::complete` — directories mostly.
3. In Readline's `eds_completion` callback:
   - Parse the current `rl_line_buffer` with your tokenizer.
   - If `tokens.empty()` or user is on `tokens[0]`, complete against **built-in names** (`cd`, `ls`, `mv`, `cp`, `rm`, `exit`) and, optionally, programs in `$PATH`.
   - Otherwise, look up the command via `CommandFactory`, call `cmd->complete(tokens, curIdx, prefix)`, and return those strings.
4. Add a helper:
   ```cpp
   std::vector<std::string> listDirsIn(const std::filesystem::path& dir,
                                        const std::string& stem);
   ```

**Acceptance:**
- `cd <TAB>` lists only directories.
- `rm <TAB>` lists only files.
- `l<TAB>` completes to `ls`.
- Custom completion does not interfere with `cd /etc/<TAB>` style absolute paths.

**Pitfalls:**
- The "current token" may span an escape (`My\ Doc<TAB>`). Reuse your Phase-2 tokenizer rather than splitting on space.
- Readline calls the generator until it returns `nullptr` — don't cache state across calls to different commands.
- Path completion on large directories can be slow; cap to, say, 1000 entries.

---

## Phase 6 — GUI Frontend

### 6.1 Qt desktop application

**Goal:** A native desktop GUI that uses the `eds_core` static library, with the feature set you described.

**Prerequisites:** Phases 1–5 done. The shell core must already be a library (Phase 3.3 achieved this).

**Estimated effort:** 30–40 hours (this is the biggest item on the list).

**Step-by-step:**

#### 6.1.1 Environment setup

1. Install Qt 6 (LTS):
   - macOS: `brew install qt` or download from qt.io and install Qt Creator.
   - Ubuntu: `sudo apt install qt6-base-dev qt6-base-dev-tools`
2. Verify: `qmake --version`, `cmake --find-package -DNAME=Qt6 -DCOMPILER_ID=GNU -DLANGUAGE=CXX -DMODE=EXIST`

#### 6.1.2 Project layout

```
purbasha oopd/
├── include/             (eds_core headers, unchanged)
├── src/                 (eds_core sources, unchanged)
├── gui/                 (NEW)
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── MainWindow.{h,cpp,ui}
│   ├── TerminalWidget.{h,cpp}
│   ├── CommandHighlighter.{h,cpp}
│   ├── AnsiTextEdit.{h,cpp}
│   ├── CommandPalette.{h,cpp}
│   └── resources.qrc
└── CMakeLists.txt
```

#### 6.1.3 Wire Qt into CMake

In the root `CMakeLists.txt`:
```cmake
option(EDS_BUILD_GUI "Build the Qt GUI" ON)
if(EDS_BUILD_GUI)
    find_package(Qt6 COMPONENTS Widgets REQUIRED)
    qt_standard_project_setup()
    add_subdirectory(gui)
endif()
```

In `gui/CMakeLists.txt`:
```cmake
qt_add_executable(eds-gui
    main.cpp
    MainWindow.cpp MainWindow.h MainWindow.ui
    TerminalWidget.cpp TerminalWidget.h
    CommandHighlighter.cpp CommandHighlighter.h
    AnsiTextEdit.cpp AnsiTextEdit.h
    CommandPalette.cpp CommandPalette.h
)
target_link_libraries(eds-gui PRIVATE Qt6::Widgets eds_core)
```

#### 6.1.4 Main window skeleton

`MainWindow` uses a `QSplitter` or dock widgets:

```
┌─────────────────────────────────────────────────────┐
│ File  View  Settings  Help                          │
├──────────────┬──────────────────────────────────────┤
│              │ [Tab 1] [Tab 2] [+]                  │
│  File Tree   ├──────────────────────────────────────┤
│  QTreeView   │                                      │
│              │   Terminal output (AnsiTextEdit)     │
│              │                                      │
│              │                                      │
│              ├──────────────────────────────────────┤
│              │  >> [QLineEdit with completions]     │
└──────────────┴──────────────────────────────────────┘
```

Implementation steps, one feature at a time:

1. **Main window & input/output.**
   - `QPlainTextEdit` set `readOnly(true)` for output.
   - `QLineEdit` at the bottom for input; `returnPressed` → execute.
   - Connect the `TerminalWidget::executeCommand(QString)` signal to a method that runs the command via `eds_core` on a worker thread, appends the output to the text edit.

2. **Run commands off the UI thread.**
   - Use `QThreadPool` + `QRunnable` or `QtConcurrent::run`.
   - Stream stdout back to the UI via a `QMetaObject::invokeMethod(ui, "appendOutput", Qt::QueuedConnection, Q_ARG(QString, chunk))` call.
   - Never touch `QWidget` from a non-UI thread.

3. **File tree side panel.**
   ```cpp
   auto* model = new QFileSystemModel(this);
   model->setRootPath(QDir::currentPath());
   ui->fileTree->setModel(model);
   ui->fileTree->setRootIndex(model->index(QDir::currentPath()));
   ```
   When your shell core executes a `cd`, emit a signal that updates `rootPath`.

4. **Tab support.**
   - `QTabWidget` as central widget.
   - Each tab hosts a `TerminalWidget` that owns its own Readline-like state (for GUI, we implement completion ourselves — see below).
   - "+" button to add tabs; `tabCloseRequested` to close.

5. **Syntax highlighting.**
   - Subclass `QSyntaxHighlighter` over the input `QLineEdit` (trick: use a hidden `QTextDocument` to highlight, then render formatted text).
   - Simpler alternative: switch input to a single-line `QPlainTextEdit` with a real `QSyntaxHighlighter`.
   - Rules:
     - First token (command): green if known built-in or found in PATH, red otherwise.
     - Quoted strings: orange.
     - Flags (`-x`, `--xxx`): cyan.
     - Paths that exist: underlined.

6. **ANSI color rendering.**
   - Write an `AnsiTextEdit` (subclass of `QPlainTextEdit`) that parses ANSI escape codes (`\033[31m` etc.) and applies `QTextCharFormat` via `QTextCursor::insertText(str, format)`.
   - There are open-source snippets; referencing one is fine, but writing your own is a strong skill demo.

7. **Search-in-scrollback (Ctrl+F).**
   - `QShortcut(QKeySequence::Find, this)` opens a small search bar over the output.
   - `document()->find(query, flags)` highlights matches.
   - N/Shift-N for next/previous.

8. **Command palette (Ctrl+Shift+P).**
   - A `QDialog` with a `QLineEdit` and `QListView`.
   - Fuzzy-match against: built-in commands, history entries, files under cwd.
   - Simple fuzzy score = subsequence match + bonus for consecutive matches. Libraries like `fts_fuzzy_match` (single header) work well here.

9. **Theming.**
   - Store `lightMode`, `fontFamily`, `fontSize`, color scheme as `QSettings` keys.
   - Ship two default color schemes (solarized-light, solarized-dark).
   - Apply via a global `QApplication::setPalette` and a QSS stylesheet in a resource file.

10. **Dockable panels.**
    - Use `QMainWindow::addDockWidget` with `QDockWidget` for file tree and history.
    - Users can drag them to left/right/top/bottom or detach into floating windows.
    - `saveState()`/`restoreState()` in `QSettings` persists the layout.

11. **Warp-style command blocks (bonus).**
    - Each executed command is a widget with a header (command text, timestamp, duration, exit code) and a collapsible output area.
    - Use a `QListView` backed by a custom model, one row per block, with a custom delegate rendering the block.

#### 6.1.5 Integration with core

**Rule:** the GUI only talks to `eds_core` via a narrow C++ interface. No Qt types in `eds_core`; no `std::cout` usage in `eds_core` — replace with an abstract `OutputSink` or callback:

```cpp
class OutputSink {
public:
    virtual void write(std::string_view chunk) = 0;
    virtual ~OutputSink() = default;
};
```

- CLI frontend passes a sink that writes to `std::cout`.
- GUI frontend passes a sink that emits a Qt signal with the chunk.

This decoupling is the single most important architectural decision of the GUI phase.

**Acceptance:**
- `eds-gui` launches a native Qt window.
- Running `ls` in the GUI shows colored output identical to CLI.
- File tree updates on `cd`.
- Tabs, syntax highlighting, command palette, theme switcher all work.
- Core remains usable as a CLI binary; both frontends share 100% of command logic.

**Pitfalls:**
- **UI freezes** from running commands on the GUI thread. Always dispatch to a worker.
- **Race conditions** on appending output: use `Qt::QueuedConnection` when signaling cross-thread.
- **Qt deployment:** on macOS, use `macdeployqt`; on Linux, `linuxdeployqt` or AppImage. Without this, your binary won't run on anyone else's machine.
- **Readline + Qt:** don't try to use Readline in the GUI; implement completion inside the `QLineEdit` yourself. Readline is CLI-only.

---

## Master Timeline

A realistic part-time pace, assuming ~10 hours/week:

| Week | Phase | Deliverable |
|------|-------|-------------|
| 1    | Phase 0–1 | Git repo, unique_ptr, thread/exception fixes, ASan-clean build |
| 2    | Phase 2 | Robust tokenizer with quotes + tests |
| 3    | Phase 3.1–3.2 | Split headers, upgraded Makefile |
| 4    | Phase 3.3 | CMake, compile_commands.json, GoogleTest wired |
| 5    | Phase 4.1 (Path A) | External command execution + basic pipes |
| 6    | Phase 4.1 (Path B) + signals | Job control, SIGINT handling |
| 7    | Phase 5.1 | Readline integrated, history, basic tab complete |
| 8    | Phase 5.2 | Command-aware completion per class |
| 9–12 | Phase 6 | Qt GUI, tabs, syntax highlight, palette, theming |
| 13   | Polish | README, architecture diagram, demo video, benchmarks |

---

## Definition of Done

Project is "done" for interview purposes when **all** of these are true:

- [ ] Compiles clean with `-Wall -Wextra -Wpedantic` on both gcc and clang.
- [ ] Zero ASan/UBSan/TSan errors on a 10-minute smoke test.
- [ ] Zero Valgrind leaks on a 1-minute smoke test.
- [ ] `make test` passes (unit tests for tokenizer, factory, at least one command).
- [ ] Supports: quoting, pipes, external commands, tab completion, history.
- [ ] CLI and GUI share `eds_core`; no duplicated logic.
- [ ] GitHub repo has: README with architecture diagram, screenshots, build instructions, benchmarks, and a demo GIF/video.
- [ ] CI pipeline (GitHub Actions) builds + runs tests on push for Linux + macOS.
- [ ] Each phase is a merged PR with a clear commit message.

---

## Risks & Pitfalls

| Risk | Mitigation |
|------|------------|
| Getting stuck on Qt deployment issues | Start the GUI inside Qt Creator; only worry about packaging at the very end. |
| Signal/job-control complexity in pipes | Use the classic "glibc Job Control" manual chapter as a template; implement it once, step by step. |
| Over-scoping the GUI | Keep a hard-cut feature list: tabs + syntax highlight + file tree + ANSI. Save command palette and themes for "if time permits." |
| Refactor breaks everything at once | Land each phase as its own PR; never mix refactor with new features. |
| Interview is next week and nothing is done | Do Phases 1 + 2 + 3.2 only. That alone is a credible upgrade story. |
| Dependencies not available on macOS | GNU Readline via Homebrew, Qt via Homebrew, everything else is in-stdlib. |

---

*Last updated: generated as part of the shell project upgrade plan.*
