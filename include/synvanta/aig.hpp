#pragma once

#include "synvanta/diagnostic.hpp"
#include "synvanta/rtl_ir.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace synvanta {

using AigLiteral = std::uint32_t;

inline constexpr AigLiteral aig_false = 0U;
inline constexpr AigLiteral aig_true = 1U;
inline constexpr AigLiteral aig_invert(AigLiteral literal) noexcept {
    return literal ^ 1U;
}

enum class AigNodeKind {
    ConstantFalse,
    Input,
    And
};

struct AigNode {
    std::uint32_t id{0};
    AigNodeKind kind{AigNodeKind::ConstantFalse};
    AigLiteral lhs{aig_false};
    AigLiteral rhs{aig_false};
    std::string name;
};

struct AigPortBit {
    std::string port_name;
    std::uint32_t bit_index{0};
    AigLiteral literal{aig_false};
};

struct AigModule {
    std::string name;
    // Node zero is the constant-false node. Constant true is its complemented literal.
    std::vector<AigNode> nodes;
    std::vector<AigPortBit> inputs;
    std::vector<AigPortBit> outputs;
};

struct AigDesign {
    std::vector<AigModule> modules;
};

struct AigBuildResult {
    AigDesign design;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool success() const noexcept {
        return !has_errors(diagnostics);
    }
};

class AigBuilder {
public:
    [[nodiscard]] AigBuildResult build(const RtlDesign& design) const;
};

[[nodiscard]] const char* aig_node_kind_name(AigNodeKind kind);
[[nodiscard]] std::string dump_aig_design(const AigDesign& design);

} // namespace synvanta
