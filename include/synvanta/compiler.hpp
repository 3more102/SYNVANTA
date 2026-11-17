#pragma once
#include "synvanta/ast.hpp"
#include "synvanta/diagnostic.hpp"
#include <string>
#include <string_view>
#include <vector>
namespace synvanta {
struct CompilationResult {
    CompilationUnit unit;
    std::vector<Diagnostic> diagnostics;
    bool success() const noexcept { return !has_errors(diagnostics); }
};
class Compiler {
public:
    CompilationResult compile(std::string_view source, std::string filename = "<memory>") const;
};
std::string format_diagnostic(const Diagnostic& diagnostic);
} // namespace synvanta
