#include "condition_parser.hpp"

#include <cctype>

namespace scl2 {


DefaultTokenizer::Token DefaultTokenizer::nextToken()
{
    skipWhitespace();

    if(pos >= source.size()) {
        return { TokenType::EndOfInput, {} };
    }

    char current = source[pos];

    // handle parentheses
    if(current == '(') { pos++; return { TokenType::LeftParen,  "(" }; }
    if(current == ')') { pos++; return { TokenType::RightParen, ")" }; }

    // handle constants
    if(current == '{') {
        size_t startp = ++pos;
        while (pos < source.size() && source[pos] != '}') pos++;
        std::string literal = source.substr(startp, pos - startp);
        if (pos < source.size() && source[pos] == '}') {
            pos++; // skip "}"
            if (literal == "true" || literal == "false" || literal == "1" || literal == "0") {
                return { TokenType::BoolLiteral, literal };
            } else {
                return { TokenType::Error, source.substr(startp - 1, pos - startp + 1) }; // include the braces in error
            }
        }
    }

    // handle not or not-like operators
    if (current == '!') {
        pos++;
        if(pos < source.size()) {
            if(source[pos] == '&') { pos++; return { TokenType::OperatorNand, "!&" }; }
            if(source[pos] == '|') { pos++; return { TokenType::OperatorNor,  "!|" }; }
            if(source[pos] == '^') { pos++; return { TokenType::OperatorXnor, "!^" }; }
        }
        return { TokenType::OperatorNot, "!" }; // otherwise, just a single not operator
    }

    // handle normal operators
    if (current == '&') { pos++; return { TokenType::OperatorAnd, "&" }; }
    if (current == '|') { pos++; return { TokenType::OperatorOr,  "|" }; }
    if (current == '^') { pos++; return { TokenType::OperatorXor, "^" }; }

    // handle imply operators
    if (current == '-') {
        pos++;
        if(pos < source.size() && source[pos] == '>') {
            pos++;
            return { TokenType::OperatorImplyRight, "->" };
        }
        return { TokenType::Error, "-" };
    }

    if (current == '<') {
        pos++;
        if(pos < source.size() && source[pos] == '-') {
            pos++;
            return { TokenType::OperatorImplyLeft, "<-" };
        }
        return { TokenType::Error, "<" };
    }

    // Handle identifiers
    if(isAlphaNum(current)) {
        size_t startp = pos;
        while (pos < source.size() && isAlphaNum(source[pos])) pos++;
        return { TokenType::Identifier, source.substr(startp, pos - startp) };
    }

    return { TokenType::Error, source.substr(pos) };
}


DefaultTokenizer::Token NormalTokenizer::nextToken()
{
    skipWhitespace();

    if(pos >= source.size()) {
        return { TokenType::EndOfInput, {} };
    }

    char current = source[pos];

    // handle parentheses
    if(current == '(') { pos++; return { TokenType::LeftParen,  "(" }; }
    if(current == ')') { pos++; return { TokenType::RightParen, ")" }; }

    // handle not operator
    if (current == '\'') {
        pos++;
        return { TokenType::OperatorNot, "\'" };
    }

    // handle and/or/xor operators
    if (current == '&') { pos++; return { TokenType::OperatorAnd, "&" }; }
    if (current == '|') { pos++; return { TokenType::OperatorOr,  "|" }; }
    if (current == '^') { pos++; return { TokenType::OperatorXor, "^" }; }

    // Handle identifiers
    if(isAlphaNum(current)) {
        size_t startp = pos;
        while (pos < source.size() && isAlphaNum(source[pos])) pos++;
        std::string value = source.substr(startp, pos - startp);
        if(value == "true" || value == "false") {
            return { TokenType::BoolLiteral, value };
        }
        return { TokenType::Identifier, value };
    }

    return { TokenType::Error, source.substr(pos) };
}

void BaseTokenizer::skipWhitespace()
{
    while(pos < source.size() && std::isspace(static_cast<unsigned char>(source[pos]))) pos++;
}

bool BaseTokenizer::isAlphaNum(char c) const
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}


template<typename Tokenizer_Type>
ConditionParser<Tokenizer_Type>::ConditionParser(const std::string &input)
    : tokenizer(input)
{
    advance(); // pre-read the first token
}

