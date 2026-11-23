
#include <cstdio>
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <unordered_set>

#include "Colors.h"
#include "Settings.h"
#include "Prompt.h"
#include "RegexBuilder.h"
#include "FileSearcher.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

#include "Utf8.h"
using utf8::ToUtf8;

bool CopyToClipboard(const std::string& text) {
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) return false;
    EmptyClipboard();
    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size * sizeof(wchar_t));
    if (!hMem) { CloseClipboard(); return false; }
    wchar_t* mem = (wchar_t*)GlobalLock(hMem);
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, mem, size);
    GlobalUnlock(hMem);
    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();
    return true;
#elif defined(__APPLE__)
    FILE* pipe = popen("pbcopy", "w");
    if (!pipe) return false;
    size_t written = fwrite(text.data(), 1, text.size(), pipe);
    int status = pclose(pipe);
    return written == text.size() && status == 0;
#elif defined(__linux__)
    const char* cmd = nullptr;
    if (getenv("WAYLAND_DISPLAY"))      cmd = "wl-copy";
    else if (getenv("DISPLAY"))         cmd = "xclip -selection clipboard";
    if (!cmd) return false;
    FILE* pipe = popen(cmd, "w");
    if (!pipe) return false;
    size_t written = fwrite(text.data(), 1, text.size(), pipe);
    int status = pclose(pipe);
    return written == text.size() && status == 0;
#else
    (void)text;
    return false;
#endif
}

std::string ReadClipboard() {
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) return "";
    std::string result;
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        wchar_t* w = (wchar_t*)GlobalLock(hData);
        if (w) {
            int size = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
            if (size > 0) {
                std::string buf(size, 0);
                WideCharToMultiByte(CP_UTF8, 0, w, -1, &buf[0], size, nullptr, nullptr);
                buf.resize(size - 1);
                result = buf;
            }
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
    return result;
#elif defined(__APPLE__)
    FILE* pipe = popen("pbpaste", "r");
    if (!pipe) return "";
    std::string result;
    char buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
        result.append(buffer, n);
    }
    pclose(pipe);
    return result;
#elif defined(__linux__)
    const char* cmd = nullptr;
    if (getenv("WAYLAND_DISPLAY"))      cmd = "wl-paste";
    else if (getenv("DISPLAY"))         cmd = "xclip -selection clipboard -o";
    if (!cmd) return "";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "";
    std::string result;
    char buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
        result.append(buffer, n);
    }
    pclose(pipe);
    return result;
#else
    return "";
#endif
}

bool CopyAndRemember(const std::string& text, const std::string& source) {
    (void)source;   // параметр для совместимости, не используется
    std::string oldText = ReadClipboard();
    bool ok = CopyToClipboard(text);
    if (ok && !oldText.empty() && oldText != text) {
        settings::AddClipboardEntry(oldText, "before_copy");
        settings::SaveClipboardHistory();
    }
    return ok;
}

static std::string PluralMatches(int n);

void ShowResults(const std::vector<file_searcher::SearchResult>& results, const file_searcher::SearchOptions& options, int limit) {
    if (results.empty()) {
        std::cout << theme::Role::Dim << "(ничего не найдено)" << theme::Role::Reset << "\n";
        return;
    }
    size_t shown = std::min((size_t)limit, results.size());
    for (size_t i = 0; i < shown; ++i) {
        std::string rel;
        try { rel = ToUtf8(fs::relative(results[i].File, options.Directory)); }
        catch (...) { rel = ToUtf8(results[i].File); }
        std::cout << theme::Role::FileName << rel << ":" 
            << theme::Role::LineNumber << results[i].Line << ": ";
        if (!results[i].Kind.empty()) 
            std::cout << theme::Role::Dim << "[" << results[i].Kind << "] " << theme::Role::Reset;
        
        std::cout << theme::Role::Match << results[i].Text << theme::Role::Reset;
        if (results[i].MatchCount > 1)
            std::cout << theme::Role::Dim << " (" << results[i].MatchCount << " "
                      << PluralMatches(results[i].MatchCount) << ")" << theme::Role::Reset;
        std::cout << "\n"; 
    }
}

static std::string PluralMatches(int n) {
    int mod100 = n % 100;
    int mod10  = n % 10;
    if (mod100 >= 11 && mod100 <= 14) return "совпадений";
    if (mod10 == 1)                   return "совпадение";
    if (mod10 >= 2 && mod10 <= 4)     return "совпадения";
    return "совпадений";
}

