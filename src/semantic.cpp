#include "synvanta/semantic.hpp"

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace synvanta {
namespace {

struct Symbol {
    bool is_port{false};
    PortDirection direction{PortDirection::Input};
    SourceLocation location;
};

void report(std::vector<Diagnostic>& diagnostics, DiagnosticSeverity severity,
            std::string code, std::string message, const SourceLocation& location) {
    diagnostics.push_back(Diagnostic{severity, std::move(code), std::move(message), location});
}

void visit_identifiers(const Expr* expression, const std::function<void(const Expr&)>& visitor) {
    if (expression == nullptr) return;
    if (expression->kind == ExprKind::Identifier) visitor(*expression);
    visit_identifiers(expression->lhs.get(), visitor);
    visit_identifiers(expression->rhs.get(), visitor);
    visit_identifiers(expression->third.get(), visitor);
}

} // namespace

std::vector<Diagnostic> SemanticAnalyzer::analyze(const CompilationUnit& unit) const {
    std::vector<Diagnostic> diagnostics;
    std::unordered_map<std::string, SourceLocation> module_names;

    for (const auto& module : unit.modules) {
        const auto [module_it, module_inserted] = module_names.emplace(module.name, module.location);
        (void)module_it;
        if (!module_inserted) {
            report(diagnostics, DiagnosticSeverity::Error, "SEM0001",
                   "duplicate module declaration '" + module.name + "'", module.location);
        }

        std::unordered_map<std::string, Symbol> symbols;
        for (const auto& port : module.ports) {
            const auto [it, inserted] = symbols.emplace(port.name, Symbol{true, port.direction, port.location});
            (void)it;
            if (!inserted) {
                report(diagnostics, DiagnosticSeverity::Error, "SEM0002",
                       "duplicate port or signal declaration '" + port.name + "'", port.location);
            }
        }
        for (const auto& net : module.nets) {
            const auto [it, inserted] = symbols.emplace(net.name, Symbol{false, PortDirection::Input, net.location});
            (void)it;
            if (!inserted) {
                report(diagnostics, DiagnosticSeverity::Error, "SEM0002",
                       "duplicate port or signal declaration '" + net.name + "'", net.location);
            }
        }

        std::unordered_set<std::string> driven_targets;
        for (const auto& assignment : module.assignments) {
            const auto target = symbols.find(assignment.lhs);
            if (target == symbols.end()) {
                report(diagnostics, DiagnosticSeverity::Error, "SEM0003",
                       "assignment target '" + assignment.lhs + "' is not declared", assignment.location);
            } else if (target->second.is_port && target->second.direction == PortDirection::Input) {
                report(diagnostics, DiagnosticSeverity::Error, "SEM0004",
                       "continuous assignment cannot drive input port '" + assignment.lhs + "'", assignment.location);
            }

            if (!driven_targets.insert(assignment.lhs).second) {
                report(diagnostics, DiagnosticSeverity::Error, "SEM0005",
                       "multiple continuous assignments drive '" + assignment.lhs + "'", assignment.location);
            }

            visit_identifiers(assignment.rhs.get(), [&](const Expr& identifier) {
                if (symbols.find(identifier.text) == symbols.end()) {
                    report(diagnostics, DiagnosticSeverity::Error, "SEM0006",
                           "identifier '" + identifier.text + "' is not declared in module '" + module.name + "'",
                           identifier.location);
                }
            });
        }

        for (const auto& port : module.ports) {
            if (port.direction == PortDirection::Output && driven_targets.find(port.name) == driven_targets.end()) {
                report(diagnostics, DiagnosticSeverity::Warning, "SEM1001",
                       "output port '" + port.name + "' has no continuous driver in the supported subset",
                       port.location);
            }
        }
    }
    return diagnostics;
}
} // namespace synvanta
