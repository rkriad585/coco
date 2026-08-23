#pragma once
// Semantic analysis for Coco v1: symbol collection, name resolution,
// type inference/checking over the parsed AST. Pragmatic ruleset aligned
// with docs/COCO_PLAN.md §5 and the frozen example corpus.
#include "ast/ast.h"
#include "sema/symbols.h"
#include "support/diag.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace coco {
namespace sema {

class Checker {
public:
    explicit Checker(DiagEngine& diags) : diags_(diags) {}

    // Runs collection (2 passes) + body checking. Reports into diags_.
    void checkModule(const std::vector<ast::StmtP>& prog);

private:
    // ---- infrastructure ----
    void error(uint32_t line, uint32_t col, const std::string& msg);
    Scope& push();
    void pop();

    // ---- declaration collection ----
    void predeclareBuiltins();
    SymP makeBuiltinFunc(std::string name, std::vector<TyP> params,
                         std::vector<std::string> paramNames, TyP ret,
                         bool variadic, size_t required);
    SymP declareConst(std::string name, TyP t);
    void registerNominals(const std::vector<ast::StmtP>& prog);
    void fillNominals(const std::vector<ast::StmtP>& prog);
    void registerTopLevel(const std::vector<ast::StmtP>& prog);
    void fillStructBody(SymP st, const ast::Stmt& s);

    // ---- types ----
    TyP resolveType(const ast::TypeP& t);
    FuncSig resolveSig(const std::vector<ast::Param>& params, const ast::TypeP& ret,
                       bool stripSelf);
    TyP lookupNominal(TyK k, const std::string& name) const;
    bool assignable(const TyP& from, const TyP& to) const;
    TyP unify(const TyP& a, const TyP& b, uint32_t line, uint32_t col,
              const char* ctx);
    TyP unwrapOpt(const TyP& t) const;          // Opt[T] -> T ; passthrough else
    TyP unwrapResult(const TyP& t) const;       // result[T,E] -> T

    // ---- statements ----
    void checkStmt(const ast::Stmt& s);
    void checkBlock(const std::vector<ast::StmtP>& body);
    void checkFuncLike(const ast::Stmt& s, TyP selfTy, SymP funcSym);
    void checkAssignTargets(const ast::Stmt& s);
    void checkPatternBindings(const ast::Pat& p, const TyP& subject);
    void requireBool(const TyP& t, uint32_t line, uint32_t col, const char* what);
    void requireIntish(const TyP& t, const char* what, uint32_t line,
                       uint32_t col);
    void compClause(const ast::CompClause& cl, uint32_t line);
    void checkReassignable(const ast::Expr& tgt);
    void assignTarget(const ast::Expr& tgt, const TyP& vt, bool multi = false);

    // ---- expressions ----
    TyP checkExpr(const ast::Expr& e);
    TyP checkCall(const ast::Expr& e);
    TyP checkMemberCall(const TyP& recv, const std::string& name,
                        const std::vector<ast::CallArg>& args, uint32_t line,
                        uint32_t col);
    bool matchArgs(const FuncSig& sig, const std::vector<ast::CallArg>& args,
                   uint32_t line, uint32_t col, const char* what);
    TyP checkEnumCtorCall(const Symbol* vs,
                          const std::vector<ast::CallArg>& args);
    TyP memberAccess(const TyP& obj, const std::string& name, uint32_t line,
                     uint32_t col, bool nilSafe);

    // ---- helpers ----
    SymP declareLocal(SymK kind, const std::string& name, TyP t, bool mut,
                      uint32_t line, uint32_t col);
    TyP iterableElem(const TyP& t) const;       // nullopt-equivalent: unkTy()
    std::optional<FuncSig> methodLookup(const TyP& recv, const std::string& name) const;
    bool isSendable(const TyP& t, std::string* why);
    bool isSendableVisit(const TyP& t, std::string* why,
                         std::set<std::string>& seeing);

    DiagEngine& diags_;
    Scope* scope_ = nullptr;
    std::map<std::string, SymP> structs_;
    std::map<std::string, SymP> enums_;
    std::map<std::string, SymP> traits_;
    std::set<std::pair<std::string, std::string>> impls_;   // trait -> struct
    std::map<std::string, SymP> funcs_;                     // top-level funcs
    std::map<std::string, TyP> typeVars_;                   // active generics
    std::set<std::string> importRoots_;
    std::set<std::string> builtins_;                        // print/len/sqrt
    TyP selfTy_;
    TyP currentRet_;
    int loopDepth_ = 0;
    int quiet_ = 0;   // >0: error() is suppressed (speculative re-walks)
};

} // namespace sema
} // namespace coco
