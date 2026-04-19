// ============================================================================
// File:    CpCommand.h
// Summary: Implements the "cp" (copy) built-in command.
//          Supports: -r/-R (recursive copy via std::thread + promise/future),
//          -n (no-overwrite), -v/--verbose (print copied paths), -h/--help.
//          Recursive copy runs in a worker thread with exception-safe
//          promise handling — exceptions are forwarded to the caller via
//          promise.set_exception() instead of being silently swallowed.
// ============================================================================

#ifndef EDS_CP_COMMAND_H
#define EDS_CP_COMMAND_H

#include "Command.h"
#include <filesystem>
#include <thread>
#include <future>

namespace fs = std::filesystem;

class CpCommand : public ModifyCommand
{
public:
    void execute(const std::vector<std::string> &args) override;

private:
    void copyFiles(const std::string &source, const std::string &destination, bool recursive, bool noOverwrite, bool verbose);
    void recursiveCopy(const std::string &source, const std::string &destination, std::promise<void> promise);
    void displayHelp();
};

#endif // EDS_CP_COMMAND_H
