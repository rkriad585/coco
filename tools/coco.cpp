// coco — driver CLI for the Coco language.
//
//   coco new <name>              scaffold an application project
//   coco new lib <name>          scaffold a library package
//   coco run [dir|file]          run a program or project
//   coco test [dir|file...]      run *_test.co files
//   coco install|i [-g] <pkg>    install into ./coco_libs or ~/.coco/coco-pkg
//   coco update [name]           refresh installed dependencies
//   coco remove <name>           uninstall a dependency
//   coco build                   compile project -> standalone build/<name>.exe
//   coco build lib               check + pack library -> build/<n>-<v>.cocolib
//   coco doc <lib|dir> [--port]  generate API docs + serve markdown viewer
//   coco list                    show installed libraries
//
// Projects are Rust-style but with Coco's own folder names:
//   coco.toml | code/ | tests/ | docs/ | coco_libs/ | build/
// Packages installed globally live in ~/.coco/coco-pkg (binaries in bin/).
#include "ast/ast.h"
#include "interp/runtime.h"
#include "lex/lexer.h"
#include "parser/parser.h"
#include "sema/checker.h"
#include "util/tomlmini.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace fs = std::filesystem;

namespace {

using coco::tomlmini::Doc;
using Deps = std::map<std::string, std::map<std::string, std::string>>;

bool readFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool writeFile(const fs::path& path, const std::string& content) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    out << content;
    return out.good();
}

std::string trimSlashes(std::string s) {
    while (!s.empty() && (s.back() == '/' || s.back() == '\\')) s.pop_back();
    return s;
}

std::string lastSegment(const std::string& p) {
    size_t cut = p.find_last_of("/\\");
    return cut == std::string::npos ? p : p.substr(cut + 1);
}

std::string todayIso() {
    time_t t = time(nullptr);
    tm tmv{};
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    char buf[32];
    strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tmv);
    return buf;
}

// ---------------------------------------------------------------------------
// manifests: coco.toml
// ---------------------------------------------------------------------------

struct Manifest {
    std::string name, version = "0.1.0", type = "app", main, description,
                license;
    std::string docs = "docs/index.md";   // docs entry file ([package] docs)
    std::vector<std::string> authors, tags;
    Deps deps;
};

Manifest readManifest(const fs::path& dir) {
    Manifest m;
    std::string text;
    if (!readFile((dir / "coco.toml").string(), text)) return m;
    Doc d = coco::tomlmini::parse(text);
    m.name = coco::tomlmini::get(d, "package", "name");
    m.version = coco::tomlmini::get(d, "package", "version", "0.1.0");
    m.type = coco::tomlmini::get(d, "package", "type", "app");
    m.main = coco::tomlmini::get(d, "package", "main");
    m.docs = coco::tomlmini::get(d, "package", "docs", "docs/index.md");
    m.description = coco::tomlmini::get(d, "package", "description");
    m.license = coco::tomlmini::get(d, "package", "license");
    for (const auto& [k, v] : d.kv) {
        if (k.rfind("package.authors", 0) == 0 ||
            k.rfind("package.tags", 0) == 0) {
            // inline arrays ["a", "b"]
            size_t a = v.find('['), b = v.rfind(']');
            if (a == std::string::npos || b == std::string::npos) continue;
            std::string body = v.substr(a + 1, b - a - 1);
            std::string cur;
            bool q = false;
            std::vector<std::string> items;
            for (char c : body) {
                if (c == '"') q = !q;
                if (c == ',' && !q) {
                    items.push_back(cur);
                    cur.clear();
                } else
                    cur += c;
            }
            items.push_back(cur);
            for (auto& it : items) {
                std::string s = coco::tomlmini::strip(it);
                if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
                    s = s.substr(1, s.size() - 2);
                if (!s.empty())
                    (k.rfind("package.tags", 0) == 0 ? m.tags : m.authors)
                        .push_back(s);
            }
        }
    }
    for (const auto& [name, spec] : coco::tomlmini::dependencies(d)) {
        Deps::mapped_type e;
        for (const auto& [ik, iv] : spec) e[ik] = coco::tomlmini::unquote(iv);
        m.deps[name] = e;
    }
    return m;
}

std::string tomlArray(const std::vector<std::string>& v) {
    if (v.empty()) return "[]";
    std::string o = "[";
    for (size_t i = 0; i < v.size(); ++i)
        o += (i ? ", " : "") + coco::tomlmini::quote(v[i]);
    return o + "]";
}

void writeManifest(const fs::path& dir, const Manifest& m) {
    std::ostringstream o;
    o << "[package]\n";
    o << "name = " << coco::tomlmini::quote(m.name) << "\n";
    o << "version = " << coco::tomlmini::quote(m.version) << "\n";
    o << "type = " << coco::tomlmini::quote(m.type) << "\n";
    if (!m.main.empty()) o << "main = " << coco::tomlmini::quote(m.main) << "\n";
    if (!m.docs.empty()) o << "docs = " << coco::tomlmini::quote(m.docs) << "\n";
    if (!m.description.empty())
        o << "description = " << coco::tomlmini::quote(m.description) << "\n";
    if (!m.license.empty())
        o << "license = " << coco::tomlmini::quote(m.license) << "\n";
    if (!m.authors.empty()) o << "authors = " << tomlArray(m.authors) << "\n";
    if (!m.tags.empty()) o << "tags = " << tomlArray(m.tags) << "\n";
    o << "\n[dependencies]\n";
    for (const auto& [name, spec] : m.deps) {
        if (spec.size() == 1 && spec.count("version"))
            o << name << " = " << coco::tomlmini::quote(spec.at("version"))
              << "\n";
        else {
            o << name << " = { ";
            bool first = true;
            for (const auto& [ik, iv] : spec) {
                if (!first) o << ", ";
                first = false;
                o << ik << " = " << coco::tomlmini::quote(iv);
            }
            o << " }\n";
        }
    }
    writeFile(dir / "coco.toml", o.str());
}

// ---------------------------------------------------------------------------
// lockfile: coco.lock — pins exactly what is installed where
// ---------------------------------------------------------------------------

struct LockEntry {
    std::string name, source, url, tag, commit, version, installed;
};

std::vector<LockEntry> readLock(const fs::path& dir) {
    std::vector<LockEntry> out;
    std::string text;
    if (!readFile((dir / "coco.lock").string(), text)) return out;
    Doc d = coco::tomlmini::parse(text);
    for (const auto& t : d.tables) {
        if (t.name != "lock") continue;
        LockEntry e;
        auto g = [&](const char* k) {
            auto it = t.kv.find(k);
            return it == t.kv.end() ? "" : coco::tomlmini::unquote(it->second);
        };
        e.name = g("name");
        e.source = g("source");
        e.url = g("url");
        e.tag = g("tag");
        e.commit = g("commit");
        e.version = g("version");
        e.installed = g("installed");
        if (!e.name.empty()) out.push_back(e);
    }
    return out;
}

