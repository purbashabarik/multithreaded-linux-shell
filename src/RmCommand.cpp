// ============================================================================
// File:    RmCommand.cpp
// Summary: Implementation of the rm (remove) command.
//          Multiple file arguments are deleted concurrently — each gets its
//          own std::async task. Futures are stored in a vector and drained
//          after the loop to achieve true parallelism (fixes the original
//          fire-and-forget bug).
// ============================================================================

#include "RmCommand.h"
#include <iostream>

void RmCommand::execute(const std::vector<std::string> &args)
{
    bool recursive = false;
    bool force = false;
    bool verbose = false;
    std::vector<std::future<void>> futures;

    for (size_t i = 1; i < args.size(); ++i)
    {
        const std::string &arg = args[i];

        if (arg == "-h" || arg == "--help")
        {
            displayHelp();
            return;
        }
        else if (arg == "-r" || arg == "-R" || arg == "--recursive")
        {
            recursive = true;
        }
        else if (arg == "-f" || arg == "--force")
        {
            force = true;
        }
        else if (arg == "-v" || arg == "--verbose")
        {
            verbose = true;
        }
        else
        {
            // Each file/directory target runs on its own async task
            futures.push_back(
                std::async(std::launch::async,
                           &RmCommand::removeFileOrDirectory,
                           this, arg, recursive, force, verbose)
            );
        }
    }

    // Drain all futures — this is where actual parallelism happens.
    // Without storing futures, std::async's destructor blocks immediately.
    for (auto &f : futures)
    {
        try { f.get(); }
        catch (const std::exception &e)
        {
            std::cerr << "rm task failed: " << e.what() << "\n";
        }
    }
}

void RmCommand::removeFileOrDirectory(const std::string &targetPath, bool recursive, bool force, bool verbose)
{
    if (!fs::exists(targetPath))
    {
        if (!force)
        {
            std::cerr << "Error: " << targetPath << " does not exist.\n";
        }
        return;
    }

    try
    {
        if (fs::is_directory(targetPath))
        {
            if (recursive)
            {
                std::vector<std::future<void>> futures;

                for (const auto &entry : fs::recursive_directory_iterator(targetPath))
                {
                    futures.push_back(std::async(std::launch::async, &RmCommand::removeFileOrDirectory, this, entry.path().string(), true, force, verbose));
                }

                std::for_each(futures.begin(), futures.end(), [](std::future<void> &f)
                              { f.wait(); });

                fs::remove(targetPath);
                if (verbose)
                {
                    std::cout << "Removed: " << targetPath << "\n";
                }
            }
            else
            {
                std::cerr << "Error: " << targetPath << " is a directory. Use -r, -R, or --recursive to remove directories recursively.\n";
            }
        }
        else
        {
            fs::remove(targetPath);
            if (verbose)
            {
                std::cout << "Removed: " << targetPath << "\n";
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error removing " << targetPath << ": " << e.what() << "\n";
    }
}

void RmCommand::displayHelp()
{
    std::cout << "Usage: rm [OPTION] <file/directory>...\n"
              << "Remove (unlink) the specified files or directories.\n\n"
              << "Options:\n"
              << "  -r, -R, --recursive    remove directories and their contents recursively\n"
              << "  -f, --force           ignore nonexistent files, and never prompt before removing\n"
              << "  -v, --verbose         print the names of the files and directories that are removed\n"
              << "  -h, --help            display this help and exit\n";
}
