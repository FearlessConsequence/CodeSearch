#include <set>
#include "FileSearcher.h"
#include <cctype>
#include "RegexBuilder.h"
#include "Colors.h"
#include <regex>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <algorithm>
#include "Utf8.h"
#include <iostream>
#include <atomic>

namespace file_searcher {

    using utf8::ToUtf8;

    static bool MatchesMask(const std::string& fileName, const std::string& mask) {
        auto lower = [](std::string s) { for (auto& c : s) c = std::tolower((unsigned char)c); return s; };
        std::string fn = lower(fileName), m = lower(mask);
        if (m.empty()) return false;
        bool leftStar  = m.front() == '*';
        bool rightStar = m.back()  == '*';
        std::string core = m;
        if (leftStar)  core.erase(core.begin());
        if (rightStar && !core.empty()) core.pop_back();
        if (leftStar && rightStar) return fn.find(core) != std::string::npos;
        if (leftStar)  return fn.size() >= core.size() &&
                            fn.compare(fn.size() - core.size(), core.size(), core) == 0;
        if (rightStar) return fn.compare(0, core.size(), core) == 0;
        return fn == core;
    }

    static bool InVector(const std::vector<std::string>& vec, const std::string& item) {
        for (const auto& v : vec) {
            if (v.size() == item.size()) {
                bool eq = true;
                for (size_t i = 0; i < v.size(); ++i)
                    if (std::tolower((unsigned char)v[i]) != std::tolower((unsigned char)item[i])) { eq = false; break; }
                if (eq) return true;
            }
        }
        return false;
    }

    static std::vector<fs::path> EnumerateFiles(const SearchOptions& opt, size_t& skippedCount) {
        std::vector<fs::path> result;
        try {
            for (auto it = fs::recursive_directory_iterator(opt.Directory, fs::directory_options::skip_permission_denied);
                 it != fs::recursive_directory_iterator(); ++it) {
                const auto& entry = *it;
                if (entry.is_directory()) {
                    std::string name = ToUtf8(entry.path().filename());
                    if (InVector(opt.ExcludeDirs, name)) { it.disable_recursion_pending(); continue; }
                    continue;
                }
                if (!entry.is_regular_file()) continue;
                std::string fileName = ToUtf8(entry.path().filename());
                bool excluded = false;
                for (const auto& mask : opt.ExcludeFiles) if (MatchesMask(fileName, mask)) { excluded = true; break; }
                if (excluded) continue;
                if (!opt.Extensions.empty()) {
                    std::string ext = ToUtf8(entry.path().extension());
                    if (!InVector(opt.Extensions, ext)) continue;
                }
                try {
                    if (opt.MaxFileSizeMb >= 0) {
                        auto bytes = fs::file_size(entry.path());
                        if (bytes > (uintmax_t)opt.MaxFileSizeMb * 1024 * 1024) continue;
                    }
                } catch (...) { skippedCount++; continue; }
                result.push_back(entry.path());
            }
        } catch (...) { skippedCount++; }
        return result;
    }

    static std::string ReadFile(const fs::path& file) {
        std::ifstream in(file, std::ios::binary);
        if (!in) return "";
        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }

    struct LineTracker {
        const std::string& content;
        size_t pos = 0;
        int line = 1;
        int At(size_t target) {
            if (target < pos) { pos = 0; line = 1; }
            for (size_t i = pos; i < target; ++i) if (content[i] == '\n') line++;
            pos = target;
            return line;
        }
    };

    static std::string TrimmedLineAt(const std::string& content, size_t pos, size_t maxLen = 80) {
        size_t lineStart = content.rfind('\n', pos);
        lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
        size_t lineEnd = content.find('\n', pos);
        if (lineEnd == std::string::npos) lineEnd = content.size();
        std::string text = content.substr(lineStart, lineEnd - lineStart);
        size_t s = text.find_first_not_of(" \t\r");
        size_t e = text.find_last_not_of(" \t\r");
        if (s == std::string::npos) return "";
        text = text.substr(s, e - s + 1);
        if (text.size() > maxLen) {
            size_t rel = (pos >= lineStart + s) ? (pos - lineStart - s) : 0;
            size_t st = (rel > maxLen / 2) ? rel - maxLen / 2 : 0;
            if (st + maxLen > text.size()) st = text.size() - maxLen;
            st = utf8::SafeCut(text, st);
            size_t en = utf8::SafeCut(text, st + maxLen);
            text = "..." + text.substr(st, en - st) + "...";
        }
        return text;
    }

