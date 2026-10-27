#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <sstream>

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
                std::string buf(size, 0);              // size, не size-1 — сюда пишется ровно size байт с '\0'
                WideCharToMultiByte(CP_UTF8, 0, w, -1, &buf[0], size, nullptr, nullptr);
                buf.resize(size - 1);
                result = buf;
            }
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
    return result;
#else
    return "";
#endif
}

bool CopyAndRemember(const std::string& text, const std::string& source) {
    std::string oldText = ReadClipboard();
    bool ok = CopyToClipboard(text);
    if (ok && !oldText.empty() && oldText != text) {
        settings::AddClipboardEntry(oldText, source);
        settings::SaveClipboardHistory();
    }
    return ok;
}

void ShowResults(const std::vector<file_searcher::SearchResult>& results, const file_searcher::SearchOptions& options, int limit) {
    if (results.empty()) return;
    size_t shown = std::min((size_t)limit, results.size());
    for (size_t i = 0; i < shown; ++i) {
        std::string rel;
        try { rel = ToUtf8(fs::relative(results[i].File, options.Directory)); }
        catch (...) { rel = ToUtf8(results[i].File); }
        if (settings::ColoredOutput) {
            std::cout << colors::File << rel << colors::Reset << ":" << colors::LineNumber << results[i].Line << colors::Reset << ": " << colors::MatchText << results[i].Text << colors::Reset << "\n";
        } else {
            std::cout << rel << ":" << results[i].Line << ": " << results[i].Text << "\n";
        }
    }
}

void PrintTotals(const std::vector<file_searcher::SearchResult>& results) {
    std::vector<std::string> uniqueFiles;
    for (const auto& r : results) {
        std::string rel = ToUtf8(r.File);
        if (std::find(uniqueFiles.begin(), uniqueFiles.end(), rel) == uniqueFiles.end()) uniqueFiles.push_back(rel);
    }
    int files = (int)uniqueFiles.size();
    int count = (int)results.size();
    std::cout << colors::Totals << "Найдено файлов: " << files << colors::Reset << "\n";
    std::string color;
    if (count < 100) color = colors::Totals;
    else if (count < 500) color = colors::Warning;
    else color = colors::Error;
    std::cout << color << "Всего совпадений: " << count << colors::Reset << "\n";
}

static std::string DescribeRequest(const regex_builder::SearchRequest& req) {
    switch (req.Type) {
        case regex_builder::SearchType::Word: return "Слово: " + req.Word;
        case regex_builder::SearchType::Format: return "Формат";
        case regex_builder::SearchType::Structure: return "Структура: X=" + req.X + ", Y=" + req.Y;
        case regex_builder::SearchType::Combination: return "Комбинирование";
        case regex_builder::SearchType::Quantity: return "Количество";
        case regex_builder::SearchType::Comment: return "Комментарии";
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
    h.QuantityType = (int)req.Quantity;
    h.NumberCompareValue = (int)req.NumberCompareValue;
    h.MinCount = req.MinCount; h.MaxCount = req.MaxCount;
    h.CommentType = (int)req.Comment;
    h.CommentWord = req.CommentWord;
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
    f.QuantityType = (int)req.Quantity;
    f.NumberCompareValue = (int)req.NumberCompareValue;
    f.MinCount = req.MinCount; f.MaxCount = req.MaxCount;
    f.CommentType = (int)req.Comment;
    f.CommentWord = req.CommentWord;
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
    req.Quantity = (regex_builder::QuantityType)h.QuantityType;
    req.NumberCompareValue = (regex_builder::NumberCompare)h.NumberCompareValue;
    req.MinCount = h.MinCount; req.MaxCount = h.MaxCount;
    req.Comment = (regex_builder::CommentType)h.CommentType;
    req.CommentWord = h.CommentWord;
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
    req.Quantity = (regex_builder::QuantityType)f.QuantityType;
    req.NumberCompareValue = (regex_builder::NumberCompare)f.NumberCompareValue;
    req.MinCount = f.MinCount; req.MaxCount = f.MaxCount;
    req.Comment = (regex_builder::CommentType)f.CommentType;
    req.CommentWord = f.CommentWord;
}

static void ShowHistoryList() {
    std::cout << "\n=== ИСТОРИЯ ===\n\n";
    size_t N = settings::History.size();
    for (size_t i = N; i > 0; --i) {
        std::cout << "  " << i << ". " << settings::History[i - 1].Description << "\n";
        std::cout << "     " << settings::History[i - 1].Pattern << "\n\n";
    }
}

static void ShowFavoritesList() {
    std::cout << "\n=== ИЗБРАННОЕ ===\n\n";
    size_t N = settings::Favorites.size();
    for (size_t i = N; i > 0; --i) {
        std::cout << "  " << i << ". " << settings::Favorites[i - 1].Name << "\n";
        std::cout << "     " << settings::Favorites[i - 1].Pattern << "\n\n";
    }
}

static void ShowClipboardList() {
    std::cout << "\n=== ИСТОРИЯ БУФЕРА ===\n\n";
    size_t N = settings::ClipboardHistory.size();
    for (size_t i = N; i > 0; --i) {
        const auto& c = settings::ClipboardHistory[i - 1];
        std::string preview = c.Text;
        if (preview.size() > 60) preview = preview.substr(0, 60) + "...";
        for (auto& ch : preview) if (ch == '\n' || ch == '\r') ch = ' ';
        std::cout << "  " << i << ". [" << c.Time << "] (" << c.Source << ")\n";
        std::cout << "     " << preview << "\n\n";
    }
}

void PrintActionMenu(bool isInFavorites) {
    std::cout << colors::MenuHeader << "Выберите действие:" << colors::Reset << "\n";
    std::cout << "  " << colors::MenuNumber << "1." << colors::Reset << " Изменить условия\n";
    std::cout << "  " << colors::MenuNumber << "2." << colors::Reset << " Скопировать в буфер\n";
    std::cout << "  " << colors::MenuNumber << "3." << colors::Reset << " Начать поиск\n";
    std::cout << "  " << colors::MenuNumber << "4." << colors::Reset << " Избранное\n";
    std::cout << "  " << colors::MenuNumber << "5." << colors::Reset << " Добавить в избранное\n";
    std::cout << "  " << colors::MenuNumber << "6." << colors::Reset << " История поисков\n";
    std::cout << "  " << colors::MenuNumber << "7." << colors::Reset << " История буфера\n";
    std::cout << "  " << colors::MenuNumber << "8." << colors::Reset << " Настройки\n";
    if (isInFavorites) std::cout << "  " << colors::MenuNumber << "0." << colors::Reset << " Удалить из избранного\n";
    std::cout << "> ";
}

void RunActionMenu(regex_builder::SearchRequest req, std::string pattern) {
    bool restart = false;
    bool printMenu = true;

    auto isPatternInFavorites = [&](const std::string& p) -> bool {
        for (const auto& f : settings::Favorites) if (f.Pattern == p && f.ReqType == (int)req.Type) return true;
        return false;
    };

    while (!restart) {
        if (printMenu) { PrintActionMenu(isPatternInFavorites(pattern)); printMenu = false; }
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
        if (!valid) continue;
        printMenu = true;

        if (choice == "1") { restart = true; }
        else if (choice == "2") {
            bool isArgsLength = (req.Type == regex_builder::SearchType::Quantity && req.Quantity == regex_builder::QuantityType::ArgsLength);
            if (isArgsLength) std::cout << "(нечего копировать — специальный режим)\n\n";
            else {
                if (CopyAndRemember(pattern, "search")) std::cout << "(скопировано в буфер)\n";
                else std::cout << "(буфер недоступен)\n";
                std::cout << "\n";
            }
        }
        else if (choice == "3") {
            file_searcher::SearchOptions options;
            if (!prompt::AskSearchOptions(options, req)) continue;
            std::cout << "\nПоиск...\n\n";
            auto results = file_searcher::Search(req, pattern, options);
            ShowResults(results, options, options.ShowFirstN);
            if ((int)results.size() > options.ShowFirstN) std::cout << "... и ещё " << (results.size() - options.ShowFirstN) << "\n";
            std::cout << "\n";
            PrintTotals(results);
            std::cout << "\n";
        }
        else if (choice == "4") {
            if (settings::Favorites.empty()) {
                std::cout << "\nИзбранное пусто.\n";
                std::cout << "  " << colors::MenuNumber << "1." << colors::Reset << " Перейти в историю поисков\n";
                std::cout << "  " << colors::MenuNumber << "0." << colors::Reset << " Назад\n> ";
                std::string sub;
                std::getline(std::cin, sub);
                size_t sa = sub.find_first_not_of(" \t\r\n");
                size_t sb = sub.find_last_not_of(" \t\r\n");
                if (sa == std::string::npos) continue;
                sub = sub.substr(sa, sb - sa + 1);
                if (sub == "1") {
                    if (settings::History.empty()) { std::cout << "\nИстория пуста.\n\n"; continue; }
                    ShowHistoryList();
                    std::cout << "Введите номер элемента, чтобы перейти в поиск (0 — назад): ";
                    int k = prompt::ReadInt(0, (int)settings::History.size());
                    if (k == 0) continue;
                    auto& h = settings::History[k - 1];
                    pattern = h.Pattern;
                    RestoreRequest(req, h);
                    std::cout << "\nВаше выражение:\n  " << colors::MatchText << pattern << colors::Reset << "\n\n";
                    if (CopyAndRemember(pattern, "history")) std::cout << "(скопировано в буфер)\n";
                    std::cout << "\n";
                }
                std::cout << "\n";
                continue;
            }
            ShowFavoritesList();
            std::cout << "Выберите действие:\n";
            std::cout << "  " << colors::MenuNumber << "1." << colors::Reset << " Перейти в поиск по элементу\n";
            std::cout << "  " << colors::MenuNumber << "2." << colors::Reset << " Удалить из избранного\n";
            std::cout << "  " << colors::MenuNumber << "0." << colors::Reset << " Назад\n> ";
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
                std::cout << "\nВаше выражение:\n  " << colors::MatchText << pattern << colors::Reset << "\n\n";
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
            std::cout << "\nВаше выражение:\n  " << colors::MatchText << pattern << colors::Reset << "\n\n";
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
#endif

    settings::Load("settings.json");
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
            std::cout << "\nВаше выражение:\n  " << colors::MatchText << pattern << colors::Reset << "\n\n";
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
            std::cout << "\nВаше выражение:\n  " << colors::MatchText << pattern << colors::Reset << "\n\n";
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

        settings::HistoryEntry entry;
        FillHistoryEntry(entry, req, pattern);
        settings::AddToHistory(entry);
        settings::SaveHistory();

        if (!isStructure && !isCombination) {
            std::cout << "\nВаше выражение:\n";
            if (isArgsLength) {
                std::cout << "  (специальный режим: поиск по количеству аргументов, regex не используется)\n\n";
            } else {
                std::cout << "  " << colors::MatchText << pattern << colors::Reset << "\n\n";
                if (CopyAndRemember(pattern, "search")) std::cout << "(скопировано в буфер)\n";
                std::cout << "\n";
            }
        }

        if (isStructure || isCombination) {
            file_searcher::SearchOptions options;
            if (prompt::AskSearchOptions(options, req)) {
                std::cout << "\nПоиск...\n\n";
                auto results = file_searcher::Search(req, pattern, options);
                ShowResults(results, options, options.ShowFirstN);
                if ((int)results.size() > options.ShowFirstN) std::cout << "... и ещё " << (results.size() - options.ShowFirstN) << "\n";
                std::cout << "\n";
                PrintTotals(results);
                std::cout << "\n";
            }
        }

        RunActionMenu(req, pattern);
    }

    return 0;
}