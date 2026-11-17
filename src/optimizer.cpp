#include "synvanta/optimizer.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace synvanta {
namespace {

struct NodeKey {
    RtlOp op{RtlOp::Constant};
    std::uint32_t width{1};
    bool is_signed{false};
    std::vector<ValueId> operands;
    std::string literal;
    std::string identity;

    bool operator==(const NodeKey&) const = default;
};

struct NodeKeyHash {
    std::size_t operator()(const NodeKey& key) const noexcept {
        std::size_t hash = std::hash<unsigned int>{}(static_cast<unsigned int>(key.op));
        const auto combine = [&hash](std::size_t value) {
            hash ^= value + static_cast<std::size_t>(0x9e3779b9U) + (hash << 6U) + (hash >> 2U);
        };

        combine(std::hash<std::uint32_t>{}(key.width));
        combine(std::hash<bool>{}(key.is_signed));
        combine(std::hash<std::string>{}(key.literal));
        combine(std::hash<std::string>{}(key.identity));
        for (const auto operand : key.operands) {
            combine(std::hash<ValueId>{}(operand));
        }
        return hash;
    }
};

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

NodeKey make_key(const RtlNode& node) {
    NodeKey key;
    key.op = node.op;
    key.width = node.width;
    key.is_signed = node.is_signed;
    key.operands = node.operands;

    if (node.op == RtlOp::Constant) {
        key.literal = node.literal;
    } else if (node.op == RtlOp::Input) {
        key.identity = node.name;
    }

    if (is_commutative(node.op)) {
        std::sort(key.operands.begin(), key.operands.end());
    }
    return key;
}

RtlModule eliminate_common_subexpressions(
    const RtlModule& input, std::size_t& removed_count) {
    RtlModule output;
    output.name = input.name;
    output.ports = input.ports;
    output.assignments = input.assignments;

    std::unordered_map<ValueId, ValueId> remap;
    std::unordered_map<NodeKey, ValueId, NodeKeyHash> unique_nodes;
    remap.reserve(input.nodes.size());
    unique_nodes.reserve(input.nodes.size());
    output.nodes.reserve(input.nodes.size());

    for (const auto& original : input.nodes) {
        RtlNode node = original;
        for (auto& operand : node.operands) {
            const auto mapped = remap.find(operand);
            if (mapped != remap.end()) {
                operand = mapped->second;
            }
        }

        auto key = make_key(node);
        const auto existing = unique_nodes.find(key);
        if (existing != unique_nodes.end()) {
            remap[original.id] = existing->second;
            continue;
        }

        node.id = static_cast<ValueId>(output.nodes.size());
        const auto new_id = node.id;
        output.nodes.push_back(std::move(node));
        remap[original.id] = new_id;
        unique_nodes.emplace(std::move(key), new_id);
    }

    for (auto& assignment : output.assignments) {
        const auto mapped = remap.find(assignment.value);
        if (mapped != remap.end()) {
            assignment.value = mapped->second;
        }
    }

    removed_count += input.nodes.size() - output.nodes.size();
    return output;
}

RtlModule eliminate_dead_code(
    const RtlModule& input,
    std::size_t& removed_nodes,
    std::size_t& removed_assignments) {
    RtlModule output;
    output.name = input.name;
    output.ports = input.ports;

    std::unordered_set<std::string> output_names;
    for (const auto& port : input.ports) {
        if (port.direction == PortDirection::Output) {
            output_names.insert(port.name);
        }
    }

    std::unordered_map<ValueId, const RtlNode*> nodes_by_id;
    nodes_by_id.reserve(input.nodes.size());
    for (const auto& node : input.nodes) {
        nodes_by_id.emplace(node.id, &node);
    }

    std::unordered_set<ValueId> live;
    live.reserve(input.nodes.size());
    std::function<void(ValueId)> mark_live = [&](ValueId id) {
        if (!live.insert(id).second) {
            return;
        }
        const auto found = nodes_by_id.find(id);
        if (found == nodes_by_id.end()) {
            return;
        }
        for (const auto operand : found->second->operands) {
            mark_live(operand);
        }
    };

    for (const auto& assignment : input.assignments) {
        if (output_names.find(assignment.target) != output_names.end()) {
            mark_live(assignment.value);
        }
    }

    std::unordered_map<ValueId, ValueId> remap;
    remap.reserve(live.size());
    output.nodes.reserve(live.size());

    for (const auto& node : input.nodes) {
        if (live.find(node.id) == live.end()) {
            continue;
        }
        RtlNode kept = node;
        kept.id = static_cast<ValueId>(output.nodes.size());
        remap[node.id] = kept.id;
        output.nodes.push_back(std::move(kept));
    }

    for (auto& node : output.nodes) {
        for (auto& operand : node.operands) {
            const auto mapped = remap.find(operand);
            if (mapped != remap.end()) {
                operand = mapped->second;
            }
        }
    }

    for (const auto& assignment : input.assignments) {
        if (output_names.find(assignment.target) == output_names.end()) {
            continue;
        }
        const auto mapped = remap.find(assignment.value);
        if (mapped == remap.end()) {
            continue;
        }
        auto kept = assignment;
        kept.value = mapped->second;
        output.assignments.push_back(std::move(kept));
    }

    removed_nodes += input.nodes.size() - output.nodes.size();
    removed_assignments += input.assignments.size() - output.assignments.size();
    return output;
}

} // namespace

OptimizationResult RtlOptimizer::optimize(const RtlDesign& design) const {
    OptimizationResult result;
    result.design.modules.reserve(design.modules.size());

    for (const auto& module : design.modules) {
        result.stats.nodes_before += module.nodes.size();
        result.stats.assignments_before += module.assignments.size();

        auto deduplicated = eliminate_common_subexpressions(
            module, result.stats.common_subexpressions_removed);
        auto compacted = eliminate_dead_code(
            deduplicated,
            result.stats.dead_nodes_removed,
            result.stats.assignments_removed);

        result.stats.nodes_after += compacted.nodes.size();
        result.stats.assignments_after += compacted.assignments.size();
        result.design.modules.push_back(std::move(compacted));
    }

    return result;
}

} // namespace synvanta
