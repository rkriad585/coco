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
#include "vm/compiler.h"

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
struct SignalBreak { std::string label; };    // empty = unlabeled
struct SignalContinue { std::string label; };
struct PanicSignal {
    std::string msg;
    std::vector<std::string> frames;    // source-level call stack (outermost first)
};
struct SignalRaise { Value errResult; };

extern thread_local std::vector<std::string> g_panicFrames;

// RAII guard recording one source-level call frame for panic backtraces.
// Pushed at user-function/lambda entry; popped on any exit (incl. exceptions).
class CallFrameGuard {
public:
    CallFrameGuard(std::string name, uint32_t line, uint32_t col);
    ~CallFrameGuard();
    CallFrameGuard(const CallFrameGuard&) = delete;
    CallFrameGuard& operator=(const CallFrameGuard&) = delete;
};

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
    std::map<std::string, std::string> embeddedSources_;  // baked-in modules
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
    // ---- PLAN Phase 4: bytecode VM (vm/compiler.{h,cpp}, runtime.cpp) ------
    // Enable the bytecode-VM accelerator. After this, any user function whose
    // body is inside the VM core slice runs on the VM instead of the tree-walker;
    // all other functions keep their exact tree-walker behaviour.
    void enableVm();
    std::unique_ptr<vm::CompileResult> vmProg_;
    bool vmEnabled_ = false;
    const vm::VmFunction* vmFuncFor(const Stmt* fn) const;
    Value vmRunBody(const vm::VmFunction& vf, Env fenv);
    Value makeEnumVPos(const std::string& enumName, const std::string& variant,
                       std::vector<Value> pos, Env env);

    // ---- PLAN Phase 8.2: native codegen (src/backend/native.cpp) ----------
    // A "native" function is one lowered by the backend to real C++ for the
    // scalar subset (int/float/bool/char params & locals, arithmetic/comparison/
    // logical ops, if/while/for-range, returns, calls to other native fns). It
    // reads its scalar params from fenv and returns a scalar Value, bypassing
    // the tree-walker & VM bodies for that function. Preference in runFunc is:
    // native -> VM -> tree-walker. Only functions proven statically lowerable by
    // the backend are ever registered, so the typed read is sound.
    using NativeBody = std::function<Value(Env)>;
    void enableNative();
    void registerNative(const Stmt* fn, NativeBody body);
    const NativeBody* nativeFuncFor(const Stmt* fn) const;
    Value nativeRunBody(const NativeBody& nb, Env fenv, const Stmt* fn);
    std::unordered_map<const Stmt*, NativeBody> nativeFns_;
    bool nativeEnabled_ = false;

    // Reads a variable bound in `e` (walking parent scopes). Used by the code
    // generated by src/backend/native.cpp to pull scalar params out of the
    // caller's Env without exposing the EnvS layout.
    static Value nativeEnvVar(const Env& e, const std::string& name);

    // entry used by spawned threads
    void threadEntry(Value callee, std::vector<Value> args);
    // directories searched for stdlib modules (dotted imports -> files)
    void addStdlibDir(const std::string& dir) { stdlibDirs_.push_back(dir); }
    // sources embedded at build time; win over every disk search path
    void addEmbeddedSource(const std::string& name, const std::string& src) {
        std::string key;
        for (char c : name)
            key += (c == '.' || c == '/' || c == '\\') ? '/' : c;
        embeddedSources_[key] = src;
    }
};

} // namespace interp
} // namespace coco
