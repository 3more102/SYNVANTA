#include "test.hpp"

#include "synvanta/aig.hpp"
#include "synvanta/compiler.hpp"
#include "synvanta/lowering.hpp"

#include <cstddef>
#include <string>

namespace {

synvanta::RtlDesign lower_source(const std::string& source) {
    const synvanta::Compiler compiler;
    const auto parsed = compiler.compile(source, "aig_test.sv");
    REQUIRE(parsed.success());

    const synvanta::RtlLowerer lowerer;
    const auto lowered = lowerer.lower(parsed.unit);
    REQUIRE(lowered.success());
    return lowered.design;
}

std::size_t count_and_nodes(const synvanta::AigModule& module) {
    std::size_t count = 0;
    for (const auto& node : module.nodes) {
        if (node.kind == synvanta::AigNodeKind::And) ++count;
    }
    return count;
}

} // namespace

TEST_CASE(aig_builds_half_adder_network) {
    const auto design = lower_source(
        "module half_adder(input logic a,input logic b,"
        "output logic sum,output logic carry);"
        "assign sum=a^b; assign carry=a&b; endmodule");

    const synvanta::AigBuilder builder;
    const auto result = builder.build(design);
    REQUIRE(result.success());
    REQUIRE(result.design.modules.size() == 1);

    const auto& module = result.design.modules.front();
    REQUIRE(module.inputs.size() == 2);
    REQUIRE(module.outputs.size() == 2);
    REQUIRE(count_and_nodes(module) == 3);
    REQUIRE(module.outputs[0].port_name == "sum");
    REQUIRE(module.outputs[1].port_name == "carry");
}

TEST_CASE(aig_lowers_vector_bitwise_and) {
    const auto design = lower_source(
        "module and4(input logic [3:0] a,input logic [3:0] b,"
        "output logic [3:0] y); assign y=a&b; endmodule");

    const synvanta::AigBuilder builder;
    const auto result = builder.build(design);
    REQUIRE(result.success());

    const auto& module = result.design.modules.front();
    REQUIRE(module.inputs.size() == 8);
    REQUIRE(module.outputs.size() == 4);
    REQUIRE(count_and_nodes(module) == 4);
}

TEST_CASE(aig_represents_inversion_with_a_complemented_edge) {
    const auto design = lower_source(
        "module invert(input logic a,output logic y); assign y=~a; endmodule");

    const synvanta::AigBuilder builder;
    const auto result = builder.build(design);
    REQUIRE(result.success());

    const auto& module = result.design.modules.front();
    REQUIRE(module.inputs.size() == 1);
    REQUIRE(module.outputs.size() == 1);
    REQUIRE(module.outputs[0].literal ==
            synvanta::aig_invert(module.inputs[0].literal));
    REQUIRE(count_and_nodes(module) == 0);
}

TEST_CASE(aig_lowers_mux_to_and_nodes) {
    const auto design = lower_source(
        "module mux(input logic sel,input logic a,input logic b,"
        "output logic y); assign y=sel?a:b; endmodule");

    const synvanta::AigBuilder builder;
    const auto result = builder.build(design);
    REQUIRE(result.success());

    const auto& module = result.design.modules.front();
    REQUIRE(module.inputs.size() == 3);
    REQUIRE(module.outputs.size() == 1);
    REQUIRE(count_and_nodes(module) == 3);
}

TEST_CASE(aig_structural_hashing_merges_commutative_and_nodes) {
    const auto design = lower_source(
        "module shared(input logic a,input logic b,"
        "output logic y0,output logic y1);"
        "assign y0=a&b; assign y1=b&a; endmodule");

    const synvanta::AigBuilder builder;
    const auto result = builder.build(design);
    REQUIRE(result.success());

    const auto& module = result.design.modules.front();
    REQUIRE(module.outputs.size() == 2);
    REQUIRE(module.outputs[0].literal == module.outputs[1].literal);
    REQUIRE(count_and_nodes(module) == 1);
}

TEST_CASE(aig_rejects_division_until_the_operator_is_lowered) {
    const auto design = lower_source(
        "module div(output logic [7:0] y); assign y=8'd7/8'd2; endmodule");

    const synvanta::AigBuilder builder;
    const auto result = builder.build(design);
    REQUIRE(!result.success());
    REQUIRE(!result.diagnostics.empty());
    REQUIRE(result.diagnostics.back().code == "AIG0016");
}

TEST_CASE(aig_lowers_signed_comparison) {
    const auto design = lower_source(
        "module compare(input logic signed [7:0] a,"
        "input logic signed [7:0] b,output logic y);"
        "assign y=a<b; endmodule");

    const synvanta::AigBuilder builder;
    const auto result = builder.build(design);
    REQUIRE(result.success());
    REQUIRE(result.design.modules.front().outputs.size() == 1);
    REQUIRE(count_and_nodes(result.design.modules.front()) > 0);
}

