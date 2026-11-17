#pragma once
#include "synvanta/ast.hpp"
#include "synvanta/diagnostic.hpp"
#include "synvanta/token.hpp"
#include <optional>
#include <string>
#include <vector>
namespace synvanta {
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    CompilationUnit parse();
    const std::vector<Diagnostic>& diagnostics() const noexcept { return diagnostics_; }
private:
    std::vector<Token> tokens_;
    std::size_t current_{0};
    std::vector<Diagnostic> diagnostics_;
    bool at_end() const;
    const Token& peek() const;
    const Token& previous() const;
    const Token& advance();
    bool check(TokenKind kind) const;
    bool match(TokenKind kind);
    bool expect(TokenKind kind, const std::string& message);
    void report(std::string code, std::string message, const SourceLocation& where);
    void synchronize_module_item();
    void synchronize_to_module();
    std::optional<Module> parse_module();
    std::optional<Port> parse_port();
    std::optional<PackedRange> parse_range();
    bool parse_net_declaration(Module& module);
    std::optional<ContinuousAssignment> parse_assignment();
    std::unique_ptr<Expr> parse_expression(int min_precedence = 1);
    std::unique_ptr<Expr> parse_prefix();
    int precedence(TokenKind kind) const;
    std::optional<int> parse_integer_literal(const Token& token) const;
};
} // namespace synvanta
