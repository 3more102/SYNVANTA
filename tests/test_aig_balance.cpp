#include "test.hpp"
#include "synvanta/aig.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
bool evaluate(const synvanta::AigModule& module, std::uint32_t pattern, const std::string& output) {
    const auto& nodes = module.network.nodes();
    std::vector<bool> values(nodes.size(), false);
    const auto lit_value = [&](synvanta::AigLit lit) {
        const bool value = values.at(lit >> 1U);
        return (lit & 1U) != 0 ? !value : value;
    };
    for (std::size_t id = 1; id < nodes.size(); ++id) {
        const auto& node = nodes[id];
        if (node.kind == synvanta::AigNodeKind::Input) {
            if (node.input_bit != 0) throw std::runtime_error("only scalar test inputs");
            const auto position = std::string("abcd").find(node.input_name);
            if (position == std::string::npos) throw std::runtime_error("unknown input");
            values[id] = ((pattern >> position) & 1U) != 0;
        } else if (node.kind == synvanta::AigNodeKind::And) {
            values[id] = lit_value(node.lhs) && lit_value(node.rhs);
        }
    }
    for (const auto& assignment : module.assignments)
        if (assignment.target == output) return lit_value(assignment.bits.at(0));
    throw std::runtime_error("output not found");
}
}

TEST_CASE(aig_balance_reduces_depth_without_changing_truth_table) {
    synvanta::AigModule module;
    auto& net = module.network;
    const auto a = net.input("a", 0), b = net.input("b", 0);
    const auto c = net.input("c", 0), d = net.input("d", 0);
    const auto ab = net.make_and(a, b);
    const auto abc = net.make_and(ab, c);
    module.assignments.push_back({"y", {net.make_and(abc, d)}, 1, false});
    REQUIRE(net.max_depth() == 3);
    net.balance(module.assignments);
    REQUIRE(net.max_depth() == 2);
    REQUIRE(net.and_count() == 3);
    for (std::uint32_t p = 0; p < 16; ++p)
        REQUIRE(evaluate(module, p, "y") == (p == 15));
    net.balance(module.assignments);
    REQUIRE(net.max_depth() == 2);
    REQUIRE(net.and_count() == 3);
}

TEST_CASE(aig_balance_preserves_shared_cones) {
    synvanta::AigModule module;
    auto& net = module.network;
    const auto a = net.input("a", 0), b = net.input("b", 0);
    const auto c = net.input("c", 0), d = net.input("d", 0);
    const auto ab = net.make_and(a, b);
    module.assignments.push_back({"y", {net.make_and(ab, c)}, 1, false});
    module.assignments.push_back({"z", {net.make_and(ab, d)}, 1, false});
    net.balance(module.assignments);
    REQUIRE(net.and_count() == 3);
    for (std::uint32_t p = 0; p < 16; ++p) {
        const bool shared = (p & 1U) && (p & 2U);
        REQUIRE(evaluate(module, p, "y") == (shared && (p & 4U)));
        REQUIRE(evaluate(module, p, "z") == (shared && (p & 8U)));
    }
}

TEST_CASE(aig_balance_preserves_complemented_edges) {
    synvanta::AigModule module;
    auto& net = module.network;
    const auto a = net.input("a", 0), b = net.input("b", 0);
    const auto c = net.input("c", 0), d = net.input("d", 0);
    const auto abc = net.make_and(net.make_and(a, b), c);
    module.assignments.push_back({"y", {net.make_and(synvanta::AigNetwork::invert(abc), d)}, 1, false});
    net.balance(module.assignments);
    for (std::uint32_t p = 0; p < 16; ++p) {
        const bool abc_value = (p & 1U) && (p & 2U) && (p & 4U);
        REQUIRE(evaluate(module, p, "y") == (!abc_value && (p & 8U)));
    }
}

TEST_CASE(aig_balance_handles_constant_output) {
    synvanta::AigModule module;
    module.assignments.push_back({"y", {synvanta::aig_true}, 1, false});
    module.network.balance(module.assignments);
    REQUIRE(module.network.max_depth() == 0);
    REQUIRE(module.network.and_count() == 0);
    REQUIRE(evaluate(module, 0, "y"));
}
