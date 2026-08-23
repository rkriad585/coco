// coco — driver CLI: run projects, scaffold apps/libraries, install packages
//
//   coco run <file.co|project dir>
//   coco new <name>            scaffold an application project
//   coco new lib <name>        scaffold a library project
//   coco install|i <pkg>       fetch a package into ./coco_modules/
//      pkg := [github.com/]user/repo[@tag] | <local path>
//
// Packages live per-project (node_modules style) under coco_modules/, so
// different projects can pin different versions of the same library.
#include "ast/ast.h"
#include "interp/runtime.h"
#include "lex/lexer.h"
#include "parser/parser.h"
#include "sema/checker.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

bool readFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

void writeFile(const fs::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
}

std::string trimSlashes(std::string s) {
    while (!s.empty() && (s.back() == '/' || s.back() == '\\')) s.pop_back();
    return s;
}

std::string lastSegment(const std::string& p) {
    size_t cut = p.find_last_of("/\\");
    return cut == std::string::npos ? p : p.substr(cut + 1);
}

// ---------------------------------------------------------------------------
// manifest (coco.json) — minimal hand-rolled handling, format is ours
// ---------------------------------------------------------------------------

std::string quote(const std::string& s) {
    std::string r = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') r += '\\';
        r += c;
    }
    return r + "\"";
}

std::string unescape(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i] == '\\' && i + 1 < s.size()) r += s[++i];
        else r += s[i];
    return r;
}

using Deps = std::map<std::string, std::string>;

Deps readDeps(const fs::path& manifest) {
    Deps deps;
    std::string text;
    if (!readFile(manifest.string(), text)) return deps;
    size_t d = text.find("\"dependencies\"");
    if (d == std::string::npos) return deps;
    size_t open = text.find('{', d);
    if (open == std::string::npos) return deps;
    size_t close = text.find('}', open);
    if (close == std::string::npos) close = text.size();
    size_t i = open;
    while (i < close) {
        size_t k0 = text.find('"', i);
        if (k0 == std::string::npos || k0 >= close) break;
        size_t k1 = text.find('"', k0 + 1);
        size_t v0 = k1 == std::string::npos ? std::string::npos
                                            : text.find('"', k1 + 1);
        size_t v1 = v0 == std::string::npos ? std::string::npos
                                            : text.find('"', v0 + 1);
        if (k1 == std::string::npos || v0 == std::string::npos ||
            v1 == std::string::npos || v1 > close)
            break;
        deps[text.substr(k0 + 1, k1 - k0 - 1)] =
            unescape(text.substr(v0 + 1, v1 - v0 - 1));
        i = v1 + 1;
    }
    return deps;
}

void writeManifest(const fs::path& path, const std::string& name,
                   const std::string& version, const std::string& type,
                   const std::string& main, const Deps& deps) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"name\": " << quote(name) << ",\n";
    o << "  \"version\": " << quote(version) << ",\n";
    o << "  \"type\": " << quote(type) << ",\n";
    o << "  \"main\": " << quote(main) << ",\n";
    o << "  \"dependencies\": {";
    bool first = true;
    for (const auto& [k, v] : deps) {
        if (!first) o << ",";
        first = false;
        o << "\n    " << quote(k) << ": " << quote(v);
    }
    o << (!deps.empty() ? "\n  }" : "}") << "\n}\n";
    writeFile(path, o.str());
}

// ---------------------------------------------------------------------------
// scaffolding
// ---------------------------------------------------------------------------

int cmdNew(const std::string& name, bool lib) {
    if (name.empty() || name.find_first_of("/\\") != std::string::npos) {
        std::cerr << "coco new: '" << name << "' is not a valid project name\n";
        return 1;
    }
    const fs::path root(name);
    if (fs::exists(root)) {
        std::cerr << "coco new: '" << name << "' already exists\n";
        return 1;
    }

    Deps deps;
    if (lib) {
        fs::create_directories(root / "src");
        writeManifest(root / "coco.json", name, "0.1.0", "lib",
                      "src/" + name + ".co", deps);
        writeFile(root / "src" / (name + ".co"),
                  "# " + name + " - a Coco library\n"
                  "# pub functions are importable by consumers:\n"
                  "#   import \"" + name + "\"\n\n"
                  "pub def hello(who: string) -> string:\n"
                  "    return \"hello from " + name + ", \" + who + \"!\"\n");
        writeFile(root / "README.md",
                  "# " + name + "\n\nA Coco library.\n\n"
                  "## Install (once published on GitHub)\n\n"
                  "    coco install github.com/user/" + name + "\n");
    } else {
        fs::create_directories(root);
        writeManifest(root / "coco.json", name, "0.1.0", "app", "main.co", deps);
        writeFile(root / "main.co",
                  "def main():\n"
                  "    print(\"hello from " + name + "\")\n");
        writeFile(root / ".gitignore", "coco_modules/\n");
    }
    std::cout << "created " << (lib ? "library" : "project") << " '" << name
              << "'\nnext:\n";
    if (lib) {
        std::cout << "  cd " << name << "            # edit src/" << name
                  << ".co\n"
                  << "  coco install ./path/to/app   # or publish to GitHub\n";
    } else {
        std::cout << "  cd " << name << "\n  coco run main.co\n";
    }
    return 0;
}

