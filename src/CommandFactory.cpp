// ============================================================================
// File:    CommandFactory.cpp
// Summary: Maps command name strings to Command subclass instances.
//          Adding a new command requires only one new else-if here
//          and the corresponding header include.
// ============================================================================

#include "CommandFactory.h"
#include "CdCommand.h"
#include "LsCommand.h"
#include "MvCommand.h"
#include "CpCommand.h"
#include "RmCommand.h"

std::unique_ptr<Command> CommandFactory::create(const std::string &name)
{
    if (name == "cd")
        return std::make_unique<CdCommand>();
    else if (name == "ls")
        return std::make_unique<LsCommand>();
    else if (name == "mv")
        return std::make_unique<MvCommand>();
    else if (name == "cp")
        return std::make_unique<CpCommand>();
    else if (name == "rm")
        return std::make_unique<RmCommand>();
    else
        return nullptr;
}