void PrintTotals(const std::vector<file_searcher::SearchResult>& results) {
    std::unordered_set<std::string> uniqueFiles;
    uniqueFiles.reserve(results.size());
    for (const auto& r : results) {
        uniqueFiles.insert(ToUtf8(r.File));
    }
    int files = (int)uniqueFiles.size();
    int count = (int)results.size();
    
    std::cout << theme::Role::Question << "Найдено в файлах: " << theme::Role::StatValue << files << theme::Role::Reset << "\n";
    
    theme::Role countColor = theme::Role::StatValue;
    if (count >= 500) countColor = theme::Role::Error;
    else if (count >= 100) countColor = theme::Role::Warning;
    
    std::cout << theme::Role::Question << "Всего найдено: " << countColor << count << theme::Role::Reset << "\n";
}

static std::string DescribeRequest(const regex_builder::SearchRequest& req) {
    switch (req.Type) {
        case regex_builder::SearchType::Word:
            return "Слово: " + req.Word;
        case regex_builder::SearchType::Format: {
            static const char* n[] = { "", "Email", "URL", "IP", "GUID", "Дата" };
                int i = (int)req.Format;
                return std::string("Формат: ") + (i >= 1 && i <= 5 ? n[i] : "?");
            }
        case regex_builder::SearchType::Structure: {
            if (req.Structure == regex_builder::StructureType::XEmpty) {
                std::string metric;
                switch (req.EmptyMetricValue) {
                    case regex_builder::EmptyMetric::StrictlyEmpty:
                        metric = "строго пусто"; break;
                    case regex_builder::EmptyMetric::MaxChars:
                        metric = "≤ " + std::to_string(req.EmptyMaxCount) + " непроб. символов"; break;
                    case regex_builder::EmptyMetric::MaxLines:
                        metric = "≤ " + std::to_string(req.EmptyMaxCount) + " строк"; break;
                }
                return "Структура: X=" + req.X + " (" + metric + ")";
            }
            return "Структура: X=" + req.X + ", Y=" + req.Y +
                (req.Z.empty() ? "" : ", Z=" + req.Z);
        }
        
        case regex_builder::SearchType::Combination: {
            if (req.Combination == regex_builder::CombinationType::XButNotY) {
                std::string mode;
                switch (req.XButNotYModeValue) {
                    case regex_builder::XButNotYMode::ByPosition: mode = "по позиции"; break;
                    case regex_builder::XButNotYMode::ByLine:     mode = "по строке"; break;
                }
                return "Комбинирование: " + req.X + " но не " + req.Y + " (" + mode + ")";
            }
            return "Комбинирование: " + req.X + " / " + req.Y;
        }

        case regex_builder::SearchType::Quantity: {
            static const char* q[] = { "", "цифр в числе", "Длина строки", "Аргументы" };
            int i = (int)req.Quantity;
            std::string cmp;
            switch (req.NumberCompareValue) {
                case regex_builder::NumberCompare::Greater: cmp = "> "  + std::to_string(req.MinCount); break;
                case regex_builder::NumberCompare::Equal:   cmp = "= "  + std::to_string(req.MinCount); break;
                case regex_builder::NumberCompare::Less:    cmp = "< "  + std::to_string(req.MinCount); break;
                case regex_builder::NumberCompare::Between: cmp = std::to_string(req.MinCount) + ".." + std::to_string(req.MaxCount); break;
            }
            return std::string("Количество: ") + (i >= 1 && i <= 3 ? q[i] : "?") + " " + cmp;
        }
        case regex_builder::SearchType::Comment: {
            std::string mode;
            switch (req.Comment) {
                case regex_builder::CommentType::StartLine:     mode = "в начале строки"; break;
                case regex_builder::CommentType::EndLine:       mode = "в конце строки";  break;
                case regex_builder::CommentType::CommentedCode: mode = "все";             break;
            }
            std::string word = req.CommentWord.empty() ? "(все)" : req.CommentWord;
            return "Комментарии (" + mode + ", " + req.Language + "): " + word;
        }
    }
    return "";
}

