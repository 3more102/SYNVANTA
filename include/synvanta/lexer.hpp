#pragma once
#include "synvanta/diagnostic.hpp"
#include "synvanta/token.hpp"
#include <string>
#include <string_view>
#include <vector>
namespace synvanta {
class Lexer {
public:
    Lexer(std::string_view source, std::string filename);
    std::vector<Token> tokenize();
    const std::vector<Diagnostic>& diagnostics() const noexcept { return diagnostics_; }
private:
    std::string_view source_;
    std::string filename_;
    std::size_t offset_{0}, line_{1}, column_{1};
    std::vector<Diagnostic> diagnostics_;
    bool at_end(std::size_t lookahead = 0) const;
    char peek(std::size_t lookahead = 0) const;
    char advance();
    bool match(char expected);
    SourceLocation location() const;
    void skip_trivia();
    Token scan_token();
    Token scan_number();
    Token make_token(TokenKind kind, std::size_t start, SourceLocation start_location) const;
    void report(std::string code, std::string message, SourceLocation where);
};
} // namespace synvanta