// ---------------------------------------------------------------------------
// package installation
// ---------------------------------------------------------------------------

struct PkgRef {
    std::string spec;     // normalized repo path or local dir
    std::string tag;      // optional @tag
    std::string destName; // directory name under coco_modules/
    bool isLocal = false;
};

bool parsePkgRef(const std::string& raw, PkgRef& ref) {
    std::string spec = raw;
    // "@version" only when it looks like repo@tag (not ./dir@x)
    size_t at = spec.find('@');
    if (at != std::string::npos && spec.find(':') == std::string::npos &&
        at > 0 && (spec[0] != '.' && spec[0] != '/')) {
        ref.tag = spec.substr(at + 1);
        if (ref.tag.empty()) {
            std::cerr << "coco install: empty version in '" << raw << "'\n";
            return false;
        }
        spec = spec.substr(0, at);
    }

    fs::path asPath(spec);
    bool looksLocal = asPath.is_absolute() || !spec.empty() &&
                                                     (spec[0] == '.' ||
                                                      spec[0] == '/' ||
                                                      spec[0] == '\\');
    if (looksLocal) {
        if (!fs::is_directory(asPath)) {
            std::cerr << "coco install: local package '" << raw
                      << "' does not exist\n";
            return false;
        }
        ref.isLocal = true;
        ref.spec = fs::absolute(asPath).string();
        ref.destName = lastSegment(trimSlashes(spec));
    } else {
        // bare user/repo implies github.com
        size_t slashes = std::count(spec.begin(), spec.end(), '/');
        if (slashes == 1) spec = "github.com/" + spec;
        if (std::count(spec.begin(), spec.end(), '/') < 2) {
            std::cerr << "coco install: '" << raw
                      << "' is not user/repo or github.com/user/repo\n";
            return false;
        }
        ref.spec = trimSlashes(spec);
        ref.destName = lastSegment(ref.spec);
    }
    if (ref.destName.empty() || ref.destName == "." || ref.destName == "..") {
        std::cerr << "coco install: cannot derive a package name from '" << raw
                  << "'\n";
        return false;
    }
    return true;
}

bool gitAvailable() { return std::system("git --version >nul 2>nul") == 0; }

int cmdInstall(const std::string& raw) {
    PkgRef ref;
    if (!parsePkgRef(raw, ref)) return 1;

    const fs::path modulesDir = "coco_modules";
    const fs::path dest = modulesDir / ref.destName;
    std::error_code ec;

    if (ref.isLocal) {
        fs::remove_all(dest, ec);
        fs::create_directories(modulesDir, ec);
        fs::copy(ref.spec, dest,
                 fs::copy_options::recursive |
                     fs::copy_options::copy_symlinks,
                 ec);
        if (ec) {
            std::cerr << "coco install: copy failed: " << ec.message() << "\n";
            return 1;
        }
    } else {
        if (!gitAvailable()) {
            std::cerr << "coco install: git not found on PATH\n";
            return 1;
        }
        fs::remove_all(dest, ec);
        fs::create_directories(modulesDir, ec);
        std::string url =
            "https://" + ref.spec + ".git";
        std::string cmd = "git clone --quiet --depth 1";
        if (!ref.tag.empty()) cmd += " --branch " + ref.tag;
        cmd += " " + url + " \"" + dest.string() + "\"";
        int rc = std::system(cmd.c_str());
        if (rc != 0) {
            std::cerr << "coco install: git clone failed for " << url << "\n";
            return 1;
        }
    }

    // record dependency in the project manifest (raw spec as key)
    Deps deps = readDeps("coco.json");
    deps[ref.isLocal ? raw : ref.spec] = ref.tag.empty() ? "*" : ref.tag;
    std::string name = "app", version = "0.1.0", type = "app", main = "main.co";
    std::string text;
    if (readFile("coco.json", text)) {
        auto grab = [&](const char* field, std::string& into) {
            size_t p = text.find(std::string("\"") + field + "\"");
            if (p == std::string::npos) return;
            size_t c0 = text.find('"', text.find(':', p) + 1);
            size_t c1 = c0 == std::string::npos ? std::string::npos
                                                : text.find('"', c0 + 1);
            if (c0 != std::string::npos && c1 != std::string::npos)
                into = text.substr(c0 + 1, c1 - c0 - 1);
        };
        grab("name", name);
        grab("version", version);
        grab("type", type);
        grab("main", main);
    }
    writeManifest("coco.json", name, version, type, main, deps);

    std::cout << "installed " << ref.spec;
    if (!ref.tag.empty()) std::cout << "@" << ref.tag;
    std::cout << " -> " << (dest / "").string() << "\n";
    return 0;
}

