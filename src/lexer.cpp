#include "synvanta/lexer.hpp"
#include <cctype>
#include <unordered_map>
#include <utility>
namespace synvanta {
namespace {
const std::unordered_map<std::string, TokenKind> keywords{
    {"module",TokenKind::Module},{"endmodule",TokenKind::Endmodule},
    {"input",TokenKind::Input},{"output",TokenKind::Output},{"inout",TokenKind::Inout},
    {"wire",TokenKind::Wire},{"logic",TokenKind::Logic},{"reg",TokenKind::Reg},
    {"bit",TokenKind::Bit},{"signed",TokenKind::Signed},{"unsigned",TokenKind::Unsigned},
    {"assign",TokenKind::Assign},{"parameter",TokenKind::Parameter},{"localparam",TokenKind::Localparam}
};
bool id_start(char c) {
    const auto u=static_cast<unsigned char>(c);
    return std::isalpha(u)!=0 || c=='_' || c=='$';
}
bool id_continue(char c) {
    const auto u=static_cast<unsigned char>(c);
    return std::isalnum(u)!=0 || c=='_' || c=='$';
}
bool based_digit(char c) {
    // Wildcard '?' digits are outside the current language subset. Keeping '?'
    // as a separate token is necessary for an adjacent conditional operator:
    // 1'b1?a:b must lex as NUMBER, QUESTION, IDENTIFIER, COLON, IDENTIFIER.
    const auto u=static_cast<unsigned char>(c);
    return std::isalnum(u)!=0 || c=='_';
}
}
const char* token_kind_name(TokenKind k) {
    switch(k) {
    case TokenKind::EndOfFile:return "end of file"; case TokenKind::Identifier:return "identifier";
    case TokenKind::Number:return "number"; case TokenKind::Module:return "module";
    case TokenKind::Endmodule:return "endmodule"; case TokenKind::Input:return "input";
    case TokenKind::Output:return "output"; case TokenKind::Inout:return "inout";
    case TokenKind::Wire:return "wire"; case TokenKind::Logic:return "logic";
    case TokenKind::Reg:return "reg"; case TokenKind::Bit:return "bit";
    case TokenKind::Signed:return "signed"; case TokenKind::Unsigned:return "unsigned";
    case TokenKind::Assign:return "assign"; case TokenKind::Parameter:return "parameter";
    case TokenKind::Localparam:return "localparam"; case TokenKind::LParen:return "(";
    case TokenKind::RParen:return ")"; case TokenKind::LBracket:return "[";
    case TokenKind::RBracket:return "]"; case TokenKind::LBrace:return "{";
    case TokenKind::RBrace:return "}"; case TokenKind::Semicolon:return ";";
    case TokenKind::Colon:return ":"; case TokenKind::Comma:return ",";
    case TokenKind::Dot:return "."; case TokenKind::Question:return "?";
    case TokenKind::Hash:return "#"; case TokenKind::Equal:return "=";
    case TokenKind::Plus:return "+"; case TokenKind::Minus:return "-";
    case TokenKind::Star:return "*"; case TokenKind::Slash:return "/";
    case TokenKind::Percent:return "%"; case TokenKind::Ampersand:return "&";
    case TokenKind::Pipe:return "|"; case TokenKind::Caret:return "^";
    case TokenKind::Tilde:return "~"; case TokenKind::Bang:return "!";
    case TokenKind::Less:return "<"; case TokenKind::Greater:return ">";
    case TokenKind::LessEqual:return "<="; case TokenKind::GreaterEqual:return ">=";
    case TokenKind::EqualEqual:return "=="; case TokenKind::NotEqual:return "!=";
    case TokenKind::LogicalAnd:return "&&"; case TokenKind::LogicalOr:return "||";
    case TokenKind::ShiftLeft:return "<<"; case TokenKind::ShiftRight:return ">>";
    case TokenKind::Xnor:return "~^"; case TokenKind::Nand:return "~&";
    case TokenKind::Nor:return "~|"; case TokenKind::Unknown:return "unknown token";
    }
    return "unknown token";
}
Lexer::Lexer(std::string_view s,std::string f):source_(s),filename_(std::move(f)){}
bool Lexer::at_end(std::size_t n) const{return offset_+n>=source_.size();}
char Lexer::peek(std::size_t n) const{return at_end(n)?'\0':source_[offset_+n];}
char Lexer::advance(){
    if(at_end())return '\0';
    const char c=source_[offset_++];
    if(c=='\n'){++line_;column_=1;}else{++column_;}
    return c;
}
bool Lexer::match(char c){if(at_end()||peek()!=c)return false;advance();return true;}
SourceLocation Lexer::location() const{return {filename_,offset_,line_,column_};}
void Lexer::report(std::string c,std::string m,SourceLocation l){
    diagnostics_.push_back({DiagnosticSeverity::Error,std::move(c),std::move(m),std::move(l)});
}
void Lexer::skip_trivia(){
    for(;;){
        if(at_end())return;
        if(std::isspace(static_cast<unsigned char>(peek()))!=0){advance();continue;}
        if(peek()=='/'&&peek(1)=='/'){while(!at_end()&&peek()!='\n')advance();continue;}
        if(peek()=='/'&&peek(1)=='*'){
            const auto start=location();advance();advance();bool closed=false;
            while(!at_end()){
                if(peek()=='*'&&peek(1)=='/'){advance();advance();closed=true;break;}
                advance();
            }
            if(!closed)report("LEX0002","unterminated block comment",start);
            continue;
        }
        return;
    }
}
Token Lexer::make_token(TokenKind k,std::size_t s,SourceLocation l) const{
    return {k,std::string(source_.substr(s,offset_-s)),std::move(l)};
}
Token Lexer::scan_number(){
    const auto start=offset_;const auto loc=location();
    if(peek()=='\''){
        advance();
        if(peek()=='0'||peek()=='1'||peek()=='x'||peek()=='X'||peek()=='z'||peek()=='Z')advance();
        else report("LEX0003","invalid unbased unsized literal",loc);
        return make_token(TokenKind::Number,start,loc);
    }
    while(!at_end()&&(std::isdigit(static_cast<unsigned char>(peek()))!=0||peek()=='_'))advance();
    if(peek()=='\''){
        advance();if(peek()=='s'||peek()=='S')advance();
        if(peek()=='b'||peek()=='B'||peek()=='o'||peek()=='O'||peek()=='d'||peek()=='D'||peek()=='h'||peek()=='H'){
            advance();while(!at_end()&&based_digit(peek()))advance();
        }else report("LEX0004","expected a base specifier after apostrophe in sized literal",loc);
    }
    return make_token(TokenKind::Number,start,loc);
}
Token Lexer::scan_token(){
    const auto start=offset_;const auto loc=location();const char c=advance();
    if(id_start(c)){
        while(!at_end()&&id_continue(peek()))advance();
        const std::string text(source_.substr(start,offset_-start));
        const auto it=keywords.find(text);
        return make_token(it==keywords.end()?TokenKind::Identifier:it->second,start,loc);
    }
    if(std::isdigit(static_cast<unsigned char>(c))!=0||c=='\''){--offset_;--column_;return scan_number();}
    auto two=[&](char expected,TokenKind yes,TokenKind no){return make_token(match(expected)?yes:no,start,loc);};
    switch(c){
    case '(':return make_token(TokenKind::LParen,start,loc); case ')':return make_token(TokenKind::RParen,start,loc);
    case '[':return make_token(TokenKind::LBracket,start,loc); case ']':return make_token(TokenKind::RBracket,start,loc);
    case '{':return make_token(TokenKind::LBrace,start,loc); case '}':return make_token(TokenKind::RBrace,start,loc);
    case ';':return make_token(TokenKind::Semicolon,start,loc); case ':':return make_token(TokenKind::Colon,start,loc);
    case ',':return make_token(TokenKind::Comma,start,loc); case '.':return make_token(TokenKind::Dot,start,loc);
    case '?':return make_token(TokenKind::Question,start,loc); case '#':return make_token(TokenKind::Hash,start,loc);
    case '=':return two('=',TokenKind::EqualEqual,TokenKind::Equal);
    case '+':return make_token(TokenKind::Plus,start,loc); case '-':return make_token(TokenKind::Minus,start,loc);
    case '*':return make_token(TokenKind::Star,start,loc); case '/':return make_token(TokenKind::Slash,start,loc);
    case '%':return make_token(TokenKind::Percent,start,loc);
    case '&':return two('&',TokenKind::LogicalAnd,TokenKind::Ampersand);
    case '|':return two('|',TokenKind::LogicalOr,TokenKind::Pipe);
    case '^':if(match('~'))return make_token(TokenKind::Xnor,start,loc);return make_token(TokenKind::Caret,start,loc);
    case '~':
        if(match('&'))return make_token(TokenKind::Nand,start,loc);
        if(match('|'))return make_token(TokenKind::Nor,start,loc);
        if(match('^'))return make_token(TokenKind::Xnor,start,loc);
        return make_token(TokenKind::Tilde,start,loc);
    case '!':return two('=',TokenKind::NotEqual,TokenKind::Bang);
    case '<':if(match('<'))return make_token(TokenKind::ShiftLeft,start,loc);return two('=',TokenKind::LessEqual,TokenKind::Less);
    case '>':if(match('>'))return make_token(TokenKind::ShiftRight,start,loc);return two('=',TokenKind::GreaterEqual,TokenKind::Greater);
    default:report("LEX0001",std::string("unexpected character '")+c+"'",loc);return make_token(TokenKind::Unknown,start,loc);
    }
}
std::vector<Token> Lexer::tokenize(){
    std::vector<Token> out;
    while(true){skip_trivia();if(at_end())break;out.push_back(scan_token());}
    out.push_back({TokenKind::EndOfFile,"",location()});return out;
}
} // namespace synvanta
