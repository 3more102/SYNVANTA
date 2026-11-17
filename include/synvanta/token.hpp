#pragma once
#include "synvanta/source.hpp"
#include <string>
namespace synvanta {
enum class TokenKind {
    EndOfFile, Identifier, Number, Module, Endmodule, Input, Output, Inout,
    Wire, Logic, Reg, Bit, Signed, Unsigned, Assign, Parameter, Localparam,
    LParen, RParen, LBracket, RBracket, LBrace, RBrace, Semicolon, Colon,
    Comma, Dot, Question, Hash, Equal, Plus, Minus, Star, Slash, Percent,
    Ampersand, Pipe, Caret, Tilde, Bang, Less, Greater, LessEqual, GreaterEqual,
    EqualEqual, NotEqual, LogicalAnd, LogicalOr, ShiftLeft, ShiftRight,
    Xnor, Nand, Nor, Unknown
};
struct Token {
    TokenKind kind{TokenKind::Unknown};
    std::string lexeme;
    SourceLocation location;
};
const char* token_kind_name(TokenKind kind);
} // namespace synvanta
