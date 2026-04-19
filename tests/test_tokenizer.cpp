// ============================================================================
// File:    test_tokenizer.cpp
// Summary: Unit tests for the state-machine tokenizer.
//          Tests: basic splitting, quoted strings, escape sequences,
//          consecutive spaces, empty input, and unterminated quotes.
//          Returns 0 if all pass, 1 on first failure.
// ============================================================================

#include "Tokenizer.h"
#include <iostream>
#include <cassert>

static int tests_run = 0;
static int tests_passed = 0;

void check(const std::string &name,
           const std::string &input,
           const std::vector<std::string> &expected)
{
    tests_run++;
    std::vector<std::string> result = tokenize(input);
    if (result == expected)
    {
        std::cout << "  PASS: " << name << "\n";
        tests_passed++;
    }
    else
    {
        std::cout << "  FAIL: " << name << "\n";
        std::cout << "    Input:    \"" << input << "\"\n";
        std::cout << "    Expected: [";
        for (size_t i = 0; i < expected.size(); ++i)
        {
            if (i > 0) std::cout << ", ";
            std::cout << "\"" << expected[i] << "\"";
        }
        std::cout << "]\n";
        std::cout << "    Got:      [";
        for (size_t i = 0; i < result.size(); ++i)
        {
            if (i > 0) std::cout << ", ";
            std::cout << "\"" << result[i] << "\"";
        }
        std::cout << "]\n";
    }
}

int main()
{
    std::cout << "Running tokenizer tests...\n\n";

    // Basic splitting
    check("simple command",
          "ls -la",
          {"ls", "-la"});

    check("three arguments",
          "cp src.txt dst.txt",
          {"cp", "src.txt", "dst.txt"});

    // Quoted strings
    check("double-quoted path",
          "cd \"My Documents\"",
          {"cd", "My Documents"});

    check("single-quoted path",
          "cd 'My Documents'",
          {"cd", "My Documents"});

    check("quotes preserve internal spaces",
          "echo \"hi   there\"",
          {"echo", "hi   there"});

    // Backslash escapes
    check("backslash-escaped space",
          "cp a\\ b.txt c\\ d.txt",
          {"cp", "a b.txt", "c d.txt"});

    check("backslash inside double quotes",
          "echo \"hello \\\"world\\\"\"",
          {"echo", "hello \"world\""});

    // Whitespace handling
    check("multiple spaces between args",
          "ls    -la",
          {"ls", "-la"});

    check("leading and trailing spaces",
          "   ls   -la   ",
          {"ls", "-la"});

    check("tabs as whitespace",
          "ls\t-la",
          {"ls", "-la"});

    // Edge cases
    check("empty input",
          "",
          {});

    check("only spaces",
          "     ",
          {});

    check("single command, no args",
          "exit",
          {"exit"});

    // Unterminated quotes should return empty
    check("unterminated double quote",
          "ls \"unterminated",
          {});

    check("unterminated single quote",
          "ls 'unterminated",
          {});

    // Summary
    std::cout << "\n" << tests_passed << "/" << tests_run << " tests passed.\n";

    return (tests_passed == tests_run) ? 0 : 1;
}
