#include "test.hpp"
#include "synvanta/compiler.hpp"
#include "synvanta/lowering.hpp"
#include <string>
TEST_CASE(lowering_builds_half_adder_boolean_ir){
 const std::string s=R"SV(module half_adder(input logic a,input logic b,output logic sum,output logic carry);assign sum=a^b;assign carry=a&b;endmodule)SV";
 const synvanta::Compiler c;const auto parsed=c.compile(s,"half_adder.sv");REQUIRE(parsed.success());
 const synvanta::RtlLowerer l;const auto r=l.lower(parsed.unit);REQUIRE(r.success());
 REQUIRE(r.design.modules.size()==1);REQUIRE(r.design.modules[0].assignments.size()==2);REQUIRE(r.design.modules[0].nodes.size()==4);
}
TEST_CASE(lowering_preserves_vector_width_and_mux_operator){
 const std::string s=R"SV(module mux(input logic [7:0] a,input logic [7:0] b,input logic sel,output logic [7:0] y);assign y=sel?a:b;endmodule)SV";
 const synvanta::Compiler c;const auto parsed=c.compile(s,"mux.sv");const synvanta::RtlLowerer l;const auto r=l.lower(parsed.unit);REQUIRE(r.success());
 const auto& m=r.design.modules[0];REQUIRE(m.ports[0].width==8);REQUIRE(m.ports[3].width==8);
 REQUIRE(m.nodes.back().op==synvanta::RtlOp::Mux);REQUIRE(m.nodes.back().width==8);
}
TEST_CASE(lowering_inserts_explicit_resize){
 const std::string s="module widen(input logic a,output logic [7:0] y);assign y=a;endmodule";
 const synvanta::Compiler c;const auto parsed=c.compile(s,"widen.sv");const synvanta::RtlLowerer l;const auto r=l.lower(parsed.unit);REQUIRE(r.success());
 REQUIRE(r.design.modules[0].nodes.back().op==synvanta::RtlOp::Resize);REQUIRE(r.design.modules[0].nodes.back().width==8);
}
TEST_CASE(lowering_resolves_internal_net_dependencies){
 const std::string s="module chain(input logic a,output logic y);logic temp;assign temp=~a;assign y=temp;endmodule";
 const synvanta::Compiler c;const auto parsed=c.compile(s,"chain.sv");const synvanta::RtlLowerer l;const auto r=l.lower(parsed.unit);
 REQUIRE(r.success());REQUIRE(r.design.modules[0].assignments.size()==2);
}
TEST_CASE(lowering_rejects_combinational_cycles){
 const std::string s="module loop(input logic a,output logic y);logic temp;assign temp=y;assign y=temp;endmodule";
 const synvanta::Compiler c;const auto parsed=c.compile(s,"loop.sv");REQUIRE(parsed.success());
 const synvanta::RtlLowerer l;const auto r=l.lower(parsed.unit);REQUIRE(!r.success());
 bool found=false;for(const auto& d:r.diagnostics)found=found||d.code=="LOW0003";REQUIRE(found);
}
TEST_CASE(lowering_types_arithmetic_and_sized_constants){
 const std::string s="module add8(input logic [7:0] a,output logic [7:0] y);assign y=a+8'h01;endmodule";
 const synvanta::Compiler c;const auto parsed=c.compile(s,"add8.sv");const synvanta::RtlLowerer l;const auto r=l.lower(parsed.unit);REQUIRE(r.success());
 const auto& n=r.design.modules[0].nodes;REQUIRE(n.size()==3);REQUIRE(n[1].op==synvanta::RtlOp::Constant);
 REQUIRE(n[1].width==8);REQUIRE(n[2].op==synvanta::RtlOp::Add);REQUIRE(n[2].width==8);
}
TEST_CASE(lowering_preserves_signed_port_metadata){
 const std::string s="module signed_passthrough(input logic signed [7:0] a,output logic signed [7:0] y);assign y=a;endmodule";
 const synvanta::Compiler c;const auto parsed=c.compile(s,"signed.sv");const synvanta::RtlLowerer l;const auto r=l.lower(parsed.unit);REQUIRE(r.success());
 REQUIRE(r.design.modules[0].ports[0].is_signed);REQUIRE(r.design.modules[0].ports[1].is_signed);
}

TEST_CASE(lowering_normalizes_binary_operand_widths){
 const std::string s="module add_mixed(input logic [7:0] a,input logic b,output logic [7:0] y);assign y=a+b;endmodule";
 const synvanta::Compiler c;const auto parsed=c.compile(s,"add_mixed.sv");const synvanta::RtlLowerer l;const auto r=l.lower(parsed.unit);REQUIRE(r.success());
 const auto& n=r.design.modules[0].nodes;REQUIRE(n.size()==4);REQUIRE(n[2].op==synvanta::RtlOp::Resize);
 REQUIRE(n[2].width==8);REQUIRE(n[3].op==synvanta::RtlOp::Add);REQUIRE(n[3].operands[0]==0);REQUIRE(n[3].operands[1]==2);
}


TEST_CASE(lowering_rejects_multiple_drivers_on_wire_nets_explicitly) {
    const std::string source =
        "module wired(input logic a, input logic b, output wire y); "
        "assign y=a; assign y=b; endmodule";
    const synvanta::Compiler compiler;
    const auto parsed = compiler.compile(source, "multi_driver_wire.sv");
    REQUIRE(parsed.success());

    const synvanta::RtlLowerer lowerer;
    const auto lowered = lowerer.lower(parsed.unit);
    REQUIRE(!lowered.success());

    bool found = false;
    for (const auto& diagnostic : lowered.diagnostics) {
        found = found || diagnostic.code == "LOW0009";
    }
    REQUIRE(found);
}
