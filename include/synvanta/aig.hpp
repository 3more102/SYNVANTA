#pragma once

#include "synvanta/diagnostic.hpp"
#include "synvanta/rtl_ir.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace synvanta {

// AIG literals encode the node index in the upper bits and edge inversion in bit 0.
// Literal 0 is constant false; literal 1 is constant true.
using AigLiteral = std::uint32_t;
inline constexpr AigLiteral AigFalse = 0;
inline constexpr AigLiteral AigTrue = 1;

constexpr AigLiteral aig_invert(AigLiteral literal) noexcept {
    return literal ^ 1U;
}

constexpr std::uint32_t aig_node_id(AigLiteral literal) noexcept {
    return literal >> 1U;
}

enum class AigNodeKind : std::uint8_t {
    ConstantFalse,
    Input,
    And
};

struct AigNode {
    std::uint32_t id{0};
    AigNodeKind kind{AigNodeKind::ConstantFalse};
    AigLiteral lhs{AigFalse};
    AigLiteral rhs{AigFalse};
    std::string input_name;
    std::uint32_t input_bit{0};
    std::uint32_t input_index{0};
    SourceLocation location;
};

struct AigInput {
    std::string port_name;
    std::uint32_t bit_index{0};
    std::uint32_t node_id{0};
};

struct AigOutput {
    std::string port_name;
    std::uint32_t bit_index{0};
    AigLiteral driver{AigFalse};
};

struct AigModule {
    std::string name;
    // Node zero is always the constant-false node. AND nodes are topologically ordered.
    std::vector<AigNode> nodes;
    std::vector<AigInput> inputs;
    std::vector<AigOutput> outputs;
};

struct AigDesign {
    std::vector<AigModule> modules;
};

struct AigBuildResult {
    AigDesign design;
    std::vector<Diagnostic> diagnostics;

    bool success() const noexcept {
        return !has_errors(diagnostics);
    }
};

class AigBuilder {
public:
    // Builds a two-state And-Inverter Graph from the supported combinational RTL IR.
    // Unsupported operators and unknown (X/Z/?) constants are reported, never guessed.
    AigBuildResult build(const RtlDesign& design) const;
};

struct AigOptimizationStats {
    std::size_t nodes_before{0};
    std::size_t nodes_after{0};
    std::size_t and_nodes_before{0};
    std::size_t and_nodes_after{0};
    std::uint32_t depth_before{0};
    std::uint32_t depth_after{0};
};

struct AigOptimizationResult {
    AigDesign design;
    AigOptimizationStats stats;
    std::vector<Diagnostic> diagnostics;

    bool success() const noexcept {
        return !has_errors(diagnostics);
    }
};

class AigOptimizer {
public:
    // Rebuilds the AIG with structurally hashed, balanced positive-AND trees and
    // removes nodes that are not reachable from primary outputs.
    AigOptimizationResult optimize(const AigDesign& design) const;
};

std::string dump_aig_design(const AigDesign& design);

} // namespace synvanta
