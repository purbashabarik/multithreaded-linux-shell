// ============================================================================
// File:    main.cpp
// Summary: Entry point for the Embedded Diagnostic Shell (EDS).
//          Runs a REPL (Read-Eval-Print Loop): reads user input, tokenizes
//          it with the state-machine tokenizer, dispatches to the right
//          Command subclass via CommandFactory, and executes it.
//          This file is intentionally small — all logic lives in the
//          individual command classes and the tokenizer.
// ============================================================================

#include "Tokenizer.h"
#include "CommandFactory.h"
#include <iostream>

int main()
{
    std::string input;
    std::cout << ".........Welcome to New Shell........\n";

    while (true)
    {
        std::cout << ">>";
        std::cout << "Enter command: ";
        std::getline(std::cin, input);

        std::vector<std::string> tokens = tokenize(input);
        if (tokens.empty()) continue;

        if (tokens[0] == "exit") break;

        auto command = CommandFactory::create(tokens[0]);
        if (!command)
        {
            std::cout << "Invalid command\n";
            continue;
        }

        command->execute(tokens);
    }

    return 0;
}
