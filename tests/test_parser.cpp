#include "test.hpp"
#include "synvanta/compiler.hpp"
#include <string>
TEST_CASE(parser_builds_module_ports_and_assignments){
    const std::string s=R"SV(module half_adder(input logic a,input logic b,output logic sum,output logic carry);
assign sum=a^b; assign carry=a&b; endmodule)SV";
    const synvanta::Compiler c;const auto r=c.compile(s,"half_adder.sv");
    REQUIRE(r.success());REQUIRE(r.unit.modules.size()==1);REQUIRE(r.unit.modules[0].name=="half_adder");
    REQUIRE(r.unit.modules[0].ports.size()==4);REQUIRE(r.unit.modules[0].assignments.size()==2);
    REQUIRE(r.unit.modules[0].assignments[0].rhs->kind==synvanta::ExprKind::Binary);
}
TEST_CASE(parser_preserves_operator_precedence){
    const std::string s="module p(input logic a,input logic b,input logic c,output logic y); assign y=a|b&c; endmodule";
    const synvanta::Compiler c;const auto r=c.compile(s,"precedence.sv");REQUIRE(r.success());
    const auto& e=*r.unit.modules[0].assignments[0].rhs;
    REQUIRE(e.op==synvanta::TokenKind::Pipe);REQUIRE(e.rhs->op==synvanta::TokenKind::Ampersand);
}
TEST_CASE(parser_supports_ranges_and_ternary_expressions){
    const std::string s="module mux(input logic [7:0] a,input logic [7:0] b,input logic sel,output logic [7:0] y); assign y=sel?a:b; endmodule";
    const synvanta::Compiler c;const auto r=c.compile(s,"mux.sv");REQUIRE(r.success());
    REQUIRE(r.unit.modules[0].ports[0].range.has_value());REQUIRE(r.unit.modules[0].ports[0].range->left==7);
    REQUIRE(r.unit.modules[0].assignments[0].rhs->kind==synvanta::ExprKind::Conditional);
}
TEST_CASE(parser_supports_reduction_unary_operators){
    const std::string s="module red(input logic [3:0] a,output logic y); assign y=~&a; endmodule";
    const synvanta::Compiler c;const auto r=c.compile(s,"red.sv");REQUIRE(r.success());
    REQUIRE(r.unit.modules[0].assignments[0].rhs->kind==synvanta::ExprKind::Unary);
    REQUIRE(r.unit.modules[0].assignments[0].rhs->op==synvanta::TokenKind::Nand);
}
TEST_CASE(parser_accepts_multiple_modules){
    const std::string s="module a(input logic x,output logic y); assign y=x; endmodule module b(input logic z,output logic q); assign q=~z; endmodule";
    const synvanta::Compiler c;const auto r=c.compile(s,"multi.sv");
    REQUIRE(r.success());REQUIRE(r.unit.modules.size()==2);REQUIRE(r.unit.modules[1].name=="b");
}
TEST_CASE(parser_reports_missing_semicolon){
    const synvanta::Compiler c;const auto r=c.compile("module bad(input logic a,output logic y); assign y=a endmodule","bad.sv");
    REQUIRE(!r.success());REQUIRE(!r.diagnostics.empty());
}

TEST_CASE(parser_separates_based_literal_from_ternary_operator){
 const synvanta::Compiler c;
 const auto r=c.compile("module mux(input logic a,input logic b,output logic y);assign y=1'b1?a:b;endmodule","literal_ternary.sv");
 REQUIRE(r.success());REQUIRE(r.unit.modules[0].assignments[0].rhs->kind==synvanta::ExprKind::Conditional);
}