    static void SearchLineByLine(const fs::path& file, const std::regex& regex, std::vector<SearchResult>& results) {
        std::ifstream in(file, std::ios::binary);
        if (!in) return;
        std::string line;
        int lineNum = 0;
        while (std::getline(in, line)) {
            lineNum++;
            if (!line.empty() && line.back() == '\r') line.pop_back();

            // Осознанно: одна строка = одна запись в выводе (см. README, «Ограничения»).
            // Считаем все совпадения, но пушим одно — со счётчиком.
            int count = 0;
            size_t firstPos = std::string::npos;
            for (auto it = std::sregex_iterator(line.begin(), line.end(), regex);
                it != std::sregex_iterator(); ++it) {
                if (count == 0) firstPos = (size_t)it->position();
                ++count;
            }
            if (count == 0) continue;

            size_t start = line.find_first_not_of(" \t");
            size_t end = line.find_last_not_of(" \t");
            std::string trimmed = (start == std::string::npos) ? "" : line.substr(start, end - start + 1);
            if (trimmed.size() > 80) {
                size_t matchRel = (start != std::string::npos && firstPos >= start) ? firstPos - start : 0;
                size_t st = (matchRel > 40) ? matchRel - 40 : 0;
                if (st + 80 > trimmed.size()) st = trimmed.size() - 80;
                st = utf8::SafeCut(trimmed, st);
                size_t en = utf8::SafeCut(trimmed, st + 80);
                trimmed = "..." + trimmed.substr(st, en - st) + "...";
            }
            results.push_back({file, lineNum, trimmed, "", count});
        }
    }
    
    static void SearchWholeFile(const fs::path& file, const std::regex& rx, std::vector<SearchResult>& results) {
        std::string content = ReadFile(file);
        if (content.empty()) return;
        LineTracker lines{content};
        for (auto it = std::sregex_iterator(content.begin(), content.end(), rx); it != std::sregex_iterator(); ++it) {
            size_t pos = it->position();
            int line = lines.At(pos);
            std::string text = TrimmedLineAt(content, pos);
            results.push_back({file, line, text, ""});
        }
    }

    static void SearchStructureXYZ(const fs::path& file, const std::regex& rx, std::vector<SearchResult>& results) {
        std::string content = ReadFile(file);
        if (content.empty()) return;
        LineTracker lines{content};
        for (auto it = std::sregex_iterator(content.begin(), content.end(), rx); it != std::sregex_iterator(); ++it) {
            const auto& m = *it;
            size_t pos = (m.size() > 1 && m[1].matched) ? m.position(1) : m.position();
            int line = lines.At(pos);
            std::string text = TrimmedLineAt(content, pos);
            results.push_back({file, line, text, ""});
        }
    }

    static void SearchXButNotY(const fs::path& file, const std::regex& rxX, const std::regex& rxY, std::vector<SearchResult>& results) {
        std::string content = ReadFile(file);
        if (content.empty()) return;

        std::vector<std::pair<size_t, size_t>> yRanges;
        for (auto it = std::sregex_iterator(content.begin(), content.end(), rxY); it != std::sregex_iterator(); ++it) {
            yRanges.push_back({(size_t)it->position(), (size_t)it->position() + it->length()});
        }

        LineTracker lines{content};
        for (auto it = std::sregex_iterator(content.begin(), content.end(), rxX); it != std::sregex_iterator(); ++it) {
            size_t xs = it->position(), xe = xs + it->length();
            bool intersects = false;
            for (auto& yr : yRanges) if (xs < yr.second && xe > yr.first) { intersects = true; break; }
            if (intersects) continue;
            int line = lines.At(xs);
            results.push_back({file, line, TrimmedLineAt(content, xs), ""});
        }
    }

    static bool IsKeywordBeforeParen(const std::string& w) {
        static const char* kws[] = {
            "if","while","for","switch","catch","using","sizeof","alignof",
            "return","throw","new","delete","typename","static_assert",
            "decltype","co_await","co_return","co_yield","requires","noexcept",
            "constexpr","consteval","constinit","typeid",
            "foreach","lock","nameof","typeof","as","is","when","fixed",
            "checked","unchecked","in","do","else","case","operator"
        };
        for (auto k : kws) if (w == k) return true;
        return false;
    }

    static bool IsWordChar(unsigned char c) {
        return std::isalnum(c) || c == '_';
    }
        
    static bool HasTypeWordBefore(const std::string& content, size_t idStart) {
        size_t p = idStart;
        while (p > 0 && std::isspace((unsigned char)content[p - 1])) --p;
        if (p == 0) return false;

        if (content[p - 1] == '>') {
            // Защита от оператора -> (стрелка), чтобы p->foo() не считалось объявлением
            if (p >= 2 && content[p - 2] == '-') return false;
            return true;  // generic: Foo<int> foo(...)
        }

        if (!IsWordChar((unsigned char)content[p - 1])) return false;
        
        size_t wordEnd = p;
        while (p > 0 && IsWordChar((unsigned char)content[p - 1])) --p;
        std::string word = content.substr(p, wordEnd - p);
        if (word.empty()) return false;
        if (std::isdigit((unsigned char)word[0])) return false;
        if (IsKeywordBeforeParen(word)) return false;
        return true;
    }

