
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
                    if (opt.MaxFileSizeMb > 0) {
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
            if (std::regex_search(line, regex)) {
                size_t start = line.find_first_not_of(" \t");
                size_t end = line.find_last_not_of(" \t");
                std::string trimmed = (start == std::string::npos) ? "" : line.substr(start, end - start + 1);
                if (trimmed.size() > 80) trimmed = trimmed.substr(0, 80) + "...";
                results.push_back({file, lineNum, trimmed});
            }
        }
    }

    static void SearchWholeFile(const fs::path& file, const std::string& pattern, std::vector<SearchResult>& results) {
        std::string content = ReadFile(file);
        if (content.empty()) return;
        std::regex rx;
        try { rx = std::regex(pattern, std::regex::multiline); } catch (...) { return; }
        LineTracker lines{content};
        for (auto it = std::sregex_iterator(content.begin(), content.end(), rx); it != std::sregex_iterator(); ++it) {
            size_t pos = it->position();
            int line = lines.At(pos);
            std::string text = TrimmedLineAt(content, pos);
            results.push_back({file, line, text});
        }
    }

    static void SearchStructureXYZ(const fs::path& file, const std::string& pattern, std::vector<SearchResult>& results) {
        std::string content = ReadFile(file);
        if (content.empty()) return;
        std::regex rx;
        try { rx = std::regex(pattern, std::regex::multiline); } catch (...) { return; }
        LineTracker lines{content};
        for (auto it = std::sregex_iterator(content.begin(), content.end(), rx); it != std::sregex_iterator(); ++it) {
            const auto& m = *it;
            size_t pos = (m.size() > 1 && m[1].matched) ? m.position(1) : m.position();
            int line = lines.At(pos);
            std::string text = TrimmedLineAt(content, pos);
            results.push_back({file, line, text});
        }
    }

    static void SearchXButNotY(const fs::path& file, const std::string& xPat, const std::string& yPat, std::vector<SearchResult>& results) {
        std::string content = ReadFile(file);
        if (content.empty()) return;
        std::regex rxX, rxY;
        try { rxX = std::regex(xPat, std::regex::multiline); } catch (...) { return; }
        try { rxY = std::regex(yPat, std::regex::multiline); } catch (...) { return; }

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
            std::string text = TrimmedLineAt(content, xs);
            results.push_back({file, line, text});
        }
    }

    static void SearchArgsLength(const fs::path& file, const regex_builder::SearchRequest& req, std::vector<SearchResult>& results) {
        std::string content = ReadFile(file);
        if (content.empty()) return;

        LineTracker lines{content};
        for (size_t i = 0; i < content.size(); ++i) {
            if (content[i] != '(') continue;
            size_t jb = i;
            while (jb > 0 && (content[jb - 1] == ' ' || content[jb - 1] == '\t')) --jb;
            if (jb == 0) continue;
            if (!std::isalnum((unsigned char)content[jb - 1]) && content[jb - 1] != '_') continue;
            

            size_t closePos = std::string::npos;
            int depth = 0;
            bool inString = false;
            char stringChar = 0;
            bool inLineComment = false;
            bool inBlockComment = false;

            for (size_t j = i; j < content.size(); ++j) {
                char c = content[j];
                char next = (j + 1 < content.size()) ? content[j + 1] : 0;

                if (inLineComment) { if (c == '\n') inLineComment = false; continue; }
                if (inBlockComment) { if (c == '*' && next == '/') { inBlockComment = false; j++; } continue; }

                if (inString) {
                    if (c == '\\') { j++; continue; }
                    if (c == stringChar) inString = false;
                    continue;
                }

                if (c == '"' || c == '\'' || c == '`') { inString = true; stringChar = c; continue; }
                if (c == '@' && next == '"') { inString = true; stringChar = '"'; j++; continue; }
                if (c == '/' && next == '/') { inLineComment = true; j++; continue; }
                if (c == '/' && next == '*') { inBlockComment = true; j++; continue; }

                if (c == '(') depth++;
                else if (c == ')') { depth--; if (depth == 0) { closePos = j; break; } }
            }
            if (closePos == std::string::npos) continue;

            std::string inner = content.substr(i + 1, closePos - i - 1);
            int commas = 0;
            int nested = 0;
            int angle = 0;
            bool inStr2 = false;
            char strChar2 = 0;
            bool inLC2 = false, inBC2 = false;
            
            for (size_t k = 0; k < inner.size(); ++k) {
                char c = inner[k];
                char next = (k + 1 < inner.size()) ? inner[k + 1] : 0;

                if (inLC2) { if (c == '\n') inLC2 = false; continue; }
                if (inBC2) { if (c == '*' && next == '/') { inBC2 = false; k++; } continue; }
                if (inStr2) {
                    if (c == '\\') { k++; continue; }
                    if (c == strChar2) inStr2 = false;
                    continue;
                }
                if (c == '"' || c == '\'' || c == '`') { inStr2 = true; strChar2 = c; continue; }
                if (c == '@' && next == '"') { inStr2 = true; strChar2 = '"'; k++; continue; }
                if (c == '/' && next == '/') { inLC2 = true; k++; continue; }
                if (c == '/' && next == '*') { inBC2 = true; k++; continue; }

                if (c == '(') nested++;
                else if (c == ')') nested--;
                else if (c == '<') angle++;
                else if (c == '>') { if (angle > 0) angle--; }
                else if (c == ',' && nested == 0 && angle == 0) { commas++;}
    
            }

            int args;
            if (inner.find_first_not_of(" \t\r\n") == std::string::npos) {
                args = 0;                    // пустые скобки — 0 аргументов
            } else {
                args = commas + 1;
            }

            bool ok = false;
            switch (req.NumberCompareValue) {
                case regex_builder::NumberCompare::Greater: ok = (args > req.MinCount); break;
                case regex_builder::NumberCompare::Equal:   ok = (args == req.MinCount); break;
                case regex_builder::NumberCompare::Less:    ok = (args < req.MinCount); break;
                case regex_builder::NumberCompare::Between: ok = (args >= req.MinCount && args <= req.MaxCount); break;
            }
            if (!ok) continue;

            int line = lines.At(i);
            std::string text = TrimmedLineAt(content, i);
            results.push_back({file, line, text});
            i = closePos;
        }
    }

    std::vector<SearchResult> Search(const regex_builder::SearchRequest& req, const std::string& pattern, const SearchOptions& options) {
        std::vector<SearchResult> results;
        auto files = EnumerateFiles(options);
        std::cout << "Найдено файлов: " << files.size() << "\n\n";

        bool isStructureXEmpty = (req.Type == regex_builder::SearchType::Structure &&
                                  req.Structure == regex_builder::StructureType::XEmpty);
        bool isStructureXYZ = (req.Type == regex_builder::SearchType::Structure &&
                               req.Structure == regex_builder::StructureType::XAnywhereBeforeYBeforeZ);
        bool isCombinationXOrY = (req.Type == regex_builder::SearchType::Combination &&
                                  req.Combination == regex_builder::CombinationType::XOrY);
        bool isCombinationXButNotY = (req.Type == regex_builder::SearchType::Combination &&
                                      req.Combination == regex_builder::CombinationType::XButNotY);
        bool isArgsLength = (req.Type == regex_builder::SearchType::Quantity &&
                             req.Quantity == regex_builder::QuantityType::ArgsLength);

        std::string yPattern;
        if (isCombinationXButNotY)
            yPattern = regex_builder::EscapeXyz(req.Y);

        std::regex regex_obj;
        bool needRegex = !isArgsLength && !isCombinationXButNotY && !isStructureXYZ && !isStructureXEmpty && !isCombinationXOrY;
        if (needRegex) {
            try { regex_obj = std::regex(pattern); }
            catch (const std::regex_error& e) { std::cerr << "Ошибка в regex: " << e.what() << "\n"; return results; }
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
                        SearchStructureXYZ(files[i], pattern, local);
                    else if (isStructureXEmpty || isCombinationXOrY)
                        SearchWholeFile(files[i], pattern, local);
                    else if (isCombinationXButNotY)
                        SearchXButNotY(files[i], pattern, yPattern, local);
                    else if (isArgsLength)
                        SearchArgsLength(files[i], req, local);
                    else
                        SearchLineByLine(files[i], regex_obj, local);
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