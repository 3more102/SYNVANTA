#include "synvanta/aig.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace synvanta {

AigNetwork::AigNetwork() {
    nodes_.push_back({AigNodeKind::Constant, aig_false, aig_false, {}, 0});
}

AigLit AigNetwork::input(const std::string& name, std::uint32_t bit) {
    const auto key = std::make_pair(name, bit);
    const auto found = input_index_.find(key);
    if (found != input_index_.end()) return found->second;

    if (nodes_.size() >= (static_cast<std::size_t>(std::numeric_limits<AigLit>::max()) >> 1U))
        throw std::overflow_error("AIG node identifier space exhausted");
    const auto id = static_cast<AigLit>(nodes_.size());
    const AigLit lit = id << 1U;
    nodes_.push_back({AigNodeKind::Input, aig_false, aig_false, name, bit});
    input_index_.emplace(key, lit);
    return lit;
}

AigLit AigNetwork::make_and(AigLit lhs, AigLit rhs) {
    if (lhs == aig_false || rhs == aig_false) return aig_false;
    if (lhs == aig_true) return rhs;
    if (rhs == aig_true) return lhs;
    if (lhs == rhs) return lhs;
    if (lhs == invert(rhs)) return aig_false;
    if (rhs < lhs) std::swap(lhs, rhs);

    const auto key = std::make_pair(lhs, rhs);
    const auto found = and_hash_.find(key);
    if (found != and_hash_.end()) return found->second;

    if (nodes_.size() >= (static_cast<std::size_t>(std::numeric_limits<AigLit>::max()) >> 1U))
        throw std::overflow_error("AIG node identifier space exhausted");
    const auto id = static_cast<AigLit>(nodes_.size());
    const AigLit lit = id << 1U;
    nodes_.push_back({AigNodeKind::And, lhs, rhs, {}, 0});
    and_hash_.emplace(key, lit);
    return lit;
}

AigLit AigNetwork::make_or(AigLit lhs, AigLit rhs) {
    if (lhs == aig_true || rhs == aig_true) return aig_true;
    if (lhs == aig_false) return rhs;
    if (rhs == aig_false) return lhs;
    if (lhs == rhs) return lhs;
    if (lhs == invert(rhs)) return aig_true;
    return invert(make_and(invert(lhs), invert(rhs)));
}

AigLit AigNetwork::make_xor(AigLit lhs, AigLit rhs) {
    if (lhs == rhs) return aig_false;
    if (lhs == invert(rhs)) return aig_true;
    if (lhs == aig_false) return rhs;
    if (rhs == aig_false) return lhs;
    if (lhs == aig_true) return invert(rhs);
    if (rhs == aig_true) return invert(lhs);
    const AigLit left = make_and(lhs, invert(rhs));
    const AigLit right = make_and(invert(lhs), rhs);
    return make_or(left, right);
}

AigLit AigNetwork::make_xnor(AigLit lhs, AigLit rhs) {
    return invert(make_xor(lhs, rhs));
}

AigLit AigNetwork::make_mux(AigLit select, AigLit when_true, AigLit when_false) {
    if (select == aig_true) return when_true;
    if (select == aig_false) return when_false;
    if (when_true == when_false) return when_true;
    if (when_true == aig_true && when_false == aig_false) return select;
    if (when_true == aig_false && when_false == aig_true) return invert(select);
    return make_or(make_and(select, when_true), make_and(invert(select), when_false));
}

std::size_t AigNetwork::and_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(nodes_.begin(), nodes_.end(),
        [](const AigNode& node) { return node.kind == AigNodeKind::And; }));
}