    static bool IsCallContextBefore(const std::string& content, size_t idStart) {
        size_t p = idStart;
        while (p > 0 && std::isspace((unsigned char)content[p - 1])) --p;
        if (p == 0) return false;
        char c = content[p - 1];
        static const char* callOps = "=.(,;:?+-*/%&|^~!<";
        bool isOp = false;
        for (const char* q = callOps; *q; ++q) if (*q == c) { isOp = true; break; }
        if (isOp) return true;
        size_t wordEnd = p;
        while (p > 0 && IsWordChar((unsigned char)content[p - 1])) --p;
        if (p < wordEnd) {
            std::string word = content.substr(p, wordEnd - p);
            if (word == "new" || word == "return" || word == "throw" ||
                word == "await" || word == "in" || word == "as" || word == "is" ||
                word == "case" || word == "when" || word == "yield" ||
                word == "co_return" || word == "co_yield" || word == "co_await") {
                return true;
            }
        }
        return false;
    }

    static char CharAfterSpaces(const std::string& content, size_t pos) {
        size_t p = pos + 1;
        while (p < content.size() && std::isspace((unsigned char)content[p])) ++p;
        return (p < content.size()) ? content[p] : 0;
    }

    static int CountArguments(const std::string& inner) {
        int depthParen = 0, depthBrace = 0, depthBracket = 0, depthAngle = 0;
        bool inString = false, inLC = false, inBC = false, hasNonWs = false;
        char sc = 0;
        int commas = 0;

        for (size_t i = 0; i < inner.size(); ++i) {
            char c    = inner[i];
            char next = (i + 1 < inner.size()) ? inner[i + 1] : 0;

            if (inLC) { if (c == '\n') inLC = false; continue; }
            if (inBC) { if (c == '*' && next == '/') { inBC = false; i++; } continue; }
            if (inString) {
                if (c == '\\') { i++; continue; }
                if (c == sc) inString = false;
                continue;
            }
            if (c == '"' || c == '\'') { inString = true; sc = c; hasNonWs = true; continue; }
            if (c == '/' && next == '/') { inLC = true; i++; continue; }
            if (c == '/' && next == '*') { inBC = true; i++; continue; }

            if (!std::isspace((unsigned char)c)) hasNonWs = true;

            if (c == '(') depthParen++;
            else if (c == ')') depthParen--;
            else if (c == '{') depthBrace++;
            else if (c == '}') depthBrace--;
            else if (c == '[') depthBracket++;
            else if (c == ']') depthBracket--;
            else if (c == '<') {
                bool prevIdent = i > 0 && (std::isalnum((unsigned char)inner[i-1]) || inner[i-1] == '_');
                bool nextIdent = next != 0 && (std::isalnum((unsigned char)next) || next == '_');
                if (prevIdent && nextIdent) {
                    int local = 0;
                    for (size_t k = i + 1; k < inner.size(); ++k) {
                        char ch = inner[k];
                        if (ch == '<') local++;
                        else if (ch == '>') {
                            if (local == 0) { depthAngle++; break; }
                            local--;
                        } else if (ch == '(' || ch == ')') break;
                    }
                }
            }
            else if (c == '>' && depthAngle > 0 && (i == 0 || inner[i-1] != '-')) depthAngle--;
            else if (c == ',' && depthParen == 0 && depthBrace == 0
                              && depthBracket == 0 && depthAngle == 0) commas++;
        }
        if (!hasNonWs) return 0;
        return commas + 1;
    }

    static int CountUtf8Chars(const std::string& s) {
        int n = 0;
        for (unsigned char c : s)
            if ((c & 0xC0) != 0x80) n++;
        return n;
    }

    static void SearchLineLength(const fs::path& file,
                                const regex_builder::SearchRequest& req,
                                std::vector<SearchResult>& results) {
        std::ifstream in(file, std::ios::binary);
        if (!in) return;
        std::string line;
        int lineNum = 0;
        while (std::getline(in, line)) {
            lineNum++;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            int len = CountUtf8Chars(line);
            bool ok = false;
            switch (req.NumberCompareValue) {
                case regex_builder::NumberCompare::Greater: ok = (len >  req.MinCount); break;
                case regex_builder::NumberCompare::Equal:   ok = (len == req.MinCount); break;
                case regex_builder::NumberCompare::Less:    ok = (len <  req.MinCount); break;
                case regex_builder::NumberCompare::Between: ok = (len >= req.MinCount && len <= req.MaxCount); break;
            }
            if (!ok) continue;
            size_t start = line.find_first_not_of(" \t");
            size_t end   = line.find_last_not_of(" \t");
            std::string trimmed = (start == std::string::npos) ? "" : line.substr(start, end - start + 1);
            if (trimmed.size() > 80) {
                size_t st = utf8::SafeCut(trimmed, 0);
                size_t en = utf8::SafeCut(trimmed, st + 80);
                trimmed = "..." + trimmed.substr(st, en - st) + "...";
            }
            results.push_back({file, lineNum, trimmed, ""});
        }
    }

