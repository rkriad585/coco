// cococheck: lex + parse + semantic analysis for a Coco source file.
//   cococheck <file>        check; print "OK" or diagnostics, exit 0/1
#include "lex/lexer.h"
#include "parser/parser.h"
#include "sema/checker.h"
#include "support/diag.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

bool readFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "cococheck: cannot open '" << path << "'\n";
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
    bool color = false;
    bool plain = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") {
            std::cout << "usage: cococheck [--color|--no-color|--plain] <file.co>\n";
            return 0;
        }
        if (a == "--color") { color = true; continue; }
        if (a == "--no-color") { color = false; continue; }
        if (a == "--plain") { plain = true; continue; }
        file = a;
    }
    if (file.empty()) {
        std::cerr << "usage: cococheck <file.co>\n";
        return 2;
    }

    std::string src;
    if (!readFile(file, src)) return 2;

    coco::DiagEngine diags;
    auto toks = coco::Lexer(src, file, diags).lexAll();

    size_t front = diags.count();
    if (front == 0) {
        auto prog = coco::Parser(toks, diags).parseProgram();
        front = diags.count();
        if (front == 0) {
            coco::sema::Checker checker(diags);
            checker.checkModule(prog);
        }
    }

    if (diags.count()) {
        coco::SourceMap sm(src);
        std::string out;
        coco::renderDiags(file, sm, diags.diags(), color, plain, out);
        std::cout << out;
        std::cout << file << ": " << diags.errorCount() << " error(s)";
        if (diags.warningCount()) std::cout << ", " << diags.warningCount() << " warning(s)";
        std::cout << "\n";
        return diags.errorCount() ? 1 : 0;
    }
    std::cout << file << ": OK\n";
    return 0;
}
