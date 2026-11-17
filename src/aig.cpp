#include "synvanta/aig.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace synvanta {
namespace {

std::uint64_t width_mask(std::uint32_t width) {
    if (width >= 64) return std::numeric_limits<std::uint64_t>::max();
    return (std::uint64_t{1} << width) - 1U;
}

std::optional<std::uint64_t> parse_known_constant(const std::string& raw,
                                                  std::uint32_t width) {
    if (width == 0 || width > 64) return std::nullopt;

    std::string text;
    text.reserve(raw.size());
    for (const char ch : raw) {
        if (ch != '_') text.push_back(ch);
    }
    if (text.empty()) return std::nullopt;

    const auto quote = text.find('\'');
    int base = 10;
    std::string digits;

    if (quote == std::string::npos) {
        digits = text;
    } else if (quote == 0) {
        // Unbased unsized literals are represented as one-bit constants in the RTL IR.
        if (text.size() != 2 || (text[1] != '0' && text[1] != '1'))
            return std::nullopt;
        return text[1] == '1' ? 1U : 0U;
    } else {
        std::size_t pos = quote + 1;
        if (pos < text.size() && (text[pos] == 's' || text[pos] == 'S')) ++pos;
        if (pos >= text.size()) return std::nullopt;

        const char radix = text[pos++];
        switch (radix) {
            case 'b': case 'B': base = 2; break;
            case 'o': case 'O': base = 8; break;
            case 'd': case 'D': base = 10; break;
            case 'h': case 'H': base = 16; break;
            default: return std::nullopt;
        }
        digits = text.substr(pos);
    }

    if (digits.empty()) return std::nullopt;
    std::uint64_t value = 0;
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(),
                                        value, base);
    if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size())
        return std::nullopt;
    return value & width_mask(width);
}

class ModuleBuilder {
public:
    ModuleBuilder(const RtlModule& source, std::vector<Diagnostic>& diagnostics)
        : source_(source), diagnostics_(diagnostics) {
        module_.name = source.name;
        AigNode constant;
        constant.id = 0;
        constant.kind = AigNodeKind::ConstantFalse;
        module_.nodes.push_back(std::move(constant));
    }

    std::optional<AigModule> build() {
        values_.resize(source_.nodes.size());

        for (std::size_t index = 0; index < source_.nodes.size(); ++index) {
            const auto& node = source_.nodes[index];
            if (node.id != index) {
                report("AIG0001", "RTL node IDs must be dense and topologically ordered",
                       node.location);
                return std::nullopt;
            }

            values_[index] = lower_node(node);
            if (failed_) return std::nullopt;
        }

        lower_outputs();
        if (failed_) return std::nullopt;
        return std::move(module_);
    }

private:
    const RtlModule& source_;
    std::vector<Diagnostic>& diagnostics_;
    AigModule module_;
    std::vector<std::vector<AigLiteral>> values_;
    std::unordered_map<std::uint64_t, AigLiteral> and_hash_;
    bool failed_{false};

    void report(const std::string& code, const std::string& message,
                const SourceLocation& location) {
        diagnostics_.push_back({DiagnosticSeverity::Error, code, message, location});
        failed_ = true;
    }

    const std::vector<AigLiteral>* operand(const RtlNode& node, std::size_t index) {
        if (index >= node.operands.size()) {
            report("AIG0002", "malformed RTL IR: missing operand for " +
                   std::string(rtl_op_name(node.op)), node.location);
            return nullptr;
        }
        const auto id = node.operands[index];
        if (id >= values_.size() || values_[id].empty()) {
            report("AIG0002", "malformed RTL IR: operand is unavailable or not topologically earlier",
                   node.location);
            return nullptr;
        }
        return &values_[id];
    }

    std::vector<AigLiteral> fit_width(const std::vector<AigLiteral>& input,
                                      std::uint32_t width, bool sign_extend) const {
        std::vector<AigLiteral> result = input;
        if (result.size() > width) {
            result.resize(width);
        } else if (result.size() < width) {
            const AigLiteral extension =
                sign_extend && !result.empty() ? result.back() : AigFalse;
            result.resize(width, extension);
        }
        return result;
    }

