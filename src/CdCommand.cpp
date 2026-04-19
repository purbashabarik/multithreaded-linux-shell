// ============================================================================
// File:    CdCommand.cpp
// Summary: Implementation of the cd (change directory) command.
//          Uses std::filesystem to validate and navigate directories.
// ============================================================================

#include "CdCommand.h"
#include <iostream>
#include <cstdlib>

void CdCommand::execute(const std::vector<std::string> &args)
{
    bool verbose = false;

    for (size_t i = 1; i < args.size(); ++i)
    {
        if (args[i] == "-h" || args[i] == "--help")
        {
            displayHelp();
            return;
        }
        else if (args[i] == "-v" || args[i] == "--verbose")
        {
            verbose = true;
        }
    }

    if (args.size() == 1 || (args.size() == 2 && args[1] == "/"))
    {
        fs::current_path(fs::current_path().root_path());
        if (verbose)
        {
            std::cout << "Changed to the root directory: " << fs::current_path() << "\n";
        }
    }
    else
    {
        std::string targetDir = args.back();
        if (targetDir == "~")
        {
            targetDir = getenv("HOME");
        }
        else if (targetDir.substr(0, 2) == "~/" || targetDir.substr(0, 3) == "~/.")
        {
            targetDir.replace(0, 1, getenv("HOME"));
        }

        if (fs::exists(targetDir) && fs::is_directory(targetDir))
        {
            fs::current_path(targetDir);
            if (verbose)
            {
                std::cout << "Changed to: " << fs::current_path() << "\n";
            }
        }
        else
        {
            std::cout << "Invalid directory: " << targetDir << "\n";
        }
    }
}

void CdCommand::displayHelp()
{
    std::cout << "Usage: cd <directory>\n"
              << "Change the current working directory to the specified directory.\n\n"
              << "Options:\n"
              << " -h, --help    display this help and exit\n"
              << " cd ~           To change to the home directory,\n"
              << " cd ..            To go to parent directory\n"
              << " cd -v/-verbose <path>,     print the full path of the new directory\n"
              << "cd /              To go to the ROOT directory\n";
}
