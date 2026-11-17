#include "test.hpp"

#include "synvanta/aig.hpp"
#include "synvanta/compiler.hpp"
#include "synvanta/lowering.hpp"
#include "synvanta/optimizer.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace {

synvanta::AigBuildResult compile_to_aig(const std::string& source) {
    const synvanta::Compiler compiler;
    const auto parsed = compiler.compile(source, "aig_test.sv");
    if (!parsed.success()) return {{}, parsed.diagnostics};

    const synvanta::RtlLowerer lowerer;
    const auto lowered = lowerer.lower(parsed.unit);
    if (!lowered.success()) return {{}, lowered.diagnostics};

    const synvanta::RtlOptimizer optimizer;
    const auto optimized = optimizer.optimize(lowered.design);
    if (!optimized.success()) return {{}, optimized.diagnostics};

    const synvanta::AigBuilder builder;
    return builder.build(optimized.design);
}

bool literal_value(synvanta::AigLiteral literal, const std::vector<bool>& node_values) {
    const bool value = node_values.at(synvanta::aig_node_id(literal));
    return (literal & 1U) != 0U ? !value : value;
}

std::vector<bool> evaluate_outputs(const synvanta::AigModule& module,
                                   const std::vector<bool>& inputs) {
    std::vector<bool> node_values(module.nodes.size(), false);
    for (std::size_t index = 1; index < module.nodes.size(); ++index) {
        const auto& node = module.nodes[index];
        if (node.kind == synvanta::AigNodeKind::Input) {
            node_values[index] = inputs.at(node.input_index);
        } else if (node.kind == synvanta::AigNodeKind::And) {
            node_values[index] = literal_value(node.lhs, node_values) &&
                                 literal_value(node.rhs, node_values);
        }
    }

    std::vector<bool> outputs;
    outputs.reserve(module.outputs.size());
    for (const auto& output : module.outputs)
        outputs.push_back(literal_value(output.driver, node_values));
    return outputs;
}

std::uint32_t collect_vector_output(const synvanta::AigModule& module,
                                    const std::vector<bool>& outputs,
                                    const std::string& port_name) {
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < module.outputs.size(); ++index) {
        const auto& output = module.outputs[index];
        if (output.port_name == port_name && outputs[index])
            value |= (std::uint32_t{1} << output.bit_index);
    }
    return value;
}

} // namespace

TEST_CASE(aig_builds_half_adder_and_matches_all_input_combinations) {
    const auto result = compile_to_aig(
        "module half_adder(input logic a,input logic b,output logic sum,output logic carry);"
        "assign sum=a^b; assign carry=a&b; endmodule");
    REQUIRE(result.success());
    REQUIRE(result.design.modules.size() == 1);

    const auto& module = result.design.modules.front();
    REQUIRE(module.inputs.size() == 2);
    REQUIRE(module.outputs.size() == 2);

    for (std::uint32_t a = 0; a < 2; ++a) {
        for (std::uint32_t b = 0; b < 2; ++b) {
            const auto outputs = evaluate_outputs(module, {a != 0, b != 0});
            REQUIRE(outputs.size() == 2);
            REQUIRE(outputs[0] == ((a ^ b) != 0));
            REQUIRE(outputs[1] == ((a & b) != 0));
        }
    }
}

TEST_CASE(aig_ripple_adder_matches_exhaustive_four_bit_addition) {
    const auto result = compile_to_aig(
        "module add4(input logic [3:0] a,input logic [3:0] b,output logic [3:0] y);"
        "assign y=a+b; endmodule");
    REQUIRE(result.success());
    const auto& module = result.design.modules.front();
    REQUIRE(module.inputs.size() == 8);
    REQUIRE(module.outputs.size() == 4);

    for (std::uint32_t a = 0; a < 16; ++a) {
        for (std::uint32_t b = 0; b < 16; ++b) {
            std::vector<bool> inputs(8, false);
            for (std::uint32_t bit = 0; bit < 4; ++bit) {
                inputs[bit] = ((a >> bit) & 1U) != 0U;
                inputs[4U + bit] = ((b >> bit) & 1U) != 0U;
            }
            const auto outputs = evaluate_outputs(module, inputs);
            REQUIRE(collect_vector_output(module, outputs, "y") == ((a + b) & 0xFU));
        }
    }
}

