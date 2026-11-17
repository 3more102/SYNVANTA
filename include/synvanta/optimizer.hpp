#pragma once
#include "synvanta/diagnostic.hpp"
#include "synvanta/rtl_ir.hpp"
#include <cstddef>
#include <vector>
namespace synvanta {
struct OptimizationStats {
    std::size_t nodes_before{0};
    std::size_t nodes_after{0};
    std::size_t constants_folded{0};
    std::size_t common_subexpressions_removed{0};
    std::size_t dead_nodes_removed{0};
};
struct OptimizationResult {
    RtlDesign design;
    OptimizationStats stats;
    std::vector<Diagnostic> diagnostics;
    bool success() const noexcept { return !has_errors(diagnostics); }
};
class RtlOptimizer {
public:
    OptimizationResult optimize(const RtlDesign& design) const;
};
} // namespace synvanta
