#include "synvanta/optimizer.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
    std::string text;
    for (const char ch : node.literal) if (ch != '_') text.push_back(ch);

    const auto apostrophe = text.find('\'');
    if (apostrophe == std::string::npos) {
        std::uint64_t value = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 10);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) return std::nullopt;
        return value & mask_for(node.width);
    }
    if (apostrophe == 0) {
        if (text.size() != 2) return std::nullopt;
        if (text[1] == '0') return 0;
        if (text[1] == '1') return 1;
        return std::nullopt;
    }

    std::size_t position = apostrophe + 1;
    if (position < text.size() && (text[position] == 's' || text[position] == 'S')) ++position;
    if (position >= text.size()) return std::nullopt;

    const char base_character = text[position++];
    int base = 10;
    if (base_character == 'b' || base_character == 'B') base = 2;
    else if (base_character == 'o' || base_character == 'O') base = 8;
    else if (base_character == 'h' || base_character == 'H') base = 16;
    else if (base_character != 'd' && base_character != 'D') return std::nullopt;

    std::uint64_t value = 0;
    const auto parsed = std::from_chars(text.data() + static_cast<std::ptrdiff_t>(position),
                                        text.data() + text.size(), value, base);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) return std::nullopt;
    return value & mask_for(node.width);
}

std::int64_t signed_value(std::uint64_t value, std::uint32_t width) {
    const auto mask = mask_for(width);
    value &= mask;
    if (width > 0 && width < 64 && (value & (std::uint64_t{1} << (width - 1))) != 0) value |= ~mask;
    return std::bit_cast<std::int64_t>(value);
}

std::string format_constant(std::uint32_t width, std::uint64_t value) {
    std::ostringstream output;
    output << width << "'h" << std::hex << std::uppercase << (value & mask_for(width));
    return output.str();
}

std::optional<std::uint64_t> evaluate(const RtlNode& node, const std::vector<RtlNode>& nodes,
                                      const std::vector<std::optional<std::uint64_t>>& values) {
    if (node.width > 64) return std::nullopt;
    if (node.op == RtlOp::Constant) return parse_value(node);
    if (node.op == RtlOp::Input) return std::nullopt;

    std::vector<std::uint64_t> operands;
    operands.reserve(node.operands.size());
    for (const auto id : node.operands) {
        if (id >= values.size() || !values[id]) return std::nullopt;
        operands.push_back(*values[id]);
    }
    const auto result_mask = mask_for(node.width);

    if (node.op == RtlOp::Resize && operands.size() == 1) {
        const auto source_id = node.operands[0];
        if (source_id >= nodes.size() || nodes[source_id].width > 64) return std::nullopt;
        const auto& source = nodes[source_id];
        auto value = operands[0] & mask_for(source.width);

        // Unbased unsized literals fill the destination context.
        if (node.width > source.width && source.op == RtlOp::Constant) {
            if (source.literal == "'1") return result_mask;
            if (source.literal == "'0") return 0;
        }

        if (node.width > source.width && source.is_signed && source.width > 0 && source.width < 64 &&
            (value & (std::uint64_t{1} << (source.width - 1))) != 0) {
            value |= ~mask_for(source.width);
        }
        return value & result_mask;
    }

    if (operands.size() == 1) {
        const auto source_id = node.operands[0];
        if (source_id >= nodes.size()) return std::nullopt;
        const auto value = operands[0] & mask_for(nodes[source_id].width);
        switch (node.op) {
            case RtlOp::UnaryPlus: return value & result_mask;
            case RtlOp::Negate: return (std::uint64_t{0} - value) & result_mask;
            case RtlOp::BitNot: return (~value) & result_mask;
            case RtlOp::LogicalNot: return value == 0 ? 1 : 0;
            case RtlOp::ReduceNand: return value == mask_for(nodes[source_id].width) ? 0 : 1;
            case RtlOp::ReduceNor: return value == 0 ? 1 : 0;
            case RtlOp::ReduceXnor: return (std::popcount(value) % 2U) == 0U ? 1 : 0;
            default: return std::nullopt;
        }
    }

    if (operands.size() == 2) {
        const auto left_id = node.operands[0];
        const auto right_id = node.operands[1];
        if (left_id >= nodes.size() || right_id >= nodes.size()) return std::nullopt;
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
                    const auto a = signed_value(left, left_width), b = signed_value(right, right_width);
                    if (a == std::numeric_limits<std::int64_t>::min() && b == -1) return std::nullopt;
                    return static_cast<std::uint64_t>(a / b) & result_mask;
                }
                return (left / right) & result_mask;
            case RtlOp::Modulo:
                if (right == 0) return std::nullopt;
                if (is_signed) {
                    const auto a = signed_value(left, left_width), b = signed_value(right, right_width);
                    if (a == std::numeric_limits<std::int64_t>::min() && b == -1) return std::nullopt;
                    return static_cast<std::uint64_t>(a % b) & result_mask;
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
            case RtlOp::Less: return is_signed ? (signed_value(left, left_width) < signed_value(right, right_width) ? 1 : 0) : (left < right ? 1 : 0);
            case RtlOp::LessEqual: return is_signed ? (signed_value(left, left_width) <= signed_value(right, right_width) ? 1 : 0) : (left <= right ? 1 : 0);
            case RtlOp::Greater: return is_signed ? (signed_value(left, left_width) > signed_value(right, right_width) ? 1 : 0) : (left > right ? 1 : 0);
            case RtlOp::GreaterEqual: return is_signed ? (signed_value(left, left_width) >= signed_value(right, right_width) ? 1 : 0) : (left >= right ? 1 : 0);
            case RtlOp::ShiftLeft:
            case RtlOp::ShiftRight:
                if (nodes[right_id].is_signed && signed_value(right, right_width) < 0) return std::nullopt;
                if (right >= node.width) return 0;
                return node.op == RtlOp::ShiftLeft ? ((left << right) & result_mask) : ((left >> right) & result_mask);
            default: return std::nullopt;
        }
    }

    if (node.op == RtlOp::Mux && operands.size() == 3)
        return (operands[0] != 0 ? operands[1] : operands[2]) & result_mask;
    return std::nullopt;
}