static void FillHistoryEntry(settings::HistoryEntry& h, const regex_builder::SearchRequest& req, const std::string& pattern) {
    h.Type = std::to_string((int)req.Type);
    h.Language = req.Language;
    h.Pattern = pattern;
    h.Description = DescribeRequest(req);
    h.ReqType = (int)req.Type;
    h.Word = req.Word;
    h.WordWholeOnly = req.WordWholeOnly;
    h.FormatType = (int)req.Format;
    h.X = req.X; h.Y = req.Y; h.Z = req.Z;
    h.StructureType = (int)req.Structure;
    h.Exclude = req.Exclude;
    h.CombinationType = (int)req.Combination;
    h.XButNotYModeValue = (int)req.XButNotYModeValue;
    h.QuantityType = (int)req.Quantity;
    h.NumberCompareValue = (int)req.NumberCompareValue;
    h.MinCount = req.MinCount; h.MaxCount = req.MaxCount;
    h.CommentType = (int)req.Comment;
    h.CommentWord = req.CommentWord;
    h.EmptyMetricValue = (int)req.EmptyMetricValue;
    h.EmptyMaxCount    = req.EmptyMaxCount;
}

static void FillFavoriteEntry(settings::FavoriteEntry& f, const regex_builder::SearchRequest& req, const std::string& pattern, const std::string& name) {
    f.Name = name;
    f.Type = std::to_string((int)req.Type);
    f.Language = req.Language;
    f.Pattern = pattern;
    f.Description = DescribeRequest(req);
    f.ReqType = (int)req.Type;
    f.Word = req.Word;
    f.WordWholeOnly = req.WordWholeOnly;
    f.FormatType = (int)req.Format;
    f.X = req.X; f.Y = req.Y; f.Z = req.Z;
    f.StructureType = (int)req.Structure;
    f.Exclude = req.Exclude;
    f.CombinationType = (int)req.Combination;
    f.XButNotYModeValue = (int)req.XButNotYModeValue;
    f.QuantityType = (int)req.Quantity;
    f.NumberCompareValue = (int)req.NumberCompareValue;
    f.MinCount = req.MinCount; f.MaxCount = req.MaxCount;
    f.CommentType = (int)req.Comment;
    f.CommentWord = req.CommentWord;
     f.EmptyMetricValue = (int)req.EmptyMetricValue;
    f.EmptyMaxCount    = req.EmptyMaxCount;
}

static void RestoreRequest(regex_builder::SearchRequest& req, const settings::HistoryEntry& h) {
    req.Type = (regex_builder::SearchType)h.ReqType;
    req.Language = h.Language;
    req.Word = h.Word;
    req.WordWholeOnly = h.WordWholeOnly;
    req.Format = (patterns::FormatType)h.FormatType;
    req.X = h.X; req.Y = h.Y; req.Z = h.Z;
    req.Structure = (regex_builder::StructureType)h.StructureType;
    req.Exclude = h.Exclude;
    req.Combination = (regex_builder::CombinationType)h.CombinationType;
    req.XButNotYModeValue = (regex_builder::XButNotYMode)h.XButNotYModeValue;
    req.Quantity = (regex_builder::QuantityType)h.QuantityType;
    req.NumberCompareValue = (regex_builder::NumberCompare)h.NumberCompareValue;
    req.MinCount = h.MinCount; req.MaxCount = h.MaxCount;
    req.Comment = (regex_builder::CommentType)h.CommentType;
    req.CommentWord = h.CommentWord;
    req.EmptyMetricValue = (regex_builder::EmptyMetric)h.EmptyMetricValue;
    req.EmptyMaxCount    = h.EmptyMaxCount;
}

static void RestoreRequest(regex_builder::SearchRequest& req, const settings::FavoriteEntry& f) {
    req.Type = (regex_builder::SearchType)f.ReqType;
    req.Language = f.Language;
    req.Word = f.Word;
    req.WordWholeOnly = f.WordWholeOnly;
    req.Format = (patterns::FormatType)f.FormatType;
    req.X = f.X; req.Y = f.Y; req.Z = f.Z;
    req.Structure = (regex_builder::StructureType)f.StructureType;
    req.Exclude = f.Exclude;
    req.Combination = (regex_builder::CombinationType)f.CombinationType;
    req.XButNotYModeValue = (regex_builder::XButNotYMode)f.XButNotYModeValue;
    req.Quantity = (regex_builder::QuantityType)f.QuantityType;
    req.NumberCompareValue = (regex_builder::NumberCompare)f.NumberCompareValue;
    req.MinCount = f.MinCount; req.MaxCount = f.MaxCount;
    req.Comment = (regex_builder::CommentType)f.CommentType;
    req.CommentWord = f.CommentWord;
    req.EmptyMetricValue = (regex_builder::EmptyMetric)f.EmptyMetricValue;
    req.EmptyMaxCount    = f.EmptyMaxCount;
}

