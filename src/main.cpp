// ============================================================================
// File:    main.cpp
// Summary: Entry point for the Embedded Diagnostic Shell (EDS).
//          Runs a REPL: reads input, tokenizes it, splits on pipes,
//          and routes to the Pipeline executor which handles:
//            - Built-in commands (cd, ls, mv, cp, rm)
//            - External programs (grep, wc, sort, cat, etc. via $PATH)
//            - Pipes between commands (cmd1 | cmd2 | cmd3)
//            - I/O redirection (< file, > file, >> file)
//          The shell also handles EOF (Ctrl+D) for clean exit.
// ============================================================================

#include "Tokenizer.h"
#include "Pipeline.h"
#include <iostream>
#include <csignal>

int main()
{
    // Ignore SIGPIPE so piped commands don't kill the shell when a reader
    // exits early (e.g. "ls | head -n 1" — head closes before ls finishes)
    signal(SIGPIPE, SIG_IGN);

    std::string input;
    std::cout << ".........Welcome to ShellX........\n";

    while (true)
    {
        std::cout << "shellx> ";
        if (!std::getline(std::cin, input)) break;   // EOF (Ctrl+D)

        std::vector<std::string> tokens = tokenize(input);
        if (tokens.empty()) continue;

        if (tokens[0] == "exit") break;

        // Split tokens on "|" into pipeline stages and execute
        auto stages = splitPipeline(tokens);
        if (!stages.empty())
        {
            executePipeline(stages);
        }
    }

    std::cout << "\n";
    return 0;
}
