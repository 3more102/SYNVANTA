#pragma once

#include "synvanta/rtl_ir.hpp"

#include <cstddef>

namespace synvanta {

struct OptimizationStats {
    std::size_t nodes_before{0};
    std::size_t nodes_after{0};
    std::size_t common_subexpressions_removed{0};
    std::size_t dead_nodes_removed{0};
    std::size_t assignments_before{0};
    std::size_t assignments_after{0};
    std::size_t assignments_removed{0};
};

struct OptimizationResult {
    RtlDesign design;
    OptimizationStats stats;
};

class RtlOptimizer {
public:
    OptimizationResult optimize(const RtlDesign& design) const;
};

} // namespace synvanta
