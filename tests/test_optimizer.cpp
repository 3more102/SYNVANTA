#include "test.hpp"

#include "synvanta/compiler.hpp"
#include "synvanta/lowering.hpp"
#include "synvanta/optimizer.hpp"

#include <string>

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
}

TEST_CASE(optimizer_merges_duplicate_commutative_expressions) {
    const auto design = lower_source(
        "module cse(input logic a,input logic b,output logic y0,output logic y1);"
        "assign y0=a&b; assign y1=b&a; endmodule");

    const synvanta::RtlOptimizer optimizer;
    const auto result = optimizer.optimize(design);
    REQUIRE(result.success());
    REQUIRE(result.design.modules.size() == 1);
    REQUIRE(result.stats[0].cse_merged == 1);
    REQUIRE(result.design.modules[0].nodes.size() == 3);
    REQUIRE(result.design.modules[0].assignments.size() == 2);
    REQUIRE(result.design.modules[0].assignments[0].value == result.design.modules[0].assignments[1].value);
}

TEST_CASE(optimizer_removes_unobservable_internal_logic) {
    const auto design = lower_source(
        "module dce(input logic a,input logic b,output logic y);"
        "logic unused; assign unused=a&b; assign y=a|b; endmodule");

    const synvanta::RtlOptimizer optimizer;
    const auto result = optimizer.optimize(design);
    REQUIRE(result.success());
    const auto& module = result.design.modules[0];
    REQUIRE(result.stats[0].dead_nodes == 1);
    REQUIRE(module.nodes.size() == 3);
    REQUIRE(module.nodes[2].op == synvanta::RtlOp::BitOr);
    REQUIRE(module.assignments.size() == 1);
    REQUIRE(module.assignments[0].target == "y");
}

TEST_CASE(optimizer_preserves_logic_reached_through_internal_nets) {
    const auto design = lower_source(
        "module chain(input logic a,input logic b,output logic y);"
        "logic temp; assign temp=a^b; assign y=temp&a; endmodule");

    const synvanta::RtlOptimizer optimizer;
    const auto result = optimizer.optimize(design);
    REQUIRE(result.success());
    const auto& module = result.design.modules[0];
    REQUIRE(module.nodes.size() == 4);
    REQUIRE(module.nodes[2].op == synvanta::RtlOp::BitXor);
    REQUIRE(module.nodes[3].op == synvanta::RtlOp::BitAnd);
    REQUIRE(module.assignments.size() == 1);
    REQUIRE(module.assignments[0].target == "y");
}

TEST_CASE(optimizer_simplifies_boolean_identities) {
    const auto design = lower_source(
        "module simplify(input logic a,output logic y); assign y=a^a; endmodule");

    const synvanta::RtlOptimizer optimizer;
    const auto result = optimizer.optimize(design);
    REQUIRE(result.success());
    const auto& module = result.design.modules[0];
    REQUIRE(module.nodes.size() == 1);
    REQUIRE(module.nodes[0].op == synvanta::RtlOp::Constant);
    REQUIRE(module.nodes[0].literal == "1'b0");
    REQUIRE(module.assignments[0].value == 0);
}

TEST_CASE(optimizer_eliminates_mux_with_identical_data_inputs) {
    const auto design = lower_source(
        "module mux(input logic sel,input logic a,output logic y); assign y=sel?a:a; endmodule");

    const synvanta::RtlOptimizer optimizer;
    const auto result = optimizer.optimize(design);
    REQUIRE(result.success());
    const auto& module = result.design.modules[0];
    REQUIRE(module.nodes.size() == 1);
    REQUIRE(module.nodes[0].op == synvanta::RtlOp::Input);
    REQUIRE(module.nodes[0].name == "a");
    REQUIRE(module.assignments[0].value == 0);
}

TEST_CASE(optimizer_rejects_non_topological_ir) {
    synvanta::RtlDesign design;
    synvanta::RtlModule module;
    module.name = "invalid";
    synvanta::RtlNode node;
    node.id = 0;
    node.op = synvanta::RtlOp::BitAnd;
    node.width = 1;
    node.operands = {1, 1};
    module.nodes.push_back(node);
    module.ports.push_back({"y", synvanta::PortDirection::Output, 1, false});
    module.assignments.push_back({"y", 0, 1, false});
    design.modules.push_back(module);

    const synvanta::RtlOptimizer optimizer;
    const auto result = optimizer.optimize(design);
    REQUIRE(!result.success());
    REQUIRE(result.diagnostics[0].code == "OPT0002");
}
