// ============================================================================
// File:    RmCommand.h
// Summary: Implements the "rm" (remove) built-in command.
//          Supports: -r/-R/--recursive (parallel deletion using std::async),
//          -f/--force (ignore nonexistent), -v/--verbose (print removed),
//          -h/--help.
//          Multiple file arguments are deleted concurrently — futures are
//          stored in a vector and drained after the loop (fixes the original
//          fire-and-forget std::async bug that silently serialized execution).
// ============================================================================

#ifndef EDS_RM_COMMAND_H
#define EDS_RM_COMMAND_H

#include "Command.h"
#include <filesystem>
#include <future>
#include <algorithm>

namespace fs = std::filesystem;

class RmCommand : public ModifyCommand
{
public:
    void execute(const std::vector<std::string> &args) override;

private:
    void removeFileOrDirectory(const std::string &targetPath, bool recursive, bool force, bool verbose);
    void displayHelp();
};

#endif // EDS_RM_COMMAND_H
