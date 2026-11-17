#include "synvanta/rtl_ir.hpp"
#include <sstream>
namespace synvanta {
const char* rtl_op_name(RtlOp op) {
    switch (op) {
    case RtlOp::Input:return "input"; case RtlOp::Constant:return "const";
    case RtlOp::Resize:return "resize"; case RtlOp::UnaryPlus:return "pos";
    case RtlOp::Negate:return "neg"; case RtlOp::BitNot:return "not";
    case RtlOp::LogicalNot:return "logical_not"; case RtlOp::ReduceNand:return "reduce_nand";
    case RtlOp::ReduceNor:return "reduce_nor"; case RtlOp::ReduceXnor:return "reduce_xnor";
    case RtlOp::Add:return "add"; case RtlOp::Subtract:return "sub";
    case RtlOp::Multiply:return "mul"; case RtlOp::Divide:return "div";
    case RtlOp::Modulo:return "mod"; case RtlOp::BitAnd:return "and";
    case RtlOp::BitOr:return "or"; case RtlOp::BitXor:return "xor";
    case RtlOp::BitXnor:return "xnor"; case RtlOp::LogicalAnd:return "logical_and";
    case RtlOp::LogicalOr:return "logical_or"; case RtlOp::Equal:return "eq";
    case RtlOp::NotEqual:return "ne"; case RtlOp::Less:return "lt";
    case RtlOp::LessEqual:return "le"; case RtlOp::Greater:return "gt";
    case RtlOp::GreaterEqual:return "ge"; case RtlOp::ShiftLeft:return "shl";
    case RtlOp::ShiftRight:return "shr"; case RtlOp::Mux:return "mux";
    }
    return "unknown";
}
std::string dump_rtl_design(const RtlDesign& design) {
    std::ostringstream out;
    for (const auto& module : design.modules) {
        out << "module " << module.name << '\n';
        for (const auto& port : module.ports)
            out << "  port " << port_direction_name(port.direction) << ' ' << port.name
                << " : " << (port.is_signed ? 's' : 'u') << port.width << '\n';
        for (const auto& node : module.nodes) {
            out << "  %" << node.id << " = " << rtl_op_name(node.op);
            if (!node.name.empty()) out << ' ' << node.name;
            if (!node.literal.empty()) out << ' ' << node.literal;
            for (const auto operand : node.operands) out << " %" << operand;
            out << " : " << (node.is_signed ? 's' : 'u') << node.width << '\n';
        }
        for (const auto& assignment : module.assignments)
            out << "  assign " << assignment.target << " = %" << assignment.value
                << " : " << (assignment.target_is_signed ? 's' : 'u') << assignment.target_width << '\n';
        out << "endmodule\n";
    }
    return out.str();
}
} // namespace synvanta
