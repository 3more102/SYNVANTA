#include "test.hpp"

#include "synvanta/compiler.hpp"
#include "synvanta/lowering.hpp"
#include "synvanta/optimizer.hpp"

#include <string>
#include <unordered_set>

namespace {

synvanta::RtlDesign lower_source(const std::string& source) {
    const synvanta::Compiler compiler;
    const auto parsed = compiler.compile(source, "optimizer_test.sv");
    REQUIRE(parsed.success());

    const synvanta::RtlLowerer lowerer;
    const auto lowered = lowerer.lower(parsed.unit);
    REQUIRE(lowered.success());
    return lowered.design;
}

} // namespace

TEST_CASE(optimizer_merges_commutative_common_subexpressions) {
    const auto design = lower_source(
        "module cse(input logic a, input logic b, output logic y);"
        "assign y = (a & b) ^ (b & a);"
        "endmodule");

    const synvanta::RtlOptimizer optimizer;
    const auto optimized = optimizer.optimize(design);
    const auto& module = optimized.design.modules.front();

    std::size_t and_count = 0;
    for (const auto& node : module.nodes) {
        if (node.op == synvanta::RtlOp::BitAnd) {
            ++and_count;
        }
    }

    REQUIRE(and_count == 1);
    REQUIRE(optimized.stats.common_subexpressions_removed == 1);
    REQUIRE(module.assignments.size() == 1);
}

TEST_CASE(optimizer_removes_unobservable_internal_logic) {
    const auto design = lower_source(
        "module dead(input logic a, input logic b, output logic y);"
        "logic unused;"
        "assign unused = a & b;"
        "assign y = a | b;"
        "endmodule");

    const synvanta::RtlOptimizer optimizer;
    const auto optimized = optimizer.optimize(design);
    const auto& module = optimized.design.modules.front();

    REQUIRE(optimized.stats.dead_nodes_removed == 1);
    REQUIRE(optimized.stats.assignments_removed == 1);
    REQUIRE(module.nodes.size() == 3);
    REQUIRE(module.assignments.size() == 1);
    REQUIRE(module.assignments.front().target == "y");
}

TEST_CASE(optimizer_preserves_every_observable_output) {
    const auto design = lower_source(
        "module outputs(input logic a, input logic b,"
        "output logic y, output logic z);"
        "assign y = a & b;"
        "assign z = a | b;"
        "endmodule");

    const synvanta::RtlOptimizer optimizer;
    const auto optimized = optimizer.optimize(design);
    const auto& module = optimized.design.modules.front();

    REQUIRE(module.assignments.size() == 2);
    REQUIRE(module.assignments[0].target == "y");
    REQUIRE(module.assignments[1].target == "z");
}

TEST_CASE(optimizer_compacts_node_ids_and_preserves_references) {
    const auto design = lower_source(
        "module compact(input logic a, input logic b, output logic y);"
        "logic unused;"
        "assign unused = a ^ b;"
        "assign y = a & b;"
        "endmodule");

    const synvanta::RtlOptimizer optimizer;
    const auto optimized = optimizer.optimize(design);
    const auto& module = optimized.design.modules.front();

    std::unordered_set<synvanta::ValueId> ids;
    for (std::size_t i = 0; i < module.nodes.size(); ++i) {
        const auto& node = module.nodes[i];
        REQUIRE(node.id == i);
        ids.insert(node.id);
    }
    for (const auto& node : module.nodes) {
        for (const auto operand : node.operands) {
            REQUIRE(ids.find(operand) != ids.end());
        }
    }
    for (const auto& assignment : module.assignments) {
        REQUIRE(ids.find(assignment.value) != ids.end());
    }
}

TEST_CASE(optimizer_is_idempotent) {
    const auto design = lower_source(
        "module stable(input logic a, input logic b, output logic y);"
        "assign y = (a & b) | (b & a);"
        "endmodule");

    const synvanta::RtlOptimizer optimizer;
    const auto first = optimizer.optimize(design);
    const auto second = optimizer.optimize(first.design);

    REQUIRE(synvanta::dump_rtl_design(first.design) ==
            synvanta::dump_rtl_design(second.design));
    REQUIRE(second.stats.common_subexpressions_removed == 0);
    REQUIRE(second.stats.dead_nodes_removed == 0);
}