std::size_t expected_arity(RtlOp op) {
    switch (op) {
        case RtlOp::Input:
        case RtlOp::Constant: return 0;
        case RtlOp::Resize:
        case RtlOp::UnaryPlus:
        case RtlOp::Negate:
        case RtlOp::BitNot:
        case RtlOp::LogicalNot:
        case RtlOp::ReduceNand:
        case RtlOp::ReduceNor:
        case RtlOp::ReduceXnor: return 1;
        case RtlOp::Mux: return 3;
        default: return 2;
    }
}

bool validate_module(const RtlModule& module, std::vector<Diagnostic>& diagnostics) {
    for (std::size_t index = 0; index < module.nodes.size(); ++index) {
        const auto& node = module.nodes[index];
        if (node.id != index) {
            diagnostics.push_back({DiagnosticSeverity::Error, "OPT0001",
                                   "RTL node IDs must match their module-local vector indices", node.location});
            return false;
        }
        if (node.width == 0) {
            diagnostics.push_back({DiagnosticSeverity::Error, "OPT0002", "RTL node width must be positive", node.location});
            return false;
        }
        if (node.operands.size() != expected_arity(node.op)) {
            diagnostics.push_back({DiagnosticSeverity::Error, "OPT0003", "RTL node has an invalid operand count", node.location});
            return false;
        }
        for (const auto operand : node.operands) {
            if (operand >= index) {
                diagnostics.push_back({DiagnosticSeverity::Error, "OPT0004",
                                       "RTL node operand must refer to an earlier node", node.location});
                return false;
            }
        }
    }
    for (const auto& assignment : module.assignments) {
        if (assignment.value >= module.nodes.size()) {
            diagnostics.push_back({DiagnosticSeverity::Error, "OPT0005", "RTL assignment references an invalid value ID", {}});
            return false;
        }
    }
    return true;
}

bool commutative(RtlOp op) {
    switch (op) {
        case RtlOp::Add: case RtlOp::Multiply: case RtlOp::BitAnd: case RtlOp::BitOr:
        case RtlOp::BitXor: case RtlOp::BitXnor: case RtlOp::LogicalAnd: case RtlOp::LogicalOr:
        case RtlOp::Equal: case RtlOp::NotEqual: return true;
        default: return false;
    }
}

std::string key_for(const RtlNode& node) {
    std::ostringstream output;
    output << static_cast<int>(node.op) << ':' << node.width << ':' << node.is_signed << ':';
    if (node.op == RtlOp::Input) output << node.name;
    if (node.op == RtlOp::Constant) output << node.literal;
    for (const auto operand : node.operands) output << ':' << operand;
    return output.str();
}

