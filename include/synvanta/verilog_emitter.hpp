#pragma once
#include "synvanta/aig.hpp"
#include <string>
#include <vector>

namespace synvanta {
struct VerilogEmitResult {
    std::string verilog;
    std::vector<Diagnostic> diagnostics;
    bool success() const noexcept { return !has_errors(diagnostics); }
};
class GenericVerilogEmitter {
public:
    // Two-state generic gate netlist, not a standard-cell-mapped design.
    VerilogEmitResult emit(const AigDesign& design) const;
};
} // namespace synvanta
