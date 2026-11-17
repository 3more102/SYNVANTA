#pragma once
#include "synvanta/ast.hpp"
#include "synvanta/diagnostic.hpp"
#include "synvanta/rtl_ir.hpp"
#include <vector>
namespace synvanta {
struct LoweringResult {
    RtlDesign design;
    std::vector<Diagnostic> diagnostics;
    bool success() const noexcept { return !has_errors(diagnostics); }
};
class RtlLowerer {
public:
    LoweringResult lower(const CompilationUnit& unit) const;
};
} // namespace synvanta
