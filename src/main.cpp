#include "synvanta/aig.hpp"
#include "synvanta/compiler.hpp"
#include "synvanta/lowering.hpp"
#include "synvanta/optimizer.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {
void usage(std::ostream& output) {
    output << "SYNVANTA 0.1.0 - independent SystemVerilog-to-ASIC synthesis compiler\n\n"
           << "Usage:\n  synvanta --version\n  synvanta --help\n"
           << "  synvanta check <input.sv>\n  synvanta inspect <input.sv>\n"
           << "  synvanta ir <input.sv>\n  synvanta opt <input.sv>\n"
           << "  synvanta aig <input.sv>\n";
}

int run(const std::string& command, const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "error: cannot open input file: " << path << '\n';
        return 2;
    }
    const std::string source{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};

    const synvanta::Compiler compiler;
    const auto compiled = compiler.compile(source, path);
    for (const auto& diagnostic : compiled.diagnostics)
        std::cerr << synvanta::format_diagnostic(diagnostic) << '\n';
    if (!compiled.success()) return 1;

    if (command == "check") {
        std::cout << "SYNVANTA: syntax and semantic checks passed ("
                  << compiled.unit.modules.size() << " module(s)).\n";
        return 0;
    }

    if (command == "ir" || command == "opt" || command == "aig") {
        const synvanta::RtlLowerer rtl_lowerer;
        const auto rtl = rtl_lowerer.lower(compiled.unit);
        for (const auto& diagnostic : rtl.diagnostics)
            std::cerr << synvanta::format_diagnostic(diagnostic) << '\n';
        if (!rtl.success()) return 1;

        if (command == "ir") {
            std::cout << synvanta::dump_rtl_design(rtl.design);
            return 0;
        }

        const synvanta::RtlOptimizer optimizer;
        const auto optimized = optimizer.optimize(rtl.design);
        for (const auto& diagnostic : optimized.diagnostics)
            std::cerr << synvanta::format_diagnostic(diagnostic) << '\n';
        if (!optimized.success()) return 1;

        if (command == "opt") {
            for (const auto& stats : optimized.stats) {
                std::cout << "optimization " << stats.module_name
                          << ": nodes " << stats.nodes_before << " -> " << stats.nodes_after_dce
                          << " (CSE merges " << stats.cse_merged
                          << ", local simplifications " << stats.simplified_nodes
                          << ", dead nodes " << stats.dead_nodes << ")\n";
            }
            std::cout << synvanta::dump_rtl_design(optimized.design);
            return 0;
        }

        const synvanta::AigLowerer aig_lowerer;
        const auto aig = aig_lowerer.lower(optimized.design);
        for (const auto& diagnostic : aig.diagnostics)
            std::cerr << synvanta::format_diagnostic(diagnostic) << '\n';
        if (!aig.success()) return 1;
        std::cout << synvanta::dump_aig_design(aig.design);
        return 0;
    }

    for (const auto& module : compiled.unit.modules) {
        std::cout << "module " << module.name << "\n  ports: " << module.ports.size() << '\n';
        for (const auto& port : module.ports) {
            std::cout << "    " << synvanta::port_direction_name(port.direction) << ' '
                      << synvanta::net_kind_name(port.net_kind) << ' ';
            if (port.is_signed) std::cout << "signed ";
            if (port.range) std::cout << '[' << port.range->left << ':' << port.range->right << "] ";
            std::cout << port.name << '\n';
        }
        std::cout << "  continuous assignments: " << module.assignments.size() << '\n';
    }
    return 0;
}
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "SYNVANTA 0.1.0\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--help") {
        usage(std::cout);
        return 0;
    }
    if (argc == 3) {
        const std::string command(argv[1]);
        if (command == "check" || command == "inspect" || command == "ir" ||
            command == "opt" || command == "aig")
            return run(command, argv[2]);
    }
    usage(std::cerr);
    return 2;
}