static void ShowHistoryList() {
    std::cout << "\n" << theme::Role::Title << "=== ИСТОРИЯ ===\n\n" << theme::Role::Reset;
    size_t N = settings::History.size();
    for (size_t i = N; i > 0; --i) {
        std::cout << "  " << theme::Role::MenuNum << i << "." << theme::Role::Reset << " " << settings::History[i - 1].Description << "\n";
        std::cout << "     " << theme::Role::Match << settings::History[i - 1].Pattern << theme::Role::Reset << "\n\n";
    }
}

static void ShowFavoritesList() {
    std::cout << "\n" << theme::Role::Title << "=== ИЗБРАННОЕ ===\n\n" << theme::Role::Reset;
    size_t N = settings::Favorites.size();
    for (size_t i = N; i > 0; --i) {
        std::cout << "  " << theme::Role::MenuNum << i << "." << theme::Role::Reset << " " << settings::Favorites[i - 1].Name << "\n";
        std::cout << "     " << theme::Role::Match << settings::Favorites[i - 1].Pattern << theme::Role::Reset << "\n\n";
    }
}

static void ShowClipboardList() {
    std::cout << "\n=== ИСТОРИЯ БУФЕРА ===\n\n";
    size_t N = settings::ClipboardHistory.size();
    for (size_t i = N; i > 0; --i) {
        const auto& c = settings::ClipboardHistory[i - 1];
        std::string preview = c.Text;
        if (preview.size() > 60) {
            size_t cut = utf8::SafeCut(preview, 60);
            preview = preview.substr(0, cut) + "...";
        }
        for (auto& ch : preview) if (ch == '\n' || ch == '\r') ch = ' ';
        std::cout << "  " << theme::Role::MenuNum << i << "." << theme::Role::FileName << " [" << c.Time << "] (" << c.Source << ")" << theme::Role::Reset << "\n";
        std::cout << "     " << theme::Role::Match << preview << theme::Role::Reset << "\n\n";
    }
}

static bool SaveResultsToFile(const std::vector<file_searcher::SearchResult>& results,
                              const file_searcher::SearchOptions& options,
                              const fs::path& path,
                              const std::string& description) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    out << "=== CodeSearch ===\n";
    out << "Дата: " << settings::NowString() << "\n";
    out << "Условия: " << description << "\n";
    out << "Найдено: " << results.size() << " совпадений\n";
    out << "------------------------------------------------------------\n";

    for (const auto& r : results) {
        std::string rel;
        try { rel = ToUtf8(fs::relative(r.File, options.Directory)); }
        catch (...) { rel = ToUtf8(r.File); }
        out << rel << ":" << r.Line << ": ";
        if (!r.Kind.empty()) out << "[" << r.Kind << "] ";
        out << r.Text;
        if (r.MatchCount > 1) out << " (" << r.MatchCount << " " << PluralMatches(r.MatchCount) << ")";
        out << "\n";
    }
    return true;
}

void PrintActionMenu(bool isInFavorites, bool hasResults) {
    std::cout << theme::Role::Question << "Выберите действие:\n" << theme::Role::Reset;
    std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Default << " Изменить условия\n";
    std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Default << " Скопировать в буфер\n";
    std::cout << "  " << theme::Role::MenuNum << "3." << theme::Role::Default << " Начать поиск\n";
    std::cout << "  " << theme::Role::MenuNum << "4." << theme::Role::Default << " Избранное\n";
    std::cout << "  " << theme::Role::MenuNum << "5." << theme::Role::Default << " Добавить в избранное\n";
    std::cout << "  " << theme::Role::MenuNum << "6." << theme::Role::Default << " История поисков\n";
    std::cout << "  " << theme::Role::MenuNum << "7." << theme::Role::Default << " История буфера\n";
    std::cout << "  " << theme::Role::MenuNum << "8." << theme::Role::Default << " Настройки\n";
    if (isInFavorites) std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Удалить из избранного\n";
    if (hasResults)   std::cout << "  " << theme::Role::MenuNum << "9." << theme::Role::Default << " Выписать в файл\n";
    std::cout << "> ";
}

