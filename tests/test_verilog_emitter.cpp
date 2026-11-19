#include "test.hpp"
#include "synvanta/aig.hpp"
#include "synvanta/compiler.hpp"
#include "synvanta/lowering.hpp"
#include "synvanta/optimizer.hpp"
#include "synvanta/verilog_emitter.hpp"
#include <string>

namespace {
synvanta::AigBuildResult compile_aig(const std::string& source) {
    const synvanta::Compiler compiler;
    const auto parsed = compiler.compile(source, "emitter.sv");
    if (!parsed.success()) return {{}, parsed.diagnostics};
    const synvanta::RtlLowerer lowerer;
    const auto lowered = lowerer.lower(parsed.unit);
    if (!lowered.success()) return {{}, lowered.diagnostics};
    const synvanta::RtlOptimizer optimizer;
    const auto optimized = optimizer.optimize(lowered.design);
    if (!optimized.success()) return {{}, optimized.diagnostics};
    const synvanta::AigBuilder builder;
    const auto built = builder.build(optimized.design);
    if (!built.success()) return built;
    const synvanta::AigOptimizer aig_optimizer;
    const auto balanced = aig_optimizer.optimize(built.design);
    return {balanced.design, balanced.diagnostics};
}
} // namespace

TEST_CASE(generic_verilog_emits_half_adder_deterministically) {
    const auto built = compile_aig(
        "module half_adder(input logic a,input logic b,output logic sum,output logic carry);"
        "assign sum=a^b;assign carry=a&b;endmodule");
    REQUIRE(built.success());
    const synvanta::GenericVerilogEmitter emitter;
    const auto result = emitter.emit(built.design);
    REQUIRE(result.success());
    REQUIRE(result.verilog == emitter.emit(built.design).verilog);
    REQUIRE(result.verilog.find("module \\half_adder ") != std::string::npos);
    REQUIRE(result.verilog.find("input wire \\a ") != std::string::npos);
    REQUIRE(result.verilog.find("assign \\carry ") != std::string::npos);
    REQUIRE(result.verilog.find("assign \\sum ") != std::string::npos);
    REQUIRE(result.verilog.find(" & ") != std::string::npos);
}
TEST_CASE(generic_verilog_emits_vectors_and_inverted_edges) {
    const auto built = compile_aig(
        "module mixed(input logic [3:0] a,input logic [3:0] b,"
        "output logic [3:0] y);assign y=~(a&b);endmodule");
    REQUIRE(built.success());
    const auto result = synvanta::GenericVerilogEmitter{}.emit(built.design);
    REQUIRE(result.success());
    REQUIRE(result.verilog.find("input wire [3:0] \\a ") != std::string::npos);
    REQUIRE(result.verilog.find("output wire [3:0] \\y ") != std::string::npos);
    REQUIRE(result.verilog.find("\\y [3]") != std::string::npos);
    REQUIRE(result.verilog.find("~(") != std::string::npos);
}
TEST_CASE(generic_verilog_emits_constant_outputs) {
    const auto built = compile_aig(
        "module fills(output logic [3:0] ones,output logic zero);"
        "assign ones='1;assign zero='0;endmodule");
    REQUIRE(built.success());
    const auto result = synvanta::GenericVerilogEmitter{}.emit(built.design);
    REQUIRE(result.success());
    REQUIRE(result.verilog.find("1'b1") != std::string::npos);
    REQUIRE(result.verilog.find("1'b0") != std::string::npos);
}
TEST_CASE(generic_verilog_rejects_bad_output_drivers_and_bus_gaps) {
    synvanta::AigDesign design;
    synvanta::AigModule module;
    module.name = "bad";
    module.nodes.push_back({0, synvanta::AigNodeKind::ConstantFalse});
    module.outputs.push_back({"y", 0, 4U});
    design.modules.push_back(module);
    const synvanta::GenericVerilogEmitter emitter;
    auto result = emitter.emit(design);
    REQUIRE(!result.success());
    REQUIRE(result.verilog.empty());
    REQUIRE(result.diagnostics.front().code == "VLOG0007");
    design.modules[0].outputs[0] = {"y", 1, synvanta::AigTrue};
    result = emitter.emit(design);
    REQUIRE(!result.success());
    REQUIRE(result.diagnostics.front().code == "VLOG0004");
}
TEST_CASE(generic_verilog_rejects_input_output_port_collision) {
    synvanta::AigDesign design;
    synvanta::AigModule module;
    module.name = "bad";
    module.nodes.push_back({0, synvanta::AigNodeKind::ConstantFalse});
    synvanta::AigNode node;
    node.id = 1;
    node.kind = synvanta::AigNodeKind::Input;
    node.input_name = "x";
    node.input_index = 0;
    module.nodes.push_back(node);
    module.inputs.push_back({"x", 0, 1});
    module.outputs.push_back({"x", 0, synvanta::AigTrue});
    design.modules.push_back(module);
    const auto result = synvanta::GenericVerilogEmitter{}.emit(design);
    REQUIRE(!result.success());
    REQUIRE(result.diagnostics.front().code == "VLOG0008");
}
