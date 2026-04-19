// ============================================================================
// File:    Tokenizer.h
// Summary: State-machine tokenizer that splits user input into tokens.
//          Handles double quotes ("My Documents"), single quotes ('hello'),
//          backslash escapes (My\ File.txt), and collapses consecutive
//          whitespace. Returns an empty vector on unterminated quotes.
//          This replaces the naive input.find(' ') splitter that broke
//          on filenames with spaces.
// ============================================================================

#ifndef EDS_TOKENIZER_H
#define EDS_TOKENIZER_H

#include <vector>
#include <string>

enum class TokenState { NORMAL, IN_SINGLE_QUOTE, IN_DOUBLE_QUOTE, ESCAPED };

std::vector<std::string> tokenize(const std::string &input);

#endif // EDS_TOKENIZER_H
