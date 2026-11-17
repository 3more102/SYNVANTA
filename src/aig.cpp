#include "synvanta/aig.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace synvanta {
namespace {

std::uint64_t width_mask(std::uint32_t width) {
    if (width >= 64U) return std::numeric_limits<std::uint64_t>::max();
    if (width == 0U) return 0U;
    return (std::uint64_t{1} << width) - 1U;
}

std::optional<std::uint64_t> parse_constant(const RtlNode& node) {
    if (node.width == 0U || node.width > 64U) return std::nullopt;

    std::string literal;
    literal.reserve(node.literal.size());
    for (const char character : node.literal) {
        if (character != '_') literal.push_back(character);
    }

    const auto apostrophe = literal.find(static_cast<char>(39));
    if (apostrophe == std::string::npos) {
        std::uint64_t value = 0;
        const auto parsed = std::from_chars(
            literal.data(), literal.data() + literal.size(), value, 10);
        if (parsed.ec != std::errc{} || parsed.ptr != literal.data() + literal.size())
            return std::nullopt;
        return value & width_mask(node.width);
    }

    if (apostrophe == 0U) {
        if (literal.size() != 2U) return std::nullopt;
        if (literal[1] == '0') return 0U;
        if (literal[1] == '1') return 1U;
        return std::nullopt;
    }

    std::size_t position = apostrophe + 1U;
    if (position < literal.size() &&
        (literal[position] == 's' || literal[position] == 'S')) {
        ++position;
    }
    if (position >= literal.size()) return std::nullopt;

    const char base_character = literal[position++];
    int base = 10;
    if (base_character == 'b' || base_character == 'B') base = 2;
    else if (base_character == 'o' || base_character == 'O') base = 8;
    else if (base_character == 'h' || base_character == 'H') base = 16;
    else if (base_character != 'd' && base_character != 'D') return std::nullopt;

    std::uint64_t value = 0;
    const auto parsed = std::from_chars(
        literal.data() + static_cast<std::ptrdiff_t>(position),
        literal.data() + literal.size(), value, base);
    if (parsed.ec != std::errc{} || parsed.ptr != literal.data() + literal.size())
        return std::nullopt;
    return value & width_mask(node.width);
}

std::string literal_name(AigLiteral literal) {
    const auto id = literal >> 1U;
    std::string name = "%" + std::to_string(id);
    if ((literal & 1U) != 0U) name.insert(name.begin(), '!');
    return name;
}

class ModuleBuilder {
public:
    explicit ModuleBuilder(const RtlModule& source) : source_(source) {}

    std::optional<AigModule> build(std::vector<Diagnostic>& diagnostics) {
        diagnostics_ = &diagnostics;
        diagnostic_start_ = diagnostics.size();

        AigModule output;
        output.name = source_.name;
        module_ = &output;
        module_->nodes.push_back({0U, AigNodeKind::ConstantFalse, aig_false, aig_false, "false"});

        values_.resize(source_.nodes.size());
        for (std::size_t index = 0; index < source_.nodes.size(); ++index) {
            const auto& node = source_.nodes[index];
            if (node.id != index) {
                report("AIG0001", "RTL node IDs must match their module-local vector indices", node);
                return std::nullopt;
            }
            if (node.width == 0U) {
                report("AIG0002", "RTL node width must be positive", node);
                return std::nullopt;
            }
            for (const auto operand : node.operands) {
                if (operand >= index) {
                    report("AIG0003", "RTL operands must refer to earlier nodes", node);
                    return std::nullopt;
                }
            }

            auto lowered = lower_node(node);
            if (!lowered || has_errors()) return std::nullopt;
            values_[index] = std::move(*lowered);
        }

        if (!build_outputs()) return std::nullopt;
        return output;
    }

private:
    const RtlModule& source_;
    AigModule* module_{nullptr};
    std::vector<Diagnostic>* diagnostics_{nullptr};
    std::size_t diagnostic_start_{0};
    std::vector<std::vector<AigLiteral>> values_;
    std::unordered_map<std::uint64_t, AigLiteral> and_cache_;

