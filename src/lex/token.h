#pragma once
// Token model for the Coco lexer (grammar/coco.ebnf §1).
#include <cstdint>
#include <string>
#include <string_view>

namespace coco {

enum class Tok : uint8_t {
    Eof,
    Newline,
    Indent,
    Dedent,
    Ident,
    Int,
    Float,
    Char,
    StrNormal,
    StrRaw,
    StrByte,
    StrC,
    FStringStart,
    FStringText,
    FStringLBrace,
    FStringRBrace,
    FStringColon,
    FStringSpec,
    FStringEnd,
    Op,
    Punct,
};

const char* tokName(Tok t);

struct Token {
    Tok kind = Tok::Eof;
    uint32_t line = 1;
    uint32_t col = 1;
    std::string text;

    bool is(Tok k) const { return kind == k; }
};

// Frozen v1 keyword set (grammar §1 KEYWORD). Identifiers are looked up
// against this by the parser; the lexer deliberately emits plain Ident.
bool isKeyword(std::string_view ident);

} // namespace coco
