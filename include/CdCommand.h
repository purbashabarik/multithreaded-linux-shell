// ============================================================================
// File:    CdCommand.h
// Summary: Implements the "cd" (change directory) built-in command.
//          Supports: cd ~ (home), cd / (root), cd .. (parent), cd <path>,
//          ~/path expansion, and -v/--verbose flag for printing the new path.
//          Classified as NoModifyCommand since it doesn't alter files.
// ============================================================================

#ifndef EDS_CD_COMMAND_H
#define EDS_CD_COMMAND_H

#include "Command.h"
#include <filesystem>

namespace fs = std::filesystem;

class CdCommand : public NoModifyCommand
{
public:
    void execute(const std::vector<std::string> &args) override;

private:
    void displayHelp();
};

#endif // EDS_CD_COMMAND_H
