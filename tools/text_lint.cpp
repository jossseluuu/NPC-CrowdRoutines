// text_lint: rejects the kinds of corruption found in this repository in November 2026
// (files rewritten through PowerShell interpolated strings / wrong code pages).
//
// Checks every file given on the command line (or every tracked text file read from stdin with --stdin):
//   * valid UTF-8, no BOM, no U+FFFD replacement characters
//   * no stray control characters (tab/LF allowed; CR only flagged, see below)
//   * no CRLF line endings (the repository stores LF; .gitattributes converts on checkout)
//   * no escaped quotes  \"  in HTML/CSS/JS markup or C++ code outside string literals
// Exit code 0 = clean, 1 = problems found.

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Issue {
    std::string file;
    std::size_t line;
    std::string what;
};

bool has_ext(const std::string& path, std::initializer_list<const char*> exts) {
    for (const char* e : exts) {
        const std::string ext = e;
        if (path.size() >= ext.size() && path.compare(path.size() - ext.size(), ext.size(), ext) == 0) return true;
    }
    return false;
}

bool is_text_file(const std::string& path) {
    return has_ext(path, {".cpp", ".hpp", ".h", ".c", ".txt", ".md", ".json", ".html", ".css", ".js", ".yml", ".yaml",
                          ".cmake", ".gitignore", ".gitattributes", ".sh", ".toml", ".csv"}) ||
           path.find("CMakeLists.txt") != std::string::npos || path.find("pre-commit") != std::string::npos;
}

/// Returns the byte length of the UTF-8 sequence at s[i], or 0 if invalid.
std::size_t utf8_len(const std::string& s, std::size_t i, std::uint32_t& cp) {
    const auto c = static_cast<unsigned char>(s[i]);
    std::size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
    if (n == 0 || i + n > s.size()) return 0;
    cp = n == 1 ? c : n == 2 ? (c & 0x1F) : n == 3 ? (c & 0x0F) : (c & 0x07);
    for (std::size_t k = 1; k < n; ++k) {
        const auto cc = static_cast<unsigned char>(s[i + k]);
        if ((cc >> 6) != 0x2) return 0;
        cp = (cp << 6) | (cc & 0x3F);
    }
    if ((n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000) || cp > 0x10FFFF) return 0;
    return n;
}

void check_file(const std::string& path, std::vector<Issue>& issues) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return; // deleted files in a pre-commit list are fine
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string s = ss.str();

    if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF && static_cast<unsigned char>(s[1]) == 0xBB &&
        static_cast<unsigned char>(s[2]) == 0xBF)
        issues.push_back({path, 1, "UTF-8 BOM (save as UTF-8 without BOM)"});

    const bool markup = has_ext(path, {".html", ".css", ".js"});
    const bool cxx = has_ext(path, {".cpp", ".hpp", ".h"});
    std::size_t line = 1;
    bool reported_crlf = false;
    enum class Lex { Code, String, Char, Raw, LineComment, BlockComment } state = Lex::Code;
    std::string raw_end;
    bool escape_pending = false;
    std::size_t skip_bytes = 0;
    for (std::size_t i = 0; i < s.size();) {
        std::uint32_t cp = 0;
        const std::size_t n = utf8_len(s, i, cp);
        if (n == 0) {
            issues.push_back({path, line, "invalid UTF-8 byte (file saved in a legacy code page?)"});
            return;
        }
        if (cp == 0xFFFD) issues.push_back({path, line, "U+FFFD replacement character (lost accent)"});
        if (cp == '\r') {
            if (i + 1 < s.size() && s[i + 1] == '\n') {
                if (!reported_crlf) issues.push_back({path, line, "CRLF line endings"});
                reported_crlf = true;
            } else {
                issues.push_back({path, line, "bare carriage return"});
            }
        } else if (cp < 0x20 && cp != '\n' && cp != '\t') {
            issues.push_back({path, line, "control character U+00" + std::string(1, "0123456789ABCDEF"[cp >> 4]) +
                                               std::string(1, "0123456789ABCDEF"[cp & 15]) + " (PowerShell backtick escape?)"});
        }
        if (cxx) {
            // Small lexer: code / "string" / 'char' / R"d(raw)d" / line comment / block comment.
            const char c = s[i];
            const char next = i + 1 < s.size() ? s[i + 1] : '\0';
            switch (state) {
                case Lex::Code:
                    if (c == '/' && next == '/') state = Lex::LineComment;
                    else if (c == '/' && next == '*') state = Lex::BlockComment;
                    else if (c == 'R' && next == '"' && (i == 0 || !std::isalnum(static_cast<unsigned char>(s[i - 1])))) {
                        const auto open = s.find('(', i + 2);
                        raw_end = ")" + s.substr(i + 2, open == std::string::npos ? 0 : open - i - 2) + "\"";
                        state = Lex::Raw;
                        i = open == std::string::npos ? i : open;
                    } else if (c == '"') state = Lex::String;
                    else if (c == '\'') state = Lex::Char;
                    else if (c == '\\' && next == '"')
                        issues.push_back({path, line, "escaped quote \\\" outside a string literal"});
                    break;
                case Lex::String:
                case Lex::Char:
                    if (c == '\\') escape_pending = true; // the next character is escaped
                    else if (c == (state == Lex::String ? '"' : '\'') || c == '\n') state = Lex::Code;
                    break;
                case Lex::Raw:
                    if (s.compare(i, raw_end.size(), raw_end) == 0) { skip_bytes = raw_end.size() - 1; state = Lex::Code; }
                    break;
                case Lex::LineComment:
                    if (c == '\n') state = Lex::Code;
                    break;
                case Lex::BlockComment:
                    if (c == '*' && next == '/') { skip_bytes = 1; state = Lex::Code; }
                    break;
            }
            if (c == '\n') ++line;
            i += n;
            // Consume escaped / closing characters without re-lexing them (counting newlines on the way).
            if (escape_pending && (state == Lex::String || state == Lex::Char) && c == '\\') {
                escape_pending = false;
                if (i < s.size()) {
                    std::uint32_t tmp = 0;
                    const std::size_t m = utf8_len(s, i, tmp);
                    if (s[i] == '\n') ++line;
                    i += m ? m : 1;
                }
            }
            for (; skip_bytes > 0 && i < s.size(); --skip_bytes, ++i)
                if (s[i] == '\n') ++line;
            continue;
        }
        if (markup && s[i] == '=' && i + 2 < s.size() && s[i + 1] == '\\' && s[i + 2] == '"')
            issues.push_back({path, line, "escaped attribute quote =\\\" in markup"});
        if (s[i] == '\n') ++line;
        i += n;
    }
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> files;
    bool from_stdin = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--stdin") from_stdin = true;
        else files.push_back(a);
    }
    if (from_stdin) {
        std::string f;
        while (std::getline(std::cin, f))
            if (!f.empty()) files.push_back(f);
    }
    std::vector<Issue> issues;
    std::size_t checked = 0;
    for (const auto& f : files) {
        if (!is_text_file(f)) continue;
        ++checked;
        check_file(f, issues);
    }
    for (const auto& is : issues) std::printf("%s:%zu: %s\n", is.file.c_str(), is.line, is.what.c_str());
    std::printf("text_lint: %zu files checked, %zu problem(s)\n", checked, issues.size());
    return issues.empty() ? 0 : 1;
}
