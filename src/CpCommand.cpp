// ============================================================================
// File:    CpCommand.cpp
// Summary: Implementation of the cp (copy) command.
//          Recursive copy runs in a worker thread with exception-safe
//          promise handling — same pattern as MvCommand.
// ============================================================================

#include "CpCommand.h"
#include <iostream>

void CpCommand::execute(const std::vector<std::string> &args)
{
    bool recursive = false;
    bool noOverwrite = false;
    bool verbose = false;

    for (size_t i = 1; i < args.size(); ++i)
    {
        const std::string &arg = args[i];

        if (arg == "-h" || arg == "--help")
        {
            displayHelp();
            return;
        }
        else if (arg == "-r" || arg == "-R")
        {
            recursive = true;
        }
        else if (arg == "-n")
        {
            noOverwrite = true;
        }
        else if (arg == "-v" || arg == "--verbose")
        {
            verbose = true;
        }
    }

    if (args.size() < 3)
    {
        std::cerr << "Usage: cp [OPTION] <source> <destination>\n";
        return;
    }

    const std::string &source = args[args.size() - 2];
    const std::string &destination = args[args.size() - 1];

    copyFiles(source, destination, recursive, noOverwrite, verbose);
}

void CpCommand::copyFiles(const std::string &source, const std::string &destination, bool recursive, bool noOverwrite, bool verbose)
{
    try
    {
        fs::copy_options options = fs::copy_options::none;
        if (recursive)
        {
            std::promise<void> promise;
            std::future<void> future = promise.get_future();

            std::thread thread(&CpCommand::recursiveCopy, this, source, destination, std::move(promise));

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
            fs::copy(source, destination, options);
        }

        if (verbose)
        {
            std::cout << "Copied: " << source << " to " << destination << "\n";
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error copying " << source << " to " << destination << ": " << e.what() << "\n";
    }
}

void CpCommand::recursiveCopy(const std::string &source, const std::string &destination, std::promise<void> promise)
{
    try
    {
        fs::copy(source, destination, fs::copy_options::recursive | fs::copy_options::skip_existing);

        promise.set_value();
    }
    catch (...)
    {
        try { promise.set_exception(std::current_exception()); }
        catch (...) {}
    }
}

void CpCommand::displayHelp()
{
    std::cout << "Usage: cp [OPTION] <source> <destination>\n"
              << "Copy the specified source file or directory to the destination.\n\n"
              << "Options:\n"
              << "  -r, -R        copy directories recursively\n"
              << "  -n            do not overwrite existing files\n"
              << "  -v, --verbose print the names of the files and directories that are copied\n"
              << "  -h, --help    display this help and exit\n";
}
