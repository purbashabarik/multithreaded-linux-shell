// ============================================================================
// File:    MvCommand.cpp
// Summary: Implementation of the mv (move/rename) command.
//          Recursive move runs in a worker thread. On failure, the exception
//          is forwarded via promise.set_exception() so the main thread
//          never deadlocks.
// ============================================================================

#include "MvCommand.h"
#include <iostream>

void MvCommand::execute(const std::vector<std::string> &args)
{
    bool listFiles = false;
    bool recursive = false;
    bool noOverwrite = false;

    for (size_t i = 1; i < args.size(); ++i)
    {
        const std::string &arg = args[i];

        if (arg == "-h" || arg == "--help")
        {
            displayHelp();
            return;
        }
        else if (arg == "-l")
        {
            listFiles = true;
        }
        else if (arg == "-R")
        {
            recursive = true;
        }
        else if (arg == "-n")
        {
            noOverwrite = true;
        }
    }

    if (args.size() < 3)
    {
        std::cerr << "Usage: mv [OPTION] <source> <destination>\n";
        return;
    }

    const std::string &source = args[args.size() - 2];
    const std::string &destination = args[args.size() - 1];

    if (listFiles)
    {
        listFilesToBeMoved(source, destination, recursive);
    }
    else
    {
        performMove(source, destination, recursive, noOverwrite);
    }
}

void MvCommand::performMove(const std::string &source, const std::string &destination, bool recursive, bool noOverwrite)
{
    try
    {
        if (noOverwrite && fs::exists(destination))
        {
            std::cout << "Destination file already exists. Skipping move.\n";
            return;
        }

        if (recursive)
        {
            std::promise<void> promise;
            std::future<void> future = promise.get_future();

            std::thread thread(&MvCommand::recursiveMove, this, source, destination, std::move(promise));

            // future.get() re-throws any exception from the worker thread
            try { future.get(); }
            catch (const std::exception &e)
            {
                thread.join();
                throw;
            }

            thread.join();
        }
        else
        {
            fs::rename(source, destination);
            std::cout << "Moved: " << source << " to " << destination << "\n";
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error moving " << source << " to " << destination << ": " << e.what() << "\n";
    }
}

void MvCommand::recursiveMove(const std::string &source, const std::string &destination, std::promise<void> promise)
{
    try
    {
        fs::copy(source, destination, fs::copy_options::recursive | fs::copy_options::skip_existing);
        fs::remove_all(source);

        std::cout << "Moved (recursively): " << source << " to " << destination << "\n";

        promise.set_value();
    }
    catch (...)
    {
        // Forward the exception to the caller via the promise so future.get() re-throws it
        try { promise.set_exception(std::current_exception()); }
        catch (...) {}
    }
}

void MvCommand::listFilesToBeMoved(const std::string &source, const std::string &destination, bool recursive)
{
    try
    {
        if (!recursive)
        {
            for (const auto &entry : fs::recursive_directory_iterator(source))
            {
                const std::string &sourceFile = entry.path().string();
                const std::string destinationFile = destination + sourceFile.substr(source.size());
                std::cout << "Will move: " << sourceFile << " to " << destinationFile << "\n";
            }
        }
        else
        {
            std::cout << "Will move: " << source << " to " << destination << "\n";
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error listing files to be moved: " << e.what() << "\n";
    }
}

void MvCommand::displayHelp()
{
    std::cout << "Usage: mv [OPTION] <source> <destination>\n"
              << "Move (rename) the specified source file or directory to the destination.\n"
              << "If the destination is an existing directory, the source is moved into the directory.\n\n"
              << "Options:\n"
              << "  -l            list the files that will be moved\n"
              << "  -R            recursively move directories and their contents\n"
              << "  -n            do not overwrite existing files\n"
              << "  -h, --help    display this help and exit\n";
}