void writeLock(const fs::path& dir, const std::vector<LockEntry>& locks) {
    std::ostringstream o;
    o << "# coco.lock — generated by `coco install/update`. Commit this file.\n";
    for (const auto& e : locks) {
        o << "\n[[lock]]\n";
        o << "name = " << coco::tomlmini::quote(e.name) << "\n";
        o << "source = " << coco::tomlmini::quote(e.source) << "\n";
        o << "url = " << coco::tomlmini::quote(e.url) << "\n";
        o << "tag = " << coco::tomlmini::quote(e.tag) << "\n";
        o << "commit = " << coco::tomlmini::quote(e.commit) << "\n";
        o << "version = " << coco::tomlmini::quote(e.version) << "\n";
        o << "installed = " << coco::tomlmini::quote(e.installed) << "\n";
    }
    writeFile(dir / "coco.lock", o.str());
}

bool copyTree(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    // refuse to copy a tree into itself (case-insensitive on win32)
    std::string f = fs::weakly_canonical(from, ec).string();
    std::string t = fs::weakly_canonical(to, ec).string();
    for (char& c : f) c = (char)tolower((unsigned char)c);
    for (char& c : t) c = (char)tolower((unsigned char)c);
    if (t.rfind(f + "/", 0) == 0 || t.rfind(f + "\\", 0) == 0 || t == f) {
        std::cerr << "coco install: destination lies inside the source\n";
        return false;
    }
    fs::create_directories(to, ec);
    if (ec) {
        std::cerr << "coco install: mkdir " << to.string() << ": "
                  << ec.message() << "\n";
        return false;
    }
    for (fs::directory_iterator it(from, ec), end; !ec && it != end;
         it.increment(ec)) {
        if (ec) break;
        const fs::path& p = it->path();
        if (p.filename() == ".git") continue;   // never ship VCS metadata
        if (it->is_directory(ec)) {
            if (!copyTree(p, to / p.filename())) return false;
        } else if (it->is_regular_file(ec)) {
            fs::copy_file(p, to / p.filename(), fs::copy_options::overwrite_existing,
                          ec);
            if (ec) {
                std::cerr << "coco install: copy " << p.string() << ": "
                          << ec.message() << "\n";
                return false;
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// module/library search paths
// ---------------------------------------------------------------------------

std::string globalPkgDir() {
    const char* home = std::getenv("USERPROFILE");
    if (!home) home = std::getenv("HOME");
    return home ? std::string(home) + "/.coco/coco-pkg" : "";
}

std::vector<std::string> libDirsFor(const std::string& script) {
    std::vector<std::string> dirs;
    if (const char* env = std::getenv("COCO_LIBS")) dirs.push_back(env);
    size_t p = script.find_last_of("/\\");
    std::string dir = p == std::string::npos ? "." : script.substr(0, p);
    dirs.push_back(dir + "/coco_libs");
    if (!globalPkgDir().empty()) dirs.push_back(globalPkgDir());
    dirs.push_back(dir + "/../stdlib");
    dirs.push_back(dir + "/../../stdlib");
    if (const char* env = std::getenv("COCO_STDLIB")) dirs.push_back(env);
    dirs.push_back(dir);   // a project/package can import itself
    return dirs;
}

// ---------------------------------------------------------------------------
// compile pipeline shared by run/test/build
// ---------------------------------------------------------------------------

// check one source; returns program or empty vector on error (diags printed)
std::vector<coco::ast::StmtP> frontEnd(const std::string& path,
                                       const std::string& src,
                                       coco::DiagEngine& diags) {
    auto toks = coco::Lexer(src, path, diags).lexAll();
    if (diags.count()) return {};
    auto body = coco::Parser(toks, diags).parseProgram();
    if (diags.count()) return {};
    coco::sema::Checker chk(diags);
    chk.checkModule(body);
    return body;
}

void printDiags(const std::string& path, const coco::DiagEngine& diags) {
    for (const auto& d : diags.diags())
        std::cerr << path << ":" << d.line << ":" << d.col
                  << ": error: " << d.message << "\n";
}

int runProgram(const std::string& entryPath,
               const std::vector<std::string>& dirs,
               const std::map<std::string, std::string>& embedded = {}) {
    std::string src;
    if (!readFile(entryPath, src)) {
        std::cerr << "coco: cannot read '" << entryPath << "'\n";
        return 66;
    }
    coco::DiagEngine diags;
    auto body = frontEnd(entryPath, src, diags);
    if (diags.count()) {
        printDiags(entryPath, diags);
        return 65;
    }
    coco::ast::Stmt root;
    root.kind = coco::ast::StKind::Pass;
    root.body = std::move(body);
    try {
        coco::interp::Interpreter interp(root);
        for (const auto& d : dirs) interp.addStdlibDir(d);
        for (const auto& [name, esrc] : embedded)
            interp.addEmbeddedSource(name, esrc);
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

// ---------------------------------------------------------------------------
// scaffolding: rust-style structure with Coco folder names
// ---------------------------------------------------------------------------

int cmdNew(const std::string& name, bool lib) {
    if (name.empty() || name.find_first_of("/\\") != std::string::npos ||
        name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") !=
            std::string::npos) {
        std::cerr << "coco new: '" << name << "' is not a valid package name\n";
        return 1;
    }
    const fs::path root(name);
    if (fs::exists(root)) {
        std::cerr << "coco new: '" << name << "' already exists\n";
        return 1;
    }

    Manifest m;
    m.name = name;
    m.version = "0.1.0";
    m.type = lib ? "lib" : "app";
    m.description = lib ? "A Coco library" : "A Coco application";
    m.license = "MIT";
    m.docs = "docs/index.md";
    m.authors.push_back("Your Name <you@example.com>");
    if (lib) m.tags.push_back("utility");

    if (lib) {
        m.main = "code/" + name + ".co";
        writeManifest(root, m);

        writeFile(root / "code" / (name + ".co"),
                  "## " + name + " - a Coco library.\n"
                  "##\n"
                  "## Doc comments start with '##' and sit directly above a\n"
                  "## 'pub def'. `coco doc " + name + "` turns them into a\n"
                  "## browsable API reference.\n\n"
                  "## Say hello to someone.\n"
                  "pub def hello(who: string) -> string:\n"
                  "    return \"hello from " + name + ", \" + who + \"!\"\n\n"
                  "## Shout it louder.\n"
                  "pub def shout(who: string) -> string:\n"
                  "    s = hello(who)\n"
                  "    out = \"\"\n"
                  "    for ch in s:\n"
                  "        c = ord(ch)\n"
                  "        if c >= 97 and c <= 122:\n"
                  "            c = c - 32\n"
                  "        out = out + chr(c)\n"
                  "    return out\n");

        writeFile(root / "tests" / (name + "_test.co"),
                  "# tests live in tests/ and are named <file>_test.co\n"
                  "# run them all with:  coco test .\n\n"
                  "import \"" + name + "\"\n\n"
                  "def main():\n"
                  "    assert_eq(greet_lib_probe(), 42)\n"
                  "    print(\"all tests passed\")\n\n"
                  "def greet_lib_probe() -> int:\n"
                  "    return 6 * 7\n");

        writeFile(root / "docs" / "index.md",
                  "# " + name + "\n\n" + m.description +
                      ".\n\n## Install\n\n```bash\n"
                      "coco install github.com/coco-lib/" + name +
                      "\n```\n\n## Usage\n\n```co\n"
                      "import \"" + name + "\"\n\n"
                      "print(" + name + ".hello(\"world\"))\n```\n");

        writeFile(root / "README.md",
                  "# " + name + "\n\n" + m.description +
                      ".\n\nDocs live in `docs/index.md` (the manifest's "
                      "`docs` entry). Regenerate the API section with:\n\n"
                      "```bash\ncoco doc " + name + "\ncoco test .\n"
                      "```\n\n## License\n\nMIT\n");
        writeFile(root / "LICENSE",
                  "MIT License\n\nCopyright (c) 2026 " + name +
                      " authors\n\nPermission is hereby granted, free of "
                      "charge, to any person obtaining a copy of this software "
                      "to deal in the Software without restriction.\n");
        writeFile(root / ".gitignore", "build/\n");

        std::cout << "created library '" << name << "'\n"
                  << "  " << name << "/coco.toml      manifest\n"
                  << "  code/" << name << ".co   sources (with ## docs)\n"
                  << "  tests/          *_test.co files\n"
                  << "  docs/           markdown docs\n"
                  << "next:\n"
                  << "  cd " << name << " && coco build lib && coco test .\n";
    } else {
        m.main = "code/main.co";
        writeManifest(root, m);

        writeFile(root / "code" / "main.co",
                  "# " + name + " - a Coco application.\n\n"
                  "def main():\n"
                  "    print(\"hello from " + name + "\")\n");

        writeFile(root / "tests" / "main_test.co",
                  "# tests live in tests/ and are named <file>_test.co\n\n"
                  "def main():\n"
                  "    assert_eq(2 + 2, 4)\n"
                  "    print(\"all tests passed\")\n");

        writeFile(root / "docs" / "index.md",
                  "# " + name + "\n\n" + m.description + ".\n\n## Run\n\n```"
                  "bash\ncoco run\ncoco test .\ncoco build\n```\n");

        writeFile(root / "README.md",
                  "# " + name + "\n\n" + m.description +
                      ".\n\n## Run\n\n```bash\ncoco run\n```\n");
        writeFile(root / ".gitignore", "build/\ncoco_libs/\n");

        std::cout << "created project '" << name << "'\n"
                  << "  " << name << "/coco.toml      manifest\n"
                  << "  code/main.co    entry point\n"
                  << "  tests/          *_test.co files\n"
                  << "  docs/           project docs\n"
                  << "next:\n"
                  << "  cd " << name << " && coco run\n";
    }
    return 0;
}

// ---------------------------------------------------------------------------
// package sources: local path | git repo | coco-libs registry
// ---------------------------------------------------------------------------

struct PkgRef {
    std::string spec;      // normalized repo path or local dir
    std::string tag;
    std::string destName;  // directory name under coco_libs/ or coco-pkg/
    enum class Kind { Path, Git } kind = Kind::Git;
};

bool parsePkgRef(const std::string& raw, PkgRef& ref) {
    std::string spec = raw;
    size_t at = spec.find('@');
    if (at != std::string::npos && spec.find(':') == std::string::npos &&
        at > 0 && spec[0] != '.' && spec[0] != '/' && spec[0] != '\\') {
        ref.tag = spec.substr(at + 1);
        if (ref.tag.empty()) {
            std::cerr << "coco install: empty version in '" << raw << "'\n";
            return false;
        }
        spec = spec.substr(0, at);
    }

    fs::path asPath(spec);
    bool looksLocal = asPath.is_absolute() ||
                      (!spec.empty() &&
                       (spec[0] == '.' || spec[0] == '/' || spec[0] == '\\'));
    if (looksLocal) {
        if (asPath.extension() == ".cocolib") return true;  // handled later
        if (!fs::is_directory(asPath)) {
            std::cerr << "coco install: local package '" << raw
                      << "' does not exist\n";
            return false;
        }
        ref.kind = PkgRef::Kind::Path;
        ref.spec = fs::absolute(asPath).string();
        ref.destName = lastSegment(trimSlashes(spec));
    } else {
        size_t slashes = std::count(spec.begin(), spec.end(), '/');
        if (slashes == 1) spec = "github.com/" + spec;   // user/repo shorthand
        if (std::count(spec.begin(), spec.end(), '/') < 2) {
            std::cerr << "coco install: '" << raw
                      << "' is not user/repo, github.com/user/repo, a path,"
                         " or a registry name\n";
            return false;
        }
        ref.kind = PkgRef::Kind::Git;
        ref.spec = trimSlashes(spec);
        ref.destName = lastSegment(ref.spec);
    }
    if (ref.destName.empty() || ref.destName == "." || ref.destName == "..") {
        std::cerr << "coco install: cannot derive a package name from '"
                  << raw << "'\n";
        return false;
    }
    return true;
}

// resolve a bare library name through the coco-libs registry
// (github.com/coco-lib/coco-libs -> registry/lib.toml)
bool lookupRegistry(const std::string& name, std::string& url) {
    const std::string cache = ".coco-registry-lib.toml";
    std::string text;
    bool fresh = false;
    if (readFile(cache, text)) {
        // cached copy is fine for offline use; refresh opportunistically
        fresh = true;
    }
    if (!fresh || std::system("curl -s -o .coco-registry-lib.toml "
        "https://raw.githubusercontent.com/coco-lib/coco-libs/main/"
        "registry/lib.toml") == 0)
        readFile(cache, text);
    Doc d = coco::tomlmini::parse(text);
    for (const auto& t : d.tables) {
        if (t.name != "lib") continue;
        auto n = t.kv.find("name");
        if (n == t.kv.end() || coco::tomlmini::unquote(n->second) != name)
            continue;
        auto u = t.kv.find("url");
        if (u != t.kv.end()) url = coco::tomlmini::unquote(u->second);
        return !url.empty();
    }
    return false;
}

std::string gitHeadSha(const fs::path& dir) {
    std::string cmd = "git -C \"" + dir.string() + "\" rev-parse HEAD 2>nul";
    // capture via temp file (system() has no portable pipe-back)
    std::string tmp = ".coco-sha";
    std::string full = cmd + " > \"" + tmp + "\"";
    if (std::system(full.c_str()) != 0) return "";
    std::string sha;
    readFile(tmp, sha);
    std::remove(tmp.c_str());
    while (!sha.empty() && (sha.back() == '\n' || sha.back() == '\r'))
        sha.pop_back();
    return sha.size() >= 7 ? sha.substr(0, 12) : sha;
}

// copy a package source into place; returns manifest of the installed copy
bool materializePackage(const PkgRef& ref, const fs::path& dest,
                        Manifest& outManifest, std::string& commitSha) {
    std::error_code ec;
    if (ref.kind == PkgRef::Kind::Path) {
        fs::remove_all(dest, ec);
        if (ec) {
            std::cerr << "coco install: cleanup failed: " << ec.message()
                      << "\n";
            return false;
        }
        if (!copyTree(ref.spec, dest)) return false;
        commitSha.clear();
    } else {
        if (std::system("git --version >nul 2>nul") != 0) {
            std::cerr << "coco install: git not found on PATH\n";
            return false;
        }
        fs::remove_all(dest, ec);
        fs::create_directories(dest.parent_path(), ec);
        std::string url = "https://" + ref.spec + ".git";
        std::string cmd =
            "git clone --quiet --depth 1" +
            std::string(ref.tag.empty() ? "" : " --branch " + ref.tag) + " " +
            url + " \"" + dest.string() + "\"";
        if (std::system(cmd.c_str()) != 0) {
            std::cerr << "coco install: git clone failed for " << url << "\n";
            return false;
        }
        fs::remove_all(dest / ".git", ec);   // keep installs lean
        commitSha = gitHeadSha(dest);
    }
    outManifest = readManifest(dest);
    if (outManifest.name.empty())
        outManifest.name = ref.destName;   // manifest-less packages still work
    return true;
}

void upsertLock(std::vector<LockEntry>& locks, LockEntry e) {
    for (auto& x : locks)
        if (x.name == e.name) {
            x = e;
            return;
        }
    locks.push_back(e);
}

int cmdInstall(const std::string& raw, bool global_) {
    // .cocolib bundle?
    if (fs::is_regular_file(raw) && fs::path(raw).extension() == ".cocolib")
        return 99;   // replaced below by unpackCocolib path

    PkgRef ref;
    if (!parsePkgRef(raw, ref)) return 1;

    fs::path base = global_ ? fs::path(globalPkgDir())
                            : fs::path("coco_libs");
    if (global_) {
        std::error_code ec;
        fs::create_directories(base / "bin", ec);
    }
    const fs::path dest = base / ref.destName;

    Manifest pkg;
    std::string sha;
    if (!materializePackage(ref, dest, pkg, sha)) return 1;

    if (!global_) {
        // record dependency + lock entry in this project
        Manifest proj = readManifest(".");
        Deps::mapped_type spec;
        if (ref.kind == PkgRef::Kind::Path)
            spec["path"] = raw;
        else
            spec["git"] = ref.spec;
        if (!ref.tag.empty()) spec["tag"] = ref.tag;
        proj.deps[ref.destName] = spec;
        writeManifest(".", proj);

        std::vector<LockEntry> locks = readLock(".");
        LockEntry le;
        le.name = ref.destName;
        le.source = ref.kind == PkgRef::Kind::Path ? "path" : "git";
        le.url = ref.spec;
        le.tag = ref.tag;
        le.commit = sha;
        le.version = pkg.version;
        le.installed = todayIso();
        upsertLock(locks, le);
        writeLock(".", locks);
    }

    std::cout << "installed " << (ref.kind == PkgRef::Kind::Path ? raw : ref.spec);
    if (!ref.tag.empty()) std::cout << "@" << ref.tag;
    std::cout << " (" << (pkg.version.empty() ? "?" : pkg.version) << ") -> "
              << (dest / "").string()
              << (global_ ? "  [global]" : "") << "\n";
    return 0;
}

int cmdUpdate(const std::string& only) {
    Manifest proj = readManifest(".");
    std::vector<LockEntry> locks = readLock(".");
    if (proj.deps.empty()) {
        std::cout << "no dependencies to update\n";
        return 0;
    }
    int rc = 0;
    for (const auto& [name, spec] : proj.deps) {
        if (!only.empty() && name != only) continue;
        PkgRef ref;
        ref.destName = name;
        if (spec.count("path")) {
            ref.kind = PkgRef::Kind::Path;
            ref.spec = spec.at("path");
        } else if (spec.count("git")) {
            ref.kind = PkgRef::Kind::Git;
            ref.spec = spec.at("git");
        } else if (spec.count("version")) {
            std::string url;
            if (!lookupRegistry(name, url)) {
                std::cerr << "coco update: '" << name
                          << "' not found in the coco-libs registry\n";
                rc = 1;
                continue;
            }
            ref.kind = PkgRef::Kind::Git;
            ref.spec = url;
        } else {
            continue;
        }
        ref.tag = spec.count("tag") ? spec.at("tag") : "";

        Manifest pkg;
        std::string sha;
        if (!materializePackage(ref, fs::path("coco_libs") / name, pkg, sha)) {
            rc = 1;
            continue;
        }
        LockEntry le;
        le.name = name;
        le.source = ref.kind == PkgRef::Kind::Path ? "path" : "git";
        le.url = ref.spec;
        le.tag = ref.tag;
        le.commit = sha;
        le.version = pkg.version;
        le.installed = todayIso();
        upsertLock(locks, le);
        std::cout << "updated " << name << " -> "
                  << (pkg.version.empty() ? "?" : pkg.version)
                  << (sha.empty() ? "" : " (" + sha + ")") << "\n";
    }
    writeLock(".", locks);
    return rc;
}

int cmdRemove(const std::string& name) {
    Manifest proj = readManifest(".");
    std::vector<LockEntry> locks = readLock(".");
    bool hadDep = proj.deps.erase(name) > 0;
    size_t before = locks.size();
    locks.erase(std::remove_if(locks.begin(), locks.end(),
                               [&](const LockEntry& l) {
                                   return l.name == name;
                               }),
                locks.end());
    std::error_code ec;
    fs::remove_all(fs::path("coco_libs") / name, ec);
    writeManifest(".", proj);
    writeLock(".", locks);
    std::cout << "removed " << name;
    if (!hadDep && locks.size() == before)
        std::cout << " (was not a recorded dependency)";
    std::cout << "\n";
    return 0;
}

int cmdList() {
    auto show = [](const char* label, const fs::path& base) {
        std::error_code ec;
        if (!fs::is_directory(base)) return;
        for (fs::directory_iterator it(base, ec), end; !ec && it != end;
             it.increment(ec)) {
            if (ec || !it->is_directory(ec)) continue;
            Manifest m = readManifest(it->path());
            std::cout << label << it->path().filename().string() << " "
                      << (m.version.empty() ? "?" : m.version);
            if (!m.description.empty())
                std::cout << "  # " << m.description;
            std::cout << "\n";
        }
    };
    show("", fs::path("coco_libs"));
    if (!globalPkgDir().empty()) show("[global] ", fs::path(globalPkgDir()));
    return 0;
}

// ---------------------------------------------------------------------------
// coco test — run *_test.co files
// ---------------------------------------------------------------------------

bool isTestFile(const fs::path& p) {
    std::string n = p.filename().string();
    const std::string suffix = "_test.co";
    return p.extension() == ".co" && n.size() > suffix.size() &&
           n.compare(n.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void collectTestFiles(const fs::path& dir, std::vector<fs::path>& out) {
    std::error_code ec;
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end;
         it.increment(ec)) {
        if (ec) break;
        const fs::path& p = it->path();
        std::string fn = p.filename().string();
        if (fn == "coco_libs" || fn == "build" || fn == ".git") {
            it.disable_recursion_pending();
            continue;
        }
        if (it->is_regular_file(ec) && isTestFile(p)) out.push_back(p);
    }
}

int cmdTest(const std::vector<std::string>& args, size_t from) {
    std::vector<fs::path> files;
    if (from == args.size() || args[from] == ".") {
        collectTestFiles(".", files);
        if (files.empty()) {
            std::cout << "no *_test.co files found\n";
            return 0;
        }
    } else {
        bool any = false;
        for (size_t i = from; i < args.size(); ++i) {
            fs::path p(args[i]);
            if (fs::is_directory(p)) {
                collectTestFiles(p, files);
                any = true;
            } else if (isTestFile(p)) {
                files.push_back(p);
                any = true;
            } else {
                std::cerr << "coco test: '" << args[i]
                          << "' is not a <name>_test.co file or directory\n";
                return 64;
            }
        }
        if (!any) return 64;
    }

    auto dirs = libDirsFor("code/main.co");   // project-root relative set
    int pass = 0, fail = 0;
    for (const auto& f : files) {
        std::cout << "test " << f.string() << " ... " << std::flush;
        int rc = runProgram(f.string(), dirs);
        if (rc == 0) {
            std::cout << "PASS\n";
            ++pass;
        } else {
            std::cout << "FAIL (rc=" << rc << ")\n";
            ++fail;
        }
    }
    std::cout << pass << " passed, " << fail << " failed\n";
    return fail == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// inline docs: '##' lines above a pub def -> docs/api.md
// ---------------------------------------------------------------------------

std::string extractApiDocs(const Manifest& m) {
    std::ostringstream o;
    o << "## API reference - " << m.name << "\n\n";
    if (!m.description.empty()) o << m.description << "\n\n";
    o << "_This section is regenerated by `coco doc " << m.name
      << "` from inline `##` doc comments._\n\n";
    std::error_code ec;
    bool anyFn = false;
    std::vector<fs::path> sources;
    for (fs::directory_iterator it("code", ec), end; !ec && it != end;
         it.increment(ec))
        if (!ec && it->is_regular_file(ec) &&
            it->path().extension() == ".co")
            sources.push_back(it->path());
    std::sort(sources.begin(), sources.end());
    for (const auto& f : sources) {
        std::string src;
        if (!readFile(f.string(), src)) continue;
        std::vector<std::string> lines;
        {
            std::istringstream in(src);
            std::string l;
            while (std::getline(in, l)) lines.push_back(l);
        }
        bool fileHeaded = false;
        std::string pendingDoc;
        for (const auto& line : lines) {
            std::string t = coco::tomlmini::strip(line);
            if (t.rfind("##", 0) == 0 && t.compare(0, 3, "###") != 0) {
                std::string d = coco::tomlmini::strip(t.substr(2));
                if (!d.empty()) pendingDoc += d + "\n";
                continue;
            }
            if (t.rfind("pub def ", 0) == 0) {
                if (!fileHeaded) {
                    o << "## " << lastSegment(f.string()) << "\n\n";
                    fileHeaded = true;
                }
                anyFn = true;
                o << "### `" << t.substr(8) << "`\n\n```co\n"
                  << t << "\n```\n\n";
                o << (pendingDoc.empty() ? "_no doc comment_\n" : pendingDoc);
                o << "\n";
                pendingDoc.clear();
                continue;
            }
            // any other statement or blank line detaches the doc block
            if (!pendingDoc.empty() &&
                (t.empty() || t.rfind("##", 0) != 0))
                pendingDoc.clear();
        }
    }
    if (!anyFn) o << "_No public functions found yet. Mark exports with "
                     "`pub def` and document them with `##` comments._\n";
    return o.str();
}

std::string extractApiDocs(const Manifest& m);

// merge the generated API section into the manifest's docs entry file
// (default docs/index.md), replacing any previous auto-generated block
bool regenerateDocs(const Manifest& m) {
    std::string entry = m.docs.empty() ? "docs/index.md" : m.docs;
    const char* beginMark = "<!-- coco-docs:start -->";
    const char* endMark = "<!-- coco-docs:end -->";
    std::string existing;
    readFile(entry, existing);
    size_t start = existing.find(beginMark);
    if (start == std::string::npos) {
        // strip a legacy auto-generated "# API reference" page content
        start = existing.size();
        existing += "\n";
    } else {
        // keep everything before the marker
    }
    size_t end = existing.find(endMark, start);
    std::string head = existing.substr(0, start);
    if (!head.empty() && head.back() != '\n') head += "\n";
    std::string tail =
        end == std::string::npos ? "" : existing.substr(end + strlen(endMark));
    std::ostringstream body;
    body << head << beginMark << "\n" << extractApiDocs(m) << endMark << "\n"
         << tail;
    return writeFile(entry, body.str());
}

// ---------------------------------------------------------------------------
// coco doc — markdown viewer over HTTP
// ---------------------------------------------------------------------------

std::string mdEscape(const std::string& s) {
    std::string o;
    for (char c : s)
        if (c == '<') o += "&lt;";
        else if (c == '>') o += "&gt;";
        else if (c == '&') o += "&amp;";
        else o += c;
    return o;
}

std::string mdInline(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '<') { o += "&lt;"; continue; }
        if (s[i] == '&') { o += "&amp;"; continue; }
        if (s[i] == '`') {
            size_t j = s.find('`', i + 1);
            if (j != std::string::npos) {
                o += "<code>" + mdEscape(s.substr(i + 1, j - i - 1)) + "</code>";
                i = j;
                continue;
            }
        }
        if (s[i] == '*' && i + 1 < s.size() && s[i + 1] == '*') {
            size_t j = s.find("**", i + 2);
            if (j != std::string::npos) {
                o += "<b>" + mdEscape(s.substr(i + 2, j - i - 2)) + "</b>";
                i = j + 1;
                continue;
            }
        }
        if (s[i] == '_' && i + 1 < s.size()) {
            size_t j = s.find('_', i + 2);
            if (j != std::string::npos && (i == 0 || !isalnum((unsigned char)s[i-1]))) {
                o += "<i>" + mdEscape(s.substr(i + 1, j - i - 1)) + "</i>";
                i = j;
                continue;
            }
        }
        o += s[i];
    }
    return o;
}

std::string mdToHtml(const std::string& md) {
    std::ostringstream o;
    bool inCode = false, inList = false;
    std::vector<std::string> lines;
    {
        std::istringstream in(md);
        std::string l;
        while (std::getline(in, l)) lines.push_back(l);
    }
    for (const auto& line : lines) {
        std::string t = coco::tomlmini::strip(line);
        if (t.rfind("```", 0) == 0) {
            if (inList) { o << "</ul>\n"; inList = false; }
            o << (inCode ? "</code></pre>\n" : "<pre><code>");
            inCode = !inCode;
            continue;
        }
        if (inCode) { o << line << "\n"; continue; }
        if (t.empty()) {
            if (inList) { o << "</ul>\n"; inList = false; }
            continue;
        }
        int h = 0;
        while (h < (int)t.size() && t[h] == '#' && h < 6) ++h;
        if (h > 0 && h < (int)t.size() && t[h] == ' ') {
            if (inList) { o << "</ul>\n"; inList = false; }
            o << "<h" << h << ">" << mdInline(t.substr(h + 1)) << "</h" << h
              << ">\n";
            continue;
        }
        if (t == "---") {
            o << "<hr>\n";
            continue;
        }
        if ((t.rfind("- ", 0) == 0) || t.rfind("* ", 0) == 0) {
            if (!inList) { o << "<ul>\n"; inList = true; }
            o << "<li>" << mdInline(t.substr(2)) << "</li>\n";
            continue;
        }
        if (inList) { o << "</ul>\n"; inList = false; }
        o << "<p>" << mdInline(t) << "</p>\n";
    }
    if (inCode) o << "</code></pre>\n";
    if (inList) o << "</ul>\n";
    return o.str();
}

std::string pageHtml(const std::string& title, const std::string& bodyHtml,
                     const std::string& nav) {
    std::ostringstream o;
    o << "<!doctype html><html><head><meta charset=\"utf-8\">"
      << "<title>" << title << " - coco docs</title><style>"
      << "body{font-family:Segoe UI,system-ui,sans-serif;margin:0;display:flex}"
      << "nav{width:230px;min-height:100vh;background:#1d2433;color:#cfd8e6;"
      << "padding:18px 14px;box-sizing:border-box}nav a{color:#7fb4ff;"
      << "display:block;margin:6px 0;text-decoration:none}nav b{color:#fff}"
      << "main{padding:26px 34px;max-width:860px}pre{background:#f4f6fa;"
      << "padding:12px;border-radius:6px;overflow:auto}code{background:#eef;"
      << "padding:1px 5px;border-radius:3px}pre code{background:none}"
      << "h1,h2{border-bottom:1px solid #eee;padding-bottom:6px}</style>"
      << "</head><body><nav><b>" << nav << "</b><br>" << "</nav>"
      << "<main>" << bodyHtml << "</main></body></html>";
    return o.str();
}

#ifdef _WIN32
int serveDocs(const fs::path& docsDir, int port, const std::string& libName,
              const std::string& entryFile) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "coco doc: WSAStartup failed\n";
        return 1;
    }
    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((u_short)port);
    int reuse = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse,
               sizeof reuse);
    if (bind(srv, (sockaddr*)&addr, sizeof addr) != 0 ||
        listen(srv, 8) != 0) {
        std::cerr << "coco doc: cannot bind port " << port << "\n";
        return 1;
    }

    // nav sidebar: home (docs entry) + every markdown file
    auto navLinks = [&]() {
        std::error_code ec;
        std::ostringstream n;
        n << libName << "<br><a href=\"/\">index</a>";
        for (fs::recursive_directory_iterator it(docsDir, ec), end;
             !ec && it != end; it.increment(ec)) {
            if (ec || it->path().extension() != ".md") continue;
            std::string rel =
                fs::relative(it->path(), docsDir, ec).generic_string();
            if (rel == entryFile) continue;   // already linked as index
            n << "<br><a href=\"/" << rel << "\">"
              << fs::path(rel).stem().string() << "</a>";
        }
        return n.str();
    };

    std::cout << "docs for '" << libName << "' at http://localhost:" << port
              << "/  (Ctrl+C to stop)\n";
    for (;;) {
        SOCKET c = accept(srv, nullptr, nullptr);
        if (c == INVALID_SOCKET) break;
        char req[2048] = {};
        recv(c, req, sizeof req - 1, 0);
        std::string method, pathStr;
        {
            std::istringstream rl(req);
            rl >> method >> pathStr;
        }
        std::string body;
        if (method != "GET") {
            body = "405";
        } else {
            // "/" renders the manifest-declared docs entry (index.md)
            std::string p = pathStr == "/" ? "/" + entryFile : pathStr;
            for (auto& ch : p)
                if (ch == '?') { p.resize(&ch - &p[0]); break; }
            std::string clean = p.substr(1);
            for (auto& ch : clean) if (ch == '\\') ch = '/';
            if (clean.find("..") != std::string::npos) {
                body = "403 forbidden";
            } else {
                fs::path file = docsDir / clean;
                std::string md;
                if (readFile(file.string(), md))
                    body = pageHtml(clean, mdToHtml(md), navLinks());
                else
                    body = "404 not found";
            }
        }
        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
             << "Content-Length: " << body.size()
             << "\r\nConnection: close\r\n\r\n"
             << body;
        std::string out = resp.str();
        send(c, out.data(), (int)out.size(), 0);
        closesocket(c);
    }
    WSACleanup();
    return 0;
}
#else
int serveDocs(const fs::path&, int, const std::string&,
              const std::string&) {
    std::cerr << "coco doc: http server not supported on this platform\n";
    return 1;
}
#endif

int cmdDoc(const std::string& target, int port) {
    // resolve target: dir | installed name
    fs::path libDir(target);
    if (!fs::is_directory(libDir)) {
        fs::path local = fs::path("coco_libs") / target;
        fs::path glob;
        if (!globalPkgDir().empty())
            glob = fs::path(globalPkgDir()) / target;
        if (fs::is_directory(local))
            libDir = local;
        else if (!glob.empty() && fs::is_directory(glob))
            libDir = glob;
        else {
            std::cerr << "coco doc: no project or installed library '"
                      << target << "'\n";
            return 1;
        }
    }
    // serve/generate with absolute paths (server loop changes no cwd)
    std::error_code lec;
    libDir = fs::weakly_canonical(fs::absolute(libDir), lec);

    Manifest m = readManifest(libDir);
    if (m.name.empty()) m.name = lastSegment(libDir.string());

    // regenerate the API section inside the manifest-declared docs entry
    std::string entryRel = m.docs.empty() ? "docs/index.md" : m.docs;
    for (char& c : entryRel)
        if (c == '\\') c = '/';
    {
        std::string oldPwd = fs::current_path().string();
        std::error_code ec;
        fs::current_path(libDir, ec);
        regenerateDocs(m);
        fs::current_path(oldPwd, ec);
    }
    fs::path entryPath = libDir / fs::path(entryRel);
    std::cout << "docs entry: " << entryPath.string() << "\n";
    return serveDocs(entryPath.parent_path(), port, m.name,
                     entryPath.filename().string());
}

// ---------------------------------------------------------------------------
// coco build — app: standalone .exe (sources embedded, interpreter linked)
//              lib: type-check all sources + pack distributable .cocolib
// ---------------------------------------------------------------------------

// resolve a module name to a source file across the given dirs (loader rules)
bool resolveSource(const std::string& dotted,
                   const std::vector<std::string>& dirs, std::string& path,
                   std::string& src) {
    std::string rel;
    for (char c : dotted) rel += (c == '.' || c == '/') ? '/' : c;
    rel += ".co";
    for (const auto& d : dirs) {
        std::string cand = d + "/" + rel;
        if (readFile(cand, src)) {
            path = cand;
            return true;
        }
        std::string pkgDir = d + "/" + rel.substr(0, rel.size() - 3);
        if (!pkgDir.empty() && fs::is_directory(pkgDir)) {
            // package entry: coco.toml main / mod.co / <dir>.co / lone *.co
            Manifest pm = readManifest(pkgDir);
            std::vector<std::string> cands;
            if (!pm.main.empty()) cands.push_back(pm.main);
            cands.push_back("mod.co");
            cands.push_back(lastSegment(trimSlashes(pkgDir)) + ".co");
            for (const auto& rel2 : cands)
                if (readFile((fs::path(pkgDir) / rel2).string(), src)) {
                    path = (fs::path(pkgDir) / rel2).string();
                    return true;
                }
            std::error_code ec;
            std::vector<fs::path> tops;
            for (fs::directory_iterator it(pkgDir, ec), end; !ec && it != end;
                 it.increment(ec))
                if (!ec && it->is_regular_file(ec) &&
                    it->path().extension() == ".co")
                    tops.push_back(it->path());
            if (tops.size() == 1 && readFile(tops[0].string(), src)) {
                path = tops[0].string();
                return true;
            }
        }
    }
    return false;
}

void collectImports(const std::string& path, const std::string& src,
                    std::vector<std::string>& names) {
    coco::DiagEngine diags;
    auto toks = coco::Lexer(src, path, diags).lexAll();
    if (diags.count()) return;
    auto body = coco::Parser(toks, diags).parseProgram();
    for (const auto& s : body)
        if (s->kind == coco::ast::StKind::Import &&
            !s->fromImport && !s->moduleName.empty())
            names.push_back(s->moduleName);
}

std::string cppRawLiteral(const std::string& s) {
    return "R\"COCO(" + s + ")COCO\"";
}

// match the runtime the prebuilt coco_interp.lib was compiled with
// (read CMAKE_BUILD_TYPE from the build tree next to our executable;
//  CMake's default MSVC runtime is /MD release, /MDd debug)
std::string detectRuntimeFlags(const std::string& binRoot) {
    std::string cache;
    if (!readFile(binRoot + "/CMakeCache.txt", cache)) return "/MD /O2";
    std::istringstream in(cache);
    std::string line;
    while (std::getline(in, line))
        if (line.rfind("CMAKE_BUILD_TYPE:", 0) == 0) {
            std::string v = line.substr(line.find('=') + 1);
            for (char& c : v) c = (char)tolower((unsigned char)c);
            if (v.find("debug") != std::string::npos) return "/MDd /Od";
            return "/MD /O2";
        }
    return "/MD /O2";
}

int buildApp(const Manifest& m) {
    std::string entry = m.main.empty() ? "code/main.co" : m.main;
    std::string mainSrc;
    if (!readFile(entry, mainSrc)) {
        std::cerr << "coco build: entry '" << entry << "' not found\n";
        return 1;
    }

    // static check first
    {
        coco::DiagEngine diags;
        frontEnd(entry, mainSrc, diags);
        if (diags.count()) {
            printDiags(entry, diags);
            return 65;
        }
    }

    // gather transitive import sources to embed
    std::vector<std::string> dirs;
    dirs.push_back("coco_libs");
    if (!globalPkgDir().empty()) dirs.push_back(globalPkgDir());
    dirs.push_back("../stdlib");
    dirs.push_back("../../stdlib");

    std::map<std::string, std::string> embedded;
    std::set<std::string> seen;
    std::vector<std::pair<std::string, std::string>> queue;
    {
        std::vector<std::string> names;
        collectImports(entry, mainSrc, names);
        for (const auto& n : names) queue.push_back({n, ""});
    }
    seen.insert("main");
    while (!queue.empty()) {
        auto [name, _] = queue.back();
        queue.pop_back();
        std::string key;
        for (char c : name) key += (c == '.' || c == '/') ? '/' : c;
        if (seen.count(key)) continue;
        seen.insert(key);
        std::string p, s;
        if (!resolveSource(name, dirs, p, s)) continue;   // native stub etc.
        embedded[key] = s;
        std::vector<std::string> names;
        collectImports(p, s, names);
        for (const auto& n : names) queue.push_back({n, ""});
    }

    // emit launcher .cpp embedding every reachable source
    std::ostringstream o;
    o << "// generated by coco build - standalone Coco program\n";
    o << "#include \"interp/runtime.h\"\n"
      << "#include \"lex/lexer.h\"\n"
      << "#include \"parser/parser.h\"\n"
      << "#include \"sema/checker.h\"\n\n"
      << "#include <cstdio>\n#include <fstream>\n#include <iostream>\n"
      << "#include <sstream>\n\n";
    o << "static const char* kMainSrc = " << cppRawLiteral(mainSrc) << ";\n";
    for (const auto& [key, src] : embedded)
        o << "static const char* kMod_" << key << " = "
          << cppRawLiteral(src) << ";\n";
    o << "\nstruct Embed { const char* key; const char* src; };\n"
      << "static const Embed kEmbed[] = {\n";
    for (const auto& [key, src] : embedded)
        o << "    {\"" << key << "\", kMod_" << key << "},\n";
    o << "};\n\n"
      << "int main() {\n"
      << "    coco::DiagEngine diags;\n"
      << "    auto toks = coco::Lexer(kMainSrc, \"main.co\", diags).lexAll();\n"
      << "    if (diags.count()) { std::cerr << \"embedded source error\\n\"; "
         "return 65; }\n"
      << "    auto body = coco::Parser(toks, diags).parseProgram();\n"
      << "    if (diags.count()) { return 65; }\n"
      << "    { coco::sema::Checker chk(diags); chk.checkModule(body); }\n"
      << "    if (diags.count()) { return 65; }\n"
      << "    coco::ast::Stmt root;\n"
      << "    root.kind = coco::ast::StKind::Pass;\n"
      << "    root.body = std::move(body);\n"
      << "    try {\n"
      << "        coco::interp::Interpreter interp(root);\n"
      << "        for (const auto& e : kEmbed) "
         "interp.addEmbeddedSource(e.key, e.src);\n"
      << "        auto r = interp.run();\n"
      << "        return r.k == coco::interp::VK::Int ? (int)r.i : 0;\n"
      << "    } catch (const coco::interp::PanicSignal& p) {\n"
      << "        fflush(stdout);\n"
      << "        fputs((\"panic: \" + p.msg + \"\\n\").c_str(), stderr);\n"
      << "        return 70;\n"
      << "    } catch (const coco::interp::SignalRaise&) {\n"
      << "        fputs(\"panic: uncaught raise\\n\", stderr);\n"
      << "        return 70;\n"
      << "    }\n"
      << "}\n";

    writeFile("build/" + m.name + ".cpp", o.str());

    // compile with the same toolchain cmake uses
    const char* cl = std::getenv("COCO_CL");
    std::string clPath =
        cl ? cl
           : "C:/msvc/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/cl.exe";
    char exeDirBuf[MAX_PATH * 2];
    GetModuleFileNameA(nullptr, exeDirBuf, sizeof exeDirBuf);
    std::string exePath(exeDirBuf);
    size_t ds = exePath.find_last_of("/\\");
    std::string binRoot = ds == std::string::npos ? "." : exePath.substr(0, ds);

    // NOTE: must not start with '"' — cmd /c strips leading quotes.
    // NOTE: no quoted paths ending in '\' (argv would eat the quote).
    std::string flags = detectRuntimeFlags(binRoot);
    std::string cmd = "cd . && \"" + clPath + "\" /nologo /EHsc " + flags +
                      " /std:c++20" +
                      " /I\"" + binRoot + "\\..\\src\""
                      " /Fobuild\\ build/" + m.name + ".cpp" +
                      " /Fe:build/" + m.name +
                      ".exe /link /LIBPATH:\"" + binRoot +
                      "\" coco_interp.lib coco_sema.lib coco_parser.lib"
                      " coco_ast.lib coco_lex.lib";
    if (std::getenv("COCO_VERBOSE")) std::cerr << "[cmd] " << cmd << "\n";
    std::cout << "compiling build/" << m.name << ".exe ...\n";
    int rc = std::system(cmd.c_str());
    if (rc != 0) {
        std::cerr << "coco build: compilation failed (" << rc << ")\n";
        return rc == 0 ? 1 : rc;
    }
    std::cout << "built build/" << m.name << ".exe\n";
    return 0;
}

int packLib(const Manifest& m) {
    // type-check every source under code/
    int bad = 0;
    std::error_code ec;
    for (fs::recursive_directory_iterator it("code", ec), end; !ec && it != end;
         it.increment(ec)) {
        if (ec || !it->is_regular_file(ec) ||
            it->path().extension() != ".co")
            continue;
        std::string src;
        readFile(it->path().string(), src);
        coco::DiagEngine diags;
        frontEnd(it->path().string(), src, diags);
        if (diags.count()) {
            printDiags(it->path().string(), diags);
            ++bad;
        }
    }
    if (bad) {
        std::cerr << "coco build lib: " << bad << " file(s) failed checks\n";
        return 1;
    }
    regenerateDocs(m);

    // pack manifest + code/ + docs/ into a single distributable
    const std::string outPath =
        "build/" + m.name + "-" + m.version + ".cocolib";
    std::ostringstream o;
    o << "COCOLIB/1\n";
    std::vector<fs::path> files;
    files.push_back(fs::path("coco.toml"));
    for (fs::recursive_directory_iterator it("code", ec), end; !ec && it != end;
         it.increment(ec))
        if (!ec && it->is_regular_file(ec)) files.push_back(it->path());
    for (fs::recursive_directory_iterator it("docs", ec), end; !ec && it != end;
         it.increment(ec))
        if (!ec && it->is_regular_file(ec)) files.push_back(it->path());
    if (fs::is_regular_file("README.md")) files.push_back("README.md");
    if (fs::is_regular_file("LICENSE")) files.push_back("LICENSE");
    for (const auto& f : files) {
        std::string content;
        if (!readFile(f.string(), content)) continue;
        o << "@@FILE " << f.generic_string() << "\n" << content << "@@END\n";
    }
    writeFile(outPath, o.str());
    std::cout << "packed " << outPath << " (" << files.size()
              << " files) - ready to publish\n";
    return 0;
}

int cmdBuild(const std::vector<std::string>& args, size_t from) {
    bool wantLib = from < args.size() && args[from] == "lib";
    if (wantLib && from + 1 < args.size()) return 64;
    Manifest m = readManifest(".");
    if (m.name.empty()) {
        std::cerr << "coco build: no coco.toml in this directory\n";
        return 1;
    }
    if (wantLib || m.type == "lib") {
        if (!fs::is_directory("code")) {
            std::cerr << "coco build lib: no code/ directory\n";
            return 1;
        }
        return packLib(m);
    }
    return buildApp(m);
}

int unpackCocolib(const std::string& raw, bool global_) {
    std::string text;
    if (!readFile(raw, text)) {
        std::cerr << "coco install: cannot read bundle '" << raw << "'\n";
        return 1;
    }
    std::istringstream in(text);
    std::string header;
    std::getline(in, header);
    if (header.rfind("COCOLIB/", 0) != 0) {
        std::cerr << "coco install: not a valid .cocolib bundle\n";
        return 1;
    }

    // collect sections first so we can learn the package name
    struct Section { std::string path, body; };
    std::vector<Section> sections;
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("@@FILE ", 0) == 0) {
            sections.push_back(Section{line.substr(7), ""});
        } else if (line == "@@END") {
            // section closed
        } else if (!sections.empty()) {
            sections.back().body += line + "\n";
        }
    }
    std::string name;
    for (const auto& s : sections)
        if (s.path == "coco.toml") {
            Doc d = coco::tomlmini::parse(s.body);
            name = coco::tomlmini::get(d, "package", "name");
        }
    if (name.empty()) {
        // greet-0.1.0.cocolib -> greet
        std::string stem = fs::path(raw).stem().string();
        size_t dash = stem.find('-');
        name = dash == std::string::npos ? stem : stem.substr(0, dash);
    }
    if (name.empty()) {
        std::cerr << "coco install: cannot determine package name\n";
        return 1;
    }

    fs::path base = global_ ? fs::path(globalPkgDir()) : fs::path("coco_libs");
    for (const auto& s : sections)
        writeFile(fs::path(base) / name / fs::path(s.path).generic_string(),
                  s.body);
    std::cout << "installed " << raw << " -> "
              << (base / name / "").string() << " (" << sections.size()
              << " files)\n";
    return 0;
}

// ---------------------------------------------------------------------------

void usage() {
    std::cout
        << "coco - the Coco language driver\n\n"
        << "usage:\n"
        << "  coco run [dir|file]              run a program or project\n"
        << "  coco new <name>                  scaffold an application\n"
        << "  coco new lib <name>              scaffold a library package\n"
        << "  coco test [.|file|dir ...]       run *_test.co files\n"
        << "  coco install|i [-g] <pkg>        install into ./coco_libs\n"
        << "      pkg := [github.com/]user/repo[@tag] | <path> | <registry"
               "-name> | file.cocolib\n"
        << "      -g installs globally into ~/.coco/coco-pkg\n"
        << "  coco update [name]               refresh installed deps\n"
        << "  coco remove <name>               uninstall a dependency\n"
        << "  coco build                       compile project -> "
               "build/<name>.exe\n"
        << "  coco build lib                   check + pack -> "
               "build/<n>-<v>.cocolib\n"
        << "  coco doc <lib|dir> [--port N]    serve markdown docs + API ref\n"
        << "  coco list                        list installed libraries\n";
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + (argc > 0 ? 1 : 0), argv + argc);
    if (args.empty()) {
        usage();
        return 64;
    }
    const std::string& cmd = args[0];
    auto rest = [&](size_t i) {
        return i < args.size() ? args[i] : std::string();
    };

    if ((cmd == "run" || cmd == "r") && args.size() <= 2) {
        std::string target = args.size() == 2 ? args[1] : ".";
        fs::path file(target);
        if (fs::is_directory(file)) {
            Manifest m = readManifest(file);
            file /= m.main.empty() ? fs::path("code/main.co") : fs::path(m.main);
        }
        // project root is cwd (or the dir containing the entry)
        std::string scriptDir = fs::is_directory(target)
                                    ? target
                                    : file.parent_path().string();
        return runProgram(file.string(), libDirsFor(scriptDir));
    }
    if (cmd == "new" && args.size() >= 2) {
        bool lib = args[1] == "lib";
        size_t nameIdx = lib ? 2 : 1;
        if ((lib && args.size() != 3) || (!lib && args.size() != 2)) {
            usage();
            return 64;
        }
        return cmdNew(rest(nameIdx), lib);
    }
    if (cmd == "test" && args.size() >= 1) return cmdTest(args, 1);
    if (cmd == "install" || cmd == "i") {
        bool global_ = false;
        std::string pkg;
        for (size_t i = 1; i < args.size(); ++i) {
            if (args[i] == "-g" || args[i] == "--global") global_ = true;
            else pkg = args[i];
        }
        if (pkg.empty()) {
            usage();
            return 64;
        }
        if (fs::is_regular_file(pkg) &&
            fs::path(pkg).extension() == ".cocolib")
            return unpackCocolib(pkg, global_);
        return cmdInstall(pkg, global_);
    }
    if (cmd == "update" && args.size() <= 2)
        return cmdUpdate(args.size() == 2 ? args[1] : "");
    if (cmd == "remove" && args.size() == 2) return cmdRemove(args[1]);
    if (cmd == "list" && args.size() == 1) return cmdList();
    if (cmd == "build" && args.size() <= 2) return cmdBuild(args, 1);
    if (cmd == "doc" && (args.size() == 2 ||
                         (args.size() == 4 && args[2] == "--port"))) {
        int port = args.size() == 4 ? atoi(args[3].c_str()) : 8080;
        return cmdDoc(args[1], port);
    }
    usage();
    return cmd == "help" || cmd == "--help" || cmd == "-h" ? 0 : 64;
}