bool optimize_module(RtlModule& module, OptimizationStats& stats, std::vector<Diagnostic>& diagnostics) {
    if (!validate_module(module, diagnostics)) return false;
    stats.nodes_before += module.nodes.size();

    std::vector<std::optional<std::uint64_t>> constants(module.nodes.size());
    std::vector<ValueId> aliases(module.nodes.size());
    std::iota(aliases.begin(), aliases.end(), ValueId{0});

    for (auto& node : module.nodes) {
        for (auto& operand : node.operands) operand = aliases[operand];

        if (node.op == RtlOp::Mux && node.operands.size() == 3) {
            const auto condition = node.operands[0];
            if (condition < constants.size() && constants[condition]) {
                const auto selected = node.operands[*constants[condition] != 0 ? 1 : 2];
                aliases[node.id] = aliases[selected];
                if (aliases[node.id] < constants.size()) constants[node.id] = constants[aliases[node.id]];
                ++stats.muxes_simplified;
                continue;
            }
        }

        const auto value = evaluate(node, module.nodes, constants);
        if (!value) continue;

        // Keep unbased unsized literals distinct until their context width is known.
        if (node.op == RtlOp::Constant && (node.literal == "'0" || node.literal == "'1")) {
            constants[node.id] = *value;
            continue;
        }

        if (node.op != RtlOp::Constant) ++stats.constants_folded;
        node.op = RtlOp::Constant;
        node.operands.clear();
        node.name.clear();
        node.literal = format_constant(node.width, *value);
        constants[node.id] = *value;
    }

    std::vector<RtlNode> unique;
    std::vector<ValueId> remap(module.nodes.size(), 0);
    std::unordered_map<std::string, ValueId> seen;
    for (const auto& original : module.nodes) {
        if (aliases[original.id] != original.id) {
            remap[original.id] = remap[aliases[original.id]];
            continue;
        }
        RtlNode node = original;
        for (auto& operand : node.operands) operand = remap[operand];
        if (commutative(node.op) && node.operands.size() == 2 && node.operands[0] > node.operands[1])
            std::swap(node.operands[0], node.operands[1]);

        const auto key = key_for(node);
        const auto found = seen.find(key);
        if (found != seen.end()) {
            remap[original.id] = found->second;
            ++stats.common_subexpressions_removed;
            continue;
        }
        node.id = static_cast<ValueId>(unique.size());
        remap[original.id] = node.id;
        seen.emplace(key, node.id);
        unique.push_back(std::move(node));
    }

    for (auto& assignment : module.assignments) assignment.value = remap[assignment.value];
    module.nodes = std::move(unique);

    std::unordered_set<std::string> output_names;
    for (const auto& port : module.ports)
        if (port.direction == PortDirection::Output) output_names.insert(port.name);

    const bool has_output_ports = !output_names.empty();
    std::vector<RtlAssignment> observable_assignments;
    observable_assignments.reserve(module.assignments.size());
    for (const auto& assignment : module.assignments) {
        if (!has_output_ports || output_names.contains(assignment.target))
            observable_assignments.push_back(assignment);
        else
            ++stats.assignments_removed;
    }
    module.assignments = std::move(observable_assignments);

    std::vector<bool> live(module.nodes.size(), false);
    std::function<void(ValueId)> mark = [&](ValueId id) {
        if (id >= module.nodes.size() || live[id]) return;
        live[id] = true;
        for (const auto operand : module.nodes[id].operands) mark(operand);
    };
    for (const auto& assignment : module.assignments) mark(assignment.value);

    const auto invalid_id = std::numeric_limits<ValueId>::max();
    std::vector<ValueId> compact_map(module.nodes.size(), invalid_id);
    std::vector<RtlNode> compact;
    for (std::size_t index = 0; index < module.nodes.size(); ++index) {
        if (!live[index]) continue;
        compact_map[index] = static_cast<ValueId>(compact.size());
        auto node = module.nodes[index];
        node.id = compact_map[index];
        compact.push_back(std::move(node));
    }
    for (auto& node : compact)
        for (auto& operand : node.operands) operand = compact_map[operand];
    for (auto& assignment : module.assignments) assignment.value = compact_map[assignment.value];

    stats.dead_nodes_removed += module.nodes.size() - compact.size();
    module.nodes = std::move(compact);
    stats.nodes_after += module.nodes.size();
    return true;
}

} // namespace

OptimizationResult RtlOptimizer::optimize(const RtlDesign& design) const {
    OptimizationResult result;
    result.design = design;
    for (auto& module : result.design.modules) {
        if (!optimize_module(module, result.stats, result.diagnostics)) break;
    }
    return result;
}

} // namespace synvanta
