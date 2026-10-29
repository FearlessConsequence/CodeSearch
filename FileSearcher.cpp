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

    static std::vector<fs::path> EnumerateFiles(const SearchOptions& opt) {
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
                } catch (...) { continue; }
                result.push_back(entry.path());
            }
        } catch (...) { }
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
            text = "..." + text.substr(st, maxLen) + "...";
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
            std::smatch m;
            if (std::regex_search(line, m, regex)) {
                size_t start = line.find_first_not_of(" \t");
                size_t end = line.find_last_not_of(" \t");
                std::string trimmed = (start == std::string::npos) ? "" : line.substr(start, end - start + 1);
                if (trimmed.size() > 80) {
                    size_t matchRel = (m.position() >= start) ? (size_t)m.position() - start : 0;
                    size_t st = (matchRel > 40) ? matchRel - 40 : 0;
                    if (st + 80 > trimmed.size()) st = trimmed.size() - 80;
                    trimmed = "..." + trimmed.substr(st, 80) + "...";
                }
                results.push_back({file, lineNum, trimmed});
            }
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
            results.push_back({file, line, text});
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
            results.push_back({file, line, text});
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
            results.push_back({file, line, it->str()});
        }
    }

    static bool IsKeywordBeforeParen(const std::string& w) {
        static const char* kws[] = {
            "if","while","for","switch","catch","using","sizeof","alignof",
            "return","throw","new","delete","typename","static_assert",
            "decltype","co_await","co_return","co_yield","requires","noexcept",
            "constexpr","consteval","constinit","typeid"
        };
        for (auto k : kws) if (w == k) return true;
        return false;
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
            switch (req.NumberCompareValue) {
                case regex_builder::NumberCompare::Greater: ok = (args >  req.MinCount); break;
                case regex_builder::NumberCompare::Equal:   ok = (args == req.MinCount); break;
                case regex_builder::NumberCompare::Less:    ok = (args <  req.MinCount); break;
                case regex_builder::NumberCompare::Between: ok = (args >= req.MinCount && args <= req.MaxCount); break;
            }
            if (!ok) continue;

            int line = lines.At(i);
            results.push_back({file, line, TrimmedLineAt(content, i)});
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

        // 2. Regex по X с \b.
        std::string escX = regex_builder::Escape(req.X);
        bool leftB  = std::isalnum((unsigned char)req.X.front()) || req.X.front() == '_';
        bool rightB = std::isalnum((unsigned char)req.X.back())  || req.X.back()  == '_';
        std::string pattern = (leftB  ? R"(\b)" : "") + escX + (rightB ? R"(\b)" : "");
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

            size_t bestOpen = (size_t)-1;
            for (auto& bp : bracePairs)
                if (bp.first > xEnd && bp.first < bestOpen) bestOpen = bp.first;
            if (bestOpen == (size_t)-1) continue;
            if (HasStatementEndBefore(content, xEnd, bestOpen)) continue;
            if (seenOpens.count(bestOpen)) continue;
            seenOpens.insert(bestOpen);

            size_t close = (size_t)-1;
            for (auto& bp : bracePairs)
                if (bp.first == bestOpen) { close = bp.second; break; }
            if (close == (size_t)-1) continue;

            std::string inner = content.substr(bestOpen + 1, close - bestOpen - 1);
            if (!MatchesEmptyMetric(inner, req)) continue;

            int line = lines.At((size_t)it->position());
            results.push_back({file, line, TrimmedLineAt(content, (size_t)it->position())});
        }
    }

    static void SearchXButNotYByLine(const fs::path& file,
                                     const regex_builder::SearchRequest& req,
                                     std::vector<SearchResult>& results) {
        std::string content = ReadFile(file);
        if (content.empty()) return;

        std::regex rxX, rxY;
        try {
            rxX = std::regex(regex_builder::EscapeXyzWithBounds(req.X));
            rxY = std::regex(regex_builder::EscapeXyzWithBounds(req.Y));
        } catch (...) { return; }

        auto skipMask = BuildSkipMask(content);
        LineTracker lines{content};

        for (auto it = std::sregex_iterator(content.begin(), content.end(), rxX);
             it != std::sregex_iterator(); ++it) {
            size_t xPos = (size_t)it->position();

            size_t lineStart = content.rfind('\n', xPos);
            lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
            size_t lineEnd = content.find('\n', xPos);
            if (lineEnd == std::string::npos) lineEnd = content.size();

            std::string cleanLine;
            cleanLine.reserve(lineEnd - lineStart);
            for (size_t k = lineStart; k < lineEnd; ++k) {
                if (!skipMask[k]) cleanLine += content[k];
            }

            std::smatch m;
            if (std::regex_search(cleanLine, m, rxY)) continue;

            int lineNum = lines.At(xPos);
            results.push_back({file, lineNum, it->str()});
        }
    }


    std::vector<SearchResult> Search(const regex_builder::SearchRequest& req, const std::string& pattern, const SearchOptions& options) {
        std::vector<SearchResult> results;
        auto files = EnumerateFiles(options);
        std::cout << "Найдено файлов: " << files.size() << "\n\n";

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

        std::regex lineRx, wholeRx, yRx;
        bool needLineRx  = !isArgsLength && !isCombinationXButNotY && !isStructureXYZ
                           && !isEmptyBlocks && !isCombinationXOrY;
        bool needWholeRx = isCombinationXOrY || isStructureXYZ || isCombinationXButNotY;

        try {
            if (isCombinationXButNotY && req.XButNotYModeValue == regex_builder::XButNotYMode::ByPosition) {
                wholeRx = std::regex(regex_builder::EscapeXyzWithBounds(req.X), std::regex::multiline);
                yRx = std::regex(regex_builder::EscapeXyzWithBounds(req.Y), std::regex::multiline);
            } else if (!isCombinationXButNotY) {
                if (needLineRx)  lineRx  = std::regex(pattern);
                if (needWholeRx) wholeRx = std::regex(pattern, std::regex::multiline);
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
                    if (isStructureXYZ)
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
                    else
                        SearchLineByLine(files[i], lineRx, local);
                } catch (...) { }
            }
            std::lock_guard<std::mutex> lock(mtx);
            results.insert(results.end(), local.begin(), local.end());
        };

        for (unsigned int t = 0; t < numThreads; ++t) threads.emplace_back(worker);
        for (auto& t : threads) if (t.joinable()) t.join();

        std::sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
            auto sa = ToUtf8(a.File), sb = ToUtf8(b.File);
            if (sa != sb) return sa < sb;
            return a.Line < b.Line;
        });
        return results;
    }
}