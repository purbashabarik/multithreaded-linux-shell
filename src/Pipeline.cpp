// ============================================================================
// File:    Pipeline.cpp
// Summary: Core execution engine for the shell. Handles three cases:
//
//   1. Single built-in command (cd) — runs in-process because cd must
//      change the parent's working directory; forking would be pointless.
//
//   2. Single command (built-in or external) with no pipes — forks one
//      child, applies I/O redirects, runs via execvp (external) or the
//      Command class (built-in), parent waits.
//
//   3. Multi-stage pipeline (cmd1 | cmd2 | ...) — creates N-1 pipes,
//      forks N children. Each child gets its stdin/stdout wired to the
//      correct pipe ends via dup2(). Parent closes all pipe fds and
//      waits on every child.
//
// The key insight: built-in commands in a pipeline also run in forked
// children, so their stdout can be captured by the pipe. The only
// exception is "cd" alone (no pipe), which must run in the parent.
// ============================================================================

#include "Pipeline.h"
#include "Redirect.h"
#include "CommandFactory.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <csignal>

std::vector<std::vector<std::string>> splitPipeline(const std::vector<std::string> &tokens)
{
    std::vector<std::vector<std::string>> stages;
    std::vector<std::string> current;

    for (const auto &tok : tokens)
    {
        if (tok == "|")
        {
            if (!current.empty())
            {
                stages.push_back(current);
                current.clear();
            }
        }
        else
        {
            current.push_back(tok);
        }
    }

    if (!current.empty())
    {
        stages.push_back(current);
    }

    return stages;
}

// Runs a single stage in a child process.
// in_fd/out_fd are the pipe ends to wire to stdin/stdout (-1 means don't redirect).
// all_pipe_fds contains every pipe fd that must be closed in the child.
static pid_t runStage(std::vector<std::string> args,
                      int in_fd, int out_fd,
                      const std::vector<int> &all_pipe_fds)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        std::perror("fork");
        return -1;
    }

    if (pid == 0)
    {
        // --- Child process ---

        // Wire pipe ends to stdin/stdout
        if (in_fd != -1)
        {
            dup2(in_fd, STDIN_FILENO);
        }
        if (out_fd != -1)
        {
            dup2(out_fd, STDOUT_FILENO);
        }

        // Close ALL pipe fds in the child — the child only uses stdin/stdout now
        for (int fd : all_pipe_fds)
        {
            close(fd);
        }

        // Parse and apply I/O redirections (>, >>, <) from the args
        RedirectInfo redir = parseRedirects(args);
        if (!applyRedirects(redir))
        {
            _exit(1);
        }

        // Try running as a built-in command
        auto command = CommandFactory::create(args[0]);
        if (command)
        {
            command->execute(args);
            // Flush C++ streams before _exit() — _exit skips destructors
            // and stream buffers, so piped output would be lost without this
            std::cout.flush();
            std::cerr.flush();
            _exit(0);
        }

        // Not a built-in — run as an external program via execvp
        std::vector<char *> c_argv;
        for (auto &s : args)
        {
            c_argv.push_back(const_cast<char *>(s.c_str()));
        }
        c_argv.push_back(nullptr);

        execvp(c_argv[0], c_argv.data());

        // execvp only returns on failure
        std::cerr << args[0] << ": command not found\n";
        _exit(127);
    }

    return pid;
}

int executePipeline(const std::vector<std::vector<std::string>> &stages)
{
    if (stages.empty()) return 0;

    // Special case: "cd" must run in the parent process (not forked),
    // because changing the directory in a child has no effect on the parent.
    if (stages.size() == 1 && stages[0][0] == "cd")
    {
        auto command = CommandFactory::create("cd");
        if (command)
        {
            std::vector<std::string> args = stages[0];
            command->execute(args);
        }
        return 0;
    }

    size_t num_stages = stages.size();
    size_t num_pipes = num_stages - 1;

    // Create all pipes upfront.
    // pipe_fds[i] is a pair: [read_end, write_end] connecting stage i → stage i+1.
    std::vector<int> pipe_fds(num_pipes * 2);
    std::vector<int> all_fds;

    for (size_t i = 0; i < num_pipes; ++i)
    {
        if (pipe(&pipe_fds[i * 2]) < 0)
        {
            std::perror("pipe");
            return 1;
        }
        all_fds.push_back(pipe_fds[i * 2]);       // read end
        all_fds.push_back(pipe_fds[i * 2 + 1]);   // write end
    }

    // Fork one child per stage
    std::vector<pid_t> children;
    for (size_t i = 0; i < num_stages; ++i)
    {
        // Determine which pipe ends this stage reads from / writes to
        int in_fd = -1;
        int out_fd = -1;

        if (i > 0)
        {
            // Read from previous pipe's read end
            in_fd = pipe_fds[(i - 1) * 2];
        }
        if (i < num_pipes)
        {
            // Write to current pipe's write end
            out_fd = pipe_fds[i * 2 + 1];
        }

        pid_t pid = runStage(stages[i], in_fd, out_fd, all_fds);
        if (pid > 0)
        {
            children.push_back(pid);
        }
    }

    // Parent closes ALL pipe fds — critical! Without this, readers never see EOF.
    for (int fd : all_fds)
    {
        close(fd);
    }

    // Wait for every child. Return exit code of the last stage.
    int last_status = 0;
    for (pid_t pid : children)
    {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status))
        {
            last_status = WEXITSTATUS(status);
        }
    }

    return last_status;
}
