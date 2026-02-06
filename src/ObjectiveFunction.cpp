#include "ObjectiveFunction.h"
#include <cctype>
#include <cmath>
#include <stack>
#include <unordered_map>
#include <sstream>
#include <limits>

static bool isIdentChar(char c)
{
    return std::isalpha(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)) || c=='_';
}

bool ObjectiveFunction::setExpression(const std::string& expr, int dimension, std::string* errorMsg)
{
    expr_ = expr;
    dim_ = dimension;
    rpn_.clear();

    if (dim_ <= 0) {
        if (errorMsg) *errorMsg = "Dimension must be >= 1.";
        return false;
    }

    std::vector<Token> toks;
    if (!tokenize(expr_, dim_, toks, errorMsg)) return false;

    std::vector<Token> rpn;
    if (!shuntingYardToRPN(toks, rpn, errorMsg)) return false;

    if (!bindFunctions(rpn, errorMsg)) return false;

    rpn_ = std::move(rpn);
    return true;
}

double ObjectiveFunction::evaluate(const std::vector<double>& x) const
{
    if (static_cast<int>(x.size()) != dim_) return std::numeric_limits<double>::quiet_NaN();
    return evalRPN(x);
}

bool ObjectiveFunction::tokenize(const std::string& s, int dim, std::vector<Token>& out, std::string* err)
{
    out.clear();
    auto setErr=[&](const std::string& m){ if(err) *err=m; };

    const auto makeOp=[&](char op, bool unaryMinus=false)->Token{
        Token t;
        t.type = TokType::Op;
        t.op = op;
        if (unaryMinus) {
            // Represent unary minus as function-like operator: 0 - x
            // We'll handle by injecting 0 and '-' with normal precedence.
        }
        switch(op){
            case '+': case '-': t.precedence=1; t.rightAssoc=false; break;
            case '*': case '/': t.precedence=2; t.rightAssoc=false; break;
            case '^': t.precedence=3; t.rightAssoc=true; break;
            default: t.precedence=0; t.rightAssoc=false; break;
        }
        return t;
    };

    size_t i=0;
    bool expectValue=true; // for unary minus
    while(i<s.size()){
        char c = s[i];
        if (std::isspace(static_cast<unsigned char>(c))) { i++; continue; }

        if (c=='('){ out.push_back(Token{TokType::LParen}); i++; expectValue=true; continue; }
        if (c==')'){ out.push_back(Token{TokType::RParen}); i++; expectValue=false; continue; }
        if (c==','){ out.push_back(Token{TokType::Comma}); i++; expectValue=true; continue; }

        // Number (supports decimals and scientific)
        if (std::isdigit(static_cast<unsigned char>(c)) || c=='.'){
            size_t j=i;
            bool seenDot = (c=='.');
            while(j<s.size()){
                char cj=s[j];
                if (std::isdigit(static_cast<unsigned char>(cj))) { j++; continue; }
                if (cj=='.' && !seenDot){ seenDot=true; j++; continue; }
                if ((cj=='e' || cj=='E')) {
                    // exponent
                    j++;
                    if (j<s.size() && (s[j]=='+'||s[j]=='-')) j++;
                    while(j<s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) j++;
                    break;
                }
                break;
            }
            double val=0.0;
            try{
                val = std::stod(s.substr(i,j-i));
            }catch(...){
                setErr("Invalid number token.");
                return false;
            }
            Token t;
            t.type=TokType::Number;
            t.number=val;
            out.push_back(t);
            i=j;
            expectValue=false;
            continue;
        }

        // Operator
        if (c=='+' || c=='-' || c=='*' || c=='/' || c=='^'){
            if (c=='-' && expectValue){
                // unary minus: inject 0 then '-'
                Token z; z.type=TokType::Number; z.number=0.0;
                out.push_back(z);
                out.push_back(makeOp('-'));
                i++;
                expectValue=true;
                continue;
            }
            out.push_back(makeOp(c));
            i++;
            expectValue=true;
            continue;
        }

        // Identifier: variable, const, func
        if (std::isalpha(static_cast<unsigned char>(c)) || c=='_'){
            size_t j=i;
            while(j<s.size() && isIdentChar(s[j])) j++;
            std::string id = s.substr(i,j-i);

            // constants
            if (id=="pi" || id=="PI"){
                Token t; t.type=TokType::Number; t.number = 3.1415926535897932384626433832795;
                out.push_back(t);
                i=j; expectValue=false; continue;
            }
            if (id=="e"){
                Token t; t.type=TokType::Number; t.number = 2.7182818284590452353602874713527;
                out.push_back(t);
                i=j; expectValue=false; continue;
            }

            // variable x0..xN
            if (id.size()>=2 && (id[0]=='x' || id[0]=='X')){
                bool ok=true;
                int idx=0;
                for(size_t k=1;k<id.size();k++){
                    if(!std::isdigit(static_cast<unsigned char>(id[k]))){ ok=false; break; }
                    idx = idx*10 + (id[k]-'0');
                }
                if(ok){
                    if(idx<0 || idx>=dim){
                        std::ostringstream oss; oss<<"Variable "<<id<<" out of range for dimension "<<dim<<".";
                        setErr(oss.str());
                        return false;
                    }
                    Token t; t.type=TokType::Var; t.varIndex=idx;
                    out.push_back(t);
                    i=j; expectValue=false; continue;
                }
            }

            // function
            Token t; t.type=TokType::Func; t.funcName=id;
            out.push_back(t);
            i=j; expectValue=true;
            continue;
        }

        std::ostringstream oss; oss<<"Unexpected character '"<<c<<"'.";
        setErr(oss.str());
        return false;
    }

    return true;
}

