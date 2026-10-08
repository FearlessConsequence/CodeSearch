#include <iostream>
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

// ============ КОНВЕРТАЦИЯ PATH → UTF-8 ============

static std::string ToUtf8(const fs::path& p) {
#if __cplusplus >= 202002L
    auto u8 = p.u8string();
    return std::string(u8.begin(), u8.end());
#else
    return p.u8string();
#endif
}

// ============ БУФЕР ОБМЕНА ============

bool CopyToClipboard(const std::string& text) {
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) return false;
    EmptyClipboard();

    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size * sizeof(wchar_t));
    if (!hMem) {
        CloseClipboard();
        return false;
    }

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

// ============ ВЫВОД РЕЗУЛЬТАТОВ ============

void ShowResults(
    const std::vector<file_searcher::SearchResult>& results,
    const file_searcher::SearchOptions& options,
    int limit)
{
    if (results.empty()) return;

    size_t shown = std::min((size_t)limit, results.size());

    std::vector<std::string> paths;
    size_t maxPathLen = 0;
    size_t maxLineLen = 0;

    for (size_t i = 0; i < shown; ++i) {
        std::string rel;
        try {
            auto relPath = fs::relative(results[i].File, options.Directory);
            rel = ToUtf8(relPath);
        }
        catch (...) {
            rel = ToUtf8(results[i].File);
        }
        paths.push_back(rel);
        maxPathLen = std::max(maxPathLen, rel.size());

        std::string lineStr = std::to_string(results[i].Line);
        maxLineLen = std::max(maxLineLen, lineStr.size());
    }

    for (size_t i = 0; i < shown; ++i) {
        std::string pathPadded = paths[i];
        pathPadded.append(maxPathLen - paths[i].size(), ' ');

        std::string lineStr = std::to_string(results[i].Line);
        std::string linePadded;
        linePadded.append(maxLineLen - lineStr.size(), ' ');
        linePadded += lineStr;

        if (settings::ColoredOutput) {
            std::cout << colors::File << pathPadded << colors::Reset
                      << " : "
                      << colors::LineNumber << linePadded << colors::Reset
                      << " : "
                      << colors::MatchText << results[i].Text << colors::Reset
                      << "\n";
        } else {
            std::cout << pathPadded << " : " << linePadded
                      << " : " << results[i].Text << "\n";
        }
    }
}

void PrintTotals(const std::vector<file_searcher::SearchResult>& results) {
    std::vector<std::string> uniqueFiles;
    for (const auto& r : results) {
        std::string rel = ToUtf8(r.File);
        if (std::find(uniqueFiles.begin(), uniqueFiles.end(), rel) == uniqueFiles.end())
            uniqueFiles.push_back(rel);
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

// ============ МЕНЮ ============

void PrintMenu(bool hasResults) {
    std::cout << colors::MenuHeader << "Выберите действие:" << colors::Reset << "\n";
    std::cout << "  " << colors::MenuNumber << "1." << colors::Reset << " Изменить условия\n";
    std::cout << "  " << colors::MenuNumber << "2." << colors::Reset << " Скопировать в буфер\n";
    std::cout << "  " << colors::MenuNumber << "3." << colors::Reset << " Переписать с учётом регистра\n";
    std::cout << "  " << colors::MenuNumber << "4." << colors::Reset << " Начать поиск\n";
    std::cout << "  " << colors::MenuNumber << "5." << colors::Reset << " Показать настройки\n";

    if (hasResults) {
        std::cout << "  " << colors::MenuNumber << "6." << colors::Reset << " Показать все в терминале\n";
        std::cout << "  " << colors::MenuNumber << "7." << colors::Reset << " Выписать в файл\n";
    }

    std::cout << "  " << colors::MenuNumber << "0." << colors::Reset << " Выход\n";
    std::cout << "> ";
}

// ============ ГЛАВНАЯ ============

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    while (true) {
        auto req = prompt::Run();
        auto pattern = regex_builder::Build(req);

        std::cout << "\n";
        std::cout << "Ваше выражение:\n";
        std::cout << "  " << colors::MatchText << pattern << colors::Reset << "\n";
        std::cout << "\n";

        if (CopyToClipboard(pattern))
            std::cout << "(скопировано в буфер)\n";
        std::cout << "\n";

        bool restart = false;
        std::vector<file_searcher::SearchResult> lastResults;
        file_searcher::SearchOptions lastOptions;
        bool hasLastResults = false;

        while (!restart) {
            PrintMenu(hasLastResults);

            std::string choice;
            std::getline(std::cin, choice);
            size_t s = choice.find_first_not_of(" \t\r\n");
            size_t e = choice.find_last_not_of(" \t\r\n");
            if (s == std::string::npos) continue;
            choice = choice.substr(s, e - s + 1);

            if (choice == "0") return 0;

            if (choice == "1") {
                restart = true;
            }
            else if (choice == "2") {
                if (CopyToClipboard(pattern))
                    std::cout << "(скопировано в буфер)\n";
                else
                    std::cout << "(буфер недоступен)\n";
                std::cout << "\n";
            }
            else if (choice == "3") {
                auto ignorePattern = "(?i)" + pattern;
                std::cout << "\n";
                std::cout << "Ваше выражение с учётом регистра:\n";
                std::cout << "  " << colors::MatchText << ignorePattern << colors::Reset << "\n";
                std::cout << "\n";

                if (CopyToClipboard(ignorePattern))
                    std::cout << "(скопировано в буфер)\n";
                std::cout << "\n";
            }
            else if (choice == "4") {
                file_searcher::SearchOptions options;
                if (!prompt::AskSearchOptions(options)) continue;

                std::cout << "\nПоиск...\n\n";

                auto results = file_searcher::Search(req, pattern, options);

                lastResults = results;
                lastOptions = options;
                hasLastResults = !results.empty();

                ShowResults(results, options, options.ShowFirstN);

                if ((int)results.size() > options.ShowFirstN)
                    std::cout << "... и ещё " << (results.size() - options.ShowFirstN) << "\n";

                std::cout << "\n";
                PrintTotals(results);
                std::cout << "\n";
            }
            else if (choice == "5") {
                std::cout << "\n=== ТЕКУЩИЕ НАСТРОЙКИ ===\n\n";
                std::cout << "Вывод:\n";
                std::cout << "  Показывать первых: " << settings::ShowFirstN << "\n";
                std::cout << "  Цветной: " << (settings::ColoredOutput ? "да" : "нет") << "\n";
                std::cout << "  Файл результатов: " << settings::ResultsFile << "\n";
                std::cout << "\nEnter — продолжить...";
                std::cin.get();
            }
            else if (choice == "6" && hasLastResults) {
                std::cout << "\n";
                ShowResults(lastResults, lastOptions, (int)lastResults.size());
                std::cout << "\n";
            }
            else if (choice == "7" && hasLastResults) {
                std::string outFile = settings::ResultsFile;

                std::ofstream out(outFile);
                if (out) {
                    for (const auto& r : lastResults) {
                        std::string rel;
                        try {
                            rel = ToUtf8(fs::relative(r.File, lastOptions.Directory));
                        } catch (...) {
                            rel = ToUtf8(r.File);
                        }
                        out << rel << ":" << r.Line << ": " << r.Text << "\n";
                    }
                    std::cout << "Сохранено: " << outFile << "\n\n";
                }
            }
        }
    }

    return 0;
}