    bool has_errors() const {
        for (std::size_t index = diagnostic_start_; index < diagnostics_->size(); ++index) {
            if ((*diagnostics_)[index].severity == DiagnosticSeverity::Error) return true;
        }
        return false;
    }

    void report(const std::string& code, const std::string& message,
                const RtlNode& node) {
        diagnostics_->push_back(
            {DiagnosticSeverity::Error, code, message, node.location});
    }

    void report_output_error(const std::string& code, const std::string& message) {
        diagnostics_->push_back(
            {DiagnosticSeverity::Error, code, message, SourceLocation{}});
    }

    AigLiteral make_and(AigLiteral lhs, AigLiteral rhs) {
        if (lhs == aig_false || rhs == aig_false) return aig_false;
        if (lhs == aig_true) return rhs;
        if (rhs == aig_true) return lhs;
        if (lhs == rhs) return lhs;
        if (lhs == aig_invert(rhs)) return aig_false;
        if (rhs < lhs) std::swap(lhs, rhs);

        const auto key = (static_cast<std::uint64_t>(lhs) << 32U) |
                         static_cast<std::uint64_t>(rhs);
        const auto found = and_cache_.find(key);
        if (found != and_cache_.end()) return found->second;

        const auto maximum_id = std::numeric_limits<std::uint32_t>::max() >> 1U;
        if (module_->nodes.size() > static_cast<std::size_t>(maximum_id)) {
            diagnostics_->push_back(
                {DiagnosticSeverity::Error, "AIG0009", "AIG node ID space is exhausted", {}});
            return aig_false;
        }

        const auto id = static_cast<std::uint32_t>(module_->nodes.size());
        module_->nodes.push_back({id, AigNodeKind::And, lhs, rhs, {}});
        const auto result = id << 1U;
        and_cache_.emplace(key, result);
        return result;
    }

    AigLiteral make_or(AigLiteral lhs, AigLiteral rhs) {
        return aig_invert(make_and(aig_invert(lhs), aig_invert(rhs)));
    }

    AigLiteral make_xor(AigLiteral lhs, AigLiteral rhs) {
        if (lhs == rhs) return aig_false;
        if (lhs == aig_invert(rhs)) return aig_true;
        const auto both = make_and(lhs, rhs);
        const auto either = make_or(lhs, rhs);
        return make_and(either, aig_invert(both));
    }

    AigLiteral make_mux(AigLiteral select, AigLiteral when_true, AigLiteral when_false) {
        if (when_true == when_false) return when_true;
        if (select == aig_true) return when_true;
        if (select == aig_false) return when_false;
        return make_or(make_and(select, when_true),
                       make_and(aig_invert(select), when_false));
    }

    AigLiteral reduce_or(const std::vector<AigLiteral>& bits) {
        AigLiteral result = aig_false;
        for (const auto bit : bits) result = make_or(result, bit);
        return result;
    }

    AigLiteral reduce_and(const std::vector<AigLiteral>& bits) {
        AigLiteral result = aig_true;
        for (const auto bit : bits) result = make_and(result, bit);
        return result;
    }

    AigLiteral reduce_xor(const std::vector<AigLiteral>& bits) {
        AigLiteral result = aig_false;
        for (const auto bit : bits) result = make_xor(result, bit);
        return result;
    }