bool ObjectiveFunction::shuntingYardToRPN(const std::vector<Token>& in, std::vector<Token>& rpn, std::string* err)
{
    rpn.clear();
    auto setErr=[&](const std::string& m){ if(err) *err=m; };

    std::vector<Token> stack;

    for(size_t i=0;i<in.size();i++){
        const Token& t = in[i];
        if(t.type==TokType::Number || t.type==TokType::Var){
            rpn.push_back(t);
        } else if(t.type==TokType::Func){
            stack.push_back(t);
        } else if(t.type==TokType::Comma){
            bool foundLParen=false;
            while(!stack.empty()){
                if(stack.back().type==TokType::LParen){ foundLParen=true; break; }
                rpn.push_back(stack.back()); stack.pop_back();
            }
            if(!foundLParen){
                setErr("Misplaced comma or missing parentheses in function arguments.");
                return false;
            }
        } else if(t.type==TokType::Op){
            while(!stack.empty()){
                const Token& top=stack.back();
                if(top.type!=TokType::Op) break;
                if( ( !t.rightAssoc && t.precedence<=top.precedence ) ||
                    (  t.rightAssoc && t.precedence< top.precedence ) ){
                    rpn.push_back(top);
                    stack.pop_back();
                    continue;
                }
                break;
            }
            stack.push_back(t);
        } else if(t.type==TokType::LParen){
            stack.push_back(t);
        } else if(t.type==TokType::RParen){
            bool found=false;
            while(!stack.empty()){
                if(stack.back().type==TokType::LParen){ found=true; stack.pop_back(); break; }
                rpn.push_back(stack.back()); stack.pop_back();
            }
            if(!found){
                setErr("Mismatched parentheses.");
                return false;
            }
            // If function on top, pop it too.
            if(!stack.empty() && stack.back().type==TokType::Func){
                rpn.push_back(stack.back());
                stack.pop_back();
            }
        }
    }

    while(!stack.empty()){
        if(stack.back().type==TokType::LParen || stack.back().type==TokType::RParen){
            if(err) *err="Mismatched parentheses at end.";
            return false;
        }
        rpn.push_back(stack.back());
        stack.pop_back();
    }
    return true;
}