    static void SearchComments(const fs::path& file,
                           const regex_builder::SearchRequest& req,
                           std::vector<SearchResult>& results) {
        std::string content = ReadFile(file);
        if (content.empty()) return;

        auto lang = settings::GetLanguage(req.Language);
        std::string prefix = lang.Prefix;

        std::string blockOpen, blockClose;
        if (prefix == "//" || prefix == "--") { blockOpen = "/*"; blockClose = "*/"; }

        struct Comment {
            size_t start, end;          // позиции в файле: от префикса до закрытия
            size_t contentStart, contentEnd;
            bool atLineStart, atLineEnd;
        };
        std::vector<Comment> comments;

        bool inStr = false;
        char sc = 0;
        for (size_t i = 0; i < content.size(); ) {
            char c = content[i];

            // Строки
            if (inStr) {
                if (c == '\\' && i + 1 < content.size()) { i += 2; continue; }
                if (c == sc) inStr = false;
                ++i; continue;
            }
            if (c == '"' || c == '\'') { inStr = true; sc = c; ++i; continue; }

            // Строчный комментарий
            if (!prefix.empty() && c == prefix[0]) {
                bool isLine = true;
                for (size_t k = 1; k < prefix.size(); ++k) {
                    if (i + k >= content.size() || content[i + k] != prefix[k]) { isLine = false; break; }
                }
                if (isLine) {
                    size_t end = content.find('\n', i);
                    if (end == std::string::npos) end = content.size();
                    size_t cs = i + prefix.size();
                    // начало строки: до i только пробелы
                    size_t ls = content.rfind('\n', i);
                    ls = (ls == std::string::npos) ? 0 : ls + 1;
                    bool atStart = true;
                    for (size_t k = ls; k < i; ++k) if (!std::isspace((unsigned char)content[k])) { atStart = false; break; }
                    // конец строки: // всегда до конца
                    comments.push_back({i, end, cs, end, atStart, true});
                    i = end; continue;
                }
            }

            // Блочный комментарий
            if (!blockOpen.empty() && c == blockOpen[0]) {
                bool isBlock = true;
                for (size_t k = 1; k < blockOpen.size(); ++k) {
                    if (i + k >= content.size() || content[i + k] != blockOpen[k]) { isBlock = false; break; }
                }
                if (isBlock) {
                    size_t close = content.find(blockClose, i + blockOpen.size());
                    if (close == std::string::npos) { i = content.size(); break; }
                    size_t cs = i + blockOpen.size();
                    size_t ce = close;
                    // начало строки
                    size_t ls = content.rfind('\n', i);
                    ls = (ls == std::string::npos) ? 0 : ls + 1;
                    bool atStart = true;
                    for (size_t k = ls; k < i; ++k) if (!std::isspace((unsigned char)content[k])) { atStart = false; break; }
                    // конец строки
                    size_t le = content.find('\n', close + blockClose.size());
                    if (le == std::string::npos) le = content.size();
                    bool atEnd = true;
                    for (size_t k = close + blockClose.size(); k < le; ++k)
                        if (!std::isspace((unsigned char)content[k])) { atEnd = false; break; }
                    comments.push_back({i, close + blockClose.size(), cs, ce, atStart, atEnd});
                    i = close + blockClose.size(); continue;
                }
            }
            ++i;
        }

    LineTracker lines{content};
        for (const auto& cm : comments) {
            bool matchMode = false;
            switch (req.Comment) {
                case regex_builder::CommentType::StartLine:     matchMode = cm.atLineStart; break;
                case regex_builder::CommentType::EndLine:       matchMode = cm.atLineEnd;   break;
                case regex_builder::CommentType::CommentedCode: matchMode = true;           break;
            }
            if (!matchMode) continue;

            size_t foundPos = cm.start;
            if (!req.CommentWord.empty()) {
                size_t inBody = content.find(req.CommentWord, cm.contentStart);
                if (inBody == std::string::npos || inBody >= cm.contentEnd) continue;
                foundPos = inBody;
            }

            int line = lines.At(foundPos);
            results.push_back({file, line, TrimmedLineAt(content, foundPos), ""});
        }
    }