    std::optional<std::vector<AigLiteral>> lower_input(const RtlNode& node) {
        const auto port = std::find_if(
            source_.ports.begin(), source_.ports.end(),
            [&](const RtlPort& candidate) {
                return candidate.name == node.name &&
                       candidate.direction == PortDirection::Input;
            });
        if (port == source_.ports.end()) {
            report("AIG0004", "input IR node does not match an input port", node);
            return std::nullopt;
        }
        if (port->width != node.width) {
            report("AIG0005", "input IR width does not match its port declaration", node);
            return std::nullopt;
        }

        std::vector<AigLiteral> bits;
        bits.reserve(node.width);
        for (std::uint32_t bit = 0; bit < node.width; ++bit) {
            std::string name = node.name;
            if (node.width > 1U) name += "[" + std::to_string(bit) + "]";
            const auto maximum_id = std::numeric_limits<std::uint32_t>::max() >> 1U;
            if (module_->nodes.size() > static_cast<std::size_t>(maximum_id)) {
                report("AIG0009", "AIG node ID space is exhausted", node);
                return std::nullopt;
            }
            const auto id = static_cast<std::uint32_t>(module_->nodes.size());
            module_->nodes.push_back({id, AigNodeKind::Input, aig_false, aig_false, name});
            const auto literal = id << 1U;
            bits.push_back(literal);
            module_->inputs.push_back({node.name, bit, literal});
        }
        return bits;
    }

    std::optional<std::vector<AigLiteral>> lower_constant(const RtlNode& node) {
        const auto value = parse_constant(node);
        if (!value) {
            report("AIG0006",
                   "constant is unknown, wider than 64 bits, or outside the supported two-state literal subset",
                   node);
            return std::nullopt;
        }

        std::vector<AigLiteral> bits;
        bits.reserve(node.width);
        for (std::uint32_t bit = 0; bit < node.width; ++bit) {
            bits.push_back(((*value >> bit) & 1U) != 0U ? aig_true : aig_false);
        }
        return bits;
    }

    bool require_count(const RtlNode& node, std::size_t expected) {
        if (node.operands.size() == expected) return true;
        report("AIG0007", "RTL operation has an invalid operand count", node);
        return false;
    }

    const std::vector<AigLiteral>* operand_bits(const RtlNode& node, std::size_t index) {
        if (index >= node.operands.size()) return nullptr;
        const auto id = node.operands[index];
        if (id >= values_.size() || values_[id].empty()) {
            report("AIG0008", "RTL operation references an unavailable operand value", node);
            return nullptr;
        }
        return &values_[id];
    }

    std::vector<AigLiteral> negate_bits(const std::vector<AigLiteral>& input) {
        std::vector<AigLiteral> result;
        result.reserve(input.size());
        AigLiteral carry = aig_true;
        for (const auto bit : input) {
            const auto inverted = aig_invert(bit);
            result.push_back(make_xor(inverted, carry));
            carry = make_and(inverted, carry);
        }
        return result;
    }

    std::vector<AigLiteral> add_bits(const std::vector<AigLiteral>& lhs,
                                     const std::vector<AigLiteral>& rhs,
                                     bool subtract) {
        std::vector<AigLiteral> result;
        result.reserve(lhs.size());
        AigLiteral carry = subtract ? aig_true : aig_false;
        for (std::size_t bit = 0; bit < lhs.size(); ++bit) {
            const auto right = subtract ? aig_invert(rhs[bit]) : rhs[bit];
            const auto propagate = make_xor(lhs[bit], right);
            result.push_back(make_xor(propagate, carry));
            const auto generate = make_and(lhs[bit], right);
            carry = make_or(generate, make_and(propagate, carry));
        }
        return result;
    }

    AigLiteral unsigned_less(const std::vector<AigLiteral>& lhs,
                             const std::vector<AigLiteral>& rhs) {
        AigLiteral less = aig_false;
        AigLiteral equal = aig_true;
        for (std::size_t index = lhs.size(); index > 0; --index) {
            const auto bit = index - 1U;
            const auto this_bit_less = make_and(
                equal, make_and(aig_invert(lhs[bit]), rhs[bit]));
            less = make_or(less, this_bit_less);
            equal = make_and(equal, aig_invert(make_xor(lhs[bit], rhs[bit])));
        }
        return less;
    }