    AigLiteral make_and(AigLiteral lhs, AigLiteral rhs,
                        const SourceLocation& location) {
        if (lhs == AigFalse || rhs == AigFalse) return AigFalse;
        if (lhs == AigTrue) return rhs;
        if (rhs == AigTrue) return lhs;
        if (lhs == rhs) return lhs;
        if (lhs == aig_invert(rhs)) return AigFalse;
        if (rhs < lhs) std::swap(lhs, rhs);

        const std::uint64_t key =
            (static_cast<std::uint64_t>(lhs) << 32U) | rhs;
        const auto found = and_hash_.find(key);
        if (found != and_hash_.end()) return found->second;

        const auto id = static_cast<std::uint32_t>(module_.nodes.size());
        if (id > (std::numeric_limits<AigLiteral>::max() >> 1U)) {
            report("AIG0008", "AIG node limit exceeded", location);
            return AigFalse;
        }

        AigNode node;
        node.id = id;
        node.kind = AigNodeKind::And;
        node.lhs = lhs;
        node.rhs = rhs;
        node.location = location;
        module_.nodes.push_back(std::move(node));

        const AigLiteral result = id << 1U;
        and_hash_.emplace(key, result);
        return result;
    }

    AigLiteral make_or(AigLiteral lhs, AigLiteral rhs,
                       const SourceLocation& location) {
        return aig_invert(make_and(aig_invert(lhs), aig_invert(rhs), location));
    }

    AigLiteral make_xor(AigLiteral lhs, AigLiteral rhs,
                        const SourceLocation& location) {
        const auto lhs_rhs_not = make_and(lhs, aig_invert(rhs), location);
        const auto lhs_not_rhs = make_and(aig_invert(lhs), rhs, location);
        return make_or(lhs_rhs_not, lhs_not_rhs, location);
    }

    AigLiteral make_mux(AigLiteral select, AigLiteral when_true,
                        AigLiteral when_false, const SourceLocation& location) {
        const auto true_path = make_and(select, when_true, location);
        const auto false_path = make_and(aig_invert(select), when_false, location);
        return make_or(true_path, false_path, location);
    }

    AigLiteral reduce_and(const std::vector<AigLiteral>& bits,
                          const SourceLocation& location) {
        AigLiteral result = AigTrue;
        for (const auto bit : bits) result = make_and(result, bit, location);
        return result;
    }

    AigLiteral reduce_or(const std::vector<AigLiteral>& bits,
                         const SourceLocation& location) {
        AigLiteral result = AigFalse;
        for (const auto bit : bits) result = make_or(result, bit, location);
        return result;
    }

    AigLiteral reduce_xor(const std::vector<AigLiteral>& bits,
                          const SourceLocation& location) {
        AigLiteral result = AigFalse;
        for (const auto bit : bits) result = make_xor(result, bit, location);
        return result;
    }

    std::vector<AigLiteral> add_vectors(const std::vector<AigLiteral>& lhs,
                                        const std::vector<AigLiteral>& rhs,
                                        AigLiteral carry,
                                        const SourceLocation& location) {
        const auto width = std::min(lhs.size(), rhs.size());
        std::vector<AigLiteral> result(width, AigFalse);
        for (std::size_t bit = 0; bit < width; ++bit) {
            const auto propagate = make_xor(lhs[bit], rhs[bit], location);
            result[bit] = make_xor(propagate, carry, location);
            const auto generate = make_and(lhs[bit], rhs[bit], location);
            const auto carried = make_and(propagate, carry, location);
            carry = make_or(generate, carried, location);
        }
        return result;
    }

    AigLiteral equal_bits(const std::vector<AigLiteral>& lhs,
                          const std::vector<AigLiteral>& rhs,
                          const SourceLocation& location) {
        const auto width = std::min(lhs.size(), rhs.size());
        AigLiteral equal = AigTrue;
        for (std::size_t bit = 0; bit < width; ++bit)
            equal = make_and(equal, aig_invert(make_xor(lhs[bit], rhs[bit], location)),
                             location);
        return equal;
    }

    AigLiteral unsigned_less(const std::vector<AigLiteral>& lhs,
                             const std::vector<AigLiteral>& rhs,
                             const SourceLocation& location) {
        const auto width = std::min(lhs.size(), rhs.size());
        AigLiteral less = AigFalse;
        AigLiteral equal_prefix = AigTrue;
        for (std::size_t bit = width; bit-- > 0;) {
            const auto less_here = make_and(aig_invert(lhs[bit]), rhs[bit], location);
            less = make_or(less,
                           make_and(equal_prefix, less_here, location), location);
            const auto bit_equal =
                aig_invert(make_xor(lhs[bit], rhs[bit], location));
            equal_prefix = make_and(equal_prefix, bit_equal, location);
        }
        return less;
    }