TEST_CASE(aig_signed_less_than_matches_four_bit_twos_complement) {
    const auto result = compile_to_aig(
        "module signed_lt(input logic signed [3:0] a,input logic signed [3:0] b,output logic y);"
        "assign y=a<b; endmodule");
    REQUIRE(result.success());
    const auto& module = result.design.modules.front();

    const auto signed_value = [](std::uint32_t value) {
        return (value & 8U) != 0U ? static_cast<int>(value) - 16 : static_cast<int>(value);
    };

    for (std::uint32_t a = 0; a < 16; ++a) {
        for (std::uint32_t b = 0; b < 16; ++b) {
            std::vector<bool> inputs(8, false);
            for (std::uint32_t bit = 0; bit < 4; ++bit) {
                inputs[bit] = ((a >> bit) & 1U) != 0U;
                inputs[4U + bit] = ((b >> bit) & 1U) != 0U;
            }
            const auto outputs = evaluate_outputs(module, inputs);
            REQUIRE(outputs.size() == 1);
            REQUIRE(outputs[0] == (signed_value(a) < signed_value(b)));
        }
    }
}

TEST_CASE(aig_mux_selects_each_vector_bit) {
    const auto result = compile_to_aig(
        "module mux4(input logic [3:0] a,input logic [3:0] b,input logic sel,output logic [3:0] y);"
        "assign y=sel?a:b; endmodule");
    REQUIRE(result.success());
    const auto& module = result.design.modules.front();

    for (std::uint32_t a = 0; a < 16; ++a) {
        for (std::uint32_t b = 0; b < 16; ++b) {
            for (std::uint32_t select = 0; select < 2; ++select) {
                std::vector<bool> inputs(9, false);
                for (std::uint32_t bit = 0; bit < 4; ++bit) {
                    inputs[bit] = ((a >> bit) & 1U) != 0U;
                    inputs[4U + bit] = ((b >> bit) & 1U) != 0U;
                }
                inputs[8] = select != 0;
                const auto outputs = evaluate_outputs(module, inputs);
                REQUIRE(collect_vector_output(module, outputs, "y") ==
                        (select != 0 ? a : b));
            }
        }
    }
}

TEST_CASE(aig_multiply_matches_exhaustive_three_bit_multiplication) {
    const auto result = compile_to_aig(
        "module mul3(input logic [2:0] a,input logic [2:0] b,output logic [2:0] y);"
        "assign y=a*b; endmodule");
    REQUIRE(result.success());
    const auto& module = result.design.modules.front();

    for (std::uint32_t a = 0; a < 8; ++a) {
        for (std::uint32_t b = 0; b < 8; ++b) {
            std::vector<bool> inputs(6, false);
            for (std::uint32_t bit = 0; bit < 3; ++bit) {
                inputs[bit] = ((a >> bit) & 1U) != 0U;
                inputs[3U + bit] = ((b >> bit) & 1U) != 0U;
            }
            const auto outputs = evaluate_outputs(module, inputs);
            REQUIRE(collect_vector_output(module, outputs, "y") == ((a * b) & 7U));
        }
    }
}

TEST_CASE(aig_preserves_unbased_unsized_fill_literals) {
    const auto result = compile_to_aig(
        "module fills(output logic [7:0] ones,output logic [7:0] zeros);"
        "assign ones='1; assign zeros='0; endmodule");
    REQUIRE(result.success());
    const auto& module = result.design.modules.front();
    const auto outputs = evaluate_outputs(module, {});
    REQUIRE(collect_vector_output(module, outputs, "ones") == 0xFFU);
    REQUIRE(collect_vector_output(module, outputs, "zeros") == 0U);
}

TEST_CASE(aig_structural_hashing_shares_commutative_and_nodes) {
    const auto result = compile_to_aig(
        "module shared(input logic a,input logic b,output logic y,output logic z);"
        "assign y=a&b; assign z=b&a; endmodule");
    REQUIRE(result.success());
    const auto& module = result.design.modules.front();

    std::size_t and_count = 0;
    for (const auto& node : module.nodes)
        if (node.kind == synvanta::AigNodeKind::And) ++and_count;
    REQUIRE(and_count == 1);
    REQUIRE(module.outputs[0].driver == module.outputs[1].driver);
}

TEST_CASE(aig_reports_unsupported_division_and_unknown_constants) {
    const auto division = compile_to_aig(
        "module div(input logic [3:0] a,input logic [3:0] b,output logic [3:0] y);"
        "assign y=a/b; endmodule");
    REQUIRE(!division.success());
    bool division_reported = false;
    for (const auto& diagnostic : division.diagnostics)
        division_reported = division_reported || diagnostic.code == "AIG0006";
    REQUIRE(division_reported);

    const auto unknown = compile_to_aig(
        "module unknown(input logic a,output logic y);assign y=a&1'bx;endmodule");
    REQUIRE(!unknown.success());
    bool unknown_reported = false;
    for (const auto& diagnostic : unknown.diagnostics)
        unknown_reported = unknown_reported || diagnostic.code == "AIG0004";
    REQUIRE(unknown_reported);
}
