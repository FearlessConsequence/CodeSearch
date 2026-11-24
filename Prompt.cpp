#include "Prompt.h"
#include "Utf8.h"
#include <cctype>
#include "Colors.h"
#include "Settings.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace prompt {

    bool GoBack = false;
    bool WantHistory = false;
    bool WantFavorites = false;
    bool WantClipboardHistory = false;

    int ReadInt(int minVal, int maxVal) {
        GoBack = false;
        while (true) {
            std::string input;
            std::getline(std::cin, input);

            size_t start = input.find_first_not_of(" \t\r\n");
            size_t end = input.find_last_not_of(" \t\r\n");
            if (start == std::string::npos) {
                std::cout << theme::Role::Question << "> " << theme::Role::Reset;
                continue;
            }
            input = input.substr(start, end - start + 1);

            if (input == "0" && minVal > 0) {
                GoBack = true;
                return minVal;
            }

            try {
                int value = std::stoi(input);
                if (value >= minVal && value <= maxVal)
                    return value;
            }
            catch (...) {}

            std::cout << theme::Role::Question << "> " << theme::Role::Reset;
        }
    }

    std::string ReadLine(const std::string& promptText) {
        std::cout << theme::Role::Question << promptText << theme::Role::Reset;
        GoBack = false; 
        std::string input;
        std::getline(std::cin, input);

        size_t start = input.find_first_not_of(" \t\r\n");
        size_t end = input.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        std::string trimmed = input.substr(start, end - start + 1);

        // Если ввели что-то в кавычках — это буквальный текст (например, "0")
        if (trimmed.length() >= 2) {
            if ((trimmed.front() == '"' && trimmed.back() == '"') ||
                (trimmed.front() == '\'' && trimmed.back() == '\'')) {
                return trimmed.substr(1, trimmed.length() - 2);
            }
        }

        if (trimmed == "0") {
            GoBack = true;
            return "";
        }
        return trimmed;
    }

    static std::string FormatSet(const std::vector<std::string>& items) {
        if (items.empty()) return "[пусто]";
        std::string result;
        for (size_t i = 0; i < items.size(); ++i) {
            if (i > 0) result += ", ";
            result += items[i];
        }
        return result;
    }

    static std::vector<std::string> SplitByComma(const std::string& s) {
        std::vector<std::string> result;
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, ',')) {
            size_t start = item.find_first_not_of(" \t");
            size_t end = item.find_last_not_of(" \t");
            if (start == std::string::npos) continue;
            result.push_back(item.substr(start, end - start + 1));
        }
        return result;
    }

    static bool QuotesBalanced(const std::string& s) {
        bool inQuote = false;
        char q = 0;
        for (char c : s) {
            if (inQuote) { if (c == q) inQuote = false; }
            else if (c == '"' || c == '\'') { inQuote = true; q = c; }
        }
        return !inQuote;
    }

    // ============ ТИП 1 ============

    static bool AskWord(regex_builder::SearchRequest& req) {
        while (true) {
            req.Word = ReadLine("Введите слово (0 — назад): ");
            if (GoBack) return false;
            if (!req.Word.empty()) break;
            std::cout << theme::Role::Error << "Слово не может быть пустым." << theme::Role::Reset << "\n";
        }

        auto w = ReadLine("Искать как отдельное слово? (y/n, Enter — да, 0 — назад): ");
        if (GoBack) return false;

        std::string lw = w;
        for (auto& c : lw) c = (char)std::tolower((unsigned char)c);
        req.WordWholeOnly = !(lw == "n" || lw == "no" || lw == "нет");
        return true;
    }

    // ============ ТИП 2 ============

    static bool AskFormat(regex_builder::SearchRequest& req) {
        std::cout << theme::Role::Question << "Что искать?\n" << theme::Role::Reset;
        std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Default << " Email\n";
        std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Default << " URL\n";
        std::cout << "  " << theme::Role::MenuNum << "3." << theme::Role::Default << " IP-адрес\n";
        std::cout << "  " << theme::Role::MenuNum << "4." << theme::Role::Default << " GUID\n";
        std::cout << "  " << theme::Role::MenuNum << "5." << theme::Role::Default << " Дата (2024-05-12)\n";
        std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Назад\n" << theme::Role::Question << "> ";
        req.Format = (patterns::FormatType)ReadInt(1, 5);
        return !GoBack;
    }

    // ============ ТИП 3 ============

    static bool AskStructure(regex_builder::SearchRequest& req) {
        while (true) {
            std::cout << theme::Role::Question << "Что искать?\n" << theme::Role::Reset;
            std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Default << " X, внутри которого пусто\n";
            std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Default << " X, потом что угодно, потом Y, потом что угодно, потом Z\n";
            std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Назад\n" << theme::Role::Question << "> ";
            req.Structure = (regex_builder::StructureType)ReadInt(1, 2);
            if (GoBack) return false;

            if (req.Structure == regex_builder::StructureType::XEmpty) {
                std::cout << theme::Role::Question << "Выберите параметр:\n" << theme::Role::Reset;
                std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Default << " Строго пусто\n";
                std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Default << " Не более N символов без пробелов\n";
                std::cout << "  " << theme::Role::MenuNum << "3." << theme::Role::Default << " Не более N строк\n";
                std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Назад\n" << theme::Role::Question << "> ";
                req.EmptyMetricValue = (regex_builder::EmptyMetric)ReadInt(1, 3);
                if (GoBack) continue;

                if (req.EmptyMetricValue != regex_builder::EmptyMetric::StrictlyEmpty) {
                    std::cout << theme::Role::Question << "Сколько (0 — назад): " << theme::Role::Reset;
                    req.EmptyMaxCount = ReadInt(0, 10000);
                    if (GoBack) continue;
                }

                while (true) {
                    req.X = ReadLine("Название оператора (0 — назад): ");
                    if (GoBack) break;
                    if (!req.X.empty()) return true;
                    std::cout << theme::Role::Error << "Не может быть пустым." << theme::Role::Reset << "\n";
                }
                if (GoBack) continue;
                return true;
            }

            else {
                while (true) {
                    std::cout << theme::Role::Dim
                        << "(% — любые пробелы, * — любые символы до первого совпадения,\n"
                        << " ^ — начало строки, $ — конец строки)"
                        << theme::Role::Reset << "\n";

                    req.X = ReadLine("X (0 — назад): ");
                    if (GoBack) break;
                    req.Y = ReadLine("Y (0 — назад): ");
                    if (GoBack) break;
                    req.Z = ReadLine("Z (0 — назад): ");
                    if (GoBack) break;

                    bool ok = true;
                    if (req.X.empty()) {
                        std::cout << theme::Role::Error << "X пуст." << theme::Role::Reset << "\n";
                        ok = false;
                    }
                    if (req.Y.empty()) {
                        std::cout << theme::Role::Error << "Y пуст." << theme::Role::Reset << "\n";
                        ok = false;
                    }
                    if (req.Z.empty()) {
                        std::cout << theme::Role::Error << "Z пуст." << theme::Role::Reset << "\n";
                        ok = false;
                    }
                    if (ok) return true;
                }
                if (GoBack) continue;
            }
        }
    }

    // ============ ТИП 4 ============

    static bool AskCombination(regex_builder::SearchRequest& req) {
        while (true) {
            std::cout << theme::Role::Question << "Что искать?\n" << theme::Role::Reset;
            std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Default << " X, но не Y\n";
            std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Default << " X или Y или Z\n";
            std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Назад\n" << theme::Role::Question << "> ";
            req.Combination = (regex_builder::CombinationType)ReadInt(1, 2);
            if (GoBack) return false;

            if (req.Combination == regex_builder::CombinationType::XOrY) {
                while (true) {
                    std::cout << theme::Role::Dim
                              << "(% — любые пробелы, * — любые символы до первого совпадения,\n"
                              << " ^ — начало строки, $ — конец строки.\n"
                              << " Если элемент содержит запятую — возьмите его в кавычки \"...\" или '...')"
                              << theme::Role::Reset << "\n";
                    req.X = ReadLine("Элементы через запятую (0 — назад): ");
                    if (GoBack) break;
                    if (!QuotesBalanced(req.X)) {
                        std::cout << theme::Role::Error << "Кавычки не закрыты. Попробуйте снова." << theme::Role::Reset << "\n";
                        continue;
                    }
                    bool hasItem = false;
                    for (char c : req.X) {
                        if (!std::isspace((unsigned char)c) && c != ',' && c != '"' && c != '\'') { hasItem = true; break; }
                    }
                    if (!hasItem) {
                        std::cout << theme::Role::Error << "Нужен хотя бы один элемент." << theme::Role::Reset << "\n";
                        continue;
                    }
                    return true;
                }
                if (GoBack) continue;
            }
            else {
                bool backToTop = false;
                while (true) {
                    std::cout << theme::Role::Question << "Как сравнивать X и Y?\n" << theme::Role::Reset;
                    std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Default << " X не пересекается с Y (по позиции)\n";
                    std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Default << " В строке с X нет Y (по строке)\n";
                    std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Назад\n" << theme::Role::Question << "> ";
                    req.XButNotYModeValue = (regex_builder::XButNotYMode)ReadInt(1, 2);
                    if (GoBack) { backToTop = true; break; }

                    std::cout << theme::Role::Dim
                              << "(% — любые пробелы, * — любые символы до первого совпадения,\n"
                              << " ^ — начало строки, $ — конец строки)"
                              << theme::Role::Reset << "\n";
                    bool backToSub = false;
                    while (true) {
                        req.X = ReadLine("X (0 — назад): ");
                        if (GoBack) { backToSub = true; break; }
                        req.Y = ReadLine("Y (0 — назад): ");
                        if (GoBack) { backToSub = true; break; }
                        if (req.X.empty()) { std::cout << theme::Role::Error << "X пуст." << theme::Role::Reset << "\n"; continue; }
                        if (req.Y.empty()) { std::cout << theme::Role::Error << "Y пуст." << theme::Role::Reset << "\n"; continue; }
                        req.Exclude = req.Y;
                        return true;
                    }
                    if (backToSub) continue;
                }
                if (backToTop) continue;
            }
        }
    }

    // ============ ТИП 5 ============

    static bool AskQuantity(regex_builder::SearchRequest& req) {
        while (true) {
            std::cout << theme::Role::Question << "Что ограничить?\n" << theme::Role::Reset;
            std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Default << " Количество цифр в числе\n";
            std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Default << " Длина строки\n";
            std::cout << "  " << theme::Role::MenuNum << "3." << theme::Role::Default << " Длина списка аргументов\n";
            std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Назад\n" << theme::Role::Question << "> ";            req.Quantity = (regex_builder::QuantityType)ReadInt(1, 3);
            if (GoBack) return false;

            std::cout << theme::Role::Question << "Как сравнивать?\n" << theme::Role::Reset;
            std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Default << " Больше N\n";
            std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Default << " Равно N\n";
            std::cout << "  " << theme::Role::MenuNum << "3." << theme::Role::Default << " Меньше N\n";
            std::cout << "  " << theme::Role::MenuNum << "4." << theme::Role::Default << " В диапазоне\n";
            std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Назад\n" << theme::Role::Question << "> ";
            req.NumberCompareValue = (regex_builder::NumberCompare)ReadInt(1, 4);
            if (GoBack) continue;

            if (req.NumberCompareValue == regex_builder::NumberCompare::Between) {
                std::cout << theme::Role::Question << "Минимум (0 — назад): " << theme::Role::Reset;
                req.MinCount = ReadInt(1, 10000);
                if (GoBack) continue;
                std::cout << theme::Role::Question << "Максимум (0 — назад): " << theme::Role::Reset;
                req.MaxCount = ReadInt(req.MinCount, 10000);
                if (GoBack) continue;
            }
            else {
                std::cout << theme::Role::Question << "Сколько цифр (0 — назад): " << theme::Role::Reset;
                req.MinCount = ReadInt(1, 10000);
                if (GoBack) continue;
            }
            return true;
        }
    }

    // ============ ТИП 6 ============

    static bool AskComment(regex_builder::SearchRequest& req) {
        std::cout << theme::Role::Question << "Что искать в комментариях?\n" << theme::Role::Reset;
        std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Default << " В начале строки\n";
        std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Default << " В конце строки\n";
        std::cout << "  " << theme::Role::MenuNum << "3." << theme::Role::Default << " Все комментарии\n";
        std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Назад\n" << theme::Role::Question << "> ";
        req.Comment = (regex_builder::CommentType)ReadInt(1, 3);
        if (GoBack) return false;

        // Выбор языка
        if (settings::FavoriteLanguages.empty()) {
            std::cout << theme::Role::Error << "Нет доступных языков в настройках." << theme::Role::Reset << "\n";
            return false;
        }
        std::cout << "\n" << theme::Role::Question << "Выберите язык:\n" << theme::Role::Reset;
        for (size_t i = 0; i < settings::FavoriteLanguages.size(); ++i) {
            const auto& l = settings::FavoriteLanguages[i];
            std::cout << "  " << theme::Role::MenuNum << (i + 1) << "." << theme::Role::Default << " " 
                      << l.Name << " (" << theme::Role::FileName << l.Prefix << theme::Role::Default << ")\n";
        }
        std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Назад\n" << theme::Role::Question << "> ";
        int langIdx = ReadInt(1, (int)settings::FavoriteLanguages.size());
        if (GoBack) return false;
        req.Language = settings::FavoriteLanguages[langIdx - 1].Name;

        // Слово (пустое = все комментарии этого типа)
        req.CommentWord = ReadLine("Слово для поиска (Enter — все комментарии, 0 — назад): ");
        if (GoBack) return false;
        return true;
    }

    // ============ ГЛАВНЫЙ МЕТОД ============

    regex_builder::SearchRequest Run() {
        regex_builder::SearchRequest req;
        GoBack = false;
        std::cout << theme::Role::Title << "=== Умный поиск и генератор regex ===\n" << theme::Role::Reset << "\n";

        bool printMenu = true;
        while (true) {
            if (printMenu) {
                std::cout << theme::Role::Question << "Что искать?\n" << theme::Role::Reset;
                std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Default << " Структура\n";
                std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Default << " Комбинирование\n";
                std::cout << "  " << theme::Role::MenuNum << "3." << theme::Role::Default << " Количество\n";
                std::cout << "  " << theme::Role::MenuNum << "4." << theme::Role::Default << " Формат (email, URL, IP, GUID, дата)\n";
                std::cout << "  " << theme::Role::MenuNum << "5." << theme::Role::Default << " Комментарии\n";
                std::cout << "  " << theme::Role::MenuNum << "6." << theme::Role::Default << " Слово с границами\n";
                std::cout << "  " << theme::Role::MenuNum << "7." << theme::Role::Default << " Избранное\n";
                std::cout << "  " << theme::Role::MenuNum << "8." << theme::Role::Default << " История поисков\n";
                std::cout << "  " << theme::Role::MenuNum << "9." << theme::Role::Default << " История буфера\n";
                std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Настройки\n";
                std::cout << "  " << theme::Role::MenuNum << "help" << theme::Role::Default << " — справка\n" << theme::Role::Question << "> ";
                printMenu = false;
            } else {
                std::cout << theme::Role::Question << "> " << theme::Role::Reset;
            }

            std::string input;
            std::getline(std::cin, input);
            size_t s = input.find_first_not_of(" \t\r\n");
            size_t e = input.find_last_not_of(" \t\r\n");
            if (s == std::string::npos) continue;
            input = input.substr(s, e - s + 1);

            int choice = -1;
            try { choice = std::stoi(input); } catch (...) { continue; }

            if (choice == 0) {
                EditSettings();
                std::cout << theme::Role::Reset << "\n";
                printMenu = true;
                continue;
            }
            if (choice == 8) {
                WantHistory = true;
                return req;
            }
            if (choice == 7) {
                WantFavorites = true;
                return req;
            }
            if (choice == 9) {
                WantClipboardHistory = true;
                return req;
            }
            if (choice < 1 || choice > 6) continue;

            printMenu = true;

            switch (choice) {
                case 1: req.Type = regex_builder::SearchType::Structure;   break;
                case 2: req.Type = regex_builder::SearchType::Combination; break;
                case 3: req.Type = regex_builder::SearchType::Quantity;    break;
                case 4: req.Type = regex_builder::SearchType::Format;      break;
                case 5: req.Type = regex_builder::SearchType::Comment;     break;
                case 6: req.Type = regex_builder::SearchType::Word;        break;
            }
            std::cout << theme::Role::Reset << "\n";

            bool ok = false;
            switch (req.Type) {
                case regex_builder::SearchType::Word:        ok = AskWord(req); break;
                case regex_builder::SearchType::Format:      ok = AskFormat(req); break;
                case regex_builder::SearchType::Structure:   ok = AskStructure(req); break;
                case regex_builder::SearchType::Combination: ok = AskCombination(req); break;
                case regex_builder::SearchType::Quantity:    ok = AskQuantity(req); break;
                case regex_builder::SearchType::Comment:     ok = AskComment(req); break;
            }

            if (ok) return req;

            GoBack = false;
            std::cout << theme::Role::Reset << "\n";
            printMenu = true;
        }
    }

    // ============ НАСТРОЙКИ ============

    static std::vector<std::string> GetDefaultExtensions() {
        if (!settings::DefaultExtensionSet.empty()) {
            auto it = settings::ExtensionSets.find(settings::DefaultExtensionSet);
            if (it != settings::ExtensionSets.end())
                return it->second;
        }
        return settings::FallbackExtensions;
    }

    static std::vector<std::string> GetDefaultExcludeDirs() {
        if (!settings::DefaultExcludeDirSet.empty()) {
            auto it = settings::ExcludeDirSets.find(settings::DefaultExcludeDirSet);
            if (it != settings::ExcludeDirSets.end())
                return it->second;
        }
        return settings::FallbackExcludeDirs;
    }

    static std::vector<std::string> GetDefaultExcludeFiles() {
        if (!settings::DefaultExcludeFileSet.empty()) {
            auto it = settings::ExcludeFileSets.find(settings::DefaultExcludeFileSet);
            if (it != settings::ExcludeFileSets.end())
                return it->second;
        }
        return settings::FallbackExcludeFiles;
    }

    static int GetDefaultMaxFileSize() {
        if (!settings::DefaultMaxFileSizeSet.empty()) {
            auto it = settings::MaxFileSizeSets.find(settings::DefaultMaxFileSizeSet);
            if (it != settings::MaxFileSizeSets.end())
                return it->second;
        }
        return settings::FallbackMaxFileSizeMb;
    }

    // ============ 1. РАСШИРЕНИЯ ============

    static void ShowExtensionSets() {
        std::cout << "\n";
        int i = 1;
        for (const auto& kv : settings::ExtensionSets) {
            std::cout << "  " << theme::Role::MenuNum << i++ << ". ["
                    << theme::Role::MenuNum << kv.first << "] "
                    << theme::Role::FileName << FormatSet(kv.second) << "\n";
        }
        std::cout << "  " << theme::Role::MenuNum << "По умолчанию: "
                << theme::Role::FileName << settings::DefaultExtensionSet << "\n\n";
    }

    static void EditExtensionSets() {
        ShowExtensionSets();
        bool printMenu = true;
        while (true) {
            if (printMenu) {
                std::cout << theme::Role::Question << "Что сделать?\n" << theme::Role::Reset;
                std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Default << " Добавить сет\n";
                std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Default << " Изменить сет\n";
                std::cout << "  " << theme::Role::MenuNum << "3." << theme::Role::Default << " Удалить сет\n";
                std::cout << "  " << theme::Role::MenuNum << "4." << theme::Role::Default << " Сменить сет по умолчанию\n";
                std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Назад\n" << theme::Role::Question << "> ";
                printMenu = false;
            } else {
                std::cout << theme::Role::Question << "> " << theme::Role::Reset;
            }

            std::string input;
            std::getline(std::cin, input);
            size_t s = input.find_first_not_of(" \t\r\n");
            size_t e = input.find_last_not_of(" \t\r\n");
            if (s == std::string::npos) continue;
            input = input.substr(s, e - s + 1);

            int n = -1;
            try { n = std::stoi(input); } catch (...) { continue; }
            if (n < 0 || n > 4) continue;

            if (n == 0) return;
            printMenu = true;

            std::vector<std::string> keys;
            for (const auto& kv : settings::ExtensionSets) keys.push_back(kv.first);

            if (n == 1) {
                std::string name = ReadLine("Имя нового сета (0 — назад): ");
                if (GoBack) { GoBack = false; continue; }
                if (name.empty()) continue;
                auto raw = ReadLine("Расширения через запятую (0 — назад): ");
                if (GoBack) { GoBack = false; continue; }
                settings::ExtensionSets[name] = SplitByComma(raw);
                std::cout << theme::Role::Error << "\nДобавлено.\n" << theme::Role::Reset << "\n";
            }
            else if (n == 2 && !keys.empty()) {
                std::cout << theme::Role::Question << "Введите номер сета: " << theme::Role::Reset;
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                std::string oldName = keys[k - 1];
                auto oldVals = settings::ExtensionSets[oldName];

                auto newName = ReadLine("Имя сета (Enter — оставить, 0 — назад): ");
                if (GoBack) { GoBack = false; continue; }
                auto raw = ReadLine("Расширения (Enter — оставить, 0 — назад): ");
                if (GoBack) { GoBack = false; continue; }

                auto newVals = SplitByComma(raw);
                if (newVals.empty()) newVals = oldVals;

                if (!newName.empty() && newName != oldName) {
                    settings::ExtensionSets.erase(oldName);
                    settings::ExtensionSets[newName] = newVals;
                    if (settings::DefaultExtensionSet == oldName)
                        settings::DefaultExtensionSet = newName;
                } else {
                    settings::ExtensionSets[oldName] = newVals;
                }
                std::cout << theme::Role::Error << "\nИзменено.\n" << theme::Role::Reset << "\n";
            }
            else if (n == 3 && !keys.empty()) {
                std::cout << theme::Role::Question << "Введите номер сета: " << theme::Role::Reset;
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                settings::ExtensionSets.erase(keys[k - 1]);
                std::cout << theme::Role::Error << "\nУдалено.\n" << theme::Role::Reset << "\n";
            }
            else if (n == 4 && !keys.empty()) {
                std::cout << theme::Role::Question << "Введите номер сета: " << theme::Role::Reset;
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                settings::DefaultExtensionSet = keys[k - 1];
                std::cout << "\n" << theme::Role::Error << "По умолчанию: " << theme::Role::FileName << keys[k - 1] << "\n\n";
            }
        }
    }

    // ============ 2. ИСКЛЮЧЁННЫЕ ПАПКИ ============

    static void ShowExcludeDirSets() {
        std::cout << "\n";
        int i = 1;
        for (const auto& kv : settings::ExcludeDirSets) {
            std::cout << "  " << theme::Role::MenuNum << i++ << ". ["
                    << theme::Role::MenuNum << kv.first << "] "
                    << theme::Role::FileName << FormatSet(kv.second) << "\n";
        }
        std::cout << "  " << theme::Role::MenuNum << "По умолчанию: "
                << theme::Role::FileName << settings::DefaultExcludeDirSet << "\n\n";
    }

    static void EditExcludeDirSets() {
        ShowExcludeDirSets();
        bool printMenu = true;
        while (true) {
            if (printMenu) {
                std::cout << theme::Role::Question << "Что сделать?\n" << theme::Role::Reset;
                std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Default << " Добавить сет\n";
                std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Default << " Изменить сет\n";
                std::cout << "  " << theme::Role::MenuNum << "3." << theme::Role::Default << " Удалить сет\n";
                std::cout << "  " << theme::Role::MenuNum << "4." << theme::Role::Default << " Сменить сет по умолчанию\n";
                std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Назад\n" << theme::Role::Question << "> ";
                printMenu = false;
            } else {
                std::cout << theme::Role::Question << "> " << theme::Role::Reset;
            }

            std::string input;
            std::getline(std::cin, input);
            size_t s = input.find_first_not_of(" \t\r\n");
            size_t e = input.find_last_not_of(" \t\r\n");
            if (s == std::string::npos) continue;
            input = input.substr(s, e - s + 1);

            int n = -1;
            try { n = std::stoi(input); } catch (...) { continue; }
            if (n < 0 || n > 4) continue;

            if (n == 0) return;
            printMenu = true;

            std::vector<std::string> keys;
            for (const auto& kv : settings::ExcludeDirSets) keys.push_back(kv.first);

            if (n == 1) {
                std::string name = ReadLine("Имя нового сета (0 — назад): ");
                if (GoBack) { GoBack = false; continue; }
                if (name.empty()) continue;
                auto raw = ReadLine("Папки через запятую (0 — назад): ");
                if (GoBack) { GoBack = false; continue; }
                settings::ExcludeDirSets[name] = SplitByComma(raw);
                std::cout << theme::Role::Error << "\nДобавлено.\n" << theme::Role::Reset << "\n";
            }
            else if (n == 2 && !keys.empty()) {
                std::cout << theme::Role::Question << "Введите номер сета: " << theme::Role::Reset;
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                std::string oldName = keys[k - 1];
                auto oldVals = settings::ExcludeDirSets[oldName];

                auto newName = ReadLine("Имя сета (Enter — оставить, 0 — назад): ");
                if (GoBack) { GoBack = false; continue; }
                auto raw = ReadLine("Папки (Enter — оставить, 0 — назад): ");
                if (GoBack) { GoBack = false; continue; }

                auto newVals = SplitByComma(raw);
                if (newVals.empty()) newVals = oldVals;

                if (!newName.empty() && newName != oldName) {
                    settings::ExcludeDirSets.erase(oldName);
                    settings::ExcludeDirSets[newName] = newVals;
                    if (settings::DefaultExcludeDirSet == oldName)
                        settings::DefaultExcludeDirSet = newName;
                } else {
                    settings::ExcludeDirSets[oldName] = newVals;
                }
                std::cout << theme::Role::Error << "\nИзменено.\n" << theme::Role::Reset << "\n";
            }
            else if (n == 3 && !keys.empty()) {
                std::cout << theme::Role::Question << "Введите номер сета: " << theme::Role::Reset;
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                settings::ExcludeDirSets.erase(keys[k - 1]);
                std::cout << theme::Role::Error << "\nУдалено.\n" << theme::Role::Reset << "\n";
            }
            else if (n == 4 && !keys.empty()) {
                std::cout << theme::Role::Question << "Введите номер сета: " << theme::Role::Reset;
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                settings::DefaultExcludeDirSet = keys[k - 1];
                std::cout << "\n" << theme::Role::Error << "По умолчанию: " << theme::Role::FileName << keys[k - 1] << "\n\n";
            }
        }
    }

    // ============ 3. ИСКЛЮЧЁННЫЕ ФАЙЛЫ ============

    static void ShowExcludeFileSets() {
        std::cout << "\n";
        int i = 1;
        for (const auto& kv : settings::ExcludeFileSets) {
            std::cout << "  " << theme::Role::MenuNum << i++ << ". ["
                    << theme::Role::MenuNum << kv.first << "] "
                    << theme::Role::FileName << FormatSet(kv.second) << "\n";
        }
        std::cout << "  " << theme::Role::MenuNum << "По умолчанию: "
                << theme::Role::FileName << settings::DefaultExcludeFileSet << "\n\n";
    }

    static void EditExcludeFileSets() {
        ShowExcludeFileSets();
        bool printMenu = true;
        while (true) {
            if (printMenu) {
                std::cout << theme::Role::Question << "Что искать?\n" << theme::Role::Reset;
                std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Default << " Добавить сет\n";
                std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Default << " Изменить сет\n";
                std::cout << "  " << theme::Role::MenuNum << "3." << theme::Role::Default << " Удалить сет\n";
                std::cout << "  " << theme::Role::MenuNum << "4." << theme::Role::Default << " Сменить сет по умолчанию\n";
                std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Назад\n" << theme::Role::Question << "> ";
                printMenu = false;
            } else {
                std::cout << theme::Role::Question << "> " << theme::Role::Reset;
            }

            std::string input;
            std::getline(std::cin, input);
            size_t s = input.find_first_not_of(" \t\r\n");
            size_t e = input.find_last_not_of(" \t\r\n");
            if (s == std::string::npos) continue;
            input = input.substr(s, e - s + 1);

            int n = -1;
            try { n = std::stoi(input); } catch (...) { continue; }
            if (n < 0 || n > 4) continue;

            if (n == 0) return;
            printMenu = true;

            std::vector<std::string> keys;
            for (const auto& kv : settings::ExcludeFileSets) keys.push_back(kv.first);

            if (n == 1) {
                std::string name = ReadLine("Имя нового сета (0 — назад): ");
                if (GoBack) { GoBack = false; continue; }
                if (name.empty()) continue;
                auto raw = ReadLine("Файлы через запятую (0 — назад): ");
                if (GoBack) { GoBack = false; continue; }
                settings::ExcludeFileSets[name] = SplitByComma(raw);
                std::cout << theme::Role::Error << "\nДобавлено.\n" << theme::Role::Reset << "\n";
            }
            else if (n == 2 && !keys.empty()) {
                std::cout << theme::Role::Question << "Введите номер сета: " << theme::Role::Reset;
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                std::string oldName = keys[k - 1];
                auto oldVals = settings::ExcludeFileSets[oldName];

                auto newName = ReadLine("Имя сета (Enter — оставить, 0 — назад): ");
                if (GoBack) { GoBack = false; continue; }
                auto raw = ReadLine("Файлы (Enter — оставить, 0 — назад): ");
                if (GoBack) { GoBack = false; continue; }

                auto newVals = SplitByComma(raw);
                if (newVals.empty()) newVals = oldVals;

                if (!newName.empty() && newName != oldName) {
                    settings::ExcludeFileSets.erase(oldName);
                    settings::ExcludeFileSets[newName] = newVals;
                    if (settings::DefaultExcludeFileSet == oldName)
                        settings::DefaultExcludeFileSet = newName;
                } else {
                    settings::ExcludeFileSets[oldName] = newVals;
                }
                std::cout << theme::Role::Error << "\nИзменено.\n" << theme::Role::Reset << "\n";
            }
            else if (n == 3 && !keys.empty()) {
                std::cout << theme::Role::Question << "Введите номер сета: " << theme::Role::Reset;
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                settings::ExcludeFileSets.erase(keys[k - 1]);
                std::cout << theme::Role::Error << "\nУдалено.\n" << theme::Role::Reset << "\n";
            }
            else if (n == 4 && !keys.empty()) {
                std::cout << theme::Role::Question << "Введите номер сета: " << theme::Role::Reset;
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                settings::DefaultExcludeFileSet = keys[k - 1];
                std::cout << "\n" << theme::Role::Error << "По умолчанию: " << theme::Role::FileName << keys[k - 1] << "\n\n";
            }
        }
    }

    // ============ 4. МАКСИМАЛЬНЫЙ РАЗМЕР ============

    static void ShowMaxFileSizeSets() {
        std::cout << "\n";
        int i = 1;
        for (const auto& kv : settings::MaxFileSizeSets) {
            std::cout << "  " << theme::Role::MenuNum << i++ << ". ["
                    << theme::Role::MenuNum << kv.first << "] "
                    << theme::Role::FileName 
                    << (kv.second < 0 ? std::string("без лимита") : std::to_string(kv.second) + " МБ") 
                    << "\n";
        }
        std::cout << "  " << theme::Role::MenuNum << "По умолчанию: "
                << theme::Role::FileName << settings::DefaultMaxFileSizeSet << "\n\n";
    }

    static void EditMaxFileSizeSets() {
        ShowMaxFileSizeSets();
        bool printMenu = true;
        while (true) {
            if (printMenu) {
                std::cout << theme::Role::Question << "Что сделать?\n" << theme::Role::Reset;
                std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Default << " Добавить сет\n";
                std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Default << " Изменить сет\n";
                std::cout << "  " << theme::Role::MenuNum << "3." << theme::Role::Default << " Удалить сет\n";
                std::cout << "  " << theme::Role::MenuNum << "4." << theme::Role::Default << " Сменить сет по умолчанию\n";
                std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Назад\n" << theme::Role::Question << "> ";
                printMenu = false;
            } else {
                std::cout << theme::Role::Question << "> " << theme::Role::Reset;
            }

            std::string input;
            std::getline(std::cin, input);
            size_t s = input.find_first_not_of(" \t\r\n");
            size_t e = input.find_last_not_of(" \t\r\n");
            if (s == std::string::npos) continue;
            input = input.substr(s, e - s + 1);

            int n = -1;
            try { n = std::stoi(input); } catch (...) { continue; }
            if (n < 0 || n > 4) continue;

            if (n == 0) return;
            printMenu = true;

            std::vector<std::string> keys;
            for (const auto& kv : settings::MaxFileSizeSets) keys.push_back(kv.first);

            if (n == 1) {
                std::string name = ReadLine("Имя нового сета (0 — назад): ");
                if (GoBack) { GoBack = false; continue; }
                if (name.empty()) continue;
                std::cout << theme::Role::Question << "Размер в МБ (-1 — без лимита, 0 — 0 МБ): " << theme::Role::Reset;
                int val = ReadInt(-1, 100000);
                settings::MaxFileSizeSets[name] = val;
                std::cout << theme::Role::Error << "\nДобавлено.\n" << theme::Role::Reset << "\n";
            }
            else if (n == 2 && !keys.empty()) {
                std::cout << theme::Role::Question << "Введите номер сета: " << theme::Role::Reset;
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                std::string oldName = keys[k - 1];
                int oldVal = settings::MaxFileSizeSets[oldName];

                auto newName = ReadLine("Имя сета (Enter — оставить, 0 — назад): ");
                if (GoBack) { GoBack = false; continue; }
                auto raw = ReadLine("Размер в МБ (Enter — оставить, 0 — назад): ");
                if (GoBack) { GoBack = false; continue; }

                int newVal = oldVal;
                if (!raw.empty()) {
                    try { newVal = std::stoi(raw); } catch (...) {}
                }

                if (!newName.empty() && newName != oldName) {
                    settings::MaxFileSizeSets.erase(oldName);
                    settings::MaxFileSizeSets[newName] = newVal;
                    if (settings::DefaultMaxFileSizeSet == oldName)
                        settings::DefaultMaxFileSizeSet = newName;
                } else {
                    settings::MaxFileSizeSets[oldName] = newVal;
                }
                std::cout << theme::Role::Error << "\nИзменено.\n" << theme::Role::Reset << "\n";
            }
            else if (n == 3 && !keys.empty()) {
                std::cout << theme::Role::Question << "Введите номер сета: " << theme::Role::Reset;
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                settings::MaxFileSizeSets.erase(keys[k - 1]);
                std::cout << theme::Role::Error << "\nУдалено.\n" << theme::Role::Reset << "\n";
            }
            else if (n == 4 && !keys.empty()) {
                std::cout << theme::Role::Question << "Введите номер сета: " << theme::Role::Reset;
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                settings::DefaultMaxFileSizeSet = keys[k - 1];
                std::cout << "\n" << theme::Role::Error << "По умолчанию: " << theme::Role::FileName << keys[k - 1] << "\n\n";
            }
        }
    }

    // ============ 5. ВЫВОД ============

    static void EditOutputSettings() {
        bool printMenu = true;
        while (true) {
            if (printMenu) {
                std::cout << "\n" << theme::Role::Title << "=== ВЫВОД ===\n\n" << theme::Role::Reset;
                std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Default << " Показывать первых: " << theme::Role::FileName << settings::ShowFirstN << "\n";
                std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Default << " Имя файла результатов: " << theme::Role::FileName << settings::ResultsFile << "\n";
                std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Назад\n" << theme::Role::Question << "> ";
                printMenu = false;
            } else {
                std::cout << theme::Role::Question << "> " << theme::Role::Reset;
            }

            std::string input;
            std::getline(std::cin, input);
            size_t s = input.find_first_not_of(" \t\r\n");
            size_t e = input.find_last_not_of(" \t\r\n");
            if (s == std::string::npos) continue;
            input = input.substr(s, e - s + 1);

            int n = -1;
            try { n = std::stoi(input); } catch (...) { continue; }
            if (n < 0 || n > 2) continue;

            if (n == 0) return;
            printMenu = true;

            if (n == 1) {
                std::cout << theme::Role::Question << "Сколько (0 — назад): " << theme::Role::Reset;
                int v = ReadInt(1, 100000);
                if (GoBack) { GoBack = false; continue; }
                settings::ShowFirstN = v;
                std::cout << theme::Role::Default << "\nУстановлено.\n" << theme::Role::Reset << "\n";
            }
            else if (n == 2) {
                auto v = ReadLine("Имя файла (0 — назад): ");
                if (GoBack) { GoBack = false; continue; }
                settings::ResultsFile = v;
                std::cout << theme::Role::Default << "\nУстановлено.\n" << theme::Role::Reset << "\n";
            }
        }
    }

    // ============ 6. ЯЗЫКИ ============

    static void ShowLanguages() {
        std::cout << "\n";
        int i = 1;
        for (const auto& l : settings::FavoriteLanguages) {
            std::cout << "  " << theme::Role::MenuNum << i++ << ". ["
                    << theme::Role::MenuNum << l.Name << "] "
                    << theme::Role::FileName << "prefix=" << l.Prefix
                    << ", signs=" << l.Signs << "\n";
        }
        std::cout << "  " << theme::Role::MenuNum << "По умолчанию: "
          << theme::Role::FileName << settings::DefaultLanguage << "\n\n";
    }

    static void EditLanguages() {
        ShowLanguages();
        bool printMenu = true;
        while (true) {
            if (printMenu) {
                std::cout << theme::Role::Question << "Что искать?\n" << theme::Role::Reset;
                std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Default << " Добавить язык\n";
                std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Default << " Изменить язык\n";
                std::cout << "  " << theme::Role::MenuNum << "3." << theme::Role::Default << " Удалить язык\n";
                std::cout << "  " << theme::Role::MenuNum << "4." << theme::Role::Default << " Сменить язык по умолчанию\n";
                std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Назад\n";
                std::cout << theme::Role::Question << "> " << theme::Role::Reset;
                printMenu = false;
            } else {
                std::cout << theme::Role::Question << "> " << theme::Role::Reset;
            }

            std::string input;
            std::getline(std::cin, input);
            size_t s = input.find_first_not_of(" \t\r\n");
            size_t e = input.find_last_not_of(" \t\r\n");
            if (s == std::string::npos) continue;
            input = input.substr(s, e - s + 1);

            int n = -1;
            try { n = std::stoi(input); } catch (...) { continue; }
            if (n < 0 || n > 4) continue;

            if (n == 0) return;
            printMenu = true;

            int sz = (int)settings::FavoriteLanguages.size();

            if (n == 1) {
                settings::LanguageInfo info;
                info.Name = ReadLine("Имя языка (0 — назад): ");
                if (GoBack) { GoBack = false; continue; }
                if (info.Name.empty()) continue;
                info.Prefix = ReadLine("Префикс (0 — назад): ");
                if (GoBack) { GoBack = false; continue; }
                info.Signs = ReadLine("Признаки (0 — назад): ");
                if (GoBack) { GoBack = false; continue; }
                settings::FavoriteLanguages.push_back(info);
                std::cout << theme::Role::Error << "\nДобавлено.\n" << theme::Role::Reset << "\n";
            }
            else if (n == 2 && sz > 0) {
                std::cout << theme::Role::Question << "Введите номер языка: " << theme::Role::Reset;
                int k = ReadInt(1, sz);
                if (GoBack) { GoBack = false; continue; }
                auto& lang = settings::FavoriteLanguages[k - 1];
                std::string oldName = lang.Name;

                auto name = ReadLine("Имя (Enter — оставить, 0 — назад): ");
                if (GoBack) { GoBack = false; continue; }
                auto prefix = ReadLine("Префикс (Enter — оставить, 0 — назад): ");
                if (GoBack) { GoBack = false; continue; }
                auto signs = ReadLine("Признаки (Enter — оставить, 0 — назад): ");
                if (GoBack) { GoBack = false; continue; }

                if (!name.empty()) lang.Name = name;
                if (!prefix.empty()) lang.Prefix = prefix;
                if (!signs.empty()) lang.Signs = signs;

                if (settings::DefaultLanguage == oldName && !name.empty())
                    settings::DefaultLanguage = name;
                std::cout << theme::Role::Error << "\nИзменено.\n" << theme::Role::Reset << "\n";
            }
            else if (n == 3 && sz > 0) {
                std::cout << theme::Role::Question << "Введите номер языка: " << theme::Role::Reset;
                int k = ReadInt(1, sz);
                if (GoBack) { GoBack = false; continue; }
                settings::FavoriteLanguages.erase(
                    settings::FavoriteLanguages.begin() + (k - 1));
                std::cout << theme::Role::Error << "\nУдалено.\n" << theme::Role::Reset << "\n";
            }
            else if (n == 4 && sz > 0) {
                std::cout << theme::Role::Question << "Введите номер языка: " << theme::Role::Reset;
                int k = ReadInt(1, sz);
                if (GoBack) { GoBack = false; continue; }
                settings::DefaultLanguage = settings::FavoriteLanguages[k - 1].Name;
                std::cout << theme::Role::Default << "\n" << theme::Role::Error << "По умолчанию: "
                    << theme::Role::FileName << settings::DefaultLanguage << "\n\n";
            }
        }
    }

    // ============ ГЛАВНОЕ МЕНЮ НАСТРОЕК ============

    void EditSettings() {
        bool printMenu = true;
        while (true) {
            if (printMenu) {
                std::cout << "\n" << theme::Role::Title << "=== НАСТРОЙКИ ===\n\n" << theme::Role::Reset;
                std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Default << " Расширения по языкам\n";
                std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Default << " Исключённые папки\n";
                std::cout << "  " << theme::Role::MenuNum << "3." << theme::Role::Default << " Исключённые файлы\n";
                std::cout << "  " << theme::Role::MenuNum << "4." << theme::Role::Default << " Максимальный размер файла\n";
                std::cout << "  " << theme::Role::MenuNum << "5." << theme::Role::Default << " Вывод\n";
                std::cout << "  " << theme::Role::MenuNum << "6." << theme::Role::Default << " Языки\n";
                std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Сохранить и выйти\n" << theme::Role::Question << "> ";
                printMenu = false;
            } else {
                std::cout << theme::Role::Question << "> " << theme::Role::Reset;
            }

            std::string input;
            std::getline(std::cin, input);
            size_t s = input.find_first_not_of(" \t\r\n");
            size_t e = input.find_last_not_of(" \t\r\n");
            if (s == std::string::npos) continue;
            input = input.substr(s, e - s + 1);

            int n = -1;
            try { n = std::stoi(input); } catch (...) { continue; }
            if (n < 0 || n > 6) continue;

            if (n == 0) {
                if (settings::Save()) {
                    std::cout << theme::Role::Default << "\nСохранено в settings.json.\n" << theme::Role::Reset << "\n";
                } else {
                    std::cout << theme::Role::Error << "\nОшибка сохранения.\n" << theme::Role::Reset << "\n";
                }
                return;
            }

            printMenu = true;

            switch (n) {
                case 1: EditExtensionSets(); break;
                case 2: EditExcludeDirSets(); break;
                case 3: EditExcludeFileSets(); break;
                case 4: EditMaxFileSizeSets(); break;
                case 5: EditOutputSettings(); break;
                case 6: EditLanguages(); break;
            }
        }
    }

    bool AskSearchOptions(file_searcher::SearchOptions& opt,
                        regex_builder::SearchRequest& req) {
        opt.Directory = fs::current_path();

        if (req.Type == regex_builder::SearchType::Comment) {
            auto extIt = settings::ExtensionSets.find(req.Language);
            if (extIt != settings::ExtensionSets.end())
                opt.Extensions = extIt->second;
            else
                opt.Extensions = GetDefaultExtensions();
        } else {
            opt.Extensions = GetDefaultExtensions();
        }

        opt.ExcludeDirs = GetDefaultExcludeDirs();

        opt.ExcludeFiles = GetDefaultExcludeFiles();
        opt.MaxFileSizeMb = GetDefaultMaxFileSize();
        opt.ShowFirstN = settings::ShowFirstN;

        bool printMenu = true;
        while (true) {
            if (printMenu) {
                auto currentDir = fs::current_path();
                std::string dirLabel;
                if (opt.Directory == currentDir) {
                    dirLabel = "[текущая]";
                } else {
                    dirLabel = utf8::ToUtf8(opt.Directory);
                }

                std::cout << theme::Role::Reset << "\n";
                std::cout << theme::Role::Question << "Измените параметры поиска:\n" << theme::Role::Reset;
                std::cout << "  " << theme::Role::MenuNum << "1." << theme::Role::Default << " Начать поиск\n";
                std::cout << "  " << theme::Role::MenuNum << "2." << theme::Role::Default << " Директория: " << theme::Role::FileName << dirLabel << "\n";
                std::cout << "  " << theme::Role::MenuNum << "3." << theme::Role::Default << " Расширения: " << theme::Role::FileName << FormatSet(opt.Extensions) << "\n";
                std::cout << "  " << theme::Role::MenuNum << "4." << theme::Role::Default << " Исключённые папки: " << theme::Role::FileName << FormatSet(opt.ExcludeDirs) << "\n";
                std::cout << "  " << theme::Role::MenuNum << "5." << theme::Role::Default << " Исключённые файлы: " << theme::Role::FileName << FormatSet(opt.ExcludeFiles) << "\n";
                std::cout << "  " << theme::Role::MenuNum << "6." << theme::Role::Default << " Максимальный размер: " 
                    << theme::Role::FileName << (opt.MaxFileSizeMb < 0 ? std::string("без лимита") : std::to_string(opt.MaxFileSizeMb) + " МБ") << "\n";
                if (req.Type == regex_builder::SearchType::Comment)
                    std::cout << "  " << theme::Role::MenuNum << "7." << theme::Role::Default << " Язык: " << theme::Role::FileName << req.Language << "\n";
                std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Отмена\n" << theme::Role::Question << "> ";
                printMenu = false;
            } else {
                std::cout << theme::Role::Question << "> " << theme::Role::Reset;
            }

            std::string choice;
            std::getline(std::cin, choice);
            size_t s = choice.find_first_not_of(" \t\r\n");
            size_t e = choice.find_last_not_of(" \t\r\n");
            if (s == std::string::npos) continue;
            choice = choice.substr(s, e - s + 1);

            if (choice == "0") return false;
            if (choice == "1") return true;

            bool valid = (choice == "2" || choice == "3" || choice == "4" ||
                        choice == "5" || choice == "6");
            if (req.Type == regex_builder::SearchType::Comment && choice == "7") valid = true;
            if (!valid) continue;

            printMenu = true;

            if (choice == "2") {
                auto dir = ReadLine("Директория (Enter — текущая, 0 — назад): ");
                if (GoBack) { GoBack = false; printMenu = true; continue; }
                
                if (dir.empty()) {
                    // Если нажали Enter — сбрасываем на текущую директорию
                    opt.Directory = fs::current_path();
                } else {
                    fs::path dirPath = utf8::FromUtf8(dir);
                    if (fs::exists(dirPath) && fs::is_directory(dirPath))
                        opt.Directory = dirPath;
                    else
                        std::cout << theme::Role::Error << "Директория не найдена: " << dir << theme::Role::Reset << "\n";
                }
            }

            else if (choice == "3") {
                if (!settings::ExtensionSets.empty()) {
                    std::cout << "\n" << theme::Role::Question << "Выберите сет расширений:\n" << theme::Role::Reset;
                    int i = 1;
                    std::vector<std::string> keys;
                    for (const auto& kv : settings::ExtensionSets) {
                        std::cout << "  " << theme::Role::MenuNum << i++ << "." << theme::Role::Default << " " << kv.first
                                  << " — " << theme::Role::FileName << FormatSet(kv.second) << "\n";
                        keys.push_back(kv.first);
                    }
                    std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Отмена\n" << theme::Role::Question << "> ";
                    int n = ReadInt(0, (int)keys.size());
                    if (GoBack) { GoBack = false; printMenu = true; continue; }
                    if (n > 0) opt.Extensions = settings::ExtensionSets[keys[n - 1]];
                }
            }
            else if (choice == "4") {
                if (!settings::ExcludeDirSets.empty()) {
                    std::cout << theme::Role::MenuItem << "\nВыберите сет папок:\n" << theme::Role::Reset;
                    int i = 1;
                    std::vector<std::string> keys;
                    for (const auto& kv : settings::ExcludeDirSets) {
                        std::cout << "  " << theme::Role::MenuNum << i++ << ". " << theme::Role::Reset << kv.first
                                  << " — " << FormatSet(kv.second) << "\n";
                        keys.push_back(kv.first);
                    }
                    std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Default << " Отмена\n" << theme::Role::Question << "> ";
                    int n = ReadInt(0, (int)keys.size());
                    if (GoBack) { GoBack = false; printMenu = true; continue; }
                    if (n > 0) opt.ExcludeDirs = settings::ExcludeDirSets[keys[n - 1]];
                }
            }
            else if (choice == "5") {
                if (!settings::ExcludeFileSets.empty()) {
                    std::cout << theme::Role::MenuItem << "\nВыберите сет файлов:\n" << theme::Role::Reset;
                    int i = 1;
                    std::vector<std::string> keys;
                    for (const auto& kv : settings::ExcludeFileSets) {
                        std::cout << "  " << theme::Role::MenuNum << i++ << ". " << theme::Role::Reset << kv.first
                                  << " — " << FormatSet(kv.second) << "\n";
                        keys.push_back(kv.first);
                    }
                    std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Reset << " Отмена\n" << theme::Role::Question << "> ";
                    int n = ReadInt(0, (int)keys.size());
                    if (GoBack) { GoBack = false; printMenu = true; continue; }
                    if (n > 0) opt.ExcludeFiles = settings::ExcludeFileSets[keys[n - 1]];
                }
            }
            else if (choice == "6") {
                std::cout << theme::Role::Question << "Максимум МБ (-1 — без лимита, 0 — 0 МБ): " << theme::Role::Reset;
                int mb = ReadInt(-1, 10000);
                opt.MaxFileSizeMb = mb;
            }

            else if (choice == "7" && req.Type == regex_builder::SearchType::Comment) {
                std::cout << theme::Role::MenuItem << "\nВыберите язык:\n" << theme::Role::Reset;
                for (size_t i = 0; i < settings::FavoriteLanguages.size(); ++i) {
                    const auto& l = settings::FavoriteLanguages[i];
                    std::cout << "  " << theme::Role::MenuNum << (i + 1) << ". " << theme::Role::Reset << l.Name << " (" << l.Prefix << ")\n";
                }
                std::cout << "  " << theme::Role::MenuNum << "0." << theme::Role::Reset << " Отмена\n" << theme::Role::Question << "> ";
                int langIdx = ReadInt(0, (int)settings::FavoriteLanguages.size());
                if (langIdx > 0) {
                    req.Language = settings::FavoriteLanguages[langIdx - 1].Name;
                    auto extIt = settings::ExtensionSets.find(req.Language);
                    if (extIt != settings::ExtensionSets.end())
                        opt.Extensions = extIt->second;
                }
            }
        }
    }
}