    AigLiteral compare_less(const std::vector<AigLiteral>& lhs,
                            const std::vector<AigLiteral>& rhs,
                            bool is_signed, const SourceLocation& location) {
        const auto unsigned_result = unsigned_less(lhs, rhs, location);
        if (!is_signed || lhs.empty() || rhs.empty()) return unsigned_result;

        const auto sign_diff = make_xor(lhs.back(), rhs.back(), location);
        return make_mux(sign_diff, lhs.back(), unsigned_result, location);
    }

    std::vector<AigLiteral> multiply_vectors(const std::vector<AigLiteral>& lhs,
                                             const std::vector<AigLiteral>& rhs,
                                             const SourceLocation& location) {
        const auto width = std::min(lhs.size(), rhs.size());
        std::vector<AigLiteral> result(width, AigFalse);
        for (std::size_t multiplier_bit = 0; multiplier_bit < width; ++multiplier_bit) {
            std::vector<AigLiteral> partial(width, AigFalse);
            for (std::size_t bit = multiplier_bit; bit < width; ++bit) {
                partial[bit] =
                    make_and(lhs[bit - multiplier_bit], rhs[multiplier_bit], location);
            }
            result = add_vectors(result, partial, AigFalse, location);
        }
        return result;
    }

    std::vector<AigLiteral> shift_vector(const std::vector<AigLiteral>& value,
                                         const std::vector<AigLiteral>& amount,
                                         bool left, const SourceLocation& location) {
        const auto width = value.size();
        std::vector<AigLiteral> result = value;
        std::size_t stages = 0;
        while ((std::size_t{1} << stages) < width) ++stages;

        AigLiteral out_of_range = AigFalse;
        for (std::size_t bit = stages; bit < amount.size(); ++bit)
            out_of_range = make_or(out_of_range, amount[bit], location);

        for (std::size_t stage = 0; stage < stages && stage < amount.size(); ++stage) {
            const std::size_t distance = std::size_t{1} << stage;
            std::vector<AigLiteral> shifted(width, AigFalse);
            for (std::size_t bit = 0; bit < width; ++bit) {
                if (left) {
                    if (bit >= distance) shifted[bit] = result[bit - distance];
                } else if (bit + distance < width) {
                    shifted[bit] = result[bit + distance];
                }
            }
            for (std::size_t bit = 0; bit < width; ++bit)
                shifted[bit] = make_mux(amount[stage], shifted[bit], result[bit], location);
            result = std::move(shifted);
        }

        if (out_of_range != AigFalse) {
            const auto in_range = aig_invert(out_of_range);
            for (auto& bit : result) bit = make_and(bit, in_range, location);
        }
        return result;
    }