bool ObjectiveFunction::bindFunctions(std::vector<Token>& rpn, std::string* err)
{
    auto setErr=[&](const std::string& m){ if(err) *err=m; };

    // Map function name -> arity + function pointers
    struct FnDef {
        int arity;
        std::function<double(double)> fn1;
        std::function<double(double,double)> fn2;
    };

    const std::unordered_map<std::string, FnDef> fns = {
        {"sin",   {1, [](double a){ return std::sin(a); }, {}}},
        {"cos",   {1, [](double a){ return std::cos(a); }, {}}},
        {"tan",   {1, [](double a){ return std::tan(a); }, {}}},
        {"asin",  {1, [](double a){ return std::asin(a); }, {}}},
        {"acos",  {1, [](double a){ return std::acos(a); }, {}}},
        {"atan",  {1, [](double a){ return std::atan(a); }, {}}},
        {"exp",   {1, [](double a){ return std::exp(a); }, {}}},
        {"log",   {1, [](double a){ return std::log(a); }, {}}},
        {"log10", {1, [](double a){ return std::log10(a); }, {}}},
        {"sqrt",  {1, [](double a){ return std::sqrt(a); }, {}}},
        {"abs",   {1, [](double a){ return std::fabs(a); }, {}}},
        {"floor", {1, [](double a){ return std::floor(a); }, {}}},
        {"ceil",  {1, [](double a){ return std::ceil(a); }, {}}},
        {"min",   {2, {}, [](double a,double b){ return (a<b)?a:b; }}},
        {"max",   {2, {}, [](double a,double b){ return (a>b)?a:b; }}},
        {"pow",   {2, {}, [](double a,double b){ return std::pow(a,b); }}},
    };

    for(auto& t : rpn){
        if(t.type!=TokType::Func) continue;
        auto it=fns.find(t.funcName);
        if(it==fns.end()){
            std::ostringstream oss; oss<<"Unknown function '"<<t.funcName<<"'.";
            setErr(oss.str());
            return false;
        }
        t.funcArity = it->second.arity;
        t.fn1 = it->second.fn1;
        t.fn2 = it->second.fn2;
    }
    return true;
}

double ObjectiveFunction::evalRPN(const std::vector<double>& x) const
{
    std::vector<double> st;
    st.reserve(rpn_.size());

    auto pop1=[&]()->double{
        double a = st.back(); st.pop_back(); return a;
    };
    auto pop2=[&]()->std::pair<double,double>{
        double b = st.back(); st.pop_back();
        double a = st.back(); st.pop_back();
        return {a,b};
    };

    for(const auto& t : rpn_){
        if(t.type==TokType::Number){
            st.push_back(t.number);
        } else if(t.type==TokType::Var){
            st.push_back(x[static_cast<size_t>(t.varIndex)]);
        } else if(t.type==TokType::Op){
            if(st.size()<2) return std::numeric_limits<double>::quiet_NaN();
            auto [a,b]=pop2();
            switch(t.op){
                case '+': st.push_back(a+b); break;
                case '-': st.push_back(a-b); break;
                case '*': st.push_back(a*b); break;
                case '/': st.push_back(a/b); break;
                case '^': st.push_back(std::pow(a,b)); break;
                default: return std::numeric_limits<double>::quiet_NaN();
            }
        } else if(t.type==TokType::Func){
            if(t.funcArity==1){
                if(st.empty()) return std::numeric_limits<double>::quiet_NaN();
                double a=pop1();
                st.push_back(t.fn1 ? t.fn1(a) : std::numeric_limits<double>::quiet_NaN());
            } else if(t.funcArity==2){
                if(st.size()<2) return std::numeric_limits<double>::quiet_NaN();
                auto [a,b]=pop2();
                st.push_back(t.fn2 ? t.fn2(a,b) : std::numeric_limits<double>::quiet_NaN());
            } else {
                return std::numeric_limits<double>::quiet_NaN();
            }
        } else {
            return std::numeric_limits<double>::quiet_NaN();
        }
    }
    if(st.size()!=1) return std::numeric_limits<double>::quiet_NaN();
    return st.back();
}