    AigLiteral compare_less(const RtlNode& node,
                            const std::vector<AigLiteral>& lhs,
                            const std::vector<AigLiteral>& rhs) {
        const auto unsigned_result = unsigned_less(lhs, rhs);
        if (node.operands.size() != 2U) return unsigned_result;
        const auto lhs_signed = source_.nodes[node.operands[0]].is_signed;
        const auto rhs_signed = source_.nodes[node.operands[1]].is_signed;
        if (!lhs_signed || !rhs_signed || lhs.empty()) return unsigned_result;

        const auto sign_difference = make_xor(lhs.back(), rhs.back());
        return make_mux(sign_difference, lhs.back(), unsigned_result);
    }

    std::vector<AigLiteral> shift_bits(const std::vector<AigLiteral>& lhs,
                                       const std::vector<AigLiteral>& amount,
                                       bool left_shift) {
        auto current = lhs;
        const auto width = current.size();
        for (std::size_t stage = 0; stage < amount.size(); ++stage) {
            constexpr auto size_bits = static_cast<std::size_t>(
                std::numeric_limits<std::size_t>::digits);
            const bool beyond_width =
                stage >= size_bits || (std::size_t{1} << stage) >= width;
            if (beyond_width) {
                for (auto& bit : current) bit = make_mux(amount[stage], aig_false, bit);
                continue;
            }

            const auto distance = std::size_t{1} << stage;
            std::vector<AigLiteral> next(width, aig_false);
            for (std::size_t bit = 0; bit < width; ++bit) {
                AigLiteral shifted = aig_false;
                if (left_shift) {
                    if (bit >= distance) shifted = current[bit - distance];
                } else if (bit + distance < width) {
                    shifted = current[bit + distance];
                }
                next[bit] = make_mux(amount[stage], shifted, current[bit]);
            }
            current = std::move(next);
        }
        return current;
    }