template<typename Tokenizer_Type>
condition_node ConditionParser<Tokenizer_Type>::parse()
{
    return parseImply(); // start from the lowest precedence operator
}

template<typename Tokenizer_Type>
condition_node ConditionParser<Tokenizer_Type>::parsePrimary()
{
    if(currentToken.type == TokenType::OperatorNot) {
        advance();
        auto child = parsePrimary(); // recursively handle !(!A)
        return condition_node::create_unary(std::move(child), logical_operator::Not);
    }
    if(currentToken.type == TokenType::LeftParen) {
        advance();
        auto node = parseImply(); // parse the expression inside the parentheses
        if(currentToken.type != TokenType::RightParen) {
            throw std::runtime_error("Expected ')' after expression in parentheses");
        }
        advance(); // consume the right parenthesis
        return node;
    }
    if(currentToken.type == TokenType::Identifier) {
        std::string func_id(currentToken.value);
        advance();
        // Handle postfix NOT (e.g., "A'" in NormalTokenizer)
        if(currentToken.type == TokenType::OperatorNot) {
            advance();
            return condition_node::create_unary(condition_node::create_function(func_id), logical_operator::Not);
        }
        return condition_node::create_function(func_id);
    }
    if(currentToken.type == TokenType::BoolLiteral) {
        bool value = (currentToken.value == "true" || currentToken.value == "1");
        advance();
        return condition_node::create_bool(value);
    } else {
        throw std::runtime_error("Unexpected token: " + std::string(currentToken.value));
    }
}

template<typename Tokenizer_Type>
condition_node ConditionParser<Tokenizer_Type>::parseAnd()
{
    auto left = parsePrimary();
    while(currentToken.type == TokenType::OperatorAnd || currentToken.type == TokenType::OperatorNand) {
        auto op = currentToken;
        advance();
        auto right = parsePrimary();
        left = condition_node::create_logical(std::move(left), std::move(right), maptoLogicalOp(op.type));
    }
    return left;
}

template<typename Tokenizer_Type>
condition_node ConditionParser<Tokenizer_Type>::parseOrXor()
{
    auto left = parseAnd();
    while(isOrXor(currentToken.type)) {
        auto op = currentToken;
        advance();
        auto right = parseAnd();
        left = condition_node::create_logical(std::move(left), std::move(right), maptoLogicalOp(op.type));
    }
    return left;
}

template<typename Tokenizer_Type>
condition_node ConditionParser<Tokenizer_Type>::parseImply()
{
    auto left = parseOrXor();
    while(currentToken.type == BaseTokenizer::TokenType::OperatorImplyLeft || currentToken.type == BaseTokenizer::TokenType::OperatorImplyRight) {
        auto op = currentToken;
        advance();
        auto right = parseOrXor();
        left = condition_node::create_logical(std::move(left), std::move(right), maptoLogicalOp(op.type));
    }

    return left;
}

template<typename Tokenizer_Type>
logical_operator ConditionParser<Tokenizer_Type>::maptoLogicalOp(TokenType type) const
{
    switch(type) {
        case TokenType::OperatorNot: return logical_operator::Not;
        case TokenType::OperatorAnd: return logical_operator::And;
        case TokenType::OperatorOr: return logical_operator::Or;
        case TokenType::OperatorXor: return logical_operator::Xor;
        case TokenType::OperatorNand: return logical_operator::Nand;
        case TokenType::OperatorNor: return logical_operator::Nor;
        case TokenType::OperatorXnor: return logical_operator::Xnor;
        case TokenType::OperatorImplyLeft: return logical_operator::Imply_Right;  // <- means right implies left
        case TokenType::OperatorImplyRight: return logical_operator::Imply_Left;  // -> means left implies right
        default:
            throw std::runtime_error("Invalid token type for logical operator");
    }
}

template<typename Tokenizer_Type>
void ConditionParser<Tokenizer_Type>::advance()
{
    currentToken = tokenizer.nextToken();
}


// ─── Explicit template instantiations ───────────────────────────────────
template class ConditionParser<DefaultTokenizer>;
template class ConditionParser<NormalTokenizer>;

} // namespace scl2