    std::vector<AigLiteral> lower_node(const RtlNode& node) {
        if (node.width == 0) {
            report("AIG0002", "RTL node has zero width", node.location);
            return {};
        }

        if (node.op == RtlOp::Input) {
            if (node.name.empty()) {
                report("AIG0002", "input RTL node has no signal name", node.location);
                return {};
            }
            std::vector<AigLiteral> bits;
            bits.reserve(node.width);
            for (std::uint32_t bit = 0; bit < node.width; ++bit) {
                const auto id = static_cast<std::uint32_t>(module_.nodes.size());
                const auto input_index = static_cast<std::uint32_t>(module_.inputs.size());
                AigNode input;
                input.id = id;
                input.kind = AigNodeKind::Input;
                input.input_name = node.name;
                input.input_bit = bit;
                input.input_index = input_index;
                input.location = node.location;
                module_.nodes.push_back(std::move(input));
                module_.inputs.push_back({node.name, bit, id});
                bits.push_back(id << 1U);
            }
            return bits;
        }

        if (node.op == RtlOp::Constant) {
            const auto value = parse_known_constant(node.literal, node.width);
            if (!value) {
                report("AIG0004",
                       "constant '" + node.literal +
                           "' is unknown, malformed, or wider than the two-state AIG limit (64 bits)",
                       node.location);
                return {};
            }
            std::vector<AigLiteral> bits(node.width, AigFalse);
            for (std::uint32_t bit = 0; bit < node.width; ++bit)
                bits[bit] = ((*value >> bit) & 1U) ? AigTrue : AigFalse;
            return bits;
        }

        if (node.op == RtlOp::Resize) {
            const auto* source_bits = operand(node, 0);
            if (!source_bits) return {};
            const auto source_id = node.operands[0];
            const bool sign_extend = source_.nodes[source_id].is_signed;
            return fit_width(*source_bits, node.width, sign_extend);
        }

        const auto* lhs_ptr = operand(node, 0);
        if (!lhs_ptr) return {};
        const auto& lhs_raw = *lhs_ptr;

        if (node.op == RtlOp::UnaryPlus)
            return fit_width(lhs_raw, node.width, source_.nodes[node.operands[0]].is_signed);

        if (node.op == RtlOp::BitNot) {
            auto value = fit_width(lhs_raw, node.width, false);
            for (auto& bit : value) bit = aig_invert(bit);
            return value;
        }

        if (node.op == RtlOp::LogicalNot)
            return {aig_invert(reduce_or(lhs_raw, node.location))};

        if (node.op == RtlOp::ReduceNand)
            return {aig_invert(reduce_and(lhs_raw, node.location))};

        if (node.op == RtlOp::ReduceNor)
            return {aig_invert(reduce_or(lhs_raw, node.location))};

        if (node.op == RtlOp::ReduceXnor)
            return {aig_invert(reduce_xor(lhs_raw, node.location))};

        if (node.op == RtlOp::Negate) {
            auto value = fit_width(lhs_raw, node.width, false);
            for (auto& bit : value) bit = aig_invert(bit);
            return add_vectors(value, std::vector<AigLiteral>(node.width, AigFalse),
                               AigTrue, node.location);
        }

        const auto* rhs_ptr = operand(node, 1);
        if (!rhs_ptr) return {};
        const auto& rhs_raw = *rhs_ptr;

        if (node.op == RtlOp::LogicalAnd)
            return {make_and(reduce_or(lhs_raw, node.location),
                             reduce_or(rhs_raw, node.location), node.location)};

        if (node.op == RtlOp::LogicalOr)
            return {make_or(reduce_or(lhs_raw, node.location),
                            reduce_or(rhs_raw, node.location), node.location)};

        const auto lhs_signed = source_.nodes[node.operands[0]].is_signed;
        const auto rhs_signed = source_.nodes[node.operands[1]].is_signed;
        const bool operands_signed = lhs_signed && rhs_signed;

        if (node.op == RtlOp::Add || node.op == RtlOp::Subtract ||
            node.op == RtlOp::Multiply || node.op == RtlOp::BitAnd ||
            node.op == RtlOp::BitOr || node.op == RtlOp::BitXor ||
            node.op == RtlOp::BitXnor) {
            const auto lhs = fit_width(lhs_raw, node.width, lhs_signed);
            const auto rhs = fit_width(rhs_raw, node.width, rhs_signed);

            if (node.op == RtlOp::Add)
                return add_vectors(lhs, rhs, AigFalse, node.location);

            if (node.op == RtlOp::Subtract) {
                auto inverted_rhs = rhs;
                for (auto& bit : inverted_rhs) bit = aig_invert(bit);
                return add_vectors(lhs, inverted_rhs, AigTrue, node.location);
            }

            if (node.op == RtlOp::Multiply)
                return multiply_vectors(lhs, rhs, node.location);

            std::vector<AigLiteral> result(node.width, AigFalse);
            for (std::uint32_t bit = 0; bit < node.width; ++bit) {
                if (node.op == RtlOp::BitAnd)
                    result[bit] = make_and(lhs[bit], rhs[bit], node.location);
                else if (node.op == RtlOp::BitOr)
                    result[bit] = make_or(lhs[bit], rhs[bit], node.location);
                else if (node.op == RtlOp::BitXor)
                    result[bit] = make_xor(lhs[bit], rhs[bit], node.location);
                else
                    result[bit] = aig_invert(make_xor(lhs[bit], rhs[bit], node.location));
            }
            return result;
        }

        if (node.op == RtlOp::Equal || node.op == RtlOp::NotEqual ||
            node.op == RtlOp::Less || node.op == RtlOp::LessEqual ||
            node.op == RtlOp::Greater || node.op == RtlOp::GreaterEqual) {
            const auto width = std::max(lhs_raw.size(), rhs_raw.size());
            const auto lhs = fit_width(lhs_raw, static_cast<std::uint32_t>(width), operands_signed);
            const auto rhs = fit_width(rhs_raw, static_cast<std::uint32_t>(width), operands_signed);
            const auto equal = equal_bits(lhs, rhs, node.location);
            const auto less = compare_less(lhs, rhs, operands_signed, node.location);

            switch (node.op) {
                case RtlOp::Equal: return {equal};
                case RtlOp::NotEqual: return {aig_invert(equal)};
                case RtlOp::Less: return {less};
                case RtlOp::LessEqual: return {make_or(less, equal, node.location)};
                case RtlOp::Greater:
                    return {aig_invert(make_or(less, equal, node.location))};
                case RtlOp::GreaterEqual: return {aig_invert(less)};
                default: break;
            }
        }

        if (node.op == RtlOp::ShiftLeft || node.op == RtlOp::ShiftRight) {
            const auto value = fit_width(lhs_raw, node.width, lhs_signed);
            return shift_vector(value, rhs_raw, node.op == RtlOp::ShiftLeft, node.location);
        }

        if (node.op == RtlOp::Mux) {
            const auto* false_ptr = operand(node, 2);
            if (!false_ptr) return {};
            const auto select = reduce_or(lhs_raw, node.location);
            const auto yes = fit_width(rhs_raw, node.width,
                                       source_.nodes[node.operands[1]].is_signed);
            const auto no = fit_width(*false_ptr, node.width,
                                      source_.nodes[node.operands[2]].is_signed);
            std::vector<AigLiteral> result(node.width, AigFalse);
            for (std::uint32_t bit = 0; bit < node.width; ++bit)
                result[bit] = make_mux(select, yes[bit], no[bit], node.location);
            return result;
        }

        if (node.op == RtlOp::Divide || node.op == RtlOp::Modulo) {
            report("AIG0006", "division and modulo bit-blasting are not implemented yet",
                   node.location);
            return {};
        }

        report("AIG0005", "unsupported RTL operation in AIG builder: " +
               std::string(rtl_op_name(node.op)), node.location);
        return {};
    }

