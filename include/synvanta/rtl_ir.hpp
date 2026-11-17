#pragma once
#include "synvanta/ast.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace synvanta {
using ValueId = std::uint32_t;
enum class RtlOp {
    Input, Constant, Resize, UnaryPlus, Negate, BitNot, LogicalNot,
    ReduceNand, ReduceNor, ReduceXnor, Add, Subtract, Multiply, Divide,
    Modulo, BitAnd, BitOr, BitXor, BitXnor, LogicalAnd, LogicalOr,
    Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual, ShiftLeft,
    ShiftRight, Mux
};
struct RtlNode {
    ValueId id{0};
    RtlOp op{RtlOp::Constant};
    std::uint32_t width{1};
    bool is_signed{false};
    std::vector<ValueId> operands;
    std::string literal;
    std::string name;
    SourceLocation location;
};
struct RtlPort {
    std::string name;
    PortDirection direction{PortDirection::Input};
    std::uint32_t width{1};
    bool is_signed{false};
};
struct RtlAssignment {
    std::string target;
    ValueId value{0};
    std::uint32_t target_width{1};
    bool target_is_signed{false};
};
struct RtlModule {
    std::string name;
    std::vector<RtlPort> ports;
    std::vector<RtlNode> nodes;
    std::vector<RtlAssignment> assignments;
};
struct RtlDesign { std::vector<RtlModule> modules; };
const char* rtl_op_name(RtlOp op);
std::string dump_rtl_design(const RtlDesign& design);
} // namespace synvanta
