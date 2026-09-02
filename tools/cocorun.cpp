// cocorun: lex + parse + semantic-check + interpret a Coco source file,
// or execute a self-contained bytecode bundle (.cob) produced by `coco build`
// cross-target fallbacks.
//   cocorun <file.co | file.cob> [args...]
//                                 run program; main()'s Int return is the
//                                 process exit code (default 0)
#include "interp/runtime.h"
#include "lex/lexer.h"
#include "parser/parser.h"
#include "sema/checker.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool readFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "cocorun: cannot open '" << path << "'\n";
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

// Module search paths, in priority order:
//   1. $COCO_LIBS
//   2. <script dir>/coco_libs/libs  (per-project packages)
//   3. <script dir>/coco_libs       (legacy layout)
//   4. ~/.coco/coco-pkg/{libs,/}    (global installs; libs/ preferred)
//   5. <script dir>/../stdlib       (repo checkout layout)
//   6. ./stdlib
//   7. $COCO_STDLIB
void addModuleDirs(coco::interp::Interpreter& interp,
                   const std::string& script) {
    size_t p = script.find_last_of("/\\");
    std::string dir = p == std::string::npos ? "." : script.substr(0, p);
    if (const char* env = std::getenv("COCO_LIBS"))
        interp.addStdlibDir(env);
    interp.addStdlibDir(dir + "/coco_libs/libs");
    interp.addStdlibDir(dir + "/coco_libs");
    if (const char* home = std::getenv("USERPROFILE")) {
        std::string g = std::string(home) + "/.coco/coco-pkg";
        interp.addStdlibDir(g + "/libs");
        interp.addStdlibDir(g);
    } else if (const char* home2 = std::getenv("HOME")) {
        std::string g = std::string(home2) + "/.coco/coco-pkg";
        interp.addStdlibDir(g + "/libs");
        interp.addStdlibDir(g);
    }
    interp.addStdlibDir(dir + "/../stdlib");
    interp.addStdlibDir("stdlib");
    if (const char* env = std::getenv("COCO_STDLIB")) interp.addStdlibDir(env);
}

} // namespace

// .cob container reader (see tools/coco.cpp emitCob for the writer)
static bool unpackCob(const std::string& path, std::string& mainSrc,
                      std::map<std::string, std::string>& embedded) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string b = ss.str();
    auto u32at = [&](size_t off, uint32_t& v) {
        if (off + 4 > b.size()) return false;
        v = (uint8_t)b[off] | ((uint16_t)(uint8_t)b[off + 1] << 8) |
            ((uint32_t)(uint8_t)b[off + 2] << 16) |
            ((uint32_t)(uint8_t)b[off + 3] << 24);
        return true;
    };
    if (b.size() < 10 || b.compare(0, 5, "COCOB") != 0 || b[5] != 1)
        return false;
    uint32_t count = 0;
    if (!u32at(6, count)) return false;
    size_t off = 10;
    bool haveMain = false;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t nl = 0, sl = 0;
        if (!u32at(off, nl)) return false;
        off += 4;
        if (off + nl > b.size()) return false;
        std::string name = b.substr(off, nl);
        off += nl;
        if (!u32at(off, sl)) return false;
        off += 4;
        if (off + sl > b.size()) return false;
        std::string esrc = b.substr(off, sl);
        off += sl;
        if (name == "main") {
            mainSrc = std::move(esrc);
            haveMain = true;
        } else {
            embedded[std::move(name)] = std::move(esrc);
        }
    }
    return haveMain;
}

// shared pipeline: front-end + interpret, with panic handling
// The bytecode VM is now the DEFAULT runner: it is verified-correct against the
// tree-walker (32/32 differential, 33/33 corpus, ASan-clean) and substantially
// faster (see scripts/bench.ps1). Use --no-vm to force the tree-walker.
static bool g_useVm = true;
static int runSources(const std::string& label, const std::string& src,
                      const std::map<std::string, std::string>* embedded,
                      const std::vector<std::string>& progArgs) {
    coco::DiagEngine diags;
    auto toks = coco::Lexer(src, label, diags).lexAll();
    int ret = 0;
    if (diags.errorCount() == 0) {
        auto body = coco::Parser(toks, diags).parseProgram();
        if (diags.errorCount() == 0) {
            coco::sema::Checker checker(diags);
            checker.checkModule(body);
            if (diags.errorCount() == 0) {
                coco::ast::Stmt module;
                module.kind = coco::ast::StKind::Pass;
                module.body = std::move(body);
                try {
                    coco::interp::Interpreter interp(module);
                    addModuleDirs(interp, label);
                    if (embedded)
                        for (const auto& [name, esrc] : *embedded)
                            interp.addEmbeddedSource(name, esrc);
                    interp.setProgramArgs(progArgs);
                    if (g_useVm) interp.enableVm();   // PLAN Phase 4 bytecode VM
                    coco::interp::Value r = interp.run();
                    ret = r.k == coco::interp::VK::Int ? (int)r.i : 0;
                } catch (const coco::interp::PanicSignal& p) {
                    fflush(stdout);
                    fputs(("panic: " + p.msg + "\n").c_str(), stderr);
                    for (const auto& f : p.frames)
                        fputs(("  " + f + "\n").c_str(), stderr);
                    ret = 70;
                } catch (const coco::interp::SignalRaise&) {
                    fflush(stdout);
                    fputs("panic: uncaught raise escaped main\n", stderr);
                    ret = 70;
                }
            }
        }
    }
    if (diags.errorCount() != 0) {
        for (const auto& d : diags.diags())
            if (d.sev == coco::Sev::Error || d.sev == coco::Sev::InternalError)
                std::cout << label << ":" << d.line << ":" << d.col
                          << ": error: " << d.message << "\n";
        std::cout << label << ": " << diags.errorCount() << " error(s)\n";
        return 1;
    }
    if (diags.warningCount()) {
        coco::SourceMap sm(src);
        std::string out;
        coco::renderDiags(label, sm, diags.diags(), /*color*/ false,
                          /*plain*/ false, out);
        std::cout << out;
    }
    return ret;
}

int main(int argc, char** argv) {
    std::string file;
    bool afterFile = false;
    std::vector<std::string> progArgs;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (afterFile) {
            progArgs.push_back(a);
            continue;
        }
        if (a == "-h" || a == "--help") {
            std::cout << "usage: cocorun [--no-vm|--vm] <file.co | file.cob> [args...]\n"
                         "  (the bytecode-VM accelerator is the default; --no-vm\n"
                         "   forces the tree-walker interpreter, --vm re-enables)\n"
                         "  arguments after <file> are passed to the program as os.args()\n";
            return 0;
        }
        if (a == "--vm") { g_useVm = true; continue; }
        if (a == "--no-vm") { g_useVm = false; continue; }
        file = a;
        afterFile = true;
    }
    if (file.empty()) {
        std::cerr << "usage: cocorun <file.co | file.cob> [args...]\n";
        return 2;
    }

    // bytecode bundle path
    size_t dot = file.find_last_of('.');
    std::string ext = dot == std::string::npos ? "" : file.substr(dot);
    for (auto& c : ext) c = (char)tolower((unsigned char)c);
    if (ext == ".cob") {
        std::string msrc;
        std::map<std::string, std::string> emb;
        if (!unpackCob(file, msrc, emb)) {
            std::cerr << "cocorun: invalid bytecode bundle '" << file << "'\n";
            return 66;
        }
        return runSources(file, msrc, &emb, progArgs);
    }

    std::string src;
    if (!readFile(file, src)) return 2;
    return runSources(file, src, nullptr, progArgs);
}
