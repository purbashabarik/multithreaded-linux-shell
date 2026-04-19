// ============================================================================
// File:    LsCommand.h
// Summary: Implements the "ls" (list directory) built-in command.
//          Supports: -r/--recursive (parallel directory traversal using
//          std::async), -H/--human-readable (file sizes in KB/MB/GB),
//          ~ (list home directory), and -h/--help.
//          Recursive listing spawns one async task per subdirectory.
// ============================================================================

#ifndef EDS_LS_COMMAND_H
#define EDS_LS_COMMAND_H

#include "Command.h"
#include <filesystem>
#include <future>
#include <sstream>

namespace fs = std::filesystem;

class LsCommand : public NoModifyCommand
{
public:
    void execute(const std::vector<std::string> &args) override;

private:
    void listDirectory(const fs::path &dirPath, bool recursive, bool humanReadable);
    void displayHelp();
    std::string formatSize(uintmax_t size);
};

#endif // EDS_LS_COMMAND_H
