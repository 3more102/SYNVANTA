#include "synvanta/optimizer.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace synvanta {
namespace {

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

std::string node_key(const RtlNode& node) {
    std::ostringstream key;
    key << static_cast<unsigned>(node.op) << ':' << node.width << ':' << node.is_signed << ':'
        << node.name.size() << ':' << node.name << ':' << node.literal.size() << ':' << node.literal << ':';
    for (const auto operand : node.operands) key << operand << ',';
    return key.str();
}

std::string constant_literal(std::uint32_t width, bool ones) {
    std::string literal = std::to_string(width) + "'b";
    literal.reserve(literal.size() + width);
    literal.append(width, ones ? '1' : '0');
    return literal;
}

void add_error(std::vector<Diagnostic>& diagnostics, const std::string& code,
               const std::string& message, const SourceLocation& location) {
    diagnostics.push_back(Diagnostic{DiagnosticSeverity::Error, code, message, location});
}

bool optimize_module(const RtlModule& input, RtlModule& output,
                     ModuleOptimizationStats& stats, std::vector<Diagnostic>& diagnostics) {
    stats.module_name = input.name;
    stats.nodes_before = input.nodes.size();
    stats.assignments_before = input.assignments.size();
    output.name = input.name;
    output.ports = input.ports;

    const auto invalid_id = std::numeric_limits<ValueId>::max();
    std::vector<ValueId> canonical(input.nodes.size(), invalid_id);
    std::vector<RtlNode> cse_nodes;
    std::unordered_map<std::string, ValueId> interned;

    auto intern_node = [&](RtlNode node) -> ValueId {
        if (is_commutative(node.op) && node.operands.size() == 2 && node.operands[1] < node.operands[0])
            std::swap(node.operands[0], node.operands[1]);
        const auto key = node_key(node);
        const auto found = interned.find(key);
        if (found != interned.end()) return found->second;
        if (cse_nodes.size() >= static_cast<std::size_t>(invalid_id)) return invalid_id;
        node.id = static_cast<ValueId>(cse_nodes.size());
        const auto id = node.id;
        cse_nodes.push_back(std::move(node));
        interned.emplace(key, id);
        return id;
    };

    for (std::size_t index = 0; index < input.nodes.size(); ++index) {
        const auto& original = input.nodes[index];
        if (original.id != index) {
            add_error(diagnostics, "OPT0001", "RTL node IDs must match their module-local vector indices", original.location);
            return false;
        }
        if (original.width == 0) {
            add_error(diagnostics, "OPT0008", "RTL node width must be positive", original.location);
            return false;
        }

        RtlNode node = original;
        for (auto& operand : node.operands) {
            if (operand >= index) {
                add_error(diagnostics, "OPT0002", "RTL node operand must refer to an earlier node", original.location);
                return false;
            }
            operand = canonical[operand];
            if (operand == invalid_id) {
                add_error(diagnostics, "OPT0003", "RTL node references an invalid or unprocessed operand", original.location);
                return false;
            }
        }

        bool simplified = false;
        if (node.operands.size() == 1 && node.op == RtlOp::UnaryPlus) {
            canonical[index] = node.operands[0];
            simplified = true;
        } else if (node.operands.size() == 1 && node.op == RtlOp::Resize) {
            const auto& source = cse_nodes[node.operands[0]];
            if (source.width == node.width && source.is_signed == node.is_signed) {
                canonical[index] = node.operands[0];
                simplified = true;
            }
        } else if (node.operands.size() == 2 && node.operands[0] == node.operands[1]) {
            if (node.op == RtlOp::BitAnd || node.op == RtlOp::BitOr) {
                canonical[index] = node.operands[0];
                simplified = true;
            } else if (node.op == RtlOp::BitXor || node.op == RtlOp::BitXnor) {
                RtlNode constant;
                constant.op = RtlOp::Constant;
                constant.width = node.width;
                constant.is_signed = node.is_signed;
                constant.literal = constant_literal(node.width, node.op == RtlOp::BitXnor);
                constant.location = node.location;
                canonical[index] = intern_node(std::move(constant));
                simplified = true;
            }
        } else if (node.op == RtlOp::Mux && node.operands.size() == 3 && node.operands[1] == node.operands[2]) {
            canonical[index] = node.operands[1];
            simplified = true;
        }

        if (simplified) {
            ++stats.simplified_nodes;
            continue;
        }
        const auto before = cse_nodes.size();
        canonical[index] = intern_node(std::move(node));
        if (canonical[index] == invalid_id) {
            add_error(diagnostics, "OPT0004", "RTL graph exceeds the supported value-ID range", original.location);
            return false;
        }
        if (cse_nodes.size() == before) ++stats.cse_merged;
    }

    std::vector<RtlAssignment> output_assignments;
    std::unordered_set<std::string> output_names;
    for (const auto& port : input.ports)
        if (port.direction == PortDirection::Output) output_names.insert(port.name);

    const bool has_output_ports = !output_names.empty();
    for (const auto& assignment : input.assignments) {
        if (assignment.value >= canonical.size()) {
            add_error(diagnostics, "OPT0005", "RTL assignment references an invalid value ID", {});
            return false;
        }
        if (!has_output_ports || output_names.contains(assignment.target)) {
            auto remapped = assignment;
            remapped.value = canonical[assignment.value];
            output_assignments.push_back(std::move(remapped));
        }
    }

    std::vector<bool> live(cse_nodes.size(), false);
    std::function<void(ValueId)> mark_live = [&](ValueId id) {
        if (id >= cse_nodes.size() || live[id]) return;
        live[id] = true;
        for (const auto operand : cse_nodes[id].operands) mark_live(operand);
    };
    for (const auto& assignment : output_assignments) mark_live(assignment.value);

    stats.nodes_after_cse = cse_nodes.size();
    const auto live_count = static_cast<std::size_t>(std::count(live.begin(), live.end(), true));
    stats.dead_nodes = cse_nodes.size() - live_count;
    std::vector<ValueId> compact(cse_nodes.size(), invalid_id);
    output.nodes.reserve(live_count);
    for (std::size_t index = 0; index < cse_nodes.size(); ++index) {
        if (!live[index]) continue;
        RtlNode node = cse_nodes[index];
        for (auto& operand : node.operands) {
            if (operand >= compact.size() || compact[operand] == invalid_id) {
                add_error(diagnostics, "OPT0006", "live RTL node depends on a removed or invalid node", node.location);
                return false;
            }
            operand = compact[operand];
        }
        node.id = static_cast<ValueId>(output.nodes.size());
        compact[index] = node.id;
        output.nodes.push_back(std::move(node));
    }
    for (auto& assignment : output_assignments) {
        if (assignment.value >= compact.size() || compact[assignment.value] == invalid_id) {
            add_error(diagnostics, "OPT0007", "observable assignment lost its value during dead-code elimination", {});
            return false;
        }
        assignment.value = compact[assignment.value];
        output.assignments.push_back(std::move(assignment));
    }
    stats.nodes_after_dce = output.nodes.size();
    stats.assignments_after = output.assignments.size();
    return true;
}

} // namespace

OptimizationResult RtlOptimizer::optimize(const RtlDesign& design) const {
    OptimizationResult result;
    for (const auto& module : design.modules) {
        RtlModule optimized;
        ModuleOptimizationStats stats;
        if (optimize_module(module, optimized, stats, result.diagnostics))
            result.design.modules.push_back(std::move(optimized));
        result.stats.push_back(std::move(stats));
    }
    return result;
}

} // namespace synvanta
