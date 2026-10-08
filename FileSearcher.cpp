#include "FileSearcher.h"
#include "Colors.h"
#include <regex>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <algorithm>
#include <iostream>
#include <atomic>

namespace file_searcher {

    // ============ УТИЛИТЫ ============

    static std::string ToUtf8(const fs::path& p) {
        return p.u8string();
    }

    static bool MatchesMask(const std::string& fileName, const std::string& mask) {
        auto lower = [](std::string s) {
            for (auto& c : s) c = std::tolower((unsigned char)c);
            return s;
        };
        std::string fn = lower(fileName);
        std::string m = lower(mask);

        if (!m.empty() && m.front() == '*') {
            std::string suffix = m.substr(1);
            return fn.size() >= suffix.size() &&
                   fn.compare(fn.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        if (!m.empty() && m.back() == '*') {
            std::string prefix = m.substr(0, m.size() - 1);
            return fn.compare(0, prefix.size(), prefix) == 0;
        }
        return fn == m;
    }

    static bool InVector(const std::vector<std::string>& vec, const std::string& item) {
        for (const auto& v : vec) {
            if (v.size() == item.size()) {
                bool eq = true;
                for (size_t i = 0; i < v.size(); ++i) {
                    if (std::tolower((unsigned char)v[i]) !=
                        std::tolower((unsigned char)item[i])) {
                        eq = false;
                        break;
                    }
                }
                if (eq) return true;
            }
        }
        return false;
    }

    // ============ ОБХОД ФАЙЛОВ ============

    static std::vector<fs::path> EnumerateFiles(const SearchOptions& opt) {
        std::vector<fs::path> result;

        try {
            for (auto it = fs::recursive_directory_iterator(
                     opt.Directory,
                     fs::directory_options::skip_permission_denied);
                 it != fs::recursive_directory_iterator(); ++it)
            {
                const auto& entry = *it;

                if (entry.is_directory()) {
                    std::string name = ToUtf8(entry.path().filename());
                    if (InVector(opt.ExcludeDirs, name)) {
                        it.disable_recursion_pending();
                        continue;
                    }
                    continue;
                }

                if (!entry.is_regular_file()) continue;

                std::string fileName = ToUtf8(entry.path().filename());

                bool excluded = false;
                for (const auto& mask : opt.ExcludeFiles) {
                    if (MatchesMask(fileName, mask)) {
                        excluded = true;
                        break;
                    }
                }
                if (excluded) continue;

                if (!opt.Extensions.empty()) {
                    std::string ext = ToUtf8(entry.path().extension());
                    if (!InVector(opt.Extensions, ext)) continue;
                }

                try {
                    auto sizeMb = fs::file_size(entry.path()) / 1024 / 1024;
                    if (opt.MaxFileSizeMb > 0 && sizeMb > (uintmax_t)opt.MaxFileSizeMb)
                        continue;
                }
                catch (...) { continue; }

                result.push_back(entry.path());
            }
        }
        catch (...) { }

        return result;
    }

    // ============ ЧТЕНИЕ ФАЙЛА ============

    static std::string ReadFile(const fs::path& file) {
        std::ifstream in(file, std::ios::binary);
        if (!in) return "";
        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }

    // ============ ПОСТРОЧНЫЙ ПОИСК ============

    static void SearchLineByLine(
        const fs::path& file,
        const std::regex& regex,
        std::vector<SearchResult>& results)
    {
        std::ifstream in(file, std::ios::binary);
        if (!in) return;

        std::string line;
        int lineNum = 0;

        while (std::getline(in, line)) {
            lineNum++;
            if (std::regex_search(line, regex)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                size_t start = line.find_first_not_of(" \t");
                size_t end = line.find_last_not_of(" \t");
                std::string trimmed = (start == std::string::npos)
                    ? "" : line.substr(start, end - start + 1);

                results.push_back({file, lineNum, trimmed});
            }
        }
    }

    // ============ ПОИСК ПО ВСЕМУ ФАЙЛУ (regex) ============

    static void SearchWholeFile(
        const fs::path& file,
        const std::regex& regex,
        std::vector<SearchResult>& results)
    {
        std::string content = ReadFile(file);
        if (content.empty()) return;

        auto begin = std::sregex_iterator(content.begin(), content.end(), regex);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            const auto& m = *it;
            int line = 1;
            for (size_t i = 0; i < (size_t)m.position(); ++i)
                if (content[i] == '\n') line++;

            std::string text = m.str();
            size_t nl = text.find('\n');
            if (nl != std::string::npos) text = text.substr(0, nl);
            if (text.size() > 80) text = text.substr(0, 80) + "...";

            results.push_back({file, line, text});
        }
    }

    // ============ ПАРСЕР БЛОКОВ ============

    // Разбор X по %, *, ^ — на части
    static std::vector<std::string> SplitX(const std::string& x) {
        std::vector<std::string> parts;
        std::string current;
        for (char c : x) {
            if (c == '%' || c == '*' || c == '^') {
                if (!current.empty()) parts.push_back(current);
                current.clear();
            } else {
                current += c;
            }
        }
        if (!current.empty()) parts.push_back(current);
        return parts;
    }

    static bool CheckLeftBoundary(const std::string& content, size_t pos) {
        if (pos == 0) return true;
        char prev = content[pos - 1];
        return !(std::isalnum((unsigned char)prev) || prev == '_');
    }

    static bool CheckRightBoundary(const std::string& content, size_t pos, size_t len) {
        size_t end = pos + len;
        if (end >= content.size()) return true;
        char next = content[end];
        return !(std::isalnum((unsigned char)next) || next == '_');
    }

    static bool NeedBoundaryCheck(const std::string& s) {
        if (s.empty()) return false;
        char c = s[0];
        return std::isalnum((unsigned char)c) || c == '_';
    }

    // Найти X (с учётом % — пропуск пробелов и переносов)
    static bool FindX(const std::string& content,
                      const std::vector<std::string>& xParts,
                      size_t startPos,
                      size_t& foundPos)
    {
        size_t tempPos = startPos;
        while (tempPos < content.size()) {
            size_t p = content.find(xParts[0], tempPos);
            if (p == std::string::npos) return false;

            size_t cur = p + xParts[0].size();
            bool ok = true;

            for (size_t i = 1; i < xParts.size(); ++i) {
                while (cur < content.size() &&
                       (content[cur] == ' ' || content[cur] == '\t' ||
                        content[cur] == '\n' || content[cur] == '\r'))
                    cur++;

                if (content.compare(cur, xParts[i].size(), xParts[i]) != 0) {
                    ok = false;
                    break;
                }
                cur += xParts[i].size();
            }

            if (ok) {
                if (NeedBoundaryCheck(xParts[0]) && !CheckLeftBoundary(content, p)) {
                    tempPos = p + 1;
                    continue;
                }
                foundPos = p;
                return true;
            }

            tempPos = p + 1;
        }
        return false;
    }

    // Парсер блока — считать скобки до баланса 0, игнорируя строки
    static bool ParseBlock(const std::string& content,
                           size_t openPos,
                           char openCh,
                           char closeCh,
                           size_t& closePos)
    {
        int depth = 0;
        bool inString = false;

        for (size_t i = openPos; i < content.size(); ++i) {
            char c = content[i];

            if (inString) {
                if (c == '\\') { i++; continue; }
                if (c == '"') inString = false;
                continue;
            }

            if (c == '"') { inString = true; continue; }

            if (c == openCh) depth++;
            else if (c == closeCh) {
                depth--;
                if (depth == 0) {
                    closePos = i;
                    return true;
                }
            }
        }
        return false;
    }

    // ============ ПОИСК БЛОКА ============

    static void SearchBlock(
    const fs::path& file,
    const regex_builder::SearchRequest& req,
    const SearchOptions& options,
    std::vector<SearchResult>& results)
    {
        std::string content = ReadFile(file);
        if (content.empty()) return;

        auto xParts = SplitX(req.X);
        if (xParts.empty()) return;

        // Режим: X содержит { или (
        bool hasBrace = req.X.find('{') != std::string::npos;
        bool hasParen = req.X.find('(') != std::string::npos;
        bool useParser = hasBrace || hasParen;

        char openCh = hasBrace ? '{' : '(';
        char closeCh = hasBrace ? '}' : ')';

        size_t searchPos = 0;

        while (searchPos < content.size()) {
            size_t xPos;
            if (!FindX(content, xParts, searchPos, xPos))
                break;

            size_t innerStart, innerEnd;

            if (useParser) {
                // Режим парсера — искать по всем строкам
                size_t openPos = content.find(openCh, xPos);
                if (openPos == std::string::npos) {
                    searchPos = xPos + 1;
                    continue;
                }

                if (openPos - xPos > 200) {
                    searchPos = xPos + 1;
                    continue;
                }

                size_t closePos;
                if (!ParseBlock(content, openPos, openCh, closeCh, closePos)) {
                    searchPos = xPos + 1;
                    continue;
                }

                if (req.Z.find(closeCh) == std::string::npos) {
                    searchPos = xPos + 1;
                    continue;
                }

                innerStart = openPos;
                innerEnd = closePos;
            }
            else {
                // Режим одной строки
                size_t lineEnd = content.find('\n', xPos);
                if (lineEnd == std::string::npos) lineEnd = content.size();

                size_t zPos = content.find(req.Z, xPos);
                if (zPos == std::string::npos || zPos > lineEnd) {
                    searchPos = xPos + 1;
                    continue;
                }

                // Проверка границы для Z
                if (options.WordBoundaries && NeedBoundaryCheck(req.Z)
                    && !CheckLeftBoundary(content, zPos)) {
                    searchPos = xPos + 1;
                    continue;
                }

                std::string inner = content.substr(xPos, zPos - xPos);
                if (inner.find(req.Y) == std::string::npos) {
                    searchPos = xPos + 1;
                    continue;
                }

                innerStart = xPos;
                innerEnd = zPos + req.Z.size();
            }

            // Ищем ВСЕ Y внутри [innerStart, innerEnd]
            size_t yPos = innerStart;
            while (yPos < innerEnd) {
                size_t found = content.find(req.Y, yPos);
                if (found == std::string::npos || found >= innerEnd) break;

                // Проверка границы для Y
                if (options.WordBoundaries && NeedBoundaryCheck(req.Y)
                    && !CheckLeftBoundary(content, found)) {
                    yPos = found + 1;
                    continue;
                }

                // Номер строки Y

                // Номер строки Y
                int line = 1;
                for (size_t i = 0; i < found; ++i)
                    if (content[i] == '\n') line++;

                // Текст — строка Y
                size_t lineStart = content.rfind('\n', found);
                lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
                size_t lineEnd2 = content.find('\n', found);
                if (lineEnd2 == std::string::npos) lineEnd2 = content.size();

                std::string text = content.substr(lineStart, lineEnd2 - lineStart);

                size_t s = text.find_first_not_of(" \t\r");
                size_t e = text.find_last_not_of(" \t\r");
                if (s != std::string::npos)
                    text = text.substr(s, e - s + 1);
                else
                    text = "";

                if (text.size() > 80) text = text.substr(0, 80) + "...";

                results.push_back({file, line, text});

                yPos = found + req.Y.size();
            }

            searchPos = xPos + 1;
        }
    }

    // ============ ГЛАВНЫЙ ПОИСК ============

    std::vector<SearchResult> Search(
        const regex_builder::SearchRequest& req,
        const std::string& pattern,
        const SearchOptions& options)
    {
        std::vector<SearchResult> results;

        auto files = EnumerateFiles(options);
        std::cout << "Найдено файлов: " << files.size() << "\n\n";

        std::regex regex_obj;
        try {
            regex_obj = std::regex(pattern);
        }
        catch (const std::regex_error& e) {
            std::cerr << "Ошибка в regex: " << e.what() << "\n";
            return results;
        }

        // Режим
        bool isBlock = (req.Type == regex_builder::SearchType::Structure &&
                        req.Structure == regex_builder::StructureType::XAnywhereBeforeYBeforeZ);

        bool isMultiLineRegex = (req.Type == regex_builder::SearchType::Structure &&
                                 req.Structure == regex_builder::StructureType::XEmpty);

        std::mutex mtx;
        unsigned int numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;

        std::vector<std::thread> threads;
        std::atomic<size_t> index{0};

        auto worker = [&]() {
            std::vector<SearchResult> local;
            while (true) {
                size_t i = index.fetch_add(1);
                if (i >= files.size()) break;

                try {
                    if (isBlock)
                        SearchBlock(files[i], req, options, local);
                    else if (isMultiLineRegex)
                        SearchWholeFile(files[i], regex_obj, local);
                    else
                        SearchLineByLine(files[i], regex_obj, local);
                }
                catch (...) { }
            }
            std::lock_guard<std::mutex> lock(mtx);
            results.insert(results.end(), local.begin(), local.end());
        };

        for (unsigned int t = 0; t < numThreads; ++t)
            threads.emplace_back(worker);
        for (auto& t : threads)
            if (t.joinable()) t.join();

        std::sort(results.begin(), results.end(),
            [](const SearchResult& a, const SearchResult& b) {
                auto sa = ToUtf8(a.File);
                auto sb = ToUtf8(b.File);
                if (sa != sb) return sa < sb;
                return a.Line < b.Line;
            });

        return results;
    }
}