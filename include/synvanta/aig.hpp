#pragma once

#include "synvanta/diagnostic.hpp"
#include "synvanta/rtl_ir.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace synvanta {

using AigLit = std::uint32_t;
inline constexpr AigLit aig_false = 0;
inline constexpr AigLit aig_true = 1;

enum class AigNodeKind { Constant, Input, And };

struct AigNode {
    AigNodeKind kind{AigNodeKind::Constant};
    AigLit lhs{0};
    AigLit rhs{0};
    std::string input_name;
    std::uint32_t input_bit{0};
};

struct AigAssignment;

class AigNetwork {
public:
    AigNetwork();

    static constexpr AigLit invert(AigLit lit) noexcept { return lit ^ 1U; }
    AigLit input(const std::string& name, std::uint32_t bit);
    AigLit make_and(AigLit lhs, AigLit rhs);
    AigLit make_or(AigLit lhs, AigLit rhs);
    AigLit make_xor(AigLit lhs, AigLit rhs);
    AigLit make_xnor(AigLit lhs, AigLit rhs);
    AigLit make_mux(AigLit select, AigLit when_true, AigLit when_false);

    const std::vector<AigNode>& nodes() const noexcept { return nodes_; }
    std::size_t input_count() const noexcept { return input_index_.size(); }
    std::size_t and_count() const noexcept;
    void compact(std::vector<AigAssignment>& assignments);

private:
    std::vector<AigNode> nodes_;
    std::map<std::pair<std::string, std::uint32_t>, AigLit> input_index_;
    std::map<std::pair<AigLit, AigLit>, AigLit> and_hash_;
};

struct AigAssignment {
    std::string target;
    std::vector<AigLit> bits;
    std::uint32_t target_width{1};
    bool target_is_signed{false};
};

struct AigModule {
    std::string name;
    std::vector<RtlPort> ports;
    AigNetwork network;
    std::vector<AigAssignment> assignments;
};

struct AigDesign {
    std::vector<AigModule> modules;
};

struct AigLoweringResult {
    AigDesign design;
    std::vector<Diagnostic> diagnostics;
    bool success() const noexcept { return !has_errors(diagnostics); }
};

class AigLowerer {
public:
    AigLoweringResult lower(const RtlDesign& design) const;
};

std::string dump_aig_design(const AigDesign& design);

} // namespace synvanta
