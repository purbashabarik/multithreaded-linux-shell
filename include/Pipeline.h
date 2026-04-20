// ============================================================================
// File:    Pipeline.h
// Summary: Executes a pipeline of one or more commands connected by pipes.
//          Each stage runs in its own child process via fork(). Adjacent
//          stages are connected using pipe() — stage N's stdout feeds
//          stage N+1's stdin via dup2(). Supports:
//            - External programs (looked up via execvp and $PATH)
//            - Built-in commands (cd, ls, mv, cp, rm) in forked children
//            - I/O redirection: < (stdin), > (stdout), >> (append stdout)
//          The parent waits on all children and returns the exit code of
//          the last stage (Bash convention).
//
// Key system calls used:
//   pipe()    — creates a pair of connected file descriptors
//   fork()    — clones the process; child gets pid=0
//   dup2()    — redirects stdin/stdout to a pipe end or file
//   execvp()  — replaces the child process with an external program
//   waitpid() — parent waits for each child to finish
// ============================================================================

#ifndef EDS_PIPELINE_H
#define EDS_PIPELINE_H

#include <vector>
#include <string>

// Splits a flat token list on "|" into a vector of stages.
// Example: ["ls", "-la", "|", "grep", ".cpp"] → [["ls","-la"], ["grep",".cpp"]]
std::vector<std::vector<std::string>> splitPipeline(const std::vector<std::string> &tokens);

// Executes a pipeline of one or more stages. Returns the exit code of the last stage.
// Single-stage pipelines with a built-in command (cd) are handled in-process.
int executePipeline(const std::vector<std::vector<std::string>> &stages);

#endif // EDS_PIPELINE_H
