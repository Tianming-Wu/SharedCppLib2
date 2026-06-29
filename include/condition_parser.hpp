/*
    Condition parser module for SharedCppLib2.


*/

#pragma once

#include <generator>

#include "condition.hpp"

namespace scl2 {


// Tokenizer implementation for condition_expression parsing logic.
// This is not a separate module since it is too specific, and will
// be too costly to generalize.
class BaseTokenizer {
public:
    enum class TokenType {
        EndOfInput,
        LeftParen, RightParen, // ( )
        OperatorNot, // !
        OperatorAnd, OperatorOr, OperatorXor, // & | ^
        OperatorNand, OperatorNor, OperatorXnor, // !& !| !^
        OperatorImplyLeft, OperatorImplyRight, // -> <-
        Identifier, // [A-Za-z_0-9]+
        BoolLiteral, // {true} {false}
        Error
    };

    struct Token {
        TokenType type;
        std::string value;
    };

    explicit BaseTokenizer(const std::string& input) : source(input), pos(0) {}

    virtual Token nextToken() = 0; // pure virtual

    inline std::generator<Token> tokenize() {
        Token token;
        while ((token = nextToken()).type != TokenType::EndOfInput) {
            co_yield token;
        }
    }

protected:
    void skipWhitespace();
    bool isAlphaNum(char c) const;

    std::string source;
    size_t pos;
};


class DefaultTokenizer : public BaseTokenizer {
public:
    using BaseTokenizer::BaseTokenizer;

    Token nextToken() override;
};


class NormalTokenizer : public BaseTokenizer {
public:
    using BaseTokenizer::BaseTokenizer;

    Token nextToken() override;
};

// The second layer of the parser.
template<typename Tokenizer_Type>
class ConditionParser {
public:
    using TokenType = BaseTokenizer::TokenType;
    using Token = BaseTokenizer::Token;
    using Tokenizer = Tokenizer_Type;

    explicit ConditionParser(const std::string& input);

    condition_node parse();

private:
    // Atomic / Parenthesized expressions -> NOT -> AND/NAND -> /OR/XOR/NOR/NXOR -> IMPLY
    condition_node parsePrimary(); // parse basic units: identifiers, literals, parenthesized expressions
    // Not is also parsed in parsePrimary.
    condition_node parseAnd(); // parse AND/NAND operators
    condition_node parseOrXor(); // parse OR, XOR, NOR, NXOR operators
    condition_node parseImply(); // parse implication operators, which have the lowest precedence

    // helper functions
    logical_operator maptoLogicalOp(TokenType type) const;

    inline bool isOrXor(TokenType type) const {
        return type == TokenType::OperatorOr || type == TokenType::OperatorXor ||
               type == TokenType::OperatorNor || type == TokenType::OperatorXnor;
    }

    void advance();

private:
    Tokenizer tokenizer;
    Token currentToken;
};


} // namespace scl2