    std::optional<std::vector<AigLiteral>> lower_node(const RtlNode& node) {
        if (node.op == RtlOp::Input) {
            if (!require_count(node, 0U)) return std::nullopt;
            return lower_input(node);
        }
        if (node.op == RtlOp::Constant) {
            if (!require_count(node, 0U)) return std::nullopt;
            return lower_constant(node);
        }

        if (node.op == RtlOp::Resize) {
            if (!require_count(node, 1U)) return std::nullopt;
            const auto* source_bits = operand_bits(node, 0U);
            if (source_bits == nullptr) return std::nullopt;
            const auto source_id = node.operands[0];
            const auto source_signed = source_.nodes[source_id].is_signed;
            std::vector<AigLiteral> result;
            result.reserve(node.width);
            for (std::uint32_t bit = 0; bit < node.width; ++bit) {
                if (bit < source_bits->size()) {
                    result.push_back((*source_bits)[bit]);
                } else if (source_signed && !source_bits->empty()) {
                    result.push_back(source_bits->back());
                } else {
                    result.push_back(aig_false);
                }
            }
            return result;
        }

        const auto operand_count = node.op == RtlOp::Mux ? 3U :
            ((node.op == RtlOp::UnaryPlus || node.op == RtlOp::Negate ||
              node.op == RtlOp::BitNot || node.op == RtlOp::LogicalNot ||
              node.op == RtlOp::ReduceNand || node.op == RtlOp::ReduceNor ||
              node.op == RtlOp::ReduceXnor) ? 1U : 2U);
        if (!require_count(node, operand_count)) return std::nullopt;

        std::vector<const std::vector<AigLiteral>*> operands;
        operands.reserve(operand_count);
        for (std::size_t index = 0; index < operand_count; ++index) {
            const auto* bits = operand_bits(node, index);
            if (bits == nullptr) return std::nullopt;
            operands.push_back(bits);
        }

        const auto& lhs = *operands[0];
        if (node.op == RtlOp::UnaryPlus) return lhs;
        if (node.op == RtlOp::Negate) {
            if (lhs.size() != node.width) {
                report("AIG0010", "unary arithmetic operand width does not match its result width", node);
                return std::nullopt;
            }
            return negate_bits(lhs);
        }
        if (node.op == RtlOp::BitNot) {
            std::vector<AigLiteral> result;
            result.reserve(lhs.size());
            for (const auto bit : lhs) result.push_back(aig_invert(bit));
            return result;
        }
        if (node.op == RtlOp::LogicalNot) return std::vector<AigLiteral>{aig_invert(reduce_or(lhs))};
        if (node.op == RtlOp::ReduceNand) return std::vector<AigLiteral>{aig_invert(reduce_and(lhs))};
        if (node.op == RtlOp::ReduceNor) return std::vector<AigLiteral>{aig_invert(reduce_or(lhs))};
        if (node.op == RtlOp::ReduceXnor) return std::vector<AigLiteral>{aig_invert(reduce_xor(lhs))};

        if (node.op == RtlOp::Mux) {
            const auto& when_true = *operands[1];
            const auto& when_false = *operands[2];
            if (when_true.size() != node.width || when_false.size() != node.width) {
                report("AIG0011", "mux data widths do not match the result width", node);
                return std::nullopt;
            }
            const auto select = reduce_or(lhs);
            std::vector<AigLiteral> result;
            result.reserve(node.width);
            for (std::uint32_t bit = 0; bit < node.width; ++bit)
                result.push_back(make_mux(select, when_true[bit], when_false[bit]));
            return result;
        }

        const auto& rhs = *operands[1];
        if (node.op == RtlOp::LogicalAnd)
            return std::vector<AigLiteral>{make_and(reduce_or(lhs), reduce_or(rhs))};
        if (node.op == RtlOp::LogicalOr)
            return std::vector<AigLiteral>{make_or(reduce_or(lhs), reduce_or(rhs))};

        if (node.op == RtlOp::ShiftLeft || node.op == RtlOp::ShiftRight) {
            if (lhs.size() != node.width) {
                report("AIG0012", "shift left operand width does not match the result width", node);
                return std::nullopt;
            }
            return shift_bits(lhs, rhs, node.op == RtlOp::ShiftLeft);
        }

        if (node.op == RtlOp::Add || node.op == RtlOp::Subtract) {
            if (lhs.size() != node.width || rhs.size() != node.width) {
                report("AIG0013", "arithmetic operands must be normalized to the result width", node);
                return std::nullopt;
            }
            return add_bits(lhs, rhs, node.op == RtlOp::Subtract);
        }

        if (node.op == RtlOp::BitAnd || node.op == RtlOp::BitOr ||
            node.op == RtlOp::BitXor || node.op == RtlOp::BitXnor) {
            if (lhs.size() != node.width || rhs.size() != node.width) {
                report("AIG0014", "bitwise operands must be normalized to the result width", node);
                return std::nullopt;
            }
            std::vector<AigLiteral> result;
            result.reserve(node.width);
            for (std::uint32_t bit = 0; bit < node.width; ++bit) {
                if (node.op == RtlOp::BitAnd) result.push_back(make_and(lhs[bit], rhs[bit]));
                else if (node.op == RtlOp::BitOr) result.push_back(make_or(lhs[bit], rhs[bit]));
                else if (node.op == RtlOp::BitXor) result.push_back(make_xor(lhs[bit], rhs[bit]));
                else result.push_back(aig_invert(make_xor(lhs[bit], rhs[bit])));
            }
            return result;
        }

        if (node.op == RtlOp::Equal || node.op == RtlOp::NotEqual ||
            node.op == RtlOp::Less || node.op == RtlOp::LessEqual ||
            node.op == RtlOp::Greater || node.op == RtlOp::GreaterEqual) {
            if (lhs.size() != rhs.size() || lhs.empty()) {
                report("AIG0015", "comparison operands must have equal, non-zero widths", node);
                return std::nullopt;
            }
            AigLiteral equal = aig_true;
            for (std::size_t bit = 0; bit < lhs.size(); ++bit)
                equal = make_and(equal, aig_invert(make_xor(lhs[bit], rhs[bit])));
            const auto less = compare_less(node, lhs, rhs);
            const auto greater = make_and(aig_invert(less), aig_invert(equal));

            AigLiteral result = aig_false;
            switch (node.op) {
                case RtlOp::Equal: result = equal; break;
                case RtlOp::NotEqual: result = aig_invert(equal); break;
                case RtlOp::Less: result = less; break;
                case RtlOp::LessEqual: result = make_or(less, equal); break;
                case RtlOp::Greater: result = greater; break;
                case RtlOp::GreaterEqual: result = aig_invert(less); break;
                default: break;
            }
            return std::vector<AigLiteral>{result};
        }

        if (node.op == RtlOp::Multiply || node.op == RtlOp::Divide || node.op == RtlOp::Modulo) {
            report("AIG0016",
                   "multiply, divide, and modulo are not supported by the initial AIG lowering subset",
                   node);
            return std::nullopt;
        }

        report("AIG0017", "unsupported RTL operation in AIG lowering", node);
        return std::nullopt;
    }

