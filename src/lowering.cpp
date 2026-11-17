#include "synvanta/lowering.hpp"
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
namespace synvanta {
namespace {
struct SignalInfo { std::uint32_t width{1}; bool is_signed{false}; bool is_input{false}; bool is_inout{false}; };
struct LoweredValue { ValueId id{0}; std::uint32_t width{1}; bool is_signed{false}; };
enum class VisitState { Unseen, Visiting, Complete };
std::uint32_t signal_width(const std::optional<PackedRange>& range) {
    if (!range) return 1;
    const auto l=static_cast<std::int64_t>(range->left), r=static_cast<std::int64_t>(range->right);
    const auto w=(l>r?l-r:r-l)+1;
    if(w<=0||w>static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()))return 1;
    return static_cast<std::uint32_t>(w);
}
struct ConstantInfo { std::uint32_t width{32}; bool is_signed{true}; std::string literal; };
std::optional<ConstantInfo> parse_constant(const std::string& raw) {
    std::string s; s.reserve(raw.size()); for(char ch:raw)if(ch!='_')s.push_back(ch);
    const auto apos=s.find(static_cast<char>(39));
    if(apos==std::string::npos)return ConstantInfo{32,true,s};
    if(apos==0)return ConstantInfo{1,false,s};
    std::uint32_t width=0;
    const auto wr=std::from_chars(s.data(),s.data()+apos,width,10);
    if(wr.ec!=std::errc{}||wr.ptr!=s.data()+apos||width==0)return std::nullopt;
    std::size_t pos=apos+1;bool sign=false;
    if(pos<s.size()&&(s[pos]=='s'||s[pos]=='S')){sign=true;++pos;}
    if(pos>=s.size())return std::nullopt;
    const char b=s[pos];
    if(b!='b'&&b!='B'&&b!='o'&&b!='O'&&b!='d'&&b!='D'&&b!='h'&&b!='H')return std::nullopt;
    return ConstantInfo{width,sign,s};
}
std::optional<RtlOp> unary_op(TokenKind op) {
    switch(op){case TokenKind::Plus:return RtlOp::UnaryPlus;case TokenKind::Minus:return RtlOp::Negate;
    case TokenKind::Tilde:return RtlOp::BitNot;case TokenKind::Bang:return RtlOp::LogicalNot;
    case TokenKind::Nand:return RtlOp::ReduceNand;case TokenKind::Nor:return RtlOp::ReduceNor;
    case TokenKind::Xnor:return RtlOp::ReduceXnor;default:return std::nullopt;}
}
std::optional<RtlOp> binary_op(TokenKind op) {
    switch(op){case TokenKind::Plus:return RtlOp::Add;case TokenKind::Minus:return RtlOp::Subtract;
    case TokenKind::Star:return RtlOp::Multiply;case TokenKind::Slash:return RtlOp::Divide;
    case TokenKind::Percent:return RtlOp::Modulo;case TokenKind::Ampersand:return RtlOp::BitAnd;
    case TokenKind::Pipe:return RtlOp::BitOr;case TokenKind::Caret:return RtlOp::BitXor;
    case TokenKind::Xnor:return RtlOp::BitXnor;case TokenKind::LogicalAnd:return RtlOp::LogicalAnd;
    case TokenKind::LogicalOr:return RtlOp::LogicalOr;case TokenKind::EqualEqual:return RtlOp::Equal;
    case TokenKind::NotEqual:return RtlOp::NotEqual;case TokenKind::Less:return RtlOp::Less;
    case TokenKind::LessEqual:return RtlOp::LessEqual;case TokenKind::Greater:return RtlOp::Greater;
    case TokenKind::GreaterEqual:return RtlOp::GreaterEqual;case TokenKind::ShiftLeft:return RtlOp::ShiftLeft;
    case TokenKind::ShiftRight:return RtlOp::ShiftRight;default:return std::nullopt;}
}
bool one_bit(RtlOp op) {
    return op==RtlOp::LogicalNot||op==RtlOp::ReduceNand||op==RtlOp::ReduceNor||op==RtlOp::ReduceXnor||
           op==RtlOp::LogicalAnd||op==RtlOp::LogicalOr||op==RtlOp::Equal||op==RtlOp::NotEqual||
           op==RtlOp::Less||op==RtlOp::LessEqual||op==RtlOp::Greater||op==RtlOp::GreaterEqual;
}
class ModuleBuilder {
public:
    explicit ModuleBuilder(const Module& source):source_(source){}
    std::optional<RtlModule> build(std::vector<Diagnostic>& diagnostics) {
        diagnostics_=&diagnostics; start_diagnostics_=diagnostics.size();
        RtlModule result; result.name=source_.name; module_=&result;
        for(const auto& p:source_.ports){
            SignalInfo info{signal_width(p.range),p.is_signed,p.direction==PortDirection::Input,p.direction==PortDirection::Inout};
            signals_.emplace(p.name,info); result.ports.push_back({p.name,p.direction,info.width,info.is_signed});
        }
        for(const auto& n:source_.nets)signals_.emplace(n.name,SignalInfo{signal_width(n.range),n.is_signed,false,false});
        for(const auto& a:source_.assignments)drivers_.emplace(a.lhs,&a);
        for(const auto& p:source_.ports){
            if(p.direction==PortDirection::Input){
                const auto info=signals_.at(p.name);
                const auto id=add_node(RtlOp::Input,info.width,info.is_signed,{}, {},p.name,p.location);
                values_[p.name]=LoweredValue{id,info.width,info.is_signed};states_[p.name]=VisitState::Complete;
            }else if(p.direction==PortDirection::Inout){
                report(DiagnosticSeverity::Error,"LOW0001","inout ports are not supported by the initial RTL lowering stage",p.location);
            }
        }
        for(const auto& a:source_.assignments){
            const auto target=signals_.find(a.lhs);if(target==signals_.end())continue;
            auto value=lower_expr(a.rhs.get());if(!value)continue;
            value=resize_value(*value,target->second.width,target->second.is_signed,a.location);
            result.assignments.push_back({a.lhs,value->id,target->second.width,target->second.is_signed});
            values_[a.lhs]=*value;states_[a.lhs]=VisitState::Complete;
        }
        if(has_module_errors())return std::nullopt;
        return result;
    }
private:
    const Module& source_; RtlModule* module_{nullptr}; std::vector<Diagnostic>* diagnostics_{nullptr};
    std::size_t start_diagnostics_{0};
    std::unordered_map<std::string,SignalInfo> signals_;
    std::unordered_map<std::string,const ContinuousAssignment*> drivers_;
    std::unordered_map<std::string,LoweredValue> values_;
    std::unordered_map<std::string,VisitState> states_;
    bool has_module_errors()const{
        for(std::size_t i=start_diagnostics_;i<diagnostics_->size();++i)
            if((*diagnostics_)[i].severity==DiagnosticSeverity::Error)return true;
        return false;
    }
    void report(DiagnosticSeverity severity,std::string code,std::string message,const SourceLocation& loc){
        diagnostics_->push_back({severity,std::move(code),std::move(message),loc});
    }
    ValueId add_node(RtlOp op,std::uint32_t width,bool sign,std::vector<ValueId> operands,
                     std::string literal,std::string name,const SourceLocation& loc){
        const auto id=static_cast<ValueId>(module_->nodes.size());
        module_->nodes.push_back({id,op,width,sign,std::move(operands),std::move(literal),std::move(name),loc});return id;
    }
    LoweredValue resize_value(LoweredValue value,std::uint32_t width,bool sign,const SourceLocation& loc){
        if(value.width==width&&value.is_signed==sign)return value;
        const auto id=add_node(RtlOp::Resize,width,sign,{value.id},{},{},loc);
        return LoweredValue{id,width,sign};
    }
    std::optional<LoweredValue> resolve(const Expr& e){
        const auto cached=values_.find(e.text);if(cached!=values_.end())return cached->second;
        const auto signal=signals_.find(e.text);
        if(signal==signals_.end()){report(DiagnosticSeverity::Error,"LOW0002","unknown signal '"+e.text+"'",e.location);return std::nullopt;}
        if(signal->second.is_inout){report(DiagnosticSeverity::Error,"LOW0001","inout signal cannot be lowered",e.location);return std::nullopt;}
        if(states_[e.text]==VisitState::Visiting){report(DiagnosticSeverity::Error,"LOW0003","combinational dependency cycle involving '"+e.text+"'",e.location);return std::nullopt;}
        const auto driver=drivers_.find(e.text);
        if(driver==drivers_.end()){report(DiagnosticSeverity::Error,"LOW0004","signal '"+e.text+"' has no source or continuous driver",e.location);return std::nullopt;}
        states_[e.text]=VisitState::Visiting;
        auto value=lower_expr(driver->second->rhs.get());if(!value)return std::nullopt;
        value=resize_value(*value,signal->second.width,signal->second.is_signed,e.location);
        values_[e.text]=*value;states_[e.text]=VisitState::Complete;return value;
    }
    std::optional<LoweredValue> lower_expr(const Expr* e){
        if(!e)return std::nullopt;
        if(e->kind==ExprKind::Identifier)return resolve(*e);
        if(e->kind==ExprKind::Number){
            const auto c=parse_constant(e->text);
            if(!c){report(DiagnosticSeverity::Error,"LOW0005","unsupported numeric literal '"+e->text+"'",e->location);return std::nullopt;}
            const auto id=add_node(RtlOp::Constant,c->width,c->is_signed,{},c->literal,{},e->location);
            return LoweredValue{id,c->width,c->is_signed};
        }
        if(e->kind==ExprKind::Unary){
            auto v=lower_expr(e->lhs.get());const auto op=unary_op(e->op);
            if(!v||!op){report(DiagnosticSeverity::Error,"LOW0006","unsupported unary operator",e->location);return std::nullopt;}
            const auto w=one_bit(*op)?1U:v->width;const bool sign=one_bit(*op)?false:v->is_signed;
            const auto id=add_node(*op,w,sign,{v->id},{},{},e->location);return LoweredValue{id,w,sign};
        }
        if(e->kind==ExprKind::Binary){
            auto l=lower_expr(e->lhs.get());auto r=lower_expr(e->rhs.get());const auto op=binary_op(e->op);
            if(!l||!r||!op){report(DiagnosticSeverity::Error,"LOW0007","unsupported binary operator",e->location);return std::nullopt;}
            std::uint32_t w=std::max(l->width,r->width);bool sign=l->is_signed&&r->is_signed;
            if(*op==RtlOp::LogicalAnd||*op==RtlOp::LogicalOr){w=1;sign=false;}
            else if(*op==RtlOp::ShiftLeft||*op==RtlOp::ShiftRight){w=l->width;sign=l->is_signed;}
            else{l=resize_value(*l,w,sign,e->location);r=resize_value(*r,w,sign,e->location);if(one_bit(*op)){w=1;sign=false;}}
            const auto id=add_node(*op,w,sign,{l->id,r->id},{},{},e->location);return LoweredValue{id,w,sign};
        }
        if(e->kind==ExprKind::Conditional){
            auto c=lower_expr(e->lhs.get());auto y=lower_expr(e->rhs.get());auto n=lower_expr(e->third.get());
            if(!c||!y||!n)return std::nullopt;
            const auto w=std::max(y->width,n->width);const bool sign=y->is_signed&&n->is_signed;
            y=resize_value(*y,w,sign,e->location);n=resize_value(*n,w,sign,e->location);
            const auto id=add_node(RtlOp::Mux,w,sign,{c->id,y->id,n->id},{},{},e->location);return LoweredValue{id,w,sign};
        }
        report(DiagnosticSeverity::Error,"LOW0008","unsupported expression kind",e->location);return std::nullopt;
    }
};
}
LoweringResult RtlLowerer::lower(const CompilationUnit& unit) const {
    LoweringResult result;
    for(const auto& module:unit.modules){ModuleBuilder builder(module);auto lowered=builder.build(result.diagnostics);if(lowered)result.design.modules.push_back(std::move(*lowered));}
    return result;
}
} // namespace synvanta
