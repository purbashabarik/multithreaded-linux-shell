// ============================================================================
// File:    LsCommand.cpp
// Summary: Implementation of the ls (list directory) command.
//          Recursive listing uses std::async to traverse subdirectories
//          in parallel, with futures collected and drained after traversal.
// ============================================================================

#include "LsCommand.h"
#include <iostream>
#include <cstdlib>

void LsCommand::execute(const std::vector<std::string> &args)
{
    bool recursive = false;
    bool showHelp = false;
    bool humanReadable = false;

    for (size_t i = 1; i < args.size(); ++i)
    {
        if (args[i] == "-r" || args[i] == "--recursive")
        {
            recursive = true;
        }
        else if (args[i] == "-h" || args[i] == "--help")
        {
            showHelp = true;
        }
        else if (args[i] == "-H" || args[i] == "--human-readable")
        {
            humanReadable = true;
        }
    }

    if (showHelp)
    {
        displayHelp();
    }
    else
    {
        std::string directoryPath = fs::current_path();

        if (args.size() > 1 && args[1] == "~")
        {
            directoryPath = fs::path(getenv("HOME")).string();
        }

        listDirectory(directoryPath, recursive, humanReadable);
    }
}

void LsCommand::listDirectory(const fs::path &dirPath, bool recursive, bool humanReadable)
{
    std::vector<std::future<void>> futures;

    fs::directory_iterator end_itr;
    for (fs::directory_iterator itr(dirPath); itr != end_itr; ++itr)
    {
        if (recursive && fs::is_directory(itr->path()))
        {
            futures.push_back(std::async(std::launch::async, &LsCommand::listDirectory, this, itr->path(), true, humanReadable));
        }
        else
        {
            std::cout << itr->path().filename();
            if (humanReadable)
            {
                std::cout << " (" << formatSize(fs::file_size(itr->path())) << ")";
            }
            std::cout << "\n";
        }
    }

    for (auto &future : futures)
    {
        future.get();
    }
}

void LsCommand::displayHelp()
{
    std::cout << "Usage: ls [OPTION] [DIRECTORY]\n"
              << "List information about the FILEs (the current directory by default).\n\n"
              << "Options:\n"
              << "  -r, --recursive         list subdirectories recursively\n"
              << "  -h, --help              display this help and exit\n"
              << "  -H, --human-readable    list file sizes in human-readable format\n"
              << "  - ~(tilde)              list users home directory\n";
}

std::string LsCommand::formatSize(uintmax_t size)
{
    static const char *suffixes[] = {"B", "KB", "MB", "GB", "TB"};
    int suffixIndex = 0;
    while (size >= 1024 && suffixIndex < 4)
    {
        size /= 1024;
        ++suffixIndex;
    }
    std::stringstream ss;
    ss << size << " " << suffixes[suffixIndex];
    return ss.str();
}