    void lower_outputs() {
        std::unordered_map<std::string, const RtlAssignment*> drivers;
        for (const auto& assignment : source_.assignments)
            drivers.emplace(assignment.target, &assignment);

        for (const auto& port : source_.ports) {
            if (port.direction != PortDirection::Output) continue;
            const auto found = drivers.find(port.name);
            if (found == drivers.end()) {
                report("AIG0007", "output port '" + port.name + "' has no RTL driver", {});
                return;
            }

            const auto value_id = found->second->value;
            if (value_id >= values_.size() || values_[value_id].empty()) {
                report("AIG0002", "output port '" + port.name +
                       "' references an unavailable RTL value", {});
                return;
            }
            const auto& bits = values_[value_id];
            if (bits.size() != port.width) {
                report("AIG0002", "output width mismatch while building AIG for port '" +
                       port.name + "'", {});
                return;
            }
            for (std::uint32_t bit = 0; bit < port.width; ++bit)
                module_.outputs.push_back({port.name, bit, bits[bit]});
        }
    }
};

std::string literal_name(AigLiteral literal) {
    if (literal == AigFalse) return "0";
    if (literal == AigTrue) return "1";
    const auto prefix = (literal & 1U) ? "!" : "";
    return std::string(prefix) + "n" + std::to_string(aig_node_id(literal));
}

} // namespace

AigBuildResult AigBuilder::build(const RtlDesign& design) const {
    AigBuildResult result;
    if (design.modules.empty()) {
        result.diagnostics.push_back({DiagnosticSeverity::Error, "AIG0000",
                                      "cannot build an AIG from an empty RTL design", {}});
        return result;
    }

    for (const auto& module : design.modules) {
        ModuleBuilder builder(module, result.diagnostics);
        auto built = builder.build();
        if (built) result.design.modules.push_back(std::move(*built));
    }
    return result;
}

std::string dump_aig_design(const AigDesign& design) {
    std::string output;
    for (const auto& module : design.modules) {
        output += "module " + module.name + "\n";
        output += "  constant false: n0 (literal 0)\n";
        output += "  inputs:\n";
        for (const auto& input : module.inputs) {
            output += "    " + input.port_name + "[" + std::to_string(input.bit_index) +
                      "] = n" + std::to_string(input.node_id) + "\n";
        }
        output += "  AND nodes:\n";
        for (const auto& node : module.nodes) {
            if (node.kind != AigNodeKind::And) continue;
            output += "    n" + std::to_string(node.id) + " = AND(" +
                      literal_name(node.lhs) + ", " + literal_name(node.rhs) + ")\n";
        }
        output += "  outputs:\n";
        for (const auto& port : module.outputs) {
            output += "    " + port.port_name + "[" + std::to_string(port.bit_index) +
                      "] = " + literal_name(port.driver) + "\n";
        }
        output += "  statistics: inputs=" + std::to_string(module.inputs.size()) +
                  ", ANDs=";
        std::size_t and_count = 0;
        for (const auto& node : module.nodes)
            if (node.kind == AigNodeKind::And) ++and_count;
        output += std::to_string(and_count) + ", outputs=" +
                  std::to_string(module.outputs.size()) + "\n";
        output += "endmodule\n\n";
    }
    return output;
}

} // namespace synvanta
