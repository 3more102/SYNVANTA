#include "synvanta/parser.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <utility>
namespace synvanta {
const char* port_direction_name(PortDirection d){
    switch(d){case PortDirection::Input:return "input";case PortDirection::Output:return "output";case PortDirection::Inout:return "inout";}
    return "input";
}
const char* net_kind_name(NetKind k){
    switch(k){case NetKind::Wire:return "wire";case NetKind::Logic:return "logic";case NetKind::Reg:return "reg";case NetKind::Bit:return "bit";}
    return "logic";
}
Parser::Parser(std::vector<Token> t):tokens_(std::move(t)){
    if(tokens_.empty()||tokens_.back().kind!=TokenKind::EndOfFile)tokens_.push_back({TokenKind::EndOfFile,"",{}});
}
bool Parser::at_end()const{return peek().kind==TokenKind::EndOfFile;}
const Token& Parser::peek()const{return tokens_[current_];}
const Token& Parser::previous()const{return tokens_[current_-1];}
const Token& Parser::advance(){if(!at_end())++current_;return previous();}
bool Parser::check(TokenKind k)const{return peek().kind==k;}
bool Parser::match(TokenKind k){if(!check(k))return false;advance();return true;}
bool Parser::expect(TokenKind k,const std::string& m){
    if(match(k))return true;
    report("PAR0001",m+"; found "+token_kind_name(peek().kind),peek().location);return false;
}
void Parser::report(std::string c,std::string m,const SourceLocation& l){
    diagnostics_.push_back({DiagnosticSeverity::Error,std::move(c),std::move(m),l});
}
void Parser::synchronize_module_item(){
    while(!at_end()&&!check(TokenKind::Endmodule)){
        if(previous().kind==TokenKind::Semicolon)return;
        if(check(TokenKind::Assign)||check(TokenKind::Wire)||check(TokenKind::Logic)||check(TokenKind::Reg))return;
        advance();
    }
}
void Parser::synchronize_to_module(){while(!at_end()&&!check(TokenKind::Module))advance();}
CompilationUnit Parser::parse(){
    CompilationUnit unit;
    while(!at_end()){
        if(!check(TokenKind::Module)){
            report("PAR0002","expected a module declaration",peek().location);synchronize_to_module();if(at_end())break;
        }
        const auto start=current_;auto module=parse_module();
        if(module)unit.modules.push_back(std::move(*module));
        else{synchronize_to_module();if(current_==start&&!at_end())advance();}
    }
    if(unit.modules.empty()&&diagnostics_.empty())report("PAR0003","source file contains no module declarations",peek().location);
    return unit;
}
std::optional<Module> Parser::parse_module(){
    if(!expect(TokenKind::Module,"expected 'module'"))return std::nullopt;
    const Token module_token=previous();
    if(!expect(TokenKind::Identifier,"expected module name"))return std::nullopt;
    Module module;module.name=previous().lexeme;module.location=module_token.location;
    if(!expect(TokenKind::LParen,"expected '(' after module name"))return std::nullopt;
    if(!check(TokenKind::RParen)){
        for(;;){
            auto port=parse_port();if(port)module.ports.push_back(std::move(*port));
            if(!match(TokenKind::Comma))break;
            if(!check(TokenKind::Input)&&!check(TokenKind::Output)&&!check(TokenKind::Inout)){
                report("PAR0004","each ANSI-style port must declare its direction explicitly in SYNVANTA 0.1",peek().location);
                synchronize_module_item();return std::nullopt;
            }
        }
    }
    if(!expect(TokenKind::RParen,"expected ')' after module port list"))return std::nullopt;
    if(!expect(TokenKind::Semicolon,"expected ';' after module header"))return std::nullopt;
    while(!at_end()&&!check(TokenKind::Endmodule)){
        if(check(TokenKind::Assign)){
            auto a=parse_assignment();if(a)module.assignments.push_back(std::move(*a));else synchronize_module_item();continue;
        }
        if(check(TokenKind::Wire)||check(TokenKind::Logic)||check(TokenKind::Reg)||check(TokenKind::Bit)){
            if(!parse_net_declaration(module))synchronize_module_item();continue;
        }
        report("PAR0005","unsupported module item; SYNVANTA 0.1 accepts continuous assignments and simple net declarations",peek().location);
        synchronize_module_item();if(!check(TokenKind::Endmodule)&&!at_end())advance();
    }
    if(!expect(TokenKind::Endmodule,"expected 'endmodule'"))return std::nullopt;
    return module;
}
std::optional<Port> Parser::parse_port(){
    Port p;p.location=peek().location;
    if(match(TokenKind::Input))p.direction=PortDirection::Input;
    else if(match(TokenKind::Output))p.direction=PortDirection::Output;
    else if(match(TokenKind::Inout))p.direction=PortDirection::Inout;
    else{report("PAR0006","expected input, output, or inout port declaration",peek().location);return std::nullopt;}
    if(match(TokenKind::Wire))p.net_kind=NetKind::Wire;
    else if(match(TokenKind::Logic))p.net_kind=NetKind::Logic;
    else if(match(TokenKind::Reg))p.net_kind=NetKind::Reg;
    else if(match(TokenKind::Bit))p.net_kind=NetKind::Bit;
    if(match(TokenKind::Signed))p.is_signed=true;else match(TokenKind::Unsigned);
    if(check(TokenKind::LBracket)){auto r=parse_range();if(!r)return std::nullopt;p.range=*r;}
    if(!expect(TokenKind::Identifier,"expected port identifier"))return std::nullopt;
    p.name=previous().lexeme;return p;
}
std::optional<PackedRange> Parser::parse_range(){
    if(!expect(TokenKind::LBracket,"expected '['"))return std::nullopt;
    if(!check(TokenKind::Number)){report("PAR0007","packed range bounds must be integer literals in SYNVANTA 0.1",peek().location);return std::nullopt;}
    const Token lt=advance();const auto left=parse_integer_literal(lt);
    if(!expect(TokenKind::Colon,"expected ':' in packed range"))return std::nullopt;
    if(!check(TokenKind::Number)){report("PAR0008","packed range bounds must be integer literals in SYNVANTA 0.1",peek().location);return std::nullopt;}
    const Token rt=advance();const auto right=parse_integer_literal(rt);
    if(!expect(TokenKind::RBracket,"expected ']' after packed range"))return std::nullopt;
    if(!left||!right){report("PAR0009","unsupported numeric literal in packed range",lt.location);return std::nullopt;}
    return PackedRange{*left,*right};
}
std::optional<int> Parser::parse_integer_literal(const Token& t)const{
    std::string s=t.lexeme;s.erase(std::remove(s.begin(),s.end(),'_'),s.end());
        const auto apos=s.find(static_cast<char>(39));
    if(apos!=std::string::npos){
        if(apos+1>=s.size())return std::nullopt;std::size_t pos=apos+1;
        if(s[pos]=='s'||s[pos]=='S')++pos;if(pos>=s.size())return std::nullopt;
        const char b=s[pos++];int base=10;
        if(b=='b'||b=='B')base=2;else if(b=='o'||b=='O')base=8;else if(b=='h'||b=='H')base=16;else if(b!='d'&&b!='D')return std::nullopt;
        long long value=0;const auto r=std::from_chars(s.data()+static_cast<std::ptrdiff_t>(pos),s.data()+s.size(),value,base);
        if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size()||value<std::numeric_limits<int>::min()||value>std::numeric_limits<int>::max())return std::nullopt;
        return static_cast<int>(value);
    }
    long long value=0;const auto r=std::from_chars(s.data(),s.data()+s.size(),value,10);
    if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size()||value<std::numeric_limits<int>::min()||value>std::numeric_limits<int>::max())return std::nullopt;
    return static_cast<int>(value);
}
bool Parser::parse_net_declaration(Module& module) {
    const Token type_token = advance();
    NetKind kind = NetKind::Wire;
    if (type_token.kind == TokenKind::Logic) kind = NetKind::Logic;
    else if (type_token.kind == TokenKind::Reg) kind = NetKind::Reg;
    else if (type_token.kind == TokenKind::Bit) kind = NetKind::Bit;

    bool is_signed = false;
    if (match(TokenKind::Signed)) is_signed = true;
    else match(TokenKind::Unsigned);

    std::optional<PackedRange> range;
    if (check(TokenKind::LBracket)) {
        range = parse_range();
        if (!range) return false;
    }

    for (;;) {
        if (!expect(TokenKind::Identifier, "expected internal net name")) return false;
        NetDeclaration declaration;
        declaration.net_kind = kind;
        declaration.is_signed = is_signed;
        declaration.range = range;
        declaration.name = previous().lexeme;
        declaration.location = previous().location;
        module.nets.push_back(std::move(declaration));
        if (!match(TokenKind::Comma)) break;
    }
    return expect(TokenKind::Semicolon, "expected ';' after internal net declaration");
}

