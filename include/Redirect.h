// ============================================================================
// File:    Redirect.h
// Summary: Parses and applies I/O redirection operators in a command's
//          argument list. Supports three operators:
//            <  file   — redirect stdin from file
//            >  file   — redirect stdout to file (truncate)
//            >> file   — redirect stdout to file (append)
//          parseRedirects() strips the redirection tokens from the args
//          and returns a RedirectInfo struct. applyRedirects() opens the
//          files and calls dup2() to wire them to stdin/stdout.
//          Used inside forked child processes before exec.
// ============================================================================

#ifndef EDS_REDIRECT_H
#define EDS_REDIRECT_H

#include <string>
#include <vector>

struct RedirectInfo
{
    std::string stdin_file;       // file for < redirect (empty = no redirect)
    std::string stdout_file;      // file for > or >> redirect
    bool stdout_append = false;   // true for >>, false for >
};

// Scans args for <, >, >> and their target filenames.
// Removes the redirect tokens from args so the command sees clean arguments.
RedirectInfo parseRedirects(std::vector<std::string> &args);

// Opens files and calls dup2() to apply redirections in the current process.
// Returns true on success, false on error (prints to stderr).
bool applyRedirects(const RedirectInfo &info);

#endif // EDS_REDIRECT_H
