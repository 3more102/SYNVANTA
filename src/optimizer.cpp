#include "synvanta/optimizer.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
namespace synvanta {
namespace {
std::uint64_t mask_for(std::uint32_t width){
    if(width>=64)return std::numeric_limits<std::uint64_t>::max();
    if(width==0)return 0;
    return (std::uint64_t{1}<<width)-1;
}
std::optional<std::uint64_t> parse_value(const RtlNode& node){
    if(node.width>64)return std::nullopt;
    std::string s;for(char c:node.literal)if(c!='_')s.push_back(c);
    const auto apos=s.find(static_cast<char>(39));
    if(apos==std::string::npos){
        std::uint64_t v=0;const auto r=std::from_chars(s.data(),s.data()+s.size(),v,10);
        if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size())return std::nullopt;
        return v&mask_for(node.width);
    }
    if(apos==0){if(s.size()!=2)return std::nullopt;if(s[1]=='0')return 0;if(s[1]=='1')return 1;return std::nullopt;}
    std::size_t p=apos+1;if(p<s.size()&&(s[p]=='s'||s[p]=='S'))++p;if(p>=s.size())return std::nullopt;
    const char b=s[p++];int base=10;
    if(b=='b'||b=='B')base=2;else if(b=='o'||b=='O')base=8;else if(b=='h'||b=='H')base=16;else if(b!='d'&&b!='D')return std::nullopt;
    std::uint64_t v=0;const auto r=std::from_chars(s.data()+static_cast<std::ptrdiff_t>(p),s.data()+s.size(),v,base);
    if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size())return std::nullopt;
    return v&mask_for(node.width);
}
std::int64_t signed_value(std::uint64_t v,std::uint32_t width){
    const auto mask=mask_for(width);v&=mask;
    if(width>0&&width<64&&(v&(std::uint64_t{1}<<(width-1)))!=0)v|=~mask;
    return std::bit_cast<std::int64_t>(v);
}
std::string format_constant(std::uint32_t width,std::uint64_t value){
    std::ostringstream out;out<<width<<"'h"<<std::hex<<std::uppercase<<(value&mask_for(width));return out.str();
}
std::optional<std::uint64_t> evaluate(const RtlNode& n,const std::vector<RtlNode>& nodes,
                                      const std::vector<std::optional<std::uint64_t>>& values){
    if(n.width>64)return std::nullopt;
    if(n.op==RtlOp::Constant)return parse_value(n);
    if(n.op==RtlOp::Input)return std::nullopt;
    std::vector<std::uint64_t> a;
    for(auto id:n.operands){if(id>=values.size()||!values[id])return std::nullopt;a.push_back(*values[id]);}
    const auto mask=mask_for(n.width);
    if(n.op==RtlOp::Resize&&a.size()==1){
        const auto id=n.operands[0];if(id>=nodes.size()||nodes[id].width>64)return std::nullopt;
        const auto& src=nodes[id];auto v=a[0]&mask_for(src.width);
        if(n.width>src.width&&src.is_signed&&src.width>0&&src.width<64&&(v&(std::uint64_t{1}<<(src.width-1)))!=0)v|=~mask_for(src.width);
        return v&mask;
    }
    if(a.size()==1){
        const auto id=n.operands[0];if(id>=nodes.size())return std::nullopt;
        const auto v=a[0]&mask_for(nodes[id].width);
        switch(n.op){
        case RtlOp::UnaryPlus:return v&mask;
        case RtlOp::Negate:return (std::uint64_t{0}-v)&mask;
        case RtlOp::BitNot:return (~v)&mask;
        case RtlOp::LogicalNot:return v==0?1:0;
        case RtlOp::ReduceNand:return v==mask_for(nodes[id].width)?0:1;
        case RtlOp::ReduceNor:return v==0?1:0;
        case RtlOp::ReduceXnor:return (std::popcount(v)%2U)==0U?1:0;
        default:return std::nullopt;
        }
    }
    if(a.size()==2){
        const auto li=n.operands[0],ri=n.operands[1];if(li>=nodes.size()||ri>=nodes.size())return std::nullopt;
        const auto lw=nodes[li].width,rw=nodes[ri].width;
        const auto l=a[0]&mask_for(lw),r=a[1]&mask_for(rw);
        const bool sign=nodes[li].is_signed&&nodes[ri].is_signed;
        switch(n.op){
        case RtlOp::Add:return (l+r)&mask;
        case RtlOp::Subtract:return (l-r)&mask;
        case RtlOp::Multiply:return (l*r)&mask;
        case RtlOp::Divide:
            if(r==0)return std::nullopt;
            if(sign){const auto x=signed_value(l,lw),y=signed_value(r,rw);if(x==std::numeric_limits<std::int64_t>::min()&&y==-1)return std::nullopt;return static_cast<std::uint64_t>(x/y)&mask;}
            return (l/r)&mask;
        case RtlOp::Modulo:
            if(r==0)return std::nullopt;
            if(sign){const auto x=signed_value(l,lw),y=signed_value(r,rw);if(x==std::numeric_limits<std::int64_t>::min()&&y==-1)return std::nullopt;return static_cast<std::uint64_t>(x%y)&mask;}
            return (l%r)&mask;
        case RtlOp::BitAnd:return (l&r)&mask;
        case RtlOp::BitOr:return (l|r)&mask;
        case RtlOp::BitXor:return (l^r)&mask;
        case RtlOp::BitXnor:return (~(l^r))&mask;
        case RtlOp::LogicalAnd:return l!=0&&r!=0?1:0;
        case RtlOp::LogicalOr:return l!=0||r!=0?1:0;
        case RtlOp::Equal:return l==r?1:0;
        case RtlOp::NotEqual:return l!=r?1:0;
        case RtlOp::Less:return sign?(signed_value(l,lw)<signed_value(r,rw)?1:0):(l<r?1:0);
        case RtlOp::LessEqual:return sign?(signed_value(l,lw)<=signed_value(r,rw)?1:0):(l<=r?1:0);
        case RtlOp::Greater:return sign?(signed_value(l,lw)>signed_value(r,rw)?1:0):(l>r?1:0);
        case RtlOp::GreaterEqual:return sign?(signed_value(l,lw)>=signed_value(r,rw)?1:0):(l>=r?1:0);
        case RtlOp::ShiftLeft:return r>=n.width?0:(l<<r)&mask;
        case RtlOp::ShiftRight:return r>=n.width?0:(l>>r)&mask;
        default:return std::nullopt;
        }
    }
    if(n.op==RtlOp::Mux&&a.size()==3)return (a[0]!=0?a[1]:a[2])&mask;
    return std::nullopt;
}
bool commutative(RtlOp op){
    switch(op){
    case RtlOp::Add:case RtlOp::Multiply:case RtlOp::BitAnd:case RtlOp::BitOr:
    case RtlOp::BitXor:case RtlOp::BitXnor:case RtlOp::LogicalAnd:case RtlOp::LogicalOr:
    case RtlOp::Equal:case RtlOp::NotEqual:return true;
    default:return false;
    }
}
std::string key_for(const RtlNode& n){
    std::ostringstream out;out<<static_cast<int>(n.op)<<':'<<n.width<<':'<<n.is_signed<<':';
    if(n.op==RtlOp::Input)out<<n.name;if(n.op==RtlOp::Constant)out<<n.literal;
    for(auto v:n.operands)out<<':'<<v;return out.str();
}
void optimize_module(RtlModule& m,OptimizationStats& stats){
    stats.nodes_before+=m.nodes.size();
    std::vector<std::optional<std::uint64_t>> constants(m.nodes.size());
    for(auto& n:m.nodes){
        const auto v=evaluate(n,m.nodes,constants);if(!v)continue;
        if(n.op!=RtlOp::Constant)++stats.constants_folded;
        n.op=RtlOp::Constant;n.operands.clear();n.name.clear();n.literal=format_constant(n.width,*v);constants[n.id]=*v;
    }
    std::vector<RtlNode> unique;std::vector<ValueId> remap(m.nodes.size(),0);
    std::unordered_map<std::string,ValueId> seen;
    for(const auto& original:m.nodes){
        RtlNode n=original;for(auto& op:n.operands)op=remap[op];
        if(commutative(n.op)&&n.operands.size()==2&&n.operands[0]>n.operands[1])std::swap(n.operands[0],n.operands[1]);
        const auto key=key_for(n);const auto found=seen.find(key);
        if(found!=seen.end()){remap[original.id]=found->second;++stats.common_subexpressions_removed;continue;}
        n.id=static_cast<ValueId>(unique.size());remap[original.id]=n.id;seen.emplace(key,n.id);unique.push_back(std::move(n));
    }
    for(auto& a:m.assignments)a.value=remap[a.value];m.nodes=std::move(unique);
    std::vector<bool> live(m.nodes.size(),false);
    auto mark=[&](auto&& self,ValueId id)->void{
        if(id>=m.nodes.size()||live[id])return;live[id]=true;for(auto op:m.nodes[id].operands)self(self,op);
    };
    for(const auto& a:m.assignments)mark(mark,a.value);
    std::vector<ValueId> compact_map(m.nodes.size(),0);std::vector<RtlNode> compact;
    for(std::size_t i=0;i<m.nodes.size();++i)if(live[i]){
        compact_map[i]=static_cast<ValueId>(compact.size());auto n=m.nodes[i];n.id=compact_map[i];compact.push_back(std::move(n));
    }
    for(auto& n:compact)for(auto& op:n.operands)op=compact_map[op];
    for(auto& a:m.assignments)a.value=compact_map[a.value];
    stats.dead_nodes_removed+=m.nodes.size()-compact.size();m.nodes=std::move(compact);stats.nodes_after+=m.nodes.size();
}
}
OptimizationResult RtlOptimizer::optimize(const RtlDesign& design) const{
    OptimizationResult result;result.design=design;
    for(auto& m:result.design.modules)optimize_module(m,result.stats);
    return result;
}
} // namespace synvanta
