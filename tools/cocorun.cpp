// cocorun: lex + parse + semantic-check + interpret a Coco source file.
//   cocorun <file.co> [args...]   run program; main()'s Int return is the
//                                 process exit code (default 0)
#include "interp/runtime.h"
#include "lex/lexer.h"
#include "parser/parser.h"
#include "sema/checker.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
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
//   1. $COCO_MODULES   (per-project package cache, node_modules style)
//   2. <script dir>/coco_modules
//   3. <script dir>/../stdlib   (repo checkout layout)
//   4. ./stdlib
//   5. $COCO_STDLIB
void addModuleDirs(coco::interp::Interpreter& interp,
                   const std::string& script) {
    size_t p = script.find_last_of("/\\");
    std::string dir = p == std::string::npos ? "." : script.substr(0, p);
    if (const char* env = std::getenv("COCO_MODULES"))
        interp.addStdlibDir(env);
    interp.addStdlibDir(dir + "/coco_modules");
    interp.addStdlibDir(dir + "/../stdlib");
    interp.addStdlibDir("stdlib");
    if (const char* env = std::getenv("COCO_STDLIB")) interp.addStdlibDir(env);
}

} // namespace

int main(int argc, char** argv) {
    std::string file;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") {
            std::cout << "usage: cocorun <file.co>\n";
            return 0;
        }
        if (file.empty()) file = a;
    }
    if (file.empty()) {
        std::cerr << "usage: cocorun <file.co>\n";
        return 2;
    }

    std::string src;
    if (!readFile(file, src)) return 2;

    coco::DiagEngine diags;
    auto toks = coco::Lexer(src, file, diags).lexAll();

    size_t front = diags.count();
    if (front == 0) {
        auto body = coco::Parser(toks, diags).parseProgram();
        front = diags.count();
        if (front == 0) {
            coco::sema::Checker checker(diags);
            checker.checkModule(body);
        }
        if (front == 0) {
            coco::ast::Stmt module;
            module.kind = coco::ast::StKind::Pass;
            module.body = std::move(body);

            try {
                coco::interp::Interpreter interp(module);
                addModuleDirs(interp, file);
                coco::interp::Value r = interp.run();
                return r.k == coco::interp::VK::Int ? (int)r.i : 0;
            } catch (const coco::interp::PanicSignal& p) {
                fflush(stdout);
                fputs(("panic: " + p.msg + "\n").c_str(), stderr);
                return 70;
            } catch (const coco::interp::SignalRaise&) {
                fflush(stdout);
                fputs("panic: uncaught raise escaped main\n", stderr);
                return 70;
            }
        }
    }

    for (const auto& d : diags.diags())
        std::cout << file << ":" << d.line << ":" << d.col
                  << ": error: " << d.message << "\n";
    if (diags.count() != 0) {
        std::cout << file << ": " << diags.count() << " error(s)\n";
        return 1;
    }
    return 0;
}
