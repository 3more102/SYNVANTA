#include "synvanta/aig.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace synvanta {
namespace {

class RebuildBuilder {
public:
    explicit RebuildBuilder(std::string name) {
        module_.name = std::move(name);
        AigNode constant;
        constant.id = 0;
        constant.kind = AigNodeKind::ConstantFalse;
        module_.nodes.push_back(std::move(constant));
    }

    AigLiteral add_input(const AigNode& source) {
        const auto id = static_cast<std::uint32_t>(module_.nodes.size());
        const auto input_index = static_cast<std::uint32_t>(module_.inputs.size());
        AigNode node;
        node.id = id;
        node.kind = AigNodeKind::Input;
        node.input_name = source.input_name;
        node.input_bit = source.input_bit;
        node.input_index = input_index;
        node.location = source.location;
        module_.nodes.push_back(std::move(node));
        module_.inputs.push_back({source.input_name, source.input_bit, id});
        return id << 1U;
    }

    AigLiteral make_and(AigLiteral lhs, AigLiteral rhs,
                        const SourceLocation& location) {
        if (lhs == AigFalse || rhs == AigFalse) return AigFalse;
        if (lhs == AigTrue) return rhs;
        if (rhs == AigTrue) return lhs;
        if (lhs == rhs) return lhs;
        if (lhs == aig_invert(rhs)) return AigFalse;
        if (rhs < lhs) std::swap(lhs, rhs);

        const std::uint64_t key =
            (static_cast<std::uint64_t>(lhs) << 32U) | rhs;
        const auto found = and_hash_.find(key);
        if (found != and_hash_.end()) return found->second;

        const auto id = static_cast<std::uint32_t>(module_.nodes.size());
        if (id > (std::numeric_limits<AigLiteral>::max() >> 1U))
            return AigFalse;

        AigNode node;
        node.id = id;
        node.kind = AigNodeKind::And;
        node.lhs = lhs;
        node.rhs = rhs;
        node.location = location;
        module_.nodes.push_back(std::move(node));

        const AigLiteral result = id << 1U;
        and_hash_.emplace(key, result);
        return result;
    }

