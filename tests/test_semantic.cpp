#include "test.hpp"

#include "synvanta/compiler.hpp"

#include <string>

TEST_CASE(semantic_accepts_declared_internal_nets) {
    const std::string source = R"SV(
module internal_net(input logic a, output logic y);
    logic temp;
    assign temp = ~a;
    assign y = temp;
endmodule
)SV";
    const synvanta::Compiler compiler;
    const auto result = compiler.compile(source, "internal_net.sv");
    REQUIRE(result.success());
    REQUIRE(result.unit.modules[0].nets.size() == 1);
}

TEST_CASE(semantic_rejects_undeclared_rhs_identifier) {
    const synvanta::Compiler compiler;
    const auto result = compiler.compile(
        "module bad(input logic a, output logic y); assign y = a & missing; endmodule", "undeclared.sv");
    REQUIRE(!result.success());
    REQUIRE(result.diagnostics.back().code == "SEM0006");
}

TEST_CASE(semantic_rejects_undeclared_assignment_target) {
    const synvanta::Compiler compiler;
    const auto result = compiler.compile(
        "module bad(input logic a, output logic y); assign ghost = a; assign y = a; endmodule", "target.sv");
    REQUIRE(!result.success());
    bool found = false;
    for (const auto& diagnostic : result.diagnostics) found = found || diagnostic.code == "SEM0003";
    REQUIRE(found);
}

TEST_CASE(semantic_rejects_driving_an_input_port) {
    const synvanta::Compiler compiler;
    const auto result = compiler.compile(
        "module bad(input logic a, input logic b); assign a = b; endmodule", "input_driver.sv");
    REQUIRE(!result.success());
    REQUIRE(result.diagnostics.front().code == "SEM0004");
}

TEST_CASE(semantic_rejects_multiple_continuous_drivers) {
    const synvanta::Compiler compiler;
    const auto result = compiler.compile(
        "module bad(input logic a, input logic b, output logic y); assign y = a; assign y = b; endmodule", "multi_driver.sv");
    REQUIRE(!result.success());
    bool found = false;
    for (const auto& diagnostic : result.diagnostics) found = found || diagnostic.code == "SEM0005";
    REQUIRE(found);
}

TEST_CASE(semantic_warns_about_undriven_outputs) {
    const synvanta::Compiler compiler;
    const auto result = compiler.compile("module bad(input logic a, output logic y); endmodule", "undriven.sv");
    REQUIRE(result.success());
    REQUIRE(result.diagnostics.size() == 1);
    REQUIRE(result.diagnostics.front().severity == synvanta::DiagnosticSeverity::Warning);
    REQUIRE(result.diagnostics.front().code == "SEM1001");
}

TEST_CASE(semantic_allows_multiple_continuous_drivers_on_wire_nets) {
    const synvanta::Compiler compiler;
    const auto result = compiler.compile(
        "module wired(input logic a, input logic b, output wire y); assign y=a; assign y=b; endmodule",
        "multi_driver_wire.sv");
    REQUIRE(result.success());
}
