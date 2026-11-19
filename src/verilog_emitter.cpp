#include "synvanta/verilog_emitter.hpp"
#include <cctype>
#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace synvanta {
namespace {
struct Port {
    std::string name;
    std::map<std::uint32_t, AigLiteral> bits;
};

bool valid_name(const std::string& name) {
    if (name.empty()) return false;
    const auto first = static_cast<unsigned char>(name.front());
    if (std::isalpha(first) == 0 && name.front() != '_') return false;
    for (const char ch : name) {
        const auto c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) == 0 && ch != '_' && ch != '$') return false;
    }
    return true;
}
std::string escaped(const std::string& name) { return "\\" + name + " "; }

bool insert_bit(std::vector<Port>& ports,
                std::unordered_map<std::string, std::size_t>& lookup,
                const std::string& name, std::uint32_t bit, AigLiteral literal,
                std::vector<Diagnostic>& diagnostics) {
    if (!valid_name(name)) {
        diagnostics.push_back({DiagnosticSeverity::Error, "VLOG0002",
                               "invalid AIG port identifier: " + name, {}});
        return false;
    }
    auto it = lookup.find(name);
    if (it == lookup.end()) {
        const auto position = ports.size();
        ports.push_back({name, {}});
        lookup.emplace(name, position);
        it = lookup.find(name);
    }
    if (!ports[it->second].bits.emplace(bit, literal).second) {
        diagnostics.push_back({DiagnosticSeverity::Error, "VLOG0003",
                               "duplicate AIG port bit: " + name, {}});
        return false;
    }
    return true;
}
bool complete_bits(const std::vector<Port>& ports,
                   std::vector<Diagnostic>& diagnostics) {
    for (const auto& port : ports) {
        std::size_t expected = 0;
        for (const auto& [bit, literal] : port.bits) {
            (void)literal;
            if (bit != expected) {
                diagnostics.push_back({DiagnosticSeverity::Error, "VLOG0004",
                                       "noncontiguous AIG port bits: " + port.name, {}});
                return false;
            }
            ++expected;
        }
    }
    return true;
}

bool collect(const AigModule& module, std::vector<Port>& inputs,
             std::vector<Port>& outputs, std::vector<Diagnostic>& diagnostics) {
    auto fail = [&](const std::string& code, const std::string& message) {
        diagnostics.push_back({DiagnosticSeverity::Error, code, message, {}});
        return false;
    };
    if (!valid_name(module.name))
        return fail("VLOG0001", "invalid module name");
    if (module.nodes.empty() || module.nodes[0].id != 0 ||
        module.nodes[0].kind != AigNodeKind::ConstantFalse)
        return fail("VLOG0005", "AIG node zero must be constant false");

    for (std::size_t i = 0; i < module.nodes.size(); ++i) {
        const auto& node = module.nodes[i];
        if (node.id != i)
            return fail("VLOG0005", "AIG node IDs must match dense indices");
        if (i != 0 && node.kind == AigNodeKind::ConstantFalse)
            return fail("VLOG0005", "constant node outside index zero");
        if (node.kind == AigNodeKind::And &&
            (aig_node_id(node.lhs) >= i || aig_node_id(node.rhs) >= i))
            return fail("VLOG0005", "AIG AND operands must precede their node");
    }

    std::unordered_map<std::string, std::size_t> in_lookup;
    std::unordered_map<std::string, std::size_t> out_lookup;
    std::vector<bool> registered(module.nodes.size(), false);
    for (std::size_t i = 0; i < module.inputs.size(); ++i) {
        const auto& input = module.inputs[i];
        if (input.node_id == 0 || input.node_id >= module.nodes.size())
            return fail("VLOG0006", "AIG primary input has an invalid node");
        const auto& node = module.nodes[input.node_id];
        if (node.kind != AigNodeKind::Input || node.input_index != i ||
            node.input_name != input.port_name || node.input_bit != input.bit_index ||
            registered[input.node_id])
            return fail("VLOG0006", "inconsistent AIG input metadata");
        registered[input.node_id] = true;
        if (!insert_bit(inputs, in_lookup, input.port_name, input.bit_index,
                        input.node_id << 1U, diagnostics)) return false;
    }
    for (std::size_t i = 1; i < module.nodes.size(); ++i) {
        if (module.nodes[i].kind == AigNodeKind::Input && !registered[i])
            return fail("VLOG0006", "unregistered AIG input node");
    }
    for (const auto& output : module.outputs) {
        if (aig_node_id(output.driver) >= module.nodes.size())
            return fail("VLOG0007", "AIG output has an invalid driver");
        if (in_lookup.contains(output.port_name))
            return fail("VLOG0008", "AIG port is both input and output");
        if (!insert_bit(outputs, out_lookup, output.port_name, output.bit_index,
                        output.driver, diagnostics)) return false;
    }
    return complete_bits(inputs, diagnostics) && complete_bits(outputs, diagnostics);
}

