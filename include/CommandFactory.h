// ============================================================================
// File:    CommandFactory.h
// Summary: Factory class that maps command name strings ("cd", "ls", etc.)
//          to their corresponding Command subclass instances. Centralizes
//          the dispatch logic that was previously a long if-else chain in
//          main(). Returns nullptr for unrecognized commands.
// ============================================================================

#ifndef EDS_COMMAND_FACTORY_H
#define EDS_COMMAND_FACTORY_H

#include "Command.h"
#include <memory>
#include <string>

class CommandFactory
{
public:
    static std::unique_ptr<Command> create(const std::string &name);
};

#endif // EDS_COMMAND_FACTORY_H
