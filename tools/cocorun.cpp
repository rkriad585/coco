// cocorun: lex + parse + semantic-check + interpret a Coco source file.
//   cocorun <file.co> [args...]   run program; main()'s Int return is the
//                                 process exit code (default 0)
#include "interp/runtime.h"
#include "lex/lexer.h"
#include "parser/parser.h"
#include "sema/checker.h"

#include <cstdio>
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