namespace {

using Bits = std::vector<AigLit>;

int digit_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

Bits parse_constant_bits(const RtlNode& node) {
    std::string text;
    text.reserve(node.literal.size());
    for (char c : node.literal) if (c != '_') text.push_back(c);

    if (text == "'0") return Bits{aig_false};
    if (text == "'1") return Bits{aig_true};

    const auto apostrophe = text.find('\'');
    std::uint32_t width = node.width;
    bool is_signed = node.is_signed;
    int base = 10;
    std::string digits;

    if (apostrophe == std::string::npos) {
        digits = text;
    } else {
        const auto parsed_width = std::from_chars(text.data(), text.data() + apostrophe, width, 10);
        if (parsed_width.ec != std::errc{} || parsed_width.ptr != text.data() + apostrophe)
            throw std::runtime_error("invalid sized constant: " + node.literal);
        std::size_t pos = apostrophe + 1;
        if (pos < text.size() && (text[pos] == 's' || text[pos] == 'S')) {
            is_signed = true;
            ++pos;
        }
        if (pos >= text.size()) throw std::runtime_error("missing base in constant: " + node.literal);
        const char base_char = static_cast<char>(std::tolower(static_cast<unsigned char>(text[pos++])));
        if (base_char == 'b') base = 2;
        else if (base_char == 'o') base = 8;
        else if (base_char == 'd') base = 10;
        else if (base_char == 'h') base = 16;
        else throw std::runtime_error("unsupported base in constant: " + node.literal);
        digits = text.substr(pos);
    }

    if (width == 0 || width > 64)
        throw std::runtime_error("AIG constant width must be between 1 and 64 bits: " + node.literal);
    if (digits.empty()) throw std::runtime_error("constant has no digits: " + node.literal);

    Bits result(width, aig_false);
    if (base == 10) {
        std::size_t consumed = 0;
        const auto value = std::stoull(digits, &consumed, 10);
        if (consumed != digits.size()) throw std::runtime_error("non-binary value in constant: " + node.literal);
        for (std::uint32_t bit = 0; bit < width; ++bit)
            result[bit] = ((value >> bit) & 1ULL) != 0 ? aig_true : aig_false;
        return result;
    }

    const std::uint32_t bits_per_digit = base == 2 ? 1U : base == 8 ? 3U : 4U;
    std::uint32_t bit_index = 0;
    for (auto it = digits.rbegin(); it != digits.rend() && bit_index < width; ++it) {
        const int value = digit_value(*it);
        if (value < 0 || value >= base)
            throw std::runtime_error("X/Z or invalid digit in two-state AIG constant: " + node.literal);
        for (std::uint32_t bit = 0; bit < bits_per_digit && bit_index < width; ++bit, ++bit_index)
            result[bit_index] = ((value >> bit) & 1) != 0 ? aig_true : aig_false;
    }
    (void)is_signed;
    return result;
}

Bits resize_bits(const Bits& source, std::uint32_t width, bool sign_extend) {
    Bits result = source;
    if (result.size() > width) {
        result.resize(width);
    } else if (result.size() < width) {
        const AigLit extension = sign_extend && !result.empty() ? result.back() : aig_false;
        result.resize(width, extension);
    }
    return result;
}

Bits fit_operand(const RtlModule& module, const std::vector<Bits>& values, ValueId id, std::uint32_t width) {
    if (id >= values.size() || id >= module.nodes.size())
        throw std::runtime_error("RTL operand references an invalid ValueId");
    const auto& source = module.nodes[id];
    if (source.op == RtlOp::Constant && source.literal == "'1")
        return Bits(width, aig_true);
    if (source.op == RtlOp::Constant && source.literal == "'0")
        return Bits(width, aig_false);
    return resize_bits(values[id], width, source.is_signed);
}

AigLit reduce_or(AigNetwork& network, const Bits& bits) {
    AigLit result = aig_false;
    for (const auto bit : bits) result = network.make_or(result, bit);
    return result;
}

AigLit reduce_and(AigNetwork& network, const Bits& bits) {
    AigLit result = aig_true;
    for (const auto bit : bits) result = network.make_and(result, bit);
    return result;
}

AigLit reduce_xor(AigNetwork& network, const Bits& bits) {
    AigLit result = aig_false;
    for (const auto bit : bits) result = network.make_xor(result, bit);
    return result;
}

Bits add_vectors(AigNetwork& network, const Bits& lhs, const Bits& rhs, std::uint32_t width, bool subtract) {
    const Bits a = resize_bits(lhs, width, false);
    Bits b = resize_bits(rhs, width, false);
    if (subtract) for (auto& bit : b) bit = AigNetwork::invert(bit);

    AigLit carry = subtract ? aig_true : aig_false;
    Bits result(width, aig_false);
    for (std::uint32_t bit = 0; bit < width; ++bit) {
        const AigLit propagate = network.make_xor(a[bit], b[bit]);
        result[bit] = network.make_xor(propagate, carry);
        const AigLit generate = network.make_and(a[bit], b[bit]);
        carry = network.make_or(generate, network.make_and(propagate, carry));
    }
    return result;
}

Bits multiply_vectors(AigNetwork& network, const Bits& lhs, const Bits& rhs, std::uint32_t width) {
    const Bits a = resize_bits(lhs, width, false);
    const Bits b = resize_bits(rhs, width, false);
    Bits result(width, aig_false);
    for (std::uint32_t row_index = 0; row_index < width && row_index < b.size(); ++row_index) {
        Bits row(width, aig_false);
        for (std::uint32_t col = 0; col + row_index < width && col < a.size(); ++col)
            row[col + row_index] = network.make_and(a[col], b[row_index]);
        result = add_vectors(network, result, row, width, false);
    }
    return result;
}

AigLit unsigned_less(AigNetwork& network, const Bits& lhs, const Bits& rhs) {
    const std::uint32_t width = static_cast<std::uint32_t>(std::max(lhs.size(), rhs.size()));
    const Bits a = resize_bits(lhs, width, false);
    const Bits b = resize_bits(rhs, width, false);
    AigLit less = aig_false;
    AigLit equal = aig_true;
    for (std::uint32_t index = width; index > 0; --index) {
        const auto bit = index - 1;
        const AigLit this_less = network.make_and(equal,
            network.make_and(AigNetwork::invert(a[bit]), b[bit]));
        less = network.make_or(less, this_less);
        equal = network.make_and(equal, network.make_xnor(a[bit], b[bit]));
    }
    return less;
}

AigLit compare_less(AigNetwork& network, Bits lhs, Bits rhs, bool is_signed) {
    const std::uint32_t width = static_cast<std::uint32_t>(std::max(lhs.size(), rhs.size()));
    lhs = resize_bits(lhs, width, is_signed);
    rhs = resize_bits(rhs, width, is_signed);
    if (is_signed && width > 0) {
        lhs.back() = AigNetwork::invert(lhs.back());
        rhs.back() = AigNetwork::invert(rhs.back());
    }
    return unsigned_less(network, lhs, rhs);
}

Bits barrel_shift(AigNetwork& network, const Bits& data, const Bits& amount,
                  std::uint32_t width, bool left) {
    Bits current = resize_bits(data, width, false);
    std::uint32_t stages = 0;
    while ((std::uint64_t{1} << stages) < width) ++stages;

    for (std::uint32_t stage = 0; stage < stages && stage < amount.size(); ++stage) {
        const std::uint32_t distance = std::uint32_t{1} << stage;
        Bits shifted(width, aig_false);
        for (std::uint32_t bit = 0; bit < width; ++bit) {
            if (left) {
                if (bit >= distance) shifted[bit] = current[bit - distance];
            } else if (bit + distance < width) {
                shifted[bit] = current[bit + distance];
            }
        }
        Bits next(width, aig_false);
        for (std::uint32_t bit = 0; bit < width; ++bit)
            next[bit] = network.make_mux(amount[stage], shifted[bit], current[bit]);
        current = std::move(next);
    }

    AigLit too_large = aig_false;
    for (std::size_t bit = stages; bit < amount.size(); ++bit)
        too_large = network.make_or(too_large, amount[bit]);
    if (too_large != aig_false)
        for (auto& bit : current) bit = network.make_mux(too_large, aig_false, bit);
    return current;
}

class ModuleLowerer {
public:
    ModuleLowerer(const RtlModule& source, std::vector<Diagnostic>& diagnostics)
        : source_(source), diagnostics_(diagnostics) {}

    bool run() {
        result_.name = source_.name;
        result_.ports = source_.ports;
        values_.resize(source_.nodes.size());

        for (std::size_t index = 0; index < source_.nodes.size(); ++index) {
            const auto& node = source_.nodes[index];
            if (node.id != index) {
                report(node, "AIG0002", "RTL node IDs must be dense and ordered");
                continue;
            }
            try {
                auto lowered = lower_node(node);
                if (lowered) values_[index] = std::move(*lowered);
            } catch (const std::exception& error) {
                report(node, "AIG0003", error.what());
            }
        }

        if (has_errors()) return false;
        for (const auto& assignment : source_.assignments) {
            if (assignment.value >= values_.size()) {
                diagnostics_.push_back({DiagnosticSeverity::Error, "AIG0004",
                    "assignment references an invalid RTL ValueId", {}});
                continue;
            }
            const auto& source_node = source_.nodes[assignment.value];
            Bits bits;
            if (source_node.op == RtlOp::Constant && source_node.literal == "'1")
                bits = Bits(assignment.target_width, aig_true);
            else
                bits = resize_bits(values_[assignment.value], assignment.target_width, source_node.is_signed);
            result_.assignments.push_back({assignment.target, std::move(bits),
                assignment.target_width, assignment.target_is_signed});
        }
        return !has_errors();
    }

    AigModule take_result() { return std::move(result_); }

private:
    const RtlModule& source_;
    std::vector<Diagnostic>& diagnostics_;
    AigModule result_;
    std::vector<Bits> values_;

    bool has_errors() const {
        return std::any_of(diagnostics_.begin(), diagnostics_.end(),
            [](const Diagnostic& diagnostic) { return diagnostic.severity == DiagnosticSeverity::Error; });
    }

