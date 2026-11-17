#include "test.hpp"
#include "synvanta/compiler.hpp"
#include "synvanta/lowering.hpp"
#include "synvanta/optimizer.hpp"
#include <stdexcept>
#include <string>
namespace {
synvanta::RtlDesign lower_source(const std::string& s){
 const synvanta::Compiler c;const auto p=c.compile(s,"optimizer_test.sv");
 if(!p.success())throw std::runtime_error("frontend failed");
 const synvanta::RtlLowerer l;const auto r=l.lower(p.unit);
 if(!r.success())throw std::runtime_error("lowering failed");return r.design;
}
}
TEST_CASE(optimizer_folds_constant_arithmetic){
 const auto d=lower_source("module add(output logic [7:0] y);assign y=8'd5+8'd3;endmodule");
 const synvanta::RtlOptimizer o;const auto r=o.optimize(d);REQUIRE(r.success());
 REQUIRE(r.stats.constants_folded==1);REQUIRE(r.design.modules[0].nodes.size()==1);
 REQUIRE(r.design.modules[0].nodes[0].op==synvanta::RtlOp::Constant);REQUIRE(r.design.modules[0].nodes[0].literal=="8'h8");
}
TEST_CASE(optimizer_merges_common_subexpressions){
 const auto d=lower_source("module cse(input logic a,input logic b,output logic y,output logic z);assign y=a^b;assign z=a^b;endmodule");
 const synvanta::RtlOptimizer o;const auto r=o.optimize(d);REQUIRE(r.success());
 REQUIRE(r.stats.common_subexpressions_removed==1);
 REQUIRE(r.design.modules[0].assignments[0].value==r.design.modules[0].assignments[1].value);
}
TEST_CASE(optimizer_removes_dead_input_nodes){
 const auto d=lower_source("module dead(input logic a,input logic unused,output logic y);assign y=a;endmodule");
 const synvanta::RtlOptimizer o;const auto r=o.optimize(d);REQUIRE(r.success());
 REQUIRE(r.stats.dead_nodes_removed==1);REQUIRE(r.design.modules[0].nodes.size()==1);
 REQUIRE(r.design.modules[0].nodes[0].name=="a");
}
TEST_CASE(optimizer_folds_constant_mux){
 const auto d=lower_source("module mux(input logic a,input logic b,output logic y);assign y=1'b1?a:b;endmodule");
 const synvanta::RtlOptimizer o;const auto r=o.optimize(d);REQUIRE(r.success());
 REQUIRE(r.stats.muxes_simplified==1);REQUIRE(r.design.modules[0].assignments[0].value==0);REQUIRE(r.design.modules[0].nodes.size()==1);
 REQUIRE(r.design.modules[0].nodes[0].name=="a");
}
TEST_CASE(optimizer_preserves_divide_by_zero){
 const auto d=lower_source("module divzero(output logic [7:0] y);assign y=8'd7/8'd0;endmodule");
 const synvanta::RtlOptimizer o;const auto r=o.optimize(d);REQUIRE(r.success());
 bool found=false;for(const auto& n:r.design.modules[0].nodes)found=found||n.op==synvanta::RtlOp::Divide;REQUIRE(found);
}
TEST_CASE(optimizer_uses_signed_comparison_semantics){
 const auto d=lower_source("module cmp(output logic y);assign y=8'shff<8'sh01;endmodule");
 const synvanta::RtlOptimizer o;const auto r=o.optimize(d);REQUIRE(r.success());
 REQUIRE(r.design.modules[0].nodes.size()==1);REQUIRE(r.design.modules[0].nodes[0].op==synvanta::RtlOp::Constant);
 REQUIRE(r.design.modules[0].nodes[0].literal=="1'h1");
}

TEST_CASE(optimizer_does_not_fold_negative_shift_counts){
 const auto d=lower_source("module neg_shift(output logic [7:0] y);assign y=8'h80>>8'shff;endmodule");
 const synvanta::RtlOptimizer o;const auto r=o.optimize(d);REQUIRE(r.success());
 bool found=false;for(const auto& n:r.design.modules[0].nodes)found=found||n.op==synvanta::RtlOp::ShiftRight;REQUIRE(found);
}


TEST_CASE(optimizer_removes_unobservable_internal_logic) {
    const auto design = lower_source(
        "module dce(input logic a,input logic b,output logic y);"
        "logic unused; assign unused=a&b; assign y=a|b; endmodule");

    const synvanta::RtlOptimizer optimizer;
    const auto result = optimizer.optimize(design);
    REQUIRE(result.success());
    const auto& module = result.design.modules[0];

    REQUIRE(result.stats.dead_nodes_removed == 1);
    REQUIRE(result.stats.assignments_removed == 1);
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
    REQUIRE(result.stats.assignments_removed == 1);
}

TEST_CASE(optimizer_preserves_assignments_in_modules_without_outputs) {
    const auto design = lower_source(
        "module helper(input logic a,input logic b);"
        "logic temp; assign temp=a^b; endmodule");

    const synvanta::RtlOptimizer optimizer;
    const auto result = optimizer.optimize(design);
    REQUIRE(result.success());
    REQUIRE(result.design.modules[0].assignments.size() == 1);
    REQUIRE(result.stats.assignments_removed == 0);
}

TEST_CASE(optimizer_simplifies_mux_with_identical_data_inputs) {
    const auto design = lower_source(
        "module mux(input logic sel,input logic a,output logic y);"
        "assign y=sel?a:a; endmodule");

    const synvanta::RtlOptimizer optimizer;
    const auto result = optimizer.optimize(design);
    REQUIRE(result.success());
    const auto& module = result.design.modules[0];

    REQUIRE(result.stats.local_simplifications == 1);
    REQUIRE(module.nodes.size() == 1);
    REQUIRE(module.nodes[0].op == synvanta::RtlOp::Input);
    REQUIRE(module.nodes[0].name == "a");
    REQUIRE(module.assignments[0].value == 0);
}

TEST_CASE(optimizer_does_not_apply_two_state_identities_to_logic_inputs) {
    const auto design = lower_source(
        "module four_state(input logic a,output logic y);"
        "assign y=a^a; endmodule");

    const synvanta::RtlOptimizer optimizer;
    const auto result = optimizer.optimize(design);
    REQUIRE(result.success());
    const auto& module = result.design.modules[0];

    REQUIRE(module.nodes.size() == 2);
    REQUIRE(module.nodes[1].op == synvanta::RtlOp::BitXor);
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
