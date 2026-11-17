#pragma once
#include "synvanta/source.hpp"
#include "synvanta/token.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace synvanta {
enum class PortDirection { Input, Output, Inout };
enum class NetKind { Wire, Logic, Reg, Bit };
enum class ExprKind { Identifier, Number, Unary, Binary, Conditional };
struct PackedRange { int left{0}; int right{0}; };
struct Port {
    PortDirection direction{PortDirection::Input};
    NetKind net_kind{NetKind::Logic};
    bool is_signed{false};
    std::optional<PackedRange> range;
    std::string name;
    SourceLocation location;
};
struct Expr {
    ExprKind kind{ExprKind::Number};
    std::string text;
    TokenKind op{TokenKind::Unknown};
    std::unique_ptr<Expr> lhs, rhs, third;
    SourceLocation location;
};
struct NetDeclaration {
    NetKind net_kind{NetKind::Wire};
    bool is_signed{false};
    std::optional<PackedRange> range;
    std::string name;
    SourceLocation location;
};

struct ContinuousAssignment {
    std::string lhs;
    std::unique_ptr<Expr> rhs;
    SourceLocation location;
};
struct Module {
    std::string name;
    std::vector<Port> ports;
    std::vector<NetDeclaration> nets;
    std::vector<ContinuousAssignment> assignments;
    SourceLocation location;
};
struct CompilationUnit { std::vector<Module> modules; };
const char* port_direction_name(PortDirection direction);
const char* net_kind_name(NetKind kind);
} // namespace synvanta