    void report(const RtlNode& node, const std::string& code, const std::string& message) {
        diagnostics_.push_back({DiagnosticSeverity::Error, code, message, node.location});
    }

    Bits operand(const RtlNode& node, std::size_t index, std::uint32_t width) {
        if (index >= node.operands.size()) throw std::runtime_error("RTL operation has too few operands");
        return fit_operand(source_, values_, node.operands[index], width);
    }

    std::optional<Bits> lower_node(const RtlNode& node) {
        auto& network = result_.network;
        const auto width = node.width;
        if (width == 0) throw std::runtime_error("zero-width RTL values are not supported");

        switch (node.op) {
        case RtlOp::Input: {
            Bits bits;
            bits.reserve(width);
            for (std::uint32_t bit = 0; bit < width; ++bit)
                bits.push_back(network.input(node.name, bit));
            return bits;
        }
        case RtlOp::Constant:
            return parse_constant_bits(node);
        case RtlOp::Resize: {
            if (node.operands.empty()) throw std::runtime_error("resize operation has no operand");
            return fit_operand(source_, values_, node.operands[0], width);
        }
        case RtlOp::UnaryPlus:
            return operand(node, 0, width);
        case RtlOp::Negate: {
            Bits value = operand(node, 0, width);
            for (auto& bit : value) bit = AigNetwork::invert(bit);
            Bits one(width, aig_false);
            one[0] = aig_true;
            return add_vectors(network, value, one, width, false);
        }
        case RtlOp::BitNot: {
            Bits value = operand(node, 0, width);
            for (auto& bit : value) bit = AigNetwork::invert(bit);
            return value;
        }
        case RtlOp::LogicalNot:
            return Bits{AigNetwork::invert(reduce_or(network, operand(node, 0,
                static_cast<std::uint32_t>(values_.at(node.operands.at(0)).size()))))};
        case RtlOp::ReduceNand: {
            const auto& value = values_.at(node.operands.at(0));
            return Bits{AigNetwork::invert(reduce_and(network, value))};
        }
        case RtlOp::ReduceNor: {
            const auto& value = values_.at(node.operands.at(0));
            return Bits{AigNetwork::invert(reduce_or(network, value))};
        }
        case RtlOp::ReduceXnor: {
            const auto& value = values_.at(node.operands.at(0));
            return Bits{AigNetwork::invert(reduce_xor(network, value))};
        }
        case RtlOp::Add:
        case RtlOp::Subtract: {
            const Bits lhs = operand(node, 0, width);
            const Bits rhs = operand(node, 1, width);
            return add_vectors(network, lhs, rhs, width, node.op == RtlOp::Subtract);
        }
        case RtlOp::Multiply:
            return multiply_vectors(network, operand(node, 0, width), operand(node, 1, width), width);
        case RtlOp::BitAnd:
        case RtlOp::BitOr:
        case RtlOp::BitXor:
        case RtlOp::BitXnor: {
            const Bits lhs = operand(node, 0, width);
            const Bits rhs = operand(node, 1, width);
            Bits result(width, aig_false);
            for (std::uint32_t bit = 0; bit < width; ++bit) {
                if (node.op == RtlOp::BitAnd) result[bit] = network.make_and(lhs[bit], rhs[bit]);
                else if (node.op == RtlOp::BitOr) result[bit] = network.make_or(lhs[bit], rhs[bit]);
                else if (node.op == RtlOp::BitXor) result[bit] = network.make_xor(lhs[bit], rhs[bit]);
                else result[bit] = network.make_xnor(lhs[bit], rhs[bit]);
            }
            return result;
        }
        case RtlOp::LogicalAnd:
        case RtlOp::LogicalOr: {
            const auto& lhs = values_.at(node.operands.at(0));
            const auto& rhs = values_.at(node.operands.at(1));
            const AigLit a = reduce_or(network, lhs);
            const AigLit b = reduce_or(network, rhs);
            return Bits{node.op == RtlOp::LogicalAnd ? network.make_and(a, b) : network.make_or(a, b)};
        }
        case RtlOp::Equal:
        case RtlOp::NotEqual: {
            const auto lhs = operand(node, 0, std::max(values_.at(node.operands.at(0)).size(),
                                                       values_.at(node.operands.at(1)).size()));
            const auto rhs = operand(node, 1, static_cast<std::uint32_t>(lhs.size()));
            Bits different(lhs.size(), aig_false);
            for (std::size_t bit = 0; bit < lhs.size(); ++bit)
                different[bit] = network.make_xor(lhs[bit], rhs[bit]);
            const AigLit equal = AigNetwork::invert(reduce_or(network, different));
            return Bits{node.op == RtlOp::Equal ? equal : AigNetwork::invert(equal)};
        }
        case RtlOp::Less:
        case RtlOp::LessEqual:
        case RtlOp::Greater:
        case RtlOp::GreaterEqual: {
            const auto lhs_id = node.operands.at(0);
            const auto rhs_id = node.operands.at(1);
            const bool is_signed = source_.nodes.at(lhs_id).is_signed && source_.nodes.at(rhs_id).is_signed;
            const auto lhs = values_.at(lhs_id);
            const auto rhs = values_.at(rhs_id);
            AigLit result = aig_false;
            if (node.op == RtlOp::Less) result = compare_less(network, lhs, rhs, is_signed);
            else if (node.op == RtlOp::Greater) result = compare_less(network, rhs, lhs, is_signed);
            else if (node.op == RtlOp::LessEqual) result = AigNetwork::invert(compare_less(network, rhs, lhs, is_signed));
            else result = AigNetwork::invert(compare_less(network, lhs, rhs, is_signed));
            return Bits{result};
        }
        case RtlOp::ShiftLeft:
        case RtlOp::ShiftRight: {
            const Bits data = operand(node, 0, width);
            const auto& amount = values_.at(node.operands.at(1));
            return barrel_shift(network, data, amount, width, node.op == RtlOp::ShiftLeft);
        }
        case RtlOp::Mux: {
            const auto condition_id = node.operands.at(0);
            const auto& condition = values_.at(condition_id);
            const AigLit select = reduce_or(network, condition);
            const Bits when_true = operand(node, 1, width);
            const Bits when_false = operand(node, 2, width);
            Bits result(width, aig_false);
            for (std::uint32_t bit = 0; bit < width; ++bit)
                result[bit] = network.make_mux(select, when_true[bit], when_false[bit]);
            return result;
        }
        case RtlOp::Divide:
        case RtlOp::Modulo:
            throw std::runtime_error("division and modulo are not implemented in the initial AIG lowering");
        }
        throw std::runtime_error("unknown RTL operation");
    }
};

std::string format_lit(AigLit lit) {
    if (lit == aig_false) return "0";
    if (lit == aig_true) return "1";
    return std::string((lit & 1U) != 0 ? "!" : "") + "%" + std::to_string(lit >> 1U);
}

} // namespace

