#pragma once
#include <string>
#include <vector>
#include <functional>

class ObjectiveFunction
{
public:
    ObjectiveFunction() = default;

    // Parses expression; variables are x0..x(n-1). Supports: + - * / ^, parentheses,
    // constants: pi, e
    // functions: sin cos tan asin acos atan exp log log10 sqrt abs floor ceil
    //            min max pow
    bool setExpression(const std::string& expr, int dimension, std::string* errorMsg);

    double evaluate(const std::vector<double>& x) const;
    int dimension() const { return dim_; }
    const std::string& expression() const { return expr_; }

private:
    enum class TokType { Number, Var, Op, Func, LParen, RParen, Comma };

    struct Token
    {
        TokType type{};
        double number{0.0};
        int varIndex{-1};
        char op{0};              // + - * / ^
        int precedence{0};
        bool rightAssoc{false};

        // For functions:
        std::string funcName;
        int funcArity{1};
        std::function<double(double)> fn1;
        std::function<double(double,double)> fn2;
    };

    static bool tokenize(const std::string& s, int dim, std::vector<Token>& out, std::string* err);
    static bool shuntingYardToRPN(const std::vector<Token>& in, std::vector<Token>& rpn, std::string* err);
    static bool bindFunctions(std::vector<Token>& rpn, std::string* err);

    double evalRPN(const std::vector<double>& x) const;

private:
    int dim_{0};
    std::string expr_;
    std::vector<Token> rpn_;
};
