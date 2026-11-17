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