AigLoweringResult AigLowerer::lower(const RtlDesign& design) const {
    AigLoweringResult result;
    for (const auto& module : design.modules) {
        const auto diagnostic_start = result.diagnostics.size();
        ModuleLowerer lowerer(module, result.diagnostics);
        if (lowerer.run() && !has_errors(result.diagnostics))
            result.design.modules.push_back(lowerer.take_result());
        else if (result.diagnostics.size() == diagnostic_start)
            result.diagnostics.push_back({DiagnosticSeverity::Error, "AIG0005",
                "failed to lower RTL module '" + module.name + "' into an AIG", {}});
    }
    return result;
}

std::string dump_aig_design(const AigDesign& design) {
    std::ostringstream out;
    for (const auto& module : design.modules) {
        out << "module " << module.name << '\n';
        for (const auto& port : module.ports) {
            out << "  port " << port_direction_name(port.direction) << ' ' << port.name
                << " : " << (port.is_signed ? 's' : 'u') << port.width << '\n';
        }
        const auto& nodes = module.network.nodes();
        for (std::size_t id = 1; id < nodes.size(); ++id) {
            const auto& node = nodes[id];
            out << "  %" << id << " = ";
            if (node.kind == AigNodeKind::Input)
                out << "input " << node.input_name << '[' << node.input_bit << ']';
            else
                out << "and " << format_lit(node.lhs) << ' ' << format_lit(node.rhs);
            out << '\n';
        }
        for (const auto& assignment : module.assignments) {
            out << "  assign " << assignment.target << " = ";
            if (assignment.bits.size() > 1) out << '{';
            for (std::size_t index = assignment.bits.size(); index > 0; --index) {
                if (index != assignment.bits.size()) out << ", ";
                out << format_lit(assignment.bits[index - 1]);
            }
            if (assignment.bits.size() > 1) out << '}';
            out << " : " << (assignment.target_is_signed ? 's' : 'u') << assignment.target_width << '\n';
        }
        out << "  AIG inputs=" << module.network.input_count()
            << " and_nodes=" << module.network.and_count() << '\n';
        out << "endmodule\n";
    }
    return out.str();
}

} // namespace synvanta
