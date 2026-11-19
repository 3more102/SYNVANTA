#include "synvanta/aig.hpp"

#include <algorithm>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace synvanta {

std::size_t AigNetwork::max_depth() const {
    std::vector<std::size_t> levels(nodes_.size(), 0);
    std::size_t maximum = 0;
    for (std::size_t id = 1; id < nodes_.size(); ++id) {
        const auto& node = nodes_[id];
        if (node.kind != AigNodeKind::And) continue;
        const auto lhs = static_cast<std::size_t>(node.lhs >> 1U);
        const auto rhs = static_cast<std::size_t>(node.rhs >> 1U);
        if (lhs >= id || rhs >= id)
            throw std::logic_error("AIG edges must be topological");
        levels[id] = 1 + std::max(levels[lhs], levels[rhs]);
        maximum = std::max(maximum, levels[id]);
    }
    return maximum;
}

void AigNetwork::balance(std::vector<AigAssignment>& assignments) {
    // Preserve common subgraphs: only flatten non-inverted AND cones whose
    // intermediate nodes have a single fanout in the original network.
    std::vector<std::size_t> fanout(nodes_.size(), 0);
    for (std::size_t id = 1; id < nodes_.size(); ++id) {
        const auto& node = nodes_[id];
        if (node.kind != AigNodeKind::And) continue;
        if ((node.lhs >> 1U) >= id || (node.rhs >> 1U) >= id)
            throw std::logic_error("AIG edges must be topological");
        ++fanout[node.lhs >> 1U];
        ++fanout[node.rhs >> 1U];
    }
    for (const auto& assignment : assignments) {
        for (const auto literal : assignment.bits) {
            if ((literal >> 1U) >= nodes_.size())
                throw std::out_of_range("AIG assignment has an invalid literal");
            ++fanout[literal >> 1U];
        }
    }

    AigNetwork rebuilt;
    const AigLit invalid = std::numeric_limits<AigLit>::max();
    std::vector<AigLit> remap(nodes_.size(), invalid);
    remap[0] = aig_false;
    std::vector<std::size_t> levels(1, 0);

    const auto refresh_levels = [&]() {
        while (levels.size() < rebuilt.nodes_.size()) {
            const auto& node = rebuilt.nodes_[levels.size()];
            if (node.kind == AigNodeKind::And) {
                const auto lhs = static_cast<std::size_t>(node.lhs >> 1U);
                const auto rhs = static_cast<std::size_t>(node.rhs >> 1U);
                levels.push_back(1 + std::max(levels.at(lhs), levels.at(rhs)));
            } else {
                levels.push_back(0);
            }
        }
    };
    const auto mapped = [&](AigLit literal) -> AigLit {
        const auto id = static_cast<std::size_t>(literal >> 1U);
        if (id >= remap.size() || remap[id] == invalid)
            throw std::logic_error("AIG balancing found an unmapped node");
        return remap[id] ^ (literal & 1U);
    };

    struct Item { std::size_t level; AigLit literal; };
    struct ShallowestFirst {
        bool operator()(const Item& lhs, const Item& rhs) const noexcept {
            if (lhs.level != rhs.level) return lhs.level > rhs.level;
            return lhs.literal > rhs.literal;
        }
    };

    for (std::size_t id = 1; id < nodes_.size(); ++id) {
        const auto& node = nodes_[id];
        if (node.kind == AigNodeKind::Input) {
            remap[id] = rebuilt.input(node.input_name, node.input_bit);
            refresh_levels();
            continue;
        }
        if (node.kind != AigNodeKind::And)
            throw std::logic_error("unexpected AIG node kind");

        const AigLit original = rebuilt.make_and(mapped(node.lhs), mapped(node.rhs));
        refresh_levels();

        std::priority_queue<Item, std::vector<Item>, ShallowestFirst> queue;
        std::vector<AigLit> pending{node.lhs, node.rhs};
        while (!pending.empty()) {
            const auto literal = pending.back();
            pending.pop_back();
            const auto operand_id = static_cast<std::size_t>(literal >> 1U);
            const auto& operand = nodes_[operand_id];
            if ((literal & 1U) == 0 && operand.kind == AigNodeKind::And &&
                fanout[operand_id] == 1) {
                pending.push_back(operand.lhs);
                pending.push_back(operand.rhs);
            } else {
                const AigLit translated = mapped(literal);
                queue.push({levels.at(translated >> 1U), translated});
            }
        }
        // Combine the shallowest leaves first. AND is associative; this pass
        // never distributes across inverted edges or shared AND nodes.
        while (queue.size() > 1) {
            const auto left = queue.top();
            queue.pop();
            const auto right = queue.top();
            queue.pop();
            const AigLit combined = rebuilt.make_and(left.literal, right.literal);
            refresh_levels();
            queue.push({levels.at(combined >> 1U), combined});
        }

        const AigLit candidate = queue.top().literal;
        remap[id] = levels.at(candidate >> 1U) < levels.at(original >> 1U)
            ? candidate : original;
    }

    for (auto& assignment : assignments)
        for (auto& literal : assignment.bits) literal = mapped(literal);

    *this = std::move(rebuilt);
    // Removes unselected candidates and now-unreachable original cones.
    compact(assignments);
}

} // namespace synvanta
