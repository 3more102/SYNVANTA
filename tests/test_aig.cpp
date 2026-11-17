#include "test.hpp"

#include "synvanta/aig.hpp"
#include "synvanta/compiler.hpp"
#include "synvanta/lowering.hpp"

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using Inputs = std::map<std::pair<std::string, std::uint32_t>, bool>;

synvanta::AigModule lower_source(const std::string& source, const std::string& filename = "aig_test.sv") {
    const synvanta::Compiler compiler;
    const auto parsed = compiler.compile(source, filename);
    if (!parsed.success()) throw std::runtime_error("source did not parse and pass semantic analysis");

    const synvanta::RtlLowerer rtl_lowerer;
    const auto rtl = rtl_lowerer.lower(parsed.unit);
    if (!rtl.success()) throw std::runtime_error("source did not lower into RTL IR");

    const synvanta::AigLowerer aig_lowerer;
    const auto aig = aig_lowerer.lower(rtl.design);
    if (!aig.success()) throw std::runtime_error("RTL IR did not lower into an AIG");
    return aig.design.modules.at(0);
}

bool evaluate_literal(synvanta::AigLit literal, const std::vector<bool>& values) {
    const auto node = static_cast<std::size_t>(literal >> 1U);
    const bool value = values.at(node);
    return (literal & 1U) != 0 ? !value : value;
}

std::vector<bool> evaluate_nodes(const synvanta::AigModule& module, const Inputs& inputs) {
    const auto& nodes = module.network.nodes();
    std::vector<bool> values(nodes.size(), false);
    for (std::size_t id = 1; id < nodes.size(); ++id) {
        const auto& node = nodes[id];
        if (node.kind == synvanta::AigNodeKind::Input) {
            const auto found = inputs.find({node.input_name, node.input_bit});
            if (found == inputs.end()) throw std::runtime_error("missing AIG input value");
            values[id] = found->second;
        } else if (node.kind == synvanta::AigNodeKind::And) {
            values[id] = evaluate_literal(node.lhs, values) && evaluate_literal(node.rhs, values);
        }
    }
    return values;
}

std::uint64_t output_value(const synvanta::AigModule& module, const Inputs& inputs, const std::string& name) {
    const auto values = evaluate_nodes(module, inputs);
    for (const auto& assignment : module.assignments) {
        if (assignment.target != name) continue;
        std::uint64_t result = 0;
        for (std::size_t bit = 0; bit < assignment.bits.size(); ++bit)
            if (evaluate_literal(assignment.bits[bit], values)) result |= (std::uint64_t{1} << bit);
        return result;
    }
    throw std::runtime_error("missing AIG output assignment: " + name);
}

void set_vector(Inputs& inputs, const std::string& name, std::uint32_t value, std::uint32_t width) {
    for (std::uint32_t bit = 0; bit < width; ++bit)
        inputs[{name, bit}] = ((value >> bit) & 1U) != 0;
}

int signed4(std::uint32_t value) {
    return (value & 8U) != 0 ? static_cast<int>(value) - 16 : static_cast<int>(value);
}
}

TEST_CASE(aig_lowers_and_simulates_half_adder) {
    const auto module = lower_source(
        "module half_adder(input logic a,input logic b,output logic sum,output logic carry);"
        "assign sum=a^b;assign carry=a&b;endmodule");
    REQUIRE(module.network.input_count() == 2);
    for (std::uint32_t a = 0; a < 2; ++a) {
        for (std::uint32_t b = 0; b < 2; ++b) {
            Inputs inputs{{{"a", 0}, a != 0}, {{"b", 0}, b != 0}};
            REQUIRE(output_value(module, inputs, "sum") == (a ^ b));
            REQUIRE(output_value(module, inputs, "carry") == (a & b));
        }
    }
}

TEST_CASE(aig_addition_and_subtraction_match_four_bit_reference) {
    const auto module = lower_source(
        "module arithmetic(input logic [3:0] a,input logic [3:0] b,"
        "output logic [3:0] sum,output logic [3:0] diff);"
        "assign sum=a+b;assign diff=a-b;endmodule");
    for (std::uint32_t a = 0; a < 16; ++a) {
        for (std::uint32_t b = 0; b < 16; ++b) {
            Inputs inputs;
            set_vector(inputs, "a", a, 4);
            set_vector(inputs, "b", b, 4);
            REQUIRE(output_value(module, inputs, "sum") == ((a + b) & 0xFU));
            REQUIRE(output_value(module, inputs, "diff") == ((a - b) & 0xFU));
        }
    }
}

TEST_CASE(aig_mux_matches_all_four_bit_inputs) {
    const auto module = lower_source(
        "module mux4(input logic [3:0] a,input logic [3:0] b,input logic sel,output logic [3:0] y);"
        "assign y=sel?a:b;endmodule");
    for (std::uint32_t a = 0; a < 16; ++a) {
        for (std::uint32_t b = 0; b < 16; ++b) {
            for (std::uint32_t select = 0; select < 2; ++select) {
                Inputs inputs;
                set_vector(inputs, "a", a, 4);
                set_vector(inputs, "b", b, 4);
                inputs[{"sel", 0}] = select != 0;
                REQUIRE(output_value(module, inputs, "y") == (select != 0 ? a : b));
            }
        }
    }
}

