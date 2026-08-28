#pragma once
// Abstract Syntax Tree for Coco (grammar/coco.ebnf §3).
// Ownership: unique_ptr everywhere (plan §11.2); arenas arrive with MIR.
#include <memory>
#include <string>
#include <vector>

namespace coco {
namespace ast {

struct Expr;
struct Stmt;
struct Pat;
struct Type;
using ExprP  = std::unique_ptr<Expr>;
using StmtP  = std::unique_ptr<Stmt>;
using PatP   = std::unique_ptr<Pat>;
using TypeP  = std::unique_ptr<Type>;

struct Span {
    uint32_t line = 0, col = 0;
};

// ---- Types ----------------------------------------------------------------

enum class TyKind { Name, Pointer, Ref, Optional, Fn, Tuple };

struct Type {
    TyKind kind{};
    Span span;

    std::string name;                 // Name (+ generic args)
    std::vector<TypeP> generics;
    TypeP inner;                      // Pointer/Ref/Optional
    bool refMut = false;              // Ref: &mut
    std::vector<TypeP> params;        // Fn params / Tuple elems
    TypeP ret;                        // Fn return (may be null)

    static TypeP makeName(std::string n, Span s, std::vector<TypeP> gens = {});
    static TypeP makePointer(TypeP t, Span s);
    static TypeP makeRef(TypeP t, bool mut, Span s);
    static TypeP makeOptional(TypeP t, Span s);
    static TypeP makeFn(std::vector<TypeP> ps, TypeP r, Span s);
    static TypeP makeTuple(std::vector<TypeP> ts, Span s);
};

// ---- Patterns ---------------------------------------------------------------

enum class PatKind {
    Wild, Literal, Range, Tuple, Ctor, Bind, BindAlias, Or, Slice, Rest,
};

struct PatField {                     // Ctor field: named `x: pat` or positional
    std::string name;                 // empty => positional
    PatP pat;
};

struct Pat {
    PatKind kind{};
    Span span;

    ExprP literal;                    // Literal
    PatP lo, hi;                      // Range (hi inclusive per '..=')
    bool inclusive = false;
    std::vector<PatP> elems;          // Tuple
    std::string ctorName;             // Ctor
    std::vector<PatField> fields;     // Ctor
    std::string bindName;             // Bind
    TypeP bindType;                   // Bind: `x is T`
    PatP aliasSub;                    // BindAlias: `pat @ name` (Rust `x @ pat`)
    std::vector<PatP> alts;           // Or: `p1 | p2 | ...`
    std::string restName;             // Slice: `..rest` tail capture (""=none)
};

// ---- Expressions -------------------------------------------------------------

enum class ExKind {
    Int, Float, CharLit, Str, FString,
    Ident, Unary, Binary, Call, Index, Slice, Member, Try,
    Lambda, Cond, ListComp, Generator,
    List, Dict, Set, Tuple, New, Cast,
};

enum class StrFlavor { Normal, Raw, Byte, C };

struct CallArg {
    std::string name;                 // empty for positional
    ExprP value;
};

struct CompClause {                   // `for pat in iter` | `if cond`
    bool isFor = false;
    PatP pat;                         // for-clause
    ExprP iter;                       // for-clause
    ExprP cond;                       // if-clause
};

struct FStrPart {
    bool isExpr = false;
    std::string text;                 // text part / format spec carrier
    ExprP expr;
    std::string spec;                 // format spec after ':' inside {}
};

struct Expr {
    ExKind kind{};
    Span span;

    std::string text;                 // literals / ident name / member name
    StrFlavor flavor = StrFlavor::Normal;

    std::vector<FStrPart> parts;      // FString

    ExprP lhs, rhs;                   // Unary(rhs only) / Binary / Member(obj) /
                                      // Call(callee=lhs) / Index / Try / Cond
    std::string op;                   // Unary/Binary op spelling (".." "..=" too)
    bool nilSafe = false;             // Member via .?.

    std::vector<CallArg> args;        // Call / New
    std::vector<std::string> lambdaParams;
    ExprP cond;                       // Cond / ListComp element guard base
    std::vector<ExprP> elems;         // List/Set/Tuple
    std::vector<std::pair<ExprP, ExprP>> pairs;   // Dict
    PatP pat;                         // Comprehension clause binding
    std::vector<CompClause> clauses;  // ListComp/Generator
    TypeP newType;                    // New
};

// ---- Statements & declarations -------------------------------------------------

enum class StKind {
    FuncDef, StructDef, EnumDef, TraitDef, ImplDef,
    ConstDecl, VarDecl, Pass,
    ExprStmt, Assign, AugAssign,
    Return, Raise, Break, Continue, Defer,
    If, While, For, Match, Select, Unsafe, Spawn,
    Import, Export,
};

struct Param {
    std::string name;
    bool mutable_ = false;
    bool variadic = false;
    bool selfParam = false;
    TypeP type;                       // may be null (inferred)
    ExprP defaultValue;               // may be null
    Span span;
};

struct FieldDecl {
    bool weak = false;
    bool mutable_ = false;
    bool pub = false;
    std::string name;
    TypeP type;
    ExprP defaultValue;
    Span span;
};

struct VariantPayload { std::string name; TypeP type; };

struct Variant {
    std::string name;
    std::vector<VariantPayload> payload;   // unit variants: empty
    Span span;
};

struct TraitMethodSig {               // signature-only trait method
    std::string name;
    std::vector<Param> params;
    TypeP ret;
    Span span;
};

struct Stmt {
    StKind kind{};
    Span span;

    // FuncDef / TraitDef
    std::string name;
    std::vector<std::pair<std::string, TypeP>> typeParams;   // [T is Bound]
    std::vector<Param> params;
    TypeP ret;
    std::vector<StmtP> body;
    bool pub = false;
    bool externDef = false;           // `extern def f(...) -> T` (no body)

    // Import
    std::string moduleName;           // dotted as written
    struct ImportItem { std::string name, alias; };
    std::vector<ImportItem> importItems;
    bool starImport = false;
    std::string importAlias;          // `import a.b as c`
    bool fromImport = false;

    // StructDef
    std::vector<FieldDecl> fields;

    // EnumDef
    std::vector<Variant> variants;

    // TraitDef
    std::vector<TraitMethodSig> sigs;

    // ImplDef
    TypeP implTrait, implType;

    // ConstDecl / VarDecl
    ExprP target;                     // var target / const name-as-Ident
    TypeP declType;
    ExprP value;
    bool varKw = false;               // VarDecl written with `var` keyword (mutable)

    // ExprStmt / Return/Raise/Defer/Spawn payloads
    std::vector<ExprP> exprs;         // Assign targets+values flattened:
                                      // [t1..tn, v1..vm]
    std::string augOp;

    // Labeled control flow
    std::string label;                // loop labels + break/continue targets

    // If / While / For / Match / Select
    std::vector<StmtP> elseBody;      // If else-chain tail (flat elifs below)
    std::vector<StmtP> elifBodies;    // parallel to elifConds
    std::vector<ExprP> elifConds;
    PatP pat;                         // For binding pattern
    struct Arm { PatP pat; ExprP guard; std::vector<StmtP> body; };
    std::vector<Arm> arms;            // Match
    struct SelArm { std::string bind; ExprP chanOp; std::vector<StmtP> body; };
    std::vector<SelArm> selArms;      // Select
};

// pretty-printer (used by cocoparse --ast)
void dump(const Stmt& s, int indent = 0);

} // namespace ast
} // namespace coco
