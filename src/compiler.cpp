#include "synvanta/compiler.hpp"
#include "synvanta/lexer.hpp"
#include "synvanta/parser.hpp"
#include "synvanta/semantic.hpp"
#include <sstream>
#include <utility>
namespace synvanta {
CompilationResult Compiler::compile(std::string_view source,std::string filename)const{
    Lexer lexer(source,filename);auto tokens=lexer.tokenize();Parser parser(std::move(tokens));
    CompilationResult result;result.unit=parser.parse();result.diagnostics=lexer.diagnostics();
    result.diagnostics.insert(result.diagnostics.end(),parser.diagnostics().begin(),parser.diagnostics().end());
    if (!has_errors(result.diagnostics)) {
        const SemanticAnalyzer analyzer;
        auto semantic_diagnostics = analyzer.analyze(result.unit);
        result.diagnostics.insert(result.diagnostics.end(), semantic_diagnostics.begin(), semantic_diagnostics.end());
    }
    return result;
}
std::string format_diagnostic(const Diagnostic& d){
    const char* severity="error";if(d.severity==DiagnosticSeverity::Warning)severity="warning";if(d.severity==DiagnosticSeverity::Note)severity="note";
    std::ostringstream out;out<<(d.location.file.empty()?"<source>":d.location.file)<<':'<<d.location.line<<':'<<d.location.column<<": "<<severity;
    if(!d.code.empty())out<<'['<<d.code<<']';out<<": "<<d.message;return out.str();
}
} // namespace synvanta
