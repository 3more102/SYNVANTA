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
 REQUIRE(r.design.modules[0].assignments[0].value==0);REQUIRE(r.design.modules[0].nodes.size()==1);
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
TEST_CASE(optimizer_removes_unobservable_internal_assignment_logic) {
 const auto d=lower_source("module dead_logic(input logic a,input logic b,output logic y);logic unused;assign unused=a&b;assign y=a|b;endmodule");
 const synvanta::RtlOptimizer o;const auto r=o.optimize(d);REQUIRE(r.success());
 const auto& m=r.design.modules[0];
 REQUIRE(r.stats.dead_nodes_removed==1);REQUIRE(r.stats.assignments_removed==1);
 REQUIRE(m.nodes.size()==3);REQUIRE(m.assignments.size()==1);REQUIRE(m.assignments[0].target=="y");
}
TEST_CASE(optimizer_preserves_internal_dependencies_of_outputs) {
 const auto d=lower_source("module chain(input logic a,input logic b,output logic y);logic temp;assign temp=a^b;assign y=temp&a;endmodule");
 const synvanta::RtlOptimizer o;const auto r=o.optimize(d);REQUIRE(r.success());
 const auto& m=r.design.modules[0];
 REQUIRE(m.nodes.size()==4);REQUIRE(m.nodes[2].op==synvanta::RtlOp::BitXor);REQUIRE(m.nodes[3].op==synvanta::RtlOp::BitAnd);
 REQUIRE(m.assignments.size()==1);REQUIRE(m.assignments[0].target=="y");
}
TEST_CASE(optimizer_keeps_all_assignments_when_module_has_no_outputs) {
 const auto d=lower_source("module sink(input logic a);logic temp;assign temp=~a;endmodule");
 const synvanta::RtlOptimizer o;const auto r=o.optimize(d);REQUIRE(r.success());
 REQUIRE(r.design.modules[0].assignments.size()==1);REQUIRE(r.design.modules[0].assignments[0].target=="temp");
 REQUIRE(r.design.modules[0].nodes.size()==2);
}
TEST_CASE(optimizer_expands_unbased_unsized_one_to_context_width) {
 const auto d=lower_source("module fill(output logic [7:0] y);assign y='1;endmodule");
 const synvanta::RtlOptimizer o;const auto r=o.optimize(d);REQUIRE(r.success());
 REQUIRE(r.design.modules[0].nodes.size()==1);REQUIRE(r.design.modules[0].nodes[0].literal=="8'hFF");
}
TEST_CASE(optimizer_expands_unbased_unsized_zero_to_context_width) {
 const auto d=lower_source("module fill(output logic [7:0] y);assign y='0;endmodule");
 const synvanta::RtlOptimizer o;const auto r=o.optimize(d);REQUIRE(r.success());
 REQUIRE(r.design.modules[0].nodes.size()==1);REQUIRE(r.design.modules[0].nodes[0].literal=="8'h0");
}
TEST_CASE(optimizer_rejects_non_topological_rtl_graphs) {
 synvanta::RtlDesign d;synvanta::RtlModule m;m.name="invalid";
 m.ports.push_back({"y",synvanta::PortDirection::Output,1,false});
 synvanta::RtlNode n;n.id=0;n.op=synvanta::RtlOp::BitAnd;n.width=1;n.operands={1,1};
 m.nodes.push_back(n);m.assignments.push_back({"y",0,1,false});d.modules.push_back(m);
 const synvanta::RtlOptimizer o;const auto r=o.optimize(d);REQUIRE(!r.success());
 REQUIRE(r.diagnostics.size()==1);REQUIRE(r.diagnostics[0].code=="OPT0004");
}
