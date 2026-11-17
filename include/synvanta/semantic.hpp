#pragma once

#include "synvanta/ast.hpp"
#include "synvanta/diagnostic.hpp"

#include <vector>

namespace synvanta {

class SemanticAnalyzer {
public:
    std::vector<Diagnostic> analyze(const CompilationUnit& unit) const;
};

} // namespace synvanta