    static void SearchArgsLength(const fs::path& file, const regex_builder::SearchRequest& req, std::vector<SearchResult>& results) {
        std::string content = ReadFile(file);
        if (content.empty()) return;

        LineTracker lines{content};
        bool inString = false, inLC = false, inBC = false;
        char sc = 0;

        for (size_t i = 0; i < content.size(); ++i) {
            char c    = content[i];
            char next = (i + 1 < content.size()) ? content[i + 1] : 0;

            if (inLC) { if (c == '\n') inLC = false; continue; }
            if (inBC) { if (c == '*' && next == '/') { inBC = false; i++; } continue; }
            if (inString) {
                if (c == '\\') { i++; continue; }
                if (c == sc) inString = false;
                continue;
            }
            if (c == '"' || c == '\'') { inString = true; sc = c; continue; }
            if (c == '/' && next == '/') { inLC = true; i++; continue; }
            if (c == '/' && next == '*') { inBC = true; i++; continue; }

            if (c != '(') continue;

            size_t jb = i;
            while (jb > 0 && std::isspace((unsigned char)content[jb - 1])) --jb;
            if (jb == 0) continue;

            // Шаблонный вызов: Foo<T>(...) — перед ( стоит >, не являющийся ->
            if (content[jb - 1] == '>' && !(jb >= 2 && content[jb - 2] == '-')) {
                int angleDepth = 0;
                size_t k = jb - 1;
                bool found = false;
                while (k > 0) {
                    char ch = content[k];
                    if (ch == '>') angleDepth++;
                    else if (ch == '<') {
                        angleDepth--;
                        if (angleDepth == 0) { found = true; break; }
                        if (angleDepth < 0) break;
                    }
                    --k;
                }
                if (!found || k == 0) continue;
                size_t beforeLt = k;
                while (beforeLt > 0 && std::isspace((unsigned char)content[beforeLt - 1])) --beforeLt;
                if (beforeLt == 0) continue;
                if (!std::isalnum((unsigned char)content[beforeLt - 1]) && content[beforeLt - 1] != '_') continue;
                jb = beforeLt;
            }

            if (!std::isalnum((unsigned char)content[jb - 1]) && content[jb - 1] != '_') continue;

            size_t idStart = jb;
            while (idStart > 0 && (std::isalnum((unsigned char)content[idStart - 1])
                                   || content[idStart - 1] == '_')) --idStart;
            if (idStart == jb) continue;
            if (std::isdigit((unsigned char)content[idStart])) continue;

            std::string ident = content.substr(idStart, jb - idStart);
            if (IsKeywordBeforeParen(ident)) continue;

            size_t closePos = std::string::npos;
            int depth = 0;
            bool sStr = false, sLC = false, sBC = false;
            char sCh = 0;
            for (size_t j = i; j < content.size(); ++j) {
                char ch  = content[j];
                char nxt = (j + 1 < content.size()) ? content[j + 1] : 0;
                if (sLC) { if (ch == '\n') sLC = false; continue; }
                if (sBC) { if (ch == '*' && nxt == '/') { sBC = false; j++; } continue; }
                if (sStr) {
                    if (ch == '\\') { j++; continue; }
                    if (ch == sCh) sStr = false;
                    continue;
                }
                if (ch == '"' || ch == '\'') { sStr = true; sCh = ch; continue; }
                if (ch == '/' && nxt == '/') { sLC = true; j++; continue; }
                if (ch == '/' && nxt == '*') { sBC = true; j++; continue; }
                if (ch == '(') depth++;
                else if (ch == ')') { depth--; if (depth == 0) { closePos = j; break; } }
            }
            if (closePos == std::string::npos) continue;

            int args = CountArguments(content.substr(i + 1, closePos - i - 1));
            bool ok = false;
            char afterParen = CharAfterSpaces(content, closePos);
            bool hasType = HasTypeWordBefore(content, idStart);
            bool isCallCtx = IsCallContextBefore(content, idStart);

            bool isDecl = false;
            if (hasType && (afterParen == '{' || afterParen == ';' || afterParen == ':')) {
                isDecl = true;
            } else if (afterParen == '{' && !isCallCtx) {
                isDecl = true;
            }
            std::string kind = isDecl ? "decl" : "call";
            switch (req.NumberCompareValue) {
                case regex_builder::NumberCompare::Greater: ok = (args >  req.MinCount); break;
                case regex_builder::NumberCompare::Equal:   ok = (args == req.MinCount); break;
                case regex_builder::NumberCompare::Less:    ok = (args <  req.MinCount); break;
                case regex_builder::NumberCompare::Between: ok = (args >= req.MinCount && args <= req.MaxCount); break;
            }
            if (!ok) continue;

            int line = lines.At(i);
            results.push_back({file, line, TrimmedLineAt(content, i), kind});
        }
    }

