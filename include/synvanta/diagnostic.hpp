#pragma once
#include "synvanta/source.hpp"
#include <string>
#include <vector>
namespace synvanta {
enum class DiagnosticSeverity { Error, Warning, Note };
struct Diagnostic {
    DiagnosticSeverity severity{DiagnosticSeverity::Error};
    std::string code;
    std::string message;
    SourceLocation location;
};
inline bool has_errors(const std::vector<Diagnostic>& diagnostics) {
    for (const auto& diagnostic : diagnostics)
        if (diagnostic.severity == DiagnosticSeverity::Error) return true;
    return false;
}
} // namespace synvanta
