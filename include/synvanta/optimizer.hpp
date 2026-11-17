#pragma once

#include "synvanta/diagnostic.hpp"
#include "synvanta/rtl_ir.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace synvanta {

struct ModuleOptimizationStats {
    std::string module_name;
    std::size_t nodes_before{0};
    std::size_t nodes_after_cse{0};
    std::size_t nodes_after_dce{0};
    std::size_t cse_merged{0};
    std::size_t simplified_nodes{0};
    std::size_t dead_nodes{0};
    std::size_t assignments_before{0};
    std::size_t assignments_after{0};
};

struct OptimizationResult {
    RtlDesign design;
    std::vector<Diagnostic> diagnostics;
    std::vector<ModuleOptimizationStats> stats;

    [[nodiscard]] bool success() const noexcept { return !has_errors(diagnostics); }
};

class RtlOptimizer {
public:
    [[nodiscard]] OptimizationResult optimize(const RtlDesign& design) const;
};

} // namespace synvanta