    AigModule take() {
        return std::move(module_);
    }

private:
    AigModule module_;
    std::unordered_map<std::uint64_t, AigLiteral> and_hash_;
};

std::uint32_t aig_depth(const AigModule& module) {
    std::vector<std::uint32_t> depth(module.nodes.size(), 0);
    std::uint32_t maximum = 0;
    for (std::size_t index = 1; index < module.nodes.size(); ++index) {
        const auto& node = module.nodes[index];
        if (node.kind != AigNodeKind::And) continue;
        const auto lhs = aig_node_id(node.lhs);
        const auto rhs = aig_node_id(node.rhs);
        if (lhs >= index || rhs >= index) continue;
        depth[index] = 1U + std::max(depth[lhs], depth[rhs]);
        maximum = std::max(maximum, depth[index]);
    }
    return maximum;
}

std::size_t aig_and_count(const AigModule& module) {
    std::size_t count = 0;
    for (const auto& node : module.nodes)
        if (node.kind == AigNodeKind::And) ++count;
    return count;
}

bool validate_module(const AigModule& module, std::vector<Diagnostic>& diagnostics) {
    auto report = [&](const std::string& code, const std::string& message) {
        diagnostics.push_back({DiagnosticSeverity::Error, code, message, {}});
    };

    if (module.nodes.empty() ||
        module.nodes.front().kind != AigNodeKind::ConstantFalse ||
        module.nodes.front().id != 0) {
        report("AIGOPT0001", "AIG module must begin with constant-false node zero");
        return false;
    }

    for (std::size_t index = 0; index < module.nodes.size(); ++index) {
        const auto& node = module.nodes[index];
        if (node.id != index) {
            report("AIGOPT0002", "AIG node IDs must be dense and match their vector positions");
            return false;
        }
        if (node.kind == AigNodeKind::And) {
            if (aig_node_id(node.lhs) >= index || aig_node_id(node.rhs) >= index) {
                report("AIGOPT0003", "AIG AND nodes must reference earlier nodes");
                return false;
            }
        }
        if (node.kind == AigNodeKind::Input && node.input_index >= module.inputs.size()) {
            report("AIGOPT0004", "AIG input node has an invalid primary-input index");
            return false;
        }
    }

    for (const auto& output : module.outputs) {
        if (aig_node_id(output.driver) >= module.nodes.size()) {
            report("AIGOPT0005", "AIG output references a node outside the module");
            return false;
        }
    }
    return true;
}

std::optional<AigModule> balance_module(const AigModule& source,
                                        std::vector<Diagnostic>& diagnostics) {
    if (!validate_module(source, diagnostics)) return std::nullopt;

    RebuildBuilder builder(source.name);
    std::vector<AigLiteral> remap(source.nodes.size(), AigFalse);
    remap[0] = AigFalse;

    for (std::size_t index = 1; index < source.nodes.size(); ++index) {
        const auto& node = source.nodes[index];
        if (node.kind == AigNodeKind::Input) {
            remap[index] = builder.add_input(node);
            continue;
        }
        if (node.kind != AigNodeKind::And) {
            diagnostics.push_back({DiagnosticSeverity::Error, "AIGOPT0006",
                                   "unexpected node kind in AIG optimization", node.location});
            return std::nullopt;
        }

        // Flatten only positive-polarity AND edges. A complemented AND edge
        // represents NAND and must remain an indivisible leaf.
        std::vector<AigLiteral> leaves;
        std::vector<AigLiteral> pending{node.rhs, node.lhs};
        while (!pending.empty()) {
            const auto literal = pending.back();
            pending.pop_back();
            const auto child_id = aig_node_id(literal);
            if ((literal & 1U) == 0U &&
                source.nodes[child_id].kind == AigNodeKind::And) {
                pending.push_back(source.nodes[child_id].rhs);
                pending.push_back(source.nodes[child_id].lhs);
            } else {
                remap[child_id] ^= 0U;
                leaves.push_back(remap[child_id] ^ (literal & 1U));
            }
        }

        std::sort(leaves.begin(), leaves.end());
        while (leaves.size() > 1) {
            std::vector<AigLiteral> next;
            next.reserve((leaves.size() + 1U) / 2U);
            for (std::size_t i = 0; i < leaves.size(); i += 2) {
                if (i + 1U == leaves.size()) {
                    next.push_back(leaves[i]);
                } else {
                    next.push_back(builder.make_and(leaves[i], leaves[i + 1U],
                                                    node.location));
                }
            }
            leaves = std::move(next);
        }
        remap[index] = leaves.empty() ? AigFalse : leaves.front();
    }

    auto rebuilt = builder.take();
    for (const auto& output : source.outputs) {
        const auto old_id = aig_node_id(output.driver);
        rebuilt.outputs.push_back(
            {output.port_name, output.bit_index,
             remap[old_id] ^ (output.driver & 1U)});
    }

    // Sweep nodes that became unreachable after balancing and simplification.
    std::vector<bool> live(rebuilt.nodes.size(), false);
    live[0] = true;
    for (const auto& input : rebuilt.inputs) live[input.node_id] = true;

    auto mark = [&](auto&& self, std::uint32_t id) -> void {
        if (id >= rebuilt.nodes.size() || live[id]) return;
        live[id] = true;
        const auto& node = rebuilt.nodes[id];
        if (node.kind == AigNodeKind::And) {
            self(self, aig_node_id(node.lhs));
            self(self, aig_node_id(node.rhs));
        }
    };
    for (const auto& output : rebuilt.outputs) mark(mark, aig_node_id(output.driver));

    std::vector<AigLiteral> compact_map(rebuilt.nodes.size(), AigFalse);
    std::vector<AigNode> compact_nodes;
    compact_nodes.reserve(rebuilt.nodes.size());
    for (std::size_t index = 0; index < rebuilt.nodes.size(); ++index) {
        if (!live[index]) continue;
        const auto new_id = static_cast<std::uint32_t>(compact_nodes.size());
        compact_map[index] = new_id << 1U;
        auto node = rebuilt.nodes[index];
        node.id = new_id;
        compact_nodes.push_back(std::move(node));
    }

    for (auto& node : compact_nodes) {
        if (node.kind != AigNodeKind::And) continue;
        node.lhs = compact_map[aig_node_id(node.lhs)] ^ (node.lhs & 1U);
        node.rhs = compact_map[aig_node_id(node.rhs)] ^ (node.rhs & 1U);
    }
    for (auto& input : rebuilt.inputs)
        input.node_id = aig_node_id(compact_map[input.node_id]);
    for (auto& output : rebuilt.outputs)
        output.driver = compact_map[aig_node_id(output.driver)] ^ (output.driver & 1U);
    rebuilt.nodes = std::move(compact_nodes);
    return rebuilt;
}

} // namespace

AigOptimizationResult AigOptimizer::optimize(const AigDesign& design) const {
    AigOptimizationResult result;
    if (design.modules.empty()) {
        result.diagnostics.push_back({DiagnosticSeverity::Error, "AIGOPT0000",
                                      "cannot optimize an empty AIG design", {}});
        return result;
    }

    for (const auto& module : design.modules) {
        result.stats.nodes_before += module.nodes.size();
        result.stats.and_nodes_before += aig_and_count(module);
        result.stats.depth_before =
            std::max(result.stats.depth_before, aig_depth(module));

        auto optimized = balance_module(module, result.diagnostics);
        if (!optimized) continue;

        result.stats.nodes_after += optimized->nodes.size();
        result.stats.and_nodes_after += aig_and_count(*optimized);
        result.stats.depth_after =
            std::max(result.stats.depth_after, aig_depth(*optimized));
        result.design.modules.push_back(std::move(*optimized));
    }
    return result;
}

} // namespace synvanta
