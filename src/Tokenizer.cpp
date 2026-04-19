// ============================================================================
// File:    Tokenizer.cpp
// Summary: State-machine tokenizer implementation. Walks the input string
//          character by character, tracking NORMAL / IN_SINGLE_QUOTE /
//          IN_DOUBLE_QUOTE / ESCAPED states. Produces a clean token vector
//          that handles quoted filenames, escape sequences, and whitespace.
// ============================================================================

#include "Tokenizer.h"
#include <iostream>
#include <cctype>

std::vector<std::string> tokenize(const std::string &input)
{
    std::vector<std::string> tokens;
    std::string current;
    TokenState state = TokenState::NORMAL;
    TokenState prev_state = TokenState::NORMAL;

    for (size_t i = 0; i < input.size(); ++i)
    {
        char c = input[i];

        if (c == '\r') continue;

        switch (state)
        {
        case TokenState::NORMAL:
            if (std::isspace(static_cast<unsigned char>(c)))
            {
                if (!current.empty())
                {
                    tokens.push_back(current);
                    current.clear();
                }
            }
            else if (c == '\'')
            {
                state = TokenState::IN_SINGLE_QUOTE;
            }
            else if (c == '"')
            {
                state = TokenState::IN_DOUBLE_QUOTE;
            }
            else if (c == '\\')
            {
                prev_state = TokenState::NORMAL;
                state = TokenState::ESCAPED;
            }
            else
            {
                current += c;
            }
            break;

        case TokenState::IN_SINGLE_QUOTE:
            if (c == '\'')
                state = TokenState::NORMAL;
            else
                current += c;
            break;

        case TokenState::IN_DOUBLE_QUOTE:
            if (c == '"')
                state = TokenState::NORMAL;
            else if (c == '\\')
            {
                prev_state = TokenState::IN_DOUBLE_QUOTE;
                state = TokenState::ESCAPED;
            }
            else
                current += c;
            break;

        case TokenState::ESCAPED:
            current += c;
            state = prev_state;
            break;
        }
    }

    if (state == TokenState::IN_SINGLE_QUOTE || state == TokenState::IN_DOUBLE_QUOTE)
    {
        std::cerr << "Error: unterminated quote in input\n";
        return {};
    }

    if (!current.empty())
    {
        tokens.push_back(current);
    }

    return tokens;
}
