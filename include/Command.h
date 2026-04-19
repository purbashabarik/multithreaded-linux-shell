// ============================================================================
// File:    Command.h
// Summary: Abstract base class for all shell commands. Uses the Command
//          design pattern — every shell operation (cd, ls, mv, cp, rm)
//          inherits from this class and implements execute().
//          Two intermediate classes (ModifyCommand, NoModifyCommand) classify
//          commands by whether they alter the filesystem.
// ============================================================================

#ifndef EDS_COMMAND_H
#define EDS_COMMAND_H

#include <vector>
#include <string>

class Command
{
public:
    virtual void execute(const std::vector<std::string> &args) = 0;
    virtual ~Command() {}
};

// Base for commands that modify the filesystem (mv, cp, rm)
class ModifyCommand : public Command
{
};

// Base for commands that only read the filesystem (cd, ls)
class NoModifyCommand : public Command
{
};

#endif // EDS_COMMAND_H