    bool build_outputs() {
        std::unordered_map<std::string, const RtlAssignment*> assignments;
        for (const auto& assignment : source_.assignments) {
            const auto port = std::find_if(
                source_.ports.begin(), source_.ports.end(),
                [&](const RtlPort& candidate) {
                    return candidate.name == assignment.target &&
                           candidate.direction == PortDirection::Output;
                });
            if (port == source_.ports.end()) continue;
            if (!assignments.emplace(assignment.target, &assignment).second) {
                report_output_error("AIG0018", "multiple assignments target one output port");
                return false;
            }
        }

        for (const auto& port : source_.ports) {
            if (port.direction != PortDirection::Output) continue;
            const auto assignment = assignments.find(port.name);
            if (assignment == assignments.end()) {
                report_output_error("AIG0019", "output port '" + port.name + "' has no lowered assignment");
                return false;
            }
            const auto value_id = assignment->second->value;
            if (value_id >= values_.size() || values_[value_id].size() != port.width) {
                report_output_error("AIG0020", "output assignment width does not match port '" + port.name + "'");
                return false;
            }
            for (std::uint32_t bit = 0; bit < port.width; ++bit)
                module_->outputs.push_back({port.name, bit, values_[value_id][bit]});
        }
        return !has_errors();
    }
};

} // namespace

AigBuildResult AigBuilder::build(const RtlDesign& design) const {
    AigBuildResult result;
    result.design.modules.reserve(design.modules.size());
    for (const auto& module : design.modules) {
        ModuleBuilder builder(module);
        auto lowered = builder.build(result.diagnostics);
        if (lowered) result.design.modules.push_back(std::move(*lowered));
    }
    return result;
}

const char* aig_node_kind_name(AigNodeKind kind) {
    switch (kind) {
        case AigNodeKind::ConstantFalse: return "const_false";
        case AigNodeKind::Input: return "input";
        case AigNodeKind::And: return "and";
    }
    return "unknown";
}

std::string dump_aig_design(const AigDesign& design) {
    std::ostringstream output;
    for (const auto& module : design.modules) {
        output << "module " << module.name << '\n';
        for (const auto& node : module.nodes) {
            if (node.kind == AigNodeKind::ConstantFalse) {
                output << "  %0 = false\n";
            } else if (node.kind == AigNodeKind::Input) {
                output << "  %" << node.id << " = input " << node.name << '\n';
            } else {
                output << "  %" << node.id << " = and "
                       << literal_name(node.lhs) << ' ' << literal_name(node.rhs) << '\n';
            }
        }
        for (const auto& port_bit : module.outputs) {
            output << "  output " << port_bit.port_name << '[' << port_bit.bit_index
                   << "] = " << literal_name(port_bit.literal) << '\n';
        }
        output << "endmodule\n";
    }
    return output.str();
}

} // namespace synvanta
