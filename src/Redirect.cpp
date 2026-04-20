// ============================================================================
// File:    Redirect.cpp
// Summary: Implementation of I/O redirection parsing and application.
//          parseRedirects() walks the argument list looking for <, >, >>
//          tokens and extracts the filename that follows each one.
//          applyRedirects() opens files and calls dup2() to wire them
//          to stdin (fd 0) or stdout (fd 1).
// ============================================================================

#include "Redirect.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

RedirectInfo parseRedirects(std::vector<std::string> &args)
{
    RedirectInfo info;
    std::vector<std::string> clean_args;

    for (size_t i = 0; i < args.size(); ++i)
    {
        if (args[i] == "<" && i + 1 < args.size())
        {
            info.stdin_file = args[++i];
        }
        else if (args[i] == ">>" && i + 1 < args.size())
        {
            info.stdout_file = args[++i];
            info.stdout_append = true;
        }
        else if (args[i] == ">" && i + 1 < args.size())
        {
            info.stdout_file = args[++i];
            info.stdout_append = false;
        }
        else
        {
            clean_args.push_back(args[i]);
        }
    }

    args = clean_args;
    return info;
}

bool applyRedirects(const RedirectInfo &info)
{
    // Redirect stdin from a file
    if (!info.stdin_file.empty())
    {
        int fd = open(info.stdin_file.c_str(), O_RDONLY);
        if (fd < 0)
        {
            std::perror(info.stdin_file.c_str());
            return false;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    // Redirect stdout to a file (truncate or append)
    if (!info.stdout_file.empty())
    {
        int flags = O_WRONLY | O_CREAT;
        flags |= info.stdout_append ? O_APPEND : O_TRUNC;

        int fd = open(info.stdout_file.c_str(), flags, 0644);
        if (fd < 0)
        {
            std::perror(info.stdout_file.c_str());
            return false;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    return true;
}