std::optional<ContinuousAssignment> Parser::parse_assignment(){
    const Token start=advance();if(!expect(TokenKind::Identifier,"continuous assignment target must be an identifier"))return std::nullopt;
    ContinuousAssignment a;a.lhs=previous().lexeme;a.location=start.location;
    if(!expect(TokenKind::Equal,"expected '=' in continuous assignment"))return std::nullopt;
    a.rhs=parse_expression();if(!a.rhs)return std::nullopt;
    if(!expect(TokenKind::Semicolon,"expected ';' after continuous assignment"))return std::nullopt;
    return a;
}
int Parser::precedence(TokenKind k)const{
    switch(k){
    case TokenKind::LogicalOr:return 2;case TokenKind::LogicalAnd:return 3;case TokenKind::Pipe:return 4;
    case TokenKind::Caret:case TokenKind::Xnor:return 5;case TokenKind::Ampersand:return 6;
    case TokenKind::EqualEqual:case TokenKind::NotEqual:return 7;
    case TokenKind::Less:case TokenKind::LessEqual:case TokenKind::Greater:case TokenKind::GreaterEqual:return 8;
    case TokenKind::ShiftLeft:case TokenKind::ShiftRight:return 9;
    case TokenKind::Plus:case TokenKind::Minus:return 10;
    case TokenKind::Star:case TokenKind::Slash:case TokenKind::Percent:return 11;
    default:return 0;
    }
}
std::unique_ptr<Expr> Parser::parse_expression(int min){
    auto lhs=parse_prefix();if(!lhs)return nullptr;
    for(;;){
        if(check(TokenKind::Question)&&min<=1){
            const Token q=advance();auto yes=parse_expression(1);
            if(!expect(TokenKind::Colon,"expected ':' in conditional expression"))return nullptr;
            auto no=parse_expression(1);if(!yes||!no)return nullptr;
            auto e=std::make_unique<Expr>();e->kind=ExprKind::Conditional;e->location=q.location;
            e->lhs=std::move(lhs);e->rhs=std::move(yes);e->third=std::move(no);lhs=std::move(e);continue;
        }
        const int p=precedence(peek().kind);if(p<min||p==0)break;
        const Token op=advance();auto rhs=parse_expression(p+1);if(!rhs)return nullptr;
        auto e=std::make_unique<Expr>();e->kind=ExprKind::Binary;e->op=op.kind;e->location=op.location;
        e->lhs=std::move(lhs);e->rhs=std::move(rhs);lhs=std::move(e);
    }
    return lhs;
}
std::unique_ptr<Expr> Parser::parse_prefix(){
    if(match(TokenKind::Tilde)||match(TokenKind::Bang)||match(TokenKind::Plus)||match(TokenKind::Minus)||
       match(TokenKind::Nand)||match(TokenKind::Nor)||match(TokenKind::Xnor)){
        const Token op=previous();auto child=parse_prefix();if(!child)return nullptr;
        auto e=std::make_unique<Expr>();e->kind=ExprKind::Unary;e->op=op.kind;e->location=op.location;e->lhs=std::move(child);return e;
    }
    if(match(TokenKind::Identifier)){auto e=std::make_unique<Expr>();e->kind=ExprKind::Identifier;e->text=previous().lexeme;e->location=previous().location;return e;}
    if(match(TokenKind::Number)){auto e=std::make_unique<Expr>();e->kind=ExprKind::Number;e->text=previous().lexeme;e->location=previous().location;return e;}
    if(match(TokenKind::LParen)){auto e=parse_expression();if(!expect(TokenKind::RParen,"expected ')' after expression"))return nullptr;return e;}
    report("PAR0012","expected an expression",peek().location);return nullptr;
}
} // namespace synvanta