std::string write_module(const AigModule& module,
                         const std::vector<Port>& inputs,
                         const std::vector<Port>& outputs) {
    std::unordered_set<std::string> used;
    std::unordered_map<std::string, std::size_t> input_width;
    for (const auto& port : inputs) {
        used.insert(port.name);
        input_width.emplace(port.name, port.bits.size());
    }
    for (const auto& port : outputs) used.insert(port.name);

    std::string prefix = "__synvanta_and_";
    for (;;) {
        bool collision = false;
        for (const auto& node : module.nodes) {
            if (node.kind == AigNodeKind::And &&
                used.contains(prefix + std::to_string(node.id))) collision = true;
        }
        if (!collision) break;
        prefix += "_";
    }

    const auto reference = [&](AigLiteral literal) -> std::string {
        if (literal == AigFalse) return "1'b0";
        if (literal == AigTrue) return "1'b1";
        const auto& node = module.nodes[aig_node_id(literal)];
        std::string wire;
        if (node.kind == AigNodeKind::Input) {
            wire = escaped(node.input_name);
            if (input_width.at(node.input_name) > 1)
                wire += "[" + std::to_string(node.input_bit) + "]";
        } else {
            wire = escaped(prefix + std::to_string(node.id));
        }
        return (literal & 1U) != 0U ? "~(" + wire + ")" : wire;
    };

    std::ostringstream stream;
    stream << "module " << escaped(module.name) << " (";
    bool first = true;
    for (const auto* ports : {&inputs, &outputs}) {
        for (const auto& port : *ports) {
            if (!first) stream << ", ";
            stream << escaped(port.name);
            first = false;
        }
    }
    stream << ");\n";
    for (const auto* ports : {&inputs, &outputs}) {
        const char* kind = ports == &inputs ? "input" : "output";
        for (const auto& port : *ports) {
            stream << "  " << kind << " wire ";
            if (port.bits.size() > 1)
                stream << "[" << (port.bits.size() - 1U) << ":0] ";
            stream << escaped(port.name) << ";\n";
        }
    }
    for (const auto& node : module.nodes) {
        if (node.kind == AigNodeKind::And)
            stream << "  wire " << escaped(prefix + std::to_string(node.id)) << ";\n";
    }
    for (const auto& node : module.nodes) {
        if (node.kind != AigNodeKind::And) continue;
        stream << "  assign " << escaped(prefix + std::to_string(node.id))
               << " = " << reference(node.lhs) << " & " << reference(node.rhs) << ";\n";
    }
    for (const auto& port : outputs) {
        for (const auto& [bit, driver] : port.bits) {
            stream << "  assign " << escaped(port.name);
            if (port.bits.size() > 1) stream << "[" << bit << "]";
            stream << " = " << reference(driver) << ";\n";
        }
    }
    stream << "endmodule\n";
    return stream.str();
}
} // namespace

VerilogEmitResult GenericVerilogEmitter::emit(const AigDesign& design) const {
    VerilogEmitResult result;
    if (design.modules.empty()) {
        result.diagnostics.push_back({DiagnosticSeverity::Error, "VLOG0000",
                                      "cannot emit an empty AIG design", {}});
        return result;
    }
    std::unordered_set<std::string> names;
    std::string text;
    for (const auto& module : design.modules) {
        if (!names.insert(module.name).second) {
            result.diagnostics.push_back({DiagnosticSeverity::Error, "VLOG0009",
                                          "duplicate AIG module name", {}});
            return result;
        }
        std::vector<Port> inputs;
        std::vector<Port> outputs;
        if (!collect(module, inputs, outputs, result.diagnostics)) return result;
        text += write_module(module, inputs, outputs) + "\n";
    }
    result.verilog = std::move(text);
    return result;
}
} // namespace synvanta