TEST_CASE(aig_signed_comparison_matches_twos_complement_reference) {
    const auto module = lower_source(
        "module signed_cmp(input logic signed [3:0] a,input logic signed [3:0] b,output logic lt);"
        "assign lt=a<b;endmodule");
    for (std::uint32_t a = 0; a < 16; ++a) {
        for (std::uint32_t b = 0; b < 16; ++b) {
            Inputs inputs;
            set_vector(inputs, "a", a, 4);
            set_vector(inputs, "b", b, 4);
            REQUIRE(output_value(module, inputs, "lt") == (signed4(a) < signed4(b) ? 1U : 0U));
        }
    }
}

TEST_CASE(aig_shift_and_multiply_match_four_bit_reference) {
    const auto module = lower_source(
        "module shift_mul(input logic [3:0] a,input logic [2:0] sh,input logic [3:0] b,"
        "output logic [3:0] left_shift,output logic [3:0] right_shift,output logic [3:0] product);"
        "assign left_shift=a<<sh;assign right_shift=a>>sh;assign product=a*b;endmodule");
    for (std::uint32_t a = 0; a < 16; ++a) {
        for (std::uint32_t sh = 0; sh < 8; ++sh) {
            for (std::uint32_t b = 0; b < 16; ++b) {
                Inputs inputs;
                set_vector(inputs, "a", a, 4);
                set_vector(inputs, "sh", sh, 3);
                set_vector(inputs, "b", b, 4);
                REQUIRE(output_value(module, inputs, "left_shift") == ((a << sh) & 0xFU));
                REQUIRE(output_value(module, inputs, "right_shift") == (a >> sh));
                REQUIRE(output_value(module, inputs, "product") == ((a * b) & 0xFU));
            }
        }
    }
}

TEST_CASE(aig_structural_hashing_reuses_identical_and_nodes) {
    const auto module = lower_source(
        "module common(input logic a,input logic b,output logic y,output logic z);"
        "assign y=a&b;assign z=a&b;endmodule");
    REQUIRE(module.network.and_count() == 1);
    Inputs inputs{{{"a", 0}, true}, {{"b", 0}, false}};
    REQUIRE(output_value(module, inputs, "y") == 0);
    REQUIRE(output_value(module, inputs, "z") == 0);
}

TEST_CASE(aig_sweeps_unreachable_internal_logic) {
    const auto module = lower_source(
        "module dead_cone(input logic a,input logic b,output logic y);"
        "wire unused;assign unused=a&b;assign y=a^b;endmodule");
    REQUIRE(module.assignments.size() == 1);
    REQUIRE(module.assignments.front().target == "y");
    REQUIRE(module.network.and_count() == 3);
}

TEST_CASE(aig_constant_simplification_removes_redundant_and_nodes) {
    const auto module = lower_source(
        "module constant_fold(input logic a,output logic y);assign y=a&1'b0;endmodule");
    REQUIRE(module.network.and_count() == 0);
    Inputs inputs{{{"a", 0}, true}};
    REQUIRE(output_value(module, inputs, "y") == 0);
}

TEST_CASE(aig_reports_division_as_not_yet_supported) {
    const synvanta::Compiler compiler;
    const auto parsed = compiler.compile(
        "module div4(input logic [3:0] a,input logic [3:0] b,output logic [3:0] y);"
        "assign y=a/b;endmodule", "division.sv");
    REQUIRE(parsed.success());
    const synvanta::RtlLowerer rtl_lowerer;
    const auto rtl = rtl_lowerer.lower(parsed.unit);
    REQUIRE(rtl.success());
    const synvanta::AigLowerer aig_lowerer;
    const auto aig = aig_lowerer.lower(rtl.design);
    REQUIRE(!aig.success());
    REQUIRE(!aig.diagnostics.empty());
    REQUIRE(aig.diagnostics.front().code == "AIG0003");
}

TEST_CASE(aig_dump_exposes_and_nodes_and_assignments) {
    const auto module = lower_source(
        "module invert(input logic a,output logic y);assign y=~a;endmodule");
    synvanta::AigDesign design;
    design.modules.push_back(module);
    const auto dump = synvanta::dump_aig_design(design);
    REQUIRE(dump.find("module invert") != std::string::npos);
    REQUIRE(dump.find("input a[0]") != std::string::npos);
    REQUIRE(dump.find("assign y") != std::string::npos);
}

TEST_CASE(aig_rejects_undriven_output_ports) {
    const synvanta::Compiler compiler;
    const auto parsed = compiler.compile(
        "module undriven(input logic a,output logic y); endmodule", "undriven.sv");
    REQUIRE(parsed.success());

    const synvanta::RtlLowerer rtl_lowerer;
    const auto rtl = rtl_lowerer.lower(parsed.unit);
    REQUIRE(rtl.success());

    const synvanta::AigLowerer aig_lowerer;
    const auto aig = aig_lowerer.lower(rtl.design);
    REQUIRE(!aig.success());
    REQUIRE(!aig.diagnostics.empty());
    REQUIRE(aig.diagnostics.back().code == "AIG0008");
}