    static std::string StripCommentsForCount(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        bool inLC = false, inBC = false, inStr = false;
        char sc = 0;
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            char n = (i + 1 < s.size()) ? s[i + 1] : 0;
            if (inLC) { if (c == '\n') { inLC = false; out += '\n'; } continue; }
            if (inBC) { if (c == '*' && n == '/') { inBC = false; i++; } continue; }
            if (inStr) {
                out += c;
                if (c == '\\' && i + 1 < s.size()) { out += s[i + 1]; i++; continue; }
                if (c == sc) inStr = false;
                continue;
            }
            if (c == '"' || c == '\'') { inStr = true; sc = c; out += c; continue; }
            if (c == '/' && n == '/') { inLC = true; i++; continue; }
            if (c == '/' && n == '*') { inBC = true; i++; continue; }
            out += c;
        }
        return out;
    }

    static bool MatchesEmptyMetric(const std::string& inner,
                                const regex_builder::SearchRequest& req) {
        std::string stripped = StripCommentsForCount(inner);
        switch (req.EmptyMetricValue) {
            case regex_builder::EmptyMetric::StrictlyEmpty: {
                for (char c : stripped)
                    if (!std::isspace((unsigned char)c)) return false;
                return true;
            }
            case regex_builder::EmptyMetric::MaxChars: {
                int n = 0;
                for (char c : stripped)
                    if (!std::isspace((unsigned char)c)) n++;
                return n <= req.EmptyMaxCount;
            }
            case regex_builder::EmptyMetric::MaxLines: {
                int n = 0;
                for (char c : stripped)
                    if (c == '\n') n++;
                return n <= req.EmptyMaxCount;
            }
        }
        return false;
    }


    static std::vector<bool> BuildSkipMask(const std::string& s) {
        std::vector<bool> skip(s.size(), false);
        bool inLC = false, inBC = false, inStr = false;
        char sc = 0;
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            char n = (i + 1 < s.size()) ? s[i + 1] : 0;
            if (inLC) { skip[i] = true; if (c == '\n') inLC = false; continue; }
            if (inBC) { skip[i] = true; if (c == '*' && n == '/') { inBC = false; skip[i + 1] = true; i++; } continue; }
            if (inStr) {
                skip[i] = true;
                if (c == '\\' && i + 1 < s.size()) { skip[i + 1] = true; i++; continue; }
                if (c == sc) inStr = false;
                continue;
            }
            if (c == '"' || c == '\'') { skip[i] = true; inStr = true; sc = c; continue; }
            if (c == '/' && n == '/') { skip[i] = true; skip[i + 1] = true; inLC = true; i++; continue; }
            if (c == '/' && n == '*') { skip[i] = true; skip[i + 1] = true; inBC = true; i++; continue; }
        }
        return skip;
    }

    static bool HasStatementEndBefore(const std::string& content, size_t start, size_t end) {
        int depth = 0;
        bool inStr = false, inLC = false, inBC = false;
        char sc = 0;
        for (size_t i = start; i < end; ++i) {
            char c = content[i];
            char n = (i + 1 < content.size()) ? content[i + 1] : 0;
            if (inLC) { if (c == '\n') inLC = false; continue; }
            if (inBC) { if (c == '*' && n == '/') { inBC = false; i++; } continue; }
            if (inStr) {
                if (c == '\\' && i + 1 < content.size()) { i++; continue; }
                if (c == sc) inStr = false;
                continue;
            }
            if (c == '"' || c == '\'') { inStr = true; sc = c; continue; }
            if (c == '/' && n == '/') { inLC = true; i++; continue; }
            if (c == '/' && n == '*') { inBC = true; i++; continue; }
            if (c == '(') depth++;
            else if (c == ')') depth--;
            else if (depth == 0 && (c == ';' || c == '}')) return true;
        }
        return false;
    }

    static void SearchEmptyBlocks(const fs::path& file,
                                const regex_builder::SearchRequest& req,
                                std::vector<SearchResult>& results) {
        std::string content = ReadFile(file);
        if (content.empty()) return;
        if (req.X.empty()) return;

        // 1. Собрать все пары { } с учётом вложенности и строк/комментариев.
        std::vector<std::pair<size_t, size_t>> bracePairs;
        std::vector<size_t> openStack;
        {
            bool inLC = false, inBC = false, inStr = false;
            char sc = 0;
            for (size_t i = 0; i < content.size(); ++i) {
                char c = content[i];
                char n = (i + 1 < content.size()) ? content[i + 1] : 0;
                if (inLC) { if (c == '\n') inLC = false; continue; }
                if (inBC) { if (c == '*' && n == '/') { inBC = false; i++; } continue; }
                if (inStr) {
                    if (c == '\\' && i + 1 < content.size()) { i++; continue; }
                    if (c == sc) inStr = false;
                    continue;
                }
                if (c == '"' || c == '\'') { inStr = true; sc = c; continue; }
                if (c == '/' && n == '/') { inLC = true; i++; continue; }
                if (c == '/' && n == '*') { inBC = true; i++; continue; }
                if (c == '{') openStack.push_back(i);
                else if (c == '}') {
                    if (!openStack.empty()) {
                        bracePairs.push_back({openStack.back(), i});
                        openStack.pop_back();
                    }
                }
            }
        }
        if (bracePairs.empty()) return;

        // 2. Regex по X через EscapeXyzWithBounds — понимает %, *, ^, $ и сам ставит \b.
        std::string pattern = regex_builder::EscapeXyzWithBounds(req.X);
        if (pattern.empty()) return;
        std::regex rx;
        try { rx = std::regex(pattern); } catch (...) { return; }

        auto skipMask = BuildSkipMask(content);
        LineTracker lines{content};
        std::set<size_t> seenOpens;

        // 3. Для каждой X — ближайший { после неё, парная }, проверка метрики.
        for (auto it = std::sregex_iterator(content.begin(), content.end(), rx);
            it != std::sregex_iterator(); ++it) {
            size_t xPos = (size_t)it->position();
            if (skipMask[xPos]) continue;
            if (xPos > 0 && content[xPos - 1] == '#') continue;  // #if, #elif...
            size_t xEnd = xPos + it->length();

            if (req.X == "else") {
                size_t p = xEnd;
                while (p < content.size() && std::isspace((unsigned char)content[p])) ++p;
                if (p + 1 < content.size() && content[p] == 'i' && content[p + 1] == 'f'
                    && (p + 2 >= content.size() || !IsWordChar((unsigned char)content[p + 2]))) {
                    continue;
                }
            }

            size_t bestOpen = (size_t)-1;
            for (auto& bp : bracePairs)
                if (bp.first > xEnd && bp.first < bestOpen) bestOpen = bp.first;
            if (bestOpen == (size_t)-1) continue;
            if (HasStatementEndBefore(content, xEnd, bestOpen)) continue;
            if (seenOpens.count(bestOpen)) continue;

            // Привязываем { к ближайшему X: если следующий X идёт до {,
            // то этот блок принадлежит ему, а не текущему (например, if (a) if (b) {}).
            auto nextIt = it;
            ++nextIt;
            while (nextIt != std::sregex_iterator() && skipMask[nextIt->position()]) ++nextIt;
            if (nextIt != std::sregex_iterator() && (size_t)nextIt->position() < bestOpen) continue;

            seenOpens.insert(bestOpen);

            size_t close = (size_t)-1;
            for (auto& bp : bracePairs)
                if (bp.first == bestOpen) { close = bp.second; break; }
            if (close == (size_t)-1) continue;

            std::string inner = content.substr(bestOpen + 1, close - bestOpen - 1);
            if (!MatchesEmptyMetric(inner, req)) continue;

            int line = lines.At((size_t)it->position());
            results.push_back({file, line, TrimmedLineAt(content, (size_t)it->position()), ""});
        }
    }

    static void SearchXButNotYByLine(const fs::path& file,
            const regex_builder::SearchRequest& req,
            std::vector<SearchResult>& results) {
        std::string content = ReadFile(file);
        if (content.empty()) return;

        std::string px = regex_builder::EscapeXyzWithBounds(req.X);
        std::string py = regex_builder::EscapeXyzWithBounds(req.Y);
        if (px.empty() || py.empty()) return;
        std::regex rxX, rxY;
        try {
            rxX = std::regex(px);
            rxY = std::regex(py);
        } catch (...) { return; }

        auto skipMask = BuildSkipMask(content);

        size_t lineStart = 0;
        int lineNum = 0;
        while (lineStart < content.size()) {
            size_t lineEnd = content.find('\n', lineStart);
            if (lineEnd == std::string::npos) lineEnd = content.size();
            lineNum++;

            std::string cleanLine;
            cleanLine.reserve(lineEnd - lineStart);
            for (size_t k = lineStart; k < lineEnd; ++k) {
                cleanLine += skipMask[k] ? ' ' : content[k];
            }
            if (!cleanLine.empty() && cleanLine.back() == '\r') cleanLine.pop_back();

            std::smatch m;
            bool hasY = std::regex_search(cleanLine, m, rxY);
            if (!hasY) {
                // Считаем все вхождения X в строке, чтобы не дублировать записи
                int count = 0;
                for (auto it = std::sregex_iterator(cleanLine.begin(), cleanLine.end(), rxX);
                    it != std::sregex_iterator(); ++it) {
                    count++;
                }
                if (count > 0) {
                    // Пушим одну запись на строку с указанием количества совпадений
                    results.push_back({file, lineNum, TrimmedLineAt(content, lineStart), "", count});
                }
            }

            lineStart = lineEnd + 1;
        }
    }
    std::vector<SearchResult> Search(const regex_builder::SearchRequest& req, const std::string& pattern, const SearchOptions& options, size_t* outFileCount, size_t* outSkippedCount) {
        std::vector<SearchResult> results;
        size_t enumSkipped = 0;
        auto files = EnumerateFiles(options, enumSkipped);
        if (outFileCount) *outFileCount = files.size();
        std::atomic<size_t> skipped{enumSkipped};

        bool isEmptyBlocks = (req.Type == regex_builder::SearchType::Structure &&
                            req.Structure == regex_builder::StructureType::XEmpty);
        bool isStructureXYZ = (req.Type == regex_builder::SearchType::Structure &&
                               req.Structure == regex_builder::StructureType::XAnywhereBeforeYBeforeZ);
        bool isCombinationXOrY = (req.Type == regex_builder::SearchType::Combination &&
                                  req.Combination == regex_builder::CombinationType::XOrY);
        bool isCombinationXButNotY = (req.Type == regex_builder::SearchType::Combination &&
                                      req.Combination == regex_builder::CombinationType::XButNotY);
        bool isArgsLength = (req.Type == regex_builder::SearchType::Quantity &&
                             req.Quantity == regex_builder::QuantityType::ArgsLength);
        bool isLineLength = (req.Type == regex_builder::SearchType::Quantity &&
                     req.Quantity == regex_builder::QuantityType::LineLength);
        bool isComments = (req.Type == regex_builder::SearchType::Comment);
        bool usesPattern = !isArgsLength && !isLineLength && !isComments
                   && !isEmptyBlocks && !isCombinationXButNotY;
        if (usesPattern && pattern.empty()) {
            std::cerr << colors::Error << "Пустое выражение — поиск отменён." << colors::Reset << "\n";
            return results;
        }
        std::regex lineRx, wholeRx, yRx;
        bool needLineRx  = !isArgsLength && !isLineLength && !isComments
                   && !isCombinationXButNotY && !isStructureXYZ
                   && !isEmptyBlocks && !isCombinationXOrY;
        bool needWholeRx = isCombinationXOrY || isStructureXYZ || isCombinationXButNotY;

        try {
            if (isCombinationXButNotY && req.XButNotYModeValue == regex_builder::XButNotYMode::ByPosition) {
                std::string px = regex_builder::EscapeXyzWithBounds(req.X);
                std::string py = regex_builder::EscapeXyzWithBounds(req.Y);
                if (px.empty() || py.empty()) {
                    std::cerr << colors::Error << "Пустое выражение — поиск отменён." << colors::Reset << "\n";
                    return results;
                }
                wholeRx = std::regex(px, std::regex::multiline | std::regex::optimize);
                yRx = std::regex(py, std::regex::multiline | std::regex::optimize);
            } else if (!isCombinationXButNotY) {
                if (needLineRx)  lineRx  = std::regex(pattern, std::regex::optimize);
                if (needWholeRx) wholeRx = std::regex(pattern, std::regex::multiline | std::regex::optimize);
            }
        } catch (const std::regex_error& e) {
            std::cerr << colors::Error << "Ошибка в regex: " << e.what() << colors::Reset << "\n";
            return results;
        }

        std::mutex mtx;
        unsigned int numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
        if (numThreads > files.size()) numThreads = (unsigned int)std::max<size_t>(1, files.size());
        std::vector<std::thread> threads;
        std::atomic<size_t> index{0};

        auto worker = [&]() {
            std::vector<SearchResult> local;
            while (true) {
                size_t i = index.fetch_add(1);
                if (i >= files.size()) break;
                try {
                    if (isComments)
                        SearchComments(files[i], req, local);
                    else if (isStructureXYZ)
                        SearchStructureXYZ(files[i], wholeRx, local);
                    else if (isEmptyBlocks)
                        SearchEmptyBlocks(files[i], req, local);
                    else if (isCombinationXOrY)
                        SearchWholeFile(files[i], wholeRx, local);
                    else if (isCombinationXButNotY) {
                        if (req.XButNotYModeValue == regex_builder::XButNotYMode::ByLine)
                            SearchXButNotYByLine(files[i], req, local);
                        else
                            SearchXButNotY(files[i], wholeRx, yRx, local);
                    }
                    else if (isArgsLength)
                        SearchArgsLength(files[i], req, local);
                    else if (isLineLength)
                        SearchLineLength(files[i], req, local);
                    else
                        SearchLineByLine(files[i], lineRx, local);
                } catch (...) { skipped++; }
            }
            std::lock_guard<std::mutex> lock(mtx);
            results.insert(results.end(), local.begin(), local.end());
        };

        for (unsigned int t = 0; t < numThreads; ++t) threads.emplace_back(worker);
        for (auto& t : threads) if (t.joinable()) t.join();
        if (outSkippedCount) *outSkippedCount = skipped.load();
        std::sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
            if (a.File != b.File) return a.File < b.File; // Нативное быстрое сравнение путей
            return a.Line < b.Line;
        });
        return results;
    }
}