void RunActionMenu(regex_builder::SearchRequest req, std::string pattern) {
    bool restart = false;
    bool printMenu = true;

    std::vector<file_searcher::SearchResult> lastResults;
    file_searcher::SearchOptions lastOptions;
    bool hasResults = false;

    auto isPatternInFavorites = [&](const std::string& p) -> bool {
        for (const auto& f : settings::Favorites) if (f.Pattern == p && f.ReqType == (int)req.Type) return true;
        return false;
    };

    while (!restart) {
        if (printMenu) { PrintActionMenu(isPatternInFavorites(pattern), hasResults); printMenu = false; }
        else std::cout << "> ";

        std::string choice;
        std::getline(std::cin, choice);
        size_t s = choice.find_first_not_of(" \t\r\n");
        size_t e = choice.find_last_not_of(" \t\r\n");
        if (s == std::string::npos) continue;
        choice = choice.substr(s, e - s + 1);

        bool inFav = isPatternInFavorites(pattern);
        bool valid = (choice == "1" || choice == "2" || choice == "3" || choice == "4" ||
            choice == "5" || choice == "6" || choice == "7" || choice == "8");
        if (inFav && choice == "0") valid = true;
        if (hasResults && choice == "9") valid = true;
        if (!valid) continue;
        printMenu = true;

        if (choice == "1") { restart = true; }
        else if (choice == "2") {
            bool isArgsLength  = (req.Type == regex_builder::SearchType::Quantity && req.Quantity == regex_builder::QuantityType::ArgsLength);
            bool isLineLength  = (req.Type == regex_builder::SearchType::Quantity && req.Quantity == regex_builder::QuantityType::LineLength);
            bool isEmptyBlocks = (req.Type == regex_builder::SearchType::Structure && req.Structure == regex_builder::StructureType::XEmpty);
            bool isXButNotY    = (req.Type == regex_builder::SearchType::Combination && req.Combination == regex_builder::CombinationType::XButNotY);
            bool isComments    = (req.Type == regex_builder::SearchType::Comment);
            if (isArgsLength || isLineLength || isEmptyBlocks || isXButNotY || isComments)
                std::cout << theme::Role::Dim << "(специальный режим — regex не используется)\n\n" << theme::Role::Reset;
            else {
                if (CopyAndRemember(pattern, "search")) std::cout << theme::Role::Question << "(скопировано в буфер)\n" << theme::Role::Reset;
                else std::cout << theme::Role::Error << "(буфер недоступен)\n" << theme::Role::Reset;
                std::cout << "\n";
            }
        }
        else if (choice == "3") {
            file_searcher::SearchOptions options;
            if (!prompt::AskSearchOptions(options, req)) continue;
            size_t fileCount = 0;
            size_t skippedCount = 0;
            auto results = file_searcher::Search(req, pattern, options, &fileCount, &skippedCount);
                std::cout << theme::Role::Question << "Найдено файлов: " << theme::Role::StatValue << fileCount << theme::Role::Reset << "\n";
            if (skippedCount > 0) {
                std::cout << theme::Role::Warning << "(Пропущено файлов из-за ошибок доступа: " << skippedCount << ")\n" << theme::Role::Reset;
            }
            std::cout << "\n";
            ShowResults(results, options, options.ShowFirstN);
            if ((int)results.size() > options.ShowFirstN) std::cout << "... и ещё " << (results.size() - options.ShowFirstN) << "\n";
            std::cout << "\n";
            PrintTotals(results);
            std::cout << "\n";

            lastResults = results;
            lastOptions = options;
            hasResults = true;
        }
        else if (choice == "4") {
            if (settings::Favorites.empty()) {
                                std::cout << "\n" << theme::Role::Warning << "Избранное пусто.\n" << theme::Role::Reset;
                std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Reset << " Перейти в историю поисков\n";
                std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Reset << " Назад\n> ";
                std::string sub;
                std::getline(std::cin, sub);
                size_t sa = sub.find_first_not_of(" \t\r\n");
                size_t sb = sub.find_last_not_of(" \t\r\n");
                if (sa == std::string::npos) continue;
                sub = sub.substr(sa, sb - sa + 1);
                if (sub == "1") {
                    if (settings::History.empty()) { std::cout << "\nИстория пуста.\n\n"; continue; }
                    ShowHistoryList();
                    std::cout << theme::Role::Question << "Введите номер элемента, чтобы перейти в поиск (0 — назад): " << theme::Role::Reset;
                    int k = prompt::ReadInt(0, (int)settings::History.size());
                    if (k == 0) continue;
                    auto& h = settings::History[k - 1];
                    pattern = h.Pattern;
                    RestoreRequest(req, h);
                    hasResults = false;
                    lastResults.clear();
                    std::cout << "\n" << theme::Role::Question << "Ваше выражение:\n  " << theme::Role::Match << pattern << theme::Role::Reset << "\n\n";
                    if (CopyAndRemember(pattern, "history")) std::cout << "(скопировано в буфер)\n";
                    std::cout << "\n";
                }
                std::cout << "\n";
                continue;
            }
            ShowFavoritesList();
            std::cout << "Выберите действие:\n";
            std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Reset << " Перейти в поиск по элементу\n";
            std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Reset << " Удалить из избранного\n";
            std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Reset << " Назад\n> ";
            std::string sub;
            std::getline(std::cin, sub);
            size_t sa = sub.find_first_not_of(" \t\r\n");
            size_t sb = sub.find_last_not_of(" \t\r\n");
            if (sa == std::string::npos) continue;
            sub = sub.substr(sa, sb - sa + 1);
            if (sub == "0") continue;
            if (sub == "1") {
                std::cout << "Введите номер элемента (0 — назад): ";
                int k = prompt::ReadInt(0, (int)settings::Favorites.size());
                if (k == 0) continue;
                auto& f = settings::Favorites[k - 1];
                pattern = f.Pattern;
                RestoreRequest(req, f);
                hasResults = false;
                lastResults.clear();
                std::cout << "\nВаше выражение:\n  " << theme::Role::Match << pattern << theme::Role::Reset << "\n\n";
                if (CopyAndRemember(pattern, "favorite")) std::cout << "(скопировано в буфер)\n";
                std::cout << "\n";
            } else if (sub == "2") {
                std::cout << "\nВведите номер для удаления (0 — назад): ";
                int k = prompt::ReadInt(0, (int)settings::Favorites.size());
                if (k == 0) continue;
                std::string removedName = settings::Favorites[k - 1].Name;
                settings::Favorites.erase(settings::Favorites.begin() + (k - 1));
                settings::SaveFavorites();
                std::cout << "\nУдалено из избранного: " << removedName << "\n\n";
            }
        }
        else if (choice == "5") {
            settings::FavoriteEntry f;
            std::cout << "\nИмя записи (Enter — название по умолчанию, 0 — назад): ";
            std::string name;
            std::getline(std::cin, name);
            size_t a = name.find_first_not_of(" \t\r\n");
            size_t b = name.find_last_not_of(" \t\r\n");
            if (a == std::string::npos) name = DescribeRequest(req);
            else {
                name = name.substr(a, b - a + 1);
                if (name == "0") continue;
            }
            FillFavoriteEntry(f, req, pattern, name);
            settings::AddToFavorites(f);
            settings::SaveFavorites();
            std::cout << "\nДобавлено в избранное: " << f.Name << "\n\n";
        }
        else if (choice == "6") {
            if (settings::History.empty()) { std::cout << "\nИстория пуста.\n\n"; continue; }
            ShowHistoryList();
            std::cout << "Введите номер элемента, чтобы перейти в поиск (0 — назад): ";
            int k = prompt::ReadInt(0, (int)settings::History.size());
            if (k == 0) continue;
            auto& h = settings::History[k - 1];
            pattern = h.Pattern;
            RestoreRequest(req, h);
            hasResults = false;
            lastResults.clear();
            std::cout << "\nВаше выражение:\n  " << theme::Role::Match << pattern << theme::Role::Reset << "\n\n";
            if (CopyAndRemember(pattern, "history")) std::cout << "(скопировано в буфер)\n";
            std::cout << "\n";
        }
        else if (choice == "7") {
            if (settings::ClipboardHistory.empty()) { std::cout << "\nИстория буфера пуста.\n\n"; continue; }
            ShowClipboardList();
            std::cout << "Восстановить в буфер? (номер, 0 — назад): ";
            int k = prompt::ReadInt(0, (int)settings::ClipboardHistory.size());
            if (k == 0) continue;
            if (CopyToClipboard(settings::ClipboardHistory[k - 1].Text)) std::cout << "\nВосстановлено в буфер.\n\n";
            else std::cout << "\nБуфер недоступен.\n\n";
        }
        else if (choice == "8") { prompt::EditSettings(); }

        else if (choice == "9" && hasResults) {
            std::string saveFile = settings::ResultsFile;
            fs::path saveDir = fs::current_path();

            while (true) {
                std::cout << "\n" << theme::Role::Question << "Выписать результаты в файл:\n" << theme::Role::Reset;
                std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Reset << " Сохранить\n";
                std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Reset << " Название файла: " << saveFile << "\n";
                std::cout << "  " << theme::Role::MenuNum << "3." << theme::Role::Reset << " Директория: " << utf8::ToUtf8(saveDir) << "\n";
                std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Reset << " Назад\n> ";

                std::string sub;
                std::getline(std::cin, sub);
                size_t ss = sub.find_first_not_of(" \t\r\n");
                size_t se = sub.find_last_not_of(" \t\r\n");
                if (ss == std::string::npos) continue;
                sub = sub.substr(ss, se - ss + 1);

                if (sub == "0") break;

                if (sub == "1") {
                    fs::path fullPath = saveDir / utf8::FromUtf8(saveFile);
                    if (fs::exists(fullPath)) {
                        std::cout << "Файл " << saveFile << " уже существует. Перезаписать? (y/n): ";
                        std::string ans;
                        std::getline(std::cin, ans);
                        for (auto& c : ans) c = (char)std::tolower((unsigned char)c);
                        if (ans != "y" && ans != "yes" && ans != "да") continue;
                    }
                    if (SaveResultsToFile(lastResults, lastOptions, fullPath, DescribeRequest(req))) {
                        std::cout << "\n" << theme::Role::Question << "Сохранено в " << theme::Role::FileName << utf8::ToUtf8(fullPath) << theme::Role::Question << " (" << theme::Role::StatValue << lastResults.size() << theme::Role::Question << " строк).\n\n" << theme::Role::Reset;
                        break;
                    } else {
                        std::cout << theme::Role::Error << "Не удалось сохранить файл.\n" << theme::Role::Reset << "\n";
                    }
                }
                else if (sub == "2") {
                    auto name = prompt::ReadLine("Новое название файла (0 — назад): ");
                    if (!prompt::GoBack && !name.empty()) saveFile = name;
                }
                else if (sub == "3") {
                    auto dir = prompt::ReadLine("Новая директория (Enter — текущая, 0 — назад): ");
                    if (prompt::GoBack) continue;
                    if (dir.empty()) {
                        saveDir = fs::current_path();
                    } else {
                        fs::path newDir = utf8::FromUtf8(dir);
                        if (fs::exists(newDir) && fs::is_directory(newDir)) saveDir = newDir;
                        else std::cout << theme::Role::Error << "Директория не найдена.\n" << theme::Role::Reset << "\n";
                    }
                }
            }
        }

        else if (choice == "0" && inFav) {
            size_t foundIdx = SIZE_MAX;
            std::string removedName;
            for (size_t i = 0; i < settings::Favorites.size(); ++i) {
                if (settings::Favorites[i].Pattern == pattern && settings::Favorites[i].ReqType == (int)req.Type) {
                    foundIdx = i; removedName = settings::Favorites[i].Name; break;
                }
            }
            if (foundIdx == SIZE_MAX) {
                std::cout << "\nНе найдено в избранном.\n\n";
            } else {
                settings::Favorites.erase(settings::Favorites.begin() + foundIdx);
                settings::SaveFavorites();
                std::cout << "\nУдалено из избранного: " << removedName << "\n\n";
            }
        }
    }
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Включить обработку ANSI-кодов (цветов) в Windows Console
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode))
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif

    settings::Load("settings.json");
    theme::SetTestTheme();
    settings::LoadHistory();
    settings::LoadFavorites();
    settings::LoadClipboardHistory();
    settings::CleanClipboardHistory(24);
    settings::SaveClipboardHistory();

    while (true) {
        auto req = prompt::Run();

        if (prompt::WantHistory) {
            prompt::WantHistory = false;
            if (settings::History.empty()) { std::cout << "\nИстория пуста.\n\n"; continue; }
            ShowHistoryList();
            std::cout << "Введите номер элемента, чтобы перейти в поиск (0 — назад): ";
            int k = prompt::ReadInt(0, (int)settings::History.size());
            if (k == 0) continue;
            auto& h = settings::History[k - 1];
            std::string pattern = h.Pattern;
            RestoreRequest(req, h);
            std::cout << "\nВаше выражение:\n  " << theme::Role::Match << pattern << theme::Role::Reset << "\n\n";
            if (CopyAndRemember(pattern, "history")) std::cout << "(скопировано в буфер)\n";
            std::cout << "\n";
            RunActionMenu(req, pattern);
            continue;
        }

        if (prompt::WantFavorites) {
            prompt::WantFavorites = false;
            if (settings::Favorites.empty()) { std::cout << "\nИзбранное пусто.\n\n"; continue; }
            ShowFavoritesList();
            std::cout << "Введите номер элемента, чтобы перейти в поиск (0 — назад): ";
            int k = prompt::ReadInt(0, (int)settings::Favorites.size());
            if (k == 0) continue;
            auto& f = settings::Favorites[k - 1];
            std::string pattern = f.Pattern;
            RestoreRequest(req, f);
            std::cout << "\nВаше выражение:\n  " << theme::Role::Match << pattern << theme::Role::Reset << "\n\n";
            if (CopyAndRemember(pattern, "favorite")) std::cout << "(скопировано в буфер)\n";
            std::cout << "\n";
            RunActionMenu(req, pattern);
            continue;
        }

        if (prompt::WantClipboardHistory) {
            prompt::WantClipboardHistory = false;
            if (settings::ClipboardHistory.empty()) { std::cout << "\nИстория буфера пуста.\n\n"; continue; }
            ShowClipboardList();
            std::cout << "Восстановить в буфер? (номер, 0 — назад): ";
            int k = prompt::ReadInt(0, (int)settings::ClipboardHistory.size());
            if (k == 0) continue;
            if (CopyToClipboard(settings::ClipboardHistory[k - 1].Text)) std::cout << "\nВосстановлено в буфер.\n\n";
            else std::cout << "\nБуфер недоступен.\n\n";
            continue;
        }

        auto pattern = regex_builder::Build(req);

        bool isStructure = (req.Type == regex_builder::SearchType::Structure);
        bool isCombination = (req.Type == regex_builder::SearchType::Combination);
        bool isArgsLength = (req.Type == regex_builder::SearchType::Quantity && req.Quantity == regex_builder::QuantityType::ArgsLength);
        bool isComments = (req.Type == regex_builder::SearchType::Comment);

        settings::HistoryEntry entry;
        FillHistoryEntry(entry, req, pattern);
        settings::AddToHistory(entry);
        settings::SaveHistory();

        if (!isStructure && !isCombination) {
            std::cout << "\n" << theme::Role::Question << "Ваше выражение:\n" << theme::Role::Reset;
            if (isArgsLength) {
                std::cout << "  (специальный режим: поиск по количеству аргументов, regex не используется)\n\n";
            } else if (req.Type == regex_builder::SearchType::Quantity && req.Quantity == regex_builder::QuantityType::LineLength) {
                std::cout << "  (специальный режим: поиск по длине строки в символах UTF-8, regex не используется)\n\n";
            } else if (isComments) {
                std::cout << "  (специальный режим: поиск по комментариям, regex не используется)\n\n";
            } else {
                std::cout << "  " << theme::Role::Match << pattern << theme::Role::Reset << "\n\n";
                if (CopyAndRemember(pattern, "search")) std::cout << "(скопировано в буфер)\n";
                std::cout << "\n";
            }
        }

        bool isStructureXYZ = (req.Type == regex_builder::SearchType::Structure &&
                        req.Structure == regex_builder::StructureType::XAnywhereBeforeYBeforeZ);
        bool isCombinationXOrY = (req.Type == regex_builder::SearchType::Combination &&
                                  req.Combination == regex_builder::CombinationType::XOrY);

        if (isStructureXYZ || isCombinationXOrY) {
            std::cout << "\nВаше выражение:\n";
            std::cout << "  " << theme::Role::Match << pattern << theme::Role::Reset << "\n\n";
            if (CopyAndRemember(pattern, "search")) std::cout << "(скопировано в буфер)\n";
            std::cout << "\n";
        }

        RunActionMenu(req, pattern);
    }

    return 0;
}