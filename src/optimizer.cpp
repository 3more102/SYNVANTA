#include "synvanta/optimizer.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace synvanta {
namespace {

std::uint64_t mask_for(std::uint32_t width) {
    if (width >= 64) return std::numeric_limits<std::uint64_t>::max();
    if (width == 0) return 0;
    return (std::uint64_t{1} << width) - 1;
}

std::optional<std::uint64_t> parse_value(const RtlNode& node) {
    if (node.width > 64) return std::nullopt;

    std::string value;
    value.reserve(node.literal.size());
    for (const char ch : node.literal) {
        if (ch != '_') value.push_back(ch);
    }

    const auto apostrophe = value.find(static_cast<char>(39));
    if (apostrophe == std::string::npos) {
        std::uint64_t parsed = 0;
        const auto result = std::from_chars(
            value.data(), value.data() + value.size(), parsed, 10);
        if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
            return std::nullopt;
        return parsed & mask_for(node.width);
    }

    // Unbased unsized fill literals are rejected by RTL lowering because their
    // value depends on the surrounding expression context.
    if (apostrophe == 0) return std::nullopt;

    std::size_t position = apostrophe + 1;
    if (position < value.size() &&
        (value[position] == 's' || value[position] == 'S')) {
        ++position;
    }
    if (position >= value.size()) return std::nullopt;

    const char base_character = value[position++];
    int base = 10;
    if (base_character == 'b' || base_character == 'B') base = 2;
    else if (base_character == 'o' || base_character == 'O') base = 8;
    else if (base_character == 'h' || base_character == 'H') base = 16;
    else if (base_character != 'd' && base_character != 'D') return std::nullopt;

    std::uint64_t parsed = 0;
    const auto result = std::from_chars(
        value.data() + position, value.data() + value.size(), parsed, base);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
        return std::nullopt;
    return parsed & mask_for(node.width);
}

std::int64_t signed_value(std::uint64_t value, std::uint32_t width) {
    const auto mask = mask_for(width);
    value &= mask;
    if (width > 0 && width < 64 &&
        (value & (std::uint64_t{1} << (width - 1))) != 0) {
        value |= ~mask;
    }
    return std::bit_cast<std::int64_t>(value);
}

std::string format_constant(std::uint32_t width, std::uint64_t value) {
    std::ostringstream output;
    output << width << "'h" << std::hex << std::uppercase
           << (value & mask_for(width));
    return output.str();
}

std::optional<std::uint64_t> evaluate(
    const RtlNode& node,
    const std::vector<RtlNode>& nodes,
    const std::vector<std::optional<std::uint64_t>>& values) {
    if (node.width > 64) return std::nullopt;
    if (node.op == RtlOp::Constant) return parse_value(node);
    if (node.op == RtlOp::Input) return std::nullopt;

    std::vector<std::uint64_t> operands;
    operands.reserve(node.operands.size());
    for (const auto id : node.operands) {
        if (static_cast<std::size_t>(id) >= values.size() || !values[id])
            return std::nullopt;
        operands.push_back(*values[id]);
    }

    const auto result_mask = mask_for(node.width);
    if (node.op == RtlOp::Resize && operands.size() == 1) {
        const auto source_id = node.operands[0];
        if (static_cast<std::size_t>(source_id) >= nodes.size() ||
            nodes[source_id].width > 64) {
            return std::nullopt;
        }
        const auto& source = nodes[source_id];
        auto value = operands[0] & mask_for(source.width);
        if (node.width > source.width && source.is_signed &&
            source.width > 0 && source.width < 64 &&
            (value & (std::uint64_t{1} << (source.width - 1))) != 0) {
            value |= ~mask_for(source.width);
        }
        return value & result_mask;
    }

    if (operands.size() == 1) {
        const auto source_id = node.operands[0];
        if (static_cast<std::size_t>(source_id) >= nodes.size()) return std::nullopt;
        const auto value = operands[0] & mask_for(nodes[source_id].width);
        switch (node.op) {
        case RtlOp::UnaryPlus: return value & result_mask;
        case RtlOp::Negate: return (std::uint64_t{0} - value) & result_mask;
        case RtlOp::BitNot: return (~value) & result_mask;
        case RtlOp::LogicalNot: return value == 0 ? 1 : 0;
        case RtlOp::ReduceNand:
            return value == mask_for(nodes[source_id].width) ? 0 : 1;
        case RtlOp::ReduceNor: return value == 0 ? 1 : 0;
        case RtlOp::ReduceXnor:
            return (static_cast<unsigned>(std::popcount(value)) & 1U) == 0U ? 1 : 0;
        default: return std::nullopt;
        }
    }

    if (operands.size() == 2) {
        const auto left_id = node.operands[0];
        const auto right_id = node.operands[1];
        if (static_cast<std::size_t>(left_id) >= nodes.size() ||
            static_cast<std::size_t>(right_id) >= nodes.size()) {
            return std::nullopt;
        }

        const auto left_width = nodes[left_id].width;
        const auto right_width = nodes[right_id].width;
        const auto left = operands[0] & mask_for(left_width);
        const auto right = operands[1] & mask_for(right_width);
        const bool is_signed = nodes[left_id].is_signed && nodes[right_id].is_signed;

        switch (node.op) {
        case RtlOp::Add: return (left + right) & result_mask;
        case RtlOp::Subtract: return (left - right) & result_mask;
        case RtlOp::Multiply: return (left * right) & result_mask;
        case RtlOp::Divide:
            if (right == 0) return std::nullopt;
            if (is_signed) {
                const auto lhs = signed_value(left, left_width);
                const auto rhs = signed_value(right, right_width);
                if (lhs == std::numeric_limits<std::int64_t>::min() && rhs == -1)
                    return std::nullopt;
                return static_cast<std::uint64_t>(lhs / rhs) & result_mask;
            }
            return (left / right) & result_mask;
        case RtlOp::Modulo:
            if (right == 0) return std::nullopt;
            if (is_signed) {
                const auto lhs = signed_value(left, left_width);
                const auto rhs = signed_value(right, right_width);
                if (lhs == std::numeric_limits<std::int64_t>::min() && rhs == -1)
                    return std::nullopt;
                return static_cast<std::uint64_t>(lhs % rhs) & result_mask;
            }
            return (left % right) & result_mask;
        case RtlOp::BitAnd: return (left & right) & result_mask;
        case RtlOp::BitOr: return (left | right) & result_mask;
        case RtlOp::BitXor: return (left ^ right) & result_mask;
        case RtlOp::BitXnor: return (~(left ^ right)) & result_mask;
        case RtlOp::LogicalAnd: return left != 0 && right != 0 ? 1 : 0;
        case RtlOp::LogicalOr: return left != 0 || right != 0 ? 1 : 0;
        case RtlOp::Equal: return left == right ? 1 : 0;
        case RtlOp::NotEqual: return left != right ? 1 : 0;
        case RtlOp::Less:
            return is_signed
                ? (signed_value(left, left_width) < signed_value(right, right_width) ? 1 : 0)
                : (left < right ? 1 : 0);
        case RtlOp::LessEqual:
            return is_signed
                ? (signed_value(left, left_width) <= signed_value(right, right_width) ? 1 : 0)
                : (left <= right ? 1 : 0);
        case RtlOp::Greater:
            return is_signed
                ? (signed_value(left, left_width) > signed_value(right, right_width) ? 1 : 0)
                : (left > right ? 1 : 0);
        case RtlOp::GreaterEqual:
            return is_signed
                ? (signed_value(left, left_width) >= signed_value(right, right_width) ? 1 : 0)
                : (left >= right ? 1 : 0);
        case RtlOp::ShiftLeft:
        case RtlOp::ShiftRight:
            // A negative signed shift amount produces an unknown result in
            // SystemVerilog. Leave it untouched rather than folding it to zero.
            if (nodes[right_id].is_signed &&
                signed_value(right, right_width) < 0) {
                return std::nullopt;
            }
            if (right >= node.width) return std::uint64_t{0};
            if (node.op == RtlOp::ShiftLeft)
                return (left << right) & result_mask;
            return (left >> right) & result_mask;
        default: return std::nullopt;
        }
    }

    if (node.op == RtlOp::Mux && operands.size() == 3)
        return (operands[0] != 0 ? operands[1] : operands[2]) & result_mask;

    return std::nullopt;
}

bool is_commutative(RtlOp op) {
    switch (op) {
    case RtlOp::Add:
    case RtlOp::Multiply:
    case RtlOp::BitAnd:
    case RtlOp::BitOr:
    case RtlOp::BitXor:
    case RtlOp::BitXnor:
    case RtlOp::LogicalAnd:
    case RtlOp::LogicalOr:
    case RtlOp::Equal:
    case RtlOp::NotEqual:
        return true;
    default:
        return false;
    }
}

std::string key_for(const RtlNode& node) {
    std::ostringstream key;
    key << static_cast<unsigned>(node.op) << ':' << node.width << ':'
        << node.is_signed << ':' << node.name.size() << ':' << node.name
        << ':' << node.literal.size() << ':' << node.literal << ':';
    if (node.op == RtlOp::Input && node.name.empty())
        key << "anonymous-input@" << node.id << ':';
    for (const auto operand : node.operands) key << operand << ',';
    return key.str();
}

void add_error(
    std::vector<Diagnostic>& diagnostics,
    const std::string& code,
    const std::string& message,
    const SourceLocation& location) {
    diagnostics.push_back(
        Diagnostic{DiagnosticSeverity::Error, code, message, location});
}

bool optimize_module(
    RtlModule& module,
    OptimizationStats& stats,
    std::vector<Diagnostic>& diagnostics) {
    stats.nodes_before += module.nodes.size();
    stats.assignments_before += module.assignments.size();

    const auto invalid_id = std::numeric_limits<ValueId>::max();
    for (std::size_t index = 0; index < module.nodes.size(); ++index) {
        const auto& node = module.nodes[index];
        if (static_cast<std::size_t>(node.id) != index) {
            add_error(diagnostics, "OPT0001",
                      "RTL node IDs must match their module-local vector indices",
                      node.location);
            return false;
        }
        if (node.width == 0) {
            add_error(diagnostics, "OPT0008", "RTL node width must be positive",
                      node.location);
            return false;
        }
        for (const auto operand : node.operands) {
            if (static_cast<std::size_t>(operand) >= index) {
                add_error(diagnostics, "OPT0002",
                          "RTL node operand must refer to an earlier node",
                          node.location);
                return false;
            }
        }
    }

    for (const auto& assignment : module.assignments) {
        if (static_cast<std::size_t>(assignment.value) >= module.nodes.size()) {
            add_error(diagnostics, "OPT0005",
                      "RTL assignment references an invalid value ID", {});
            return false;
        }
    }

    std::vector<std::optional<std::uint64_t>> constants(module.nodes.size());
    std::vector<ValueId> aliases(module.nodes.size());
    std::iota(aliases.begin(), aliases.end(), ValueId{0});

    for (auto& node : module.nodes) {
        for (auto& operand : node.operands)
            operand = aliases[operand];

        if (node.op == RtlOp::Mux && node.operands.size() == 3) {
            const auto condition = node.operands[0];
            if (constants[condition].has_value()) {
                const auto selected =
                    node.operands[*constants[condition] != 0 ? 1 : 2];
                aliases[node.id] = aliases[selected];
                constants[node.id] = constants[aliases[node.id]];
                ++stats.muxes_simplified;
                continue;
            }
        }

        const auto value = evaluate(node, module.nodes, constants);
        if (!value) continue;
        if (node.op != RtlOp::Constant) ++stats.constants_folded;
        node.op = RtlOp::Constant;
        node.operands.clear();
        node.name.clear();
        node.literal = format_constant(node.width, *value);
        constants[node.id] = *value;
    }

    std::vector<RtlNode> unique_nodes;
    unique_nodes.reserve(module.nodes.size());
    std::vector<ValueId> canonical(module.nodes.size(), invalid_id);
    std::unordered_map<std::string, ValueId> seen;
    seen.reserve(module.nodes.size());

    for (std::size_t index = 0; index < module.nodes.size(); ++index) {
        if (aliases[index] != index) {
            if (static_cast<std::size_t>(aliases[index]) >= index ||
                canonical[aliases[index]] == invalid_id) {
                add_error(diagnostics, "OPT0003",
                          "mux simplification produced an invalid node alias",
                          module.nodes[index].location);
                return false;
            }
            canonical[index] = canonical[aliases[index]];
            continue;
        }

        RtlNode node = module.nodes[index];
        for (auto& operand : node.operands) {
            if (static_cast<std::size_t>(operand) >= index ||
                canonical[operand] == invalid_id) {
                add_error(diagnostics, "OPT0003",
                          "RTL node references an invalid or unprocessed operand",
                          module.nodes[index].location);
                return false;
            }
            operand = canonical[operand];
        }

        bool simplified = false;
        if (node.operands.size() == 1 && node.op == RtlOp::UnaryPlus) {
            canonical[index] = node.operands[0];
            simplified = true;
        } else if (node.operands.size() == 1 && node.op == RtlOp::Resize) {
            const auto& source = unique_nodes[node.operands[0]];
            if (source.width == node.width &&
                source.is_signed == node.is_signed) {
                canonical[index] = node.operands[0];
                simplified = true;
            }
        } else if (node.op == RtlOp::Mux && node.operands.size() == 3 &&
                   node.operands[1] == node.operands[2]) {
            canonical[index] = node.operands[1];
            simplified = true;
        }

        if (simplified) {
            ++stats.local_simplifications;
            continue;
        }

        if (is_commutative(node.op) && node.operands.size() == 2 &&
            node.operands[1] < node.operands[0]) {
            std::swap(node.operands[0], node.operands[1]);
        }

        const auto key = key_for(node);
        const auto found = seen.find(key);
        if (found != seen.end()) {
            canonical[index] = found->second;
            ++stats.common_subexpressions_removed;
            continue;
        }

        node.id = static_cast<ValueId>(unique_nodes.size());
        canonical[index] = node.id;
        seen.emplace(key, node.id);
        unique_nodes.push_back(std::move(node));
    }

    for (auto& assignment : module.assignments)
        assignment.value = canonical[assignment.value];

    std::unordered_set<std::string> output_names;
    for (const auto& port : module.ports) {
        if (port.direction == PortDirection::Output)
            output_names.insert(port.name);
    }
    const bool has_output_ports = !output_names.empty();

    std::vector<RtlAssignment> observable_assignments;
    observable_assignments.reserve(module.assignments.size());
    std::vector<ValueId> pending;
    pending.reserve(module.assignments.size());
    for (const auto& assignment : module.assignments) {
        if (!has_output_ports ||
            output_names.find(assignment.target) != output_names.end()) {
            observable_assignments.push_back(assignment);
            pending.push_back(assignment.value);
        }
    }

    std::vector<bool> live(unique_nodes.size(), false);
    while (!pending.empty()) {
        const auto id = pending.back();
        pending.pop_back();
        if (static_cast<std::size_t>(id) >= unique_nodes.size() || live[id])
            continue;
        live[id] = true;
        for (const auto operand : unique_nodes[id].operands)
            pending.push_back(operand);
    }

    const auto live_count = static_cast<std::size_t>(
        std::count(live.begin(), live.end(), true));
    std::vector<ValueId> compact_map(unique_nodes.size(), invalid_id);
    std::vector<RtlNode> compact_nodes;
    compact_nodes.reserve(live_count);

    for (std::size_t index = 0; index < unique_nodes.size(); ++index) {
        if (!live[index]) continue;
        RtlNode node = unique_nodes[index];
        for (auto& operand : node.operands) {
            if (static_cast<std::size_t>(operand) >= compact_map.size() ||
                compact_map[operand] == invalid_id) {
                add_error(diagnostics, "OPT0006",
                          "live RTL node depends on a removed or invalid node",
                          node.location);
                return false;
            }
            operand = compact_map[operand];
        }
        node.id = static_cast<ValueId>(compact_nodes.size());
        compact_map[index] = node.id;
        compact_nodes.push_back(std::move(node));
    }

    for (auto& assignment : observable_assignments) {
        if (static_cast<std::size_t>(assignment.value) >= compact_map.size() ||
            compact_map[assignment.value] == invalid_id) {
            add_error(diagnostics, "OPT0007",
                      "observable assignment lost its value during dead-code elimination",
                      {});
            return false;
        }
        assignment.value = compact_map[assignment.value];
    }

    stats.nodes_after_cse += unique_nodes.size();
    stats.dead_nodes_removed += unique_nodes.size() - compact_nodes.size();
    stats.assignments_removed +=
        module.assignments.size() - observable_assignments.size();
    stats.assignments_after += observable_assignments.size();
    stats.nodes_after += compact_nodes.size();

    module.nodes = std::move(compact_nodes);
    module.assignments = std::move(observable_assignments);
    return true;
}

} // namespace

OptimizationResult RtlOptimizer::optimize(const RtlDesign& design) const {
    OptimizationResult result;
    result.design = design;

    for (auto& module : result.design.modules) {
        RtlModule original = module;
        if (!optimize_module(module, result.stats, result.diagnostics))
            module = std::move(original);
    }

    return result;
}

} // namespace synvanta
