// ============================================================================
// File:    MvCommand.h
// Summary: Implements the "mv" (move/rename) built-in command.
//          Supports: -R (recursive move via std::thread + promise/future),
//          -n (no-overwrite), -l (preview files to be moved), -h/--help.
//          Recursive move runs in a worker thread with exception-safe
//          promise handling to prevent deadlocks on failure.
// ============================================================================

#ifndef EDS_MV_COMMAND_H
#define EDS_MV_COMMAND_H

#include "Command.h"
#include <filesystem>
#include <thread>
#include <future>

namespace fs = std::filesystem;

class MvCommand : public ModifyCommand
{
public:
    void execute(const std::vector<std::string> &args) override;

private:
    void performMove(const std::string &source, const std::string &destination, bool recursive, bool noOverwrite);
    void recursiveMove(const std::string &source, const std::string &destination, std::promise<void> promise);
    void listFilesToBeMoved(const std::string &source, const std::string &destination, bool recursive);
    void displayHelp();
};

#endif // EDS_MV_COMMAND_H
