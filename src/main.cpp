#include "synvanta/compiler.hpp"
#include "synvanta/lowering.hpp"
#include "synvanta/optimizer.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
namespace {
void usage(std::ostream& o){
    o<<"SYNVANTA 0.1.0 - independent SystemVerilog-to-ASIC synthesis compiler\n\n"
     <<"Usage:\n  synvanta --version\n  synvanta --help\n  synvanta check <input.sv>\n  synvanta inspect <input.sv>\n  synvanta ir <input.sv>\n  synvanta optimize <input.sv>\n";
}
int run(const std::string& command,const std::string& path){
    std::ifstream in(path,std::ios::binary);if(!in){std::cerr<<"error: cannot open input file: "<<path<<'\n';return 2;}
    const std::string source{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
    const synvanta::Compiler compiler;const auto result=compiler.compile(source,path);
    for(const auto& d:result.diagnostics)std::cerr<<synvanta::format_diagnostic(d)<<'\n';
    if(!result.success())return 1;
    if(command=="check"){std::cout<<"SYNVANTA: syntax and semantic checks passed ("<<result.unit.modules.size()<<" module(s)).\n";return 0;}
    if(command=="ir"||command=="optimize"){
        const synvanta::RtlLowerer lowerer;
        const auto lowered=lowerer.lower(result.unit);
        for(const auto& d:lowered.diagnostics)std::cerr<<synvanta::format_diagnostic(d)<<'\n';
        if(!lowered.success())return 1;
        if(command=="ir"){std::cout<<synvanta::dump_rtl_design(lowered.design);return 0;}
        const synvanta::RtlOptimizer optimizer;
        const auto optimized=optimizer.optimize(lowered.design);
        for(const auto& d:optimized.diagnostics)std::cerr<<synvanta::format_diagnostic(d)<<'\n';
        if(!optimized.success())return 1;
        std::cout<<"# optimization: folded="<<optimized.stats.constants_folded
                 <<" cse_removed="<<optimized.stats.common_subexpressions_removed
                 <<" dead_removed="<<optimized.stats.dead_nodes_removed
                 <<" nodes="<<optimized.stats.nodes_before<<"->"<<optimized.stats.nodes_after<<'\n';
        std::cout<<synvanta::dump_rtl_design(optimized.design);
        return 0;
    }
    for(const auto& m:result.unit.modules){
        std::cout<<"module "<<m.name<<"\n  ports: "<<m.ports.size()<<'\n';
        for(const auto& p:m.ports){
            std::cout<<"    "<<synvanta::port_direction_name(p.direction)<<' '<<synvanta::net_kind_name(p.net_kind)<<' ';
            if(p.is_signed)std::cout<<"signed ";if(p.range)std::cout<<'['<<p.range->left<<':'<<p.range->right<<"] ";
            std::cout<<p.name<<'\n';
        }
        std::cout<<"  continuous assignments: "<<m.assignments.size()<<'\n';
    }
    return 0;
}
}
int main(int argc,char** argv){
    if(argc==2&&std::string(argv[1])=="--version"){std::cout<<"SYNVANTA 0.1.0\n";return 0;}
    if(argc==2&&std::string(argv[1])=="--help"){usage(std::cout);return 0;}
    if(argc==3){const std::string cmd(argv[1]);if(cmd=="check"||cmd=="inspect"||cmd=="ir"||cmd=="optimize")return run(cmd,argv[2]);}
    usage(std::cerr);return 2;
}
