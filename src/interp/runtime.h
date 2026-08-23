#pragma once
// Tree-walking interpreter over the typed AST (plan §10.4, roadmap Phase 1).
// Correctness vehicle for the corpus; the MIR/LLVM backend supersedes it later.
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "interp/value.h"

namespace coco {

namespace interp {

struct ChanImpl {
    std::mutex m;
    std::condition_variable cv;
    struct Slot { bool got = false; bool closed = false; Value v; };
    void send(Value v);
    Slot recv();
    Slot tryRecv();
    void close();
    std::deque<Value> q;
    size_t cap = 0;
    bool closed = false;
};

struct ThreadImpl {
    std::thread th;
    bool joined = false;
};

// Non-local control flow.
struct SignalReturn { Value v; };
struct SignalBreak {};
struct SignalContinue {};
struct PanicSignal { std::string msg; };
struct SignalRaise { Value errResult; };

[[noreturn]] void panicHere(const std::string& msg);

struct Deferred {
    Value callee;                 // resolved at registration time
    std::vector<Value> args;
};

class Interpreter {
public:
    explicit Interpreter(const ast::Stmt& program);
    ~Interpreter();
    Value run();                  // executes module top level, then main()

private:
    using Expr = ast::Expr;
    using Stmt = ast::Stmt;

    // ---- program tables (built once, read-only after setup) -------------------
    std::unordered_map<std::string, const Stmt*> funcs_;
    std::unordered_map<std::string, const Stmt*> structs_;
    std::unordered_map<std::string, const Stmt*> enums_;
    std::unordered_map<std::string, const Stmt*> traits_;
    struct ImplEntry {
        std::string traitName, typeName;
        std::unordered_map<std::string, const Stmt*> methods;
    };
    std::vector<ImplEntry> impls_;

    Env globals_;
    // defer frames are per-thread: spawned threads run runFunc concurrently
    std::mutex defersM_;
    std::map<std::thread::id, std::vector<std::vector<Deferred>>> defers_;

    std::mutex threadsM_;
    std::vector<std::shared_ptr<ThreadImpl>> threads_;

    // ---- setup -----------------------------------------------------------------
    void collectProgram(const Stmt& program);
    const Stmt* program_ = nullptr;
    bool collected_ = false;

    // ---- statements / expressions ----------------------------------------------
    void execBlock(const std::vector<ast::StmtP>& body, Env env);
    void exec(const Stmt& s, Env env);
    Value eval(const Expr& e, Env env);

    // calls & members
    using NamedArgs = std::vector<std::pair<std::string, Value>>;
    Value callValue(Value callee, std::vector<Value> args, int line, int col);
    Value runFunc(const Stmt* fn, std::vector<Value> pos, NamedArgs named,
                  Env closure, Value* selfOut = nullptr);
    Value runLambda(const Expr* lam, std::vector<Value> args, Env closure);
    Value invokeMethod(Value obj, const std::string& name,
                       std::vector<Value> pos, NamedArgs named, Env env,
                       int line, int col, Value* selfOut);
    Value memberRead(const Value& obj, const std::string& name, int line, int col);
    void memberWrite(Value& obj, const std::string& name, Value v, int line, int col);
    Value* lvaluePtr(const Expr& e, Env env);
    void assignTo(const Expr& target, Value v, Env env);
    Deferred prepareDeferred(const Expr& callExpr, Env env);

    // operators
    Value binop(const std::string& op, const Value& l, const Value& r, int line, int col);
    Value compareOne(const std::string& op, const Value& l, const Value& r);
    bool evalCmpChain(const Expr& b, Env env);
    bool valuesEqual(const Value& a, const Value& b);

    // constructors / iteration / patterns
    Value makeStruct(const std::string& name,
                     const std::vector<ast::CallArg>& args, Env env);
    Value makeHeapValue(const std::string& name,
                        const std::vector<ast::CallArg>& args, Env env);
    Value makeEnumV(const std::string& enumName, const std::string& variant,
                    const std::vector<ast::CallArg>& args, Env env,
                    int line, int col);
    void iterateSeq(const Value& seq,
                    const std::function<bool(Value&, Env&)>& body, Env env);
    bool matchPat(const ast::Pat& p, const Value& v, Env env);
    void bindPat(const ast::Pat& p, const Value& v, Env env);   // assumes matched
    static bool isCmpOp(const std::string& op);
    // builtins / modules / formatting
    void installBuiltins();
    Value resolveModulePath(const std::string& dotted);
    Value moduleMember(const std::string& mod, const std::string& name);
    // real stdlib: load <dir>/<dotted/path>.co as an isolated module namespace
    std::vector<std::string> stdlibDirs_;
    std::map<std::string, Env> loadedModules_;
    std::map<std::string, std::vector<ast::StmtP>> moduleAstCache_;
    Value loadModuleFile(const std::string& dotted);
    Value formatFString(const Expr& e, Env env);
    std::string applySpec(const std::string& text, const std::string& spec);
    Value sortList(Value listVal, const std::vector<ast::CallArg>& args, Env env);

    // concurrency
    Value spawnCall(const Expr& callExpr, Env env);
    Value selectExec(const Stmt& s, Env env);
    void shutdownThreads();

public:
    // entry used by spawned threads
    void threadEntry(Value callee, std::vector<Value> args);
    // directories searched for stdlib modules (dotted imports -> files)
    void addStdlibDir(const std::string& dir) { stdlibDirs_.push_back(dir); }
};

} // namespace interp
} // namespace coco