// ---------------------------------------------------------------------------
// run pipeline (same stages as cocorun)
// ---------------------------------------------------------------------------

void addModuleDirs(coco::interp::Interpreter& interp, const std::string& script) {
    size_t p = script.find_last_of("/\\");
    std::string dir = p == std::string::npos ? "." : script.substr(0, p);
    if (const char* env = std::getenv("COCO_MODULES"))
        interp.addStdlibDir(env);
    interp.addStdlibDir(dir + "/coco_modules");
    interp.addStdlibDir(dir + "/../stdlib");
    interp.addStdlibDir(dir + "/../../stdlib");
    interp.addStdlibDir("stdlib");
    if (const char* env = std::getenv("COCO_STDLIB")) interp.addStdlibDir(env);
}

int cmdRun(const std::string& target) {
    fs::path file(target);
    if (fs::is_directory(file)) {
        // project dir: run its manifest entry point
        std::string main = "main.co";
        std::string text;
        if (readFile((file / "coco.json").string(), text)) {
            size_t p = text.find("\"main\"");
            if (p != std::string::npos) {
                size_t c0 = text.find('"', text.find(':', p) + 1);
                size_t c1 = c0 == std::string::npos ? std::string::npos
                                                    : text.find('"', c0 + 1);
                if (c0 != std::string::npos && c1 != std::string::npos)
                    main = text.substr(c0 + 1, c1 - c0 - 1);
            }
        }
        file /= main;
    }
    if (!fs::is_regular_file(file)) {
        std::cerr << "coco run: cannot open '" << target << "'\n";
        return 66;
    }

    std::string src;
    if (!readFile(file.string(), src)) {
        std::cerr << "coco run: cannot read '" << file.string() << "'\n";
        return 66;
    }

    coco::DiagEngine diags;
    auto toks = coco::Lexer(src, file.string(), diags).lexAll();
    if (diags.count()) {
        for (const auto& d : diags.diags())
            std::cerr << file.string() << ":" << d.line << ":" << d.col
                      << ": error: " << d.message << "\n";
        return 65;
    }
    auto module = coco::Parser(toks, diags).parseProgram();
    if (diags.count()) {
        for (const auto& d : diags.diags())
            std::cerr << file.string() << ":" << d.line << ":" << d.col
                      << ": error: " << d.message << "\n";
        return 65;
    }
    {
        coco::sema::Checker chk(diags);
        chk.checkModule(module);
    }
    if (diags.count()) {
        for (const auto& d : diags.diags())
            std::cerr << file.string() << ":" << d.line << ":" << d.col
                      << ": error: " << d.message << "\n";
        return 65;
    }

    coco::ast::Stmt root;
    root.kind = coco::ast::StKind::Pass;
    root.body = std::move(module);
    try {
        coco::interp::Interpreter interp(root);
        addModuleDirs(interp, file.string());
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
    } catch (std::exception& ex) {
        std::cerr << "internal error: " << ex.what() << "\n";
        return 70;
    }
}

void usage() {
    std::cout
        << "coco - the Coco language driver\n\n"
        << "usage:\n"
        << "  coco run <file.co | project dir>   run a program or project\n"
        << "  coco new <name>                    scaffold an application\n"
        << "  coco new lib <name>                scaffold a library package\n"
        << "  coco install|i <pkg>               install into ./coco_modules/\n"
        << "      pkg := [github.com/]user/repo[@tag] | <local path>\n";
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + (argc > 0 ? 1 : 0), argv + argc);
    if (args.empty()) {
        usage();
        return 64;
    }
    const std::string& cmd = args[0];
    if ((cmd == "run" || cmd == "r") && args.size() == 2) return cmdRun(args[1]);
    if (cmd == "new" && args.size() >= 2) {
        bool lib = args[1] == "lib";
        if (!lib && args.size() != 2) {
            usage();
            return 64;
        }
        if (lib && args.size() != 3) {
            usage();
            return 64;
        }
        return cmdNew(lib ? args[2] : args[1], lib);
    }
    if ((cmd == "install" || cmd == "i") && args.size() == 2)
        return cmdInstall(args[1]);
    usage();
    return args[0] == "help" || args[0] == "--help" || args[0] == "-h" ? 0 : 64;
}
