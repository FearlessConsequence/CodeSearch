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
                std::cout << "> ";
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

            std::cout << "> ";
        }
    }

    std::string ReadLine(const std::string& promptText) {
        std::cout << promptText;
        GoBack = false; 
        std::string input;
        std::getline(std::cin, input);

        size_t start = input.find_first_not_of(" \t\r\n");
        size_t end = input.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        std::string trimmed = input.substr(start, end - start + 1);

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
            std::cout << colors::Error << "Слово не может быть пустым." << colors::Reset << "\n";
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
        std::cout << "Что искать?\n";
        std::cout << "  1. Email\n";
        std::cout << "  2. URL\n";
        std::cout << "  3. IP-адрес\n";
        std::cout << "  4. GUID\n";
        std::cout << "  5. Дата (2024-05-12)\n";
        std::cout << "  0. Назад\n> ";
        req.Format = (patterns::FormatType)ReadInt(1, 5);
        return !GoBack;
    }

    // ============ ТИП 3 ============

    static bool AskStructure(regex_builder::SearchRequest& req) {
        while (true) {
            std::cout << "Что искать?\n";
            std::cout << "  1. X, внутри которого пусто\n";
            std::cout << "  2. X, потом что угодно, потом Y, потом что угодно, потом Z\n";
            std::cout << "  0. Назад\n> ";
            req.Structure = (regex_builder::StructureType)ReadInt(1, 2);
            if (GoBack) return false;

            if (req.Structure == regex_builder::StructureType::XEmpty) {
                std::cout << "Выберите параметр:\n";
                std::cout << "  1. Строго пусто\n";
                std::cout << "  2. Не более N символов без пробелов\n";
                std::cout << "  3. Не более N строк\n";
                std::cout << "  0. Назад\n> ";
                req.EmptyMetricValue = (regex_builder::EmptyMetric)ReadInt(1, 3);
                if (GoBack) continue;

                if (req.EmptyMetricValue != regex_builder::EmptyMetric::StrictlyEmpty) {
                    std::cout << "Сколько (0 — назад): ";
                    req.EmptyMaxCount = ReadInt(0, 10000);
                    if (GoBack) continue;
                }

                while (true) {
                    req.X = ReadLine("Название оператора (0 — назад): ");
                    if (GoBack) break;
                    if (!req.X.empty()) return true;
                    std::cout << colors::Error << "Не может быть пустым." << colors::Reset << "\n";
                }
                if (GoBack) continue;
                return true;
            }

            else {
                while (true) {
                    std::cout << colors::Dim
                        << "(% — любые пробелы, * — любые символы до первого совпадения,\n"
                        << " ^ — начало строки, $ — конец строки)"
                        << colors::Reset << "\n";

                    req.X = ReadLine("X (0 — назад): ");
                    if (GoBack) break;
                    req.Y = ReadLine("Y (0 — назад): ");
                    if (GoBack) break;
                    req.Z = ReadLine("Z (0 — назад): ");
                    if (GoBack) break;

                    bool ok = true;
                    if (req.X.empty()) {
                        std::cout << colors::Error << "X пуст." << colors::Reset << "\n";
                        ok = false;
                    }
                    if (req.Y.empty()) {
                        std::cout << colors::Error << "Y пуст." << colors::Reset << "\n";
                        ok = false;
                    }
                    if (req.Z.empty()) {
                        std::cout << colors::Error << "Z пуст." << colors::Reset << "\n";
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
            std::cout << "Что искать?\n";
            std::cout << "  1. X, но не Y\n";
            std::cout << "  2. X или Y или Z\n";
            std::cout << "  0. Назад\n> ";
            req.Combination = (regex_builder::CombinationType)ReadInt(1, 2);
            if (GoBack) return false;

            if (req.Combination == regex_builder::CombinationType::XOrY) {
                while (true) {
                    std::cout << colors::Dim
                              << "(% — любые пробелы, * — любые символы до первого совпадения,\n"
                              << " ^ — начало строки, $ — конец строки.\n"
                              << " Если элемент содержит запятую — возьмите его в кавычки \"...\" или '...')"
                              << colors::Reset << "\n";
                    req.X = ReadLine("Элементы через запятую (0 — назад): ");
                    if (GoBack) break;
                    if (!QuotesBalanced(req.X)) {
                        std::cout << colors::Error << "Кавычки не закрыты. Попробуйте снова." << colors::Reset << "\n";
                        continue;
                    }
                    bool hasItem = false;
                    for (char c : req.X) {
                        if (!std::isspace((unsigned char)c) && c != ',') { hasItem = true; break; }
                    }
                    if (!hasItem) {
                        std::cout << colors::Error << "Нужен хотя бы один элемент." << colors::Reset << "\n";
                        continue;
                    }
                    return true;
                }
                if (GoBack) continue;
            }
            else {
                bool backToTop = false;
                while (true) {
                    std::cout << "Как сравнивать X и Y?\n";
                    std::cout << "  1. X не является частью Y (по позиции)\n";
                    std::cout << "  2. В строке с X нет Y (по строке)\n";
                    std::cout << "  0. Назад\n> ";
                    req.XButNotYModeValue = (regex_builder::XButNotYMode)ReadInt(1, 2);
                    if (GoBack) { backToTop = true; break; }

                    std::cout << colors::Dim
                              << "(% — любые пробелы, * — любые символы до первого совпадения,\n"
                              << " ^ — начало строки, $ — конец строки)"
                              << colors::Reset << "\n";
                    bool backToSub = false;
                    while (true) {
                        req.X = ReadLine("X (0 — назад): ");
                        if (GoBack) { backToSub = true; break; }
                        req.Y = ReadLine("Y (0 — назад): ");
                        if (GoBack) { backToSub = true; break; }
                        if (req.X.empty()) { std::cout << colors::Error << "X пуст." << colors::Reset << "\n"; continue; }
                        if (req.Y.empty()) { std::cout << colors::Error << "Y пуст." << colors::Reset << "\n"; continue; }
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
            std::cout << "Что ограничить?\n";
            std::cout << "  1. Количество цифр в числе\n";
            std::cout << "  2. Длина строки\n";
            std::cout << "  3. Длина списка аргументов\n";
            std::cout << "  0. Назад\n> ";
            req.Quantity = (regex_builder::QuantityType)ReadInt(1, 3);
            if (GoBack) return false;

            std::cout << "Как сравнивать?\n";
            std::cout << "  1. Больше N\n";
            std::cout << "  2. Равно N\n";
            std::cout << "  3. Меньше N\n";
            std::cout << "  4. В диапазоне\n";
            std::cout << "  0. Назад\n> ";
            req.NumberCompareValue = (regex_builder::NumberCompare)ReadInt(1, 4);
            if (GoBack) continue;

            if (req.NumberCompareValue == regex_builder::NumberCompare::Between) {
                std::cout << "Минимум (0 — назад): ";
                req.MinCount = ReadInt(1, 10000);
                if (GoBack) continue;
                std::cout << "Максимум (0 — назад): ";
                req.MaxCount = ReadInt(req.MinCount, 10000);
                if (GoBack) continue;
            }
            else {
                std::cout << "Сколько цифр (0 — назад): ";
                req.MinCount = ReadInt(1, 10000);
                if (GoBack) continue;
            }
            return true;
        }
    }

    // ============ ТИП 6 ============

    static bool AskComment(regex_builder::SearchRequest& req) {
        req.Language = settings::DefaultLanguage;

        std::cout << "Что искать?\n";
        std::cout << "  1. TODO / FIXME / HACK\n";
        std::cout << "  2. Комментарий в начале строки\n";
        std::cout << "  3. Комментарий в конце строки\n";
        std::cout << "  4. Закомментированный код\n";
        std::cout << "  0. Назад\n> ";
        req.Comment = (regex_builder::CommentType)ReadInt(1, 4);
        if (GoBack) return false;

        if (req.Comment == regex_builder::CommentType::Todo) {
            req.CommentWord = ReadLine("Слово (Enter — TODO/FIXME/HACK, 0 — назад): ");
            if (GoBack) return false;
        }
        return true;
    }

    // ============ ГЛАВНЫЙ МЕТОД ============

    regex_builder::SearchRequest Run() {
        regex_builder::SearchRequest req;
        GoBack = false;
        std::cout << "=== Умный поиск и генератор regex ===\n\n";

        bool printMenu = true;
        while (true) {
            if (printMenu) {
                std::cout << "Что искать?\n";
                std::cout << "  1. Структура\n";
                std::cout << "  2. Комбинирование\n";
                std::cout << "  3. Количество\n";
                std::cout << "  4. Формат (email, URL, IP, GUID, дата)\n";
                std::cout << "  5. Комментарии\n";
                std::cout << "  6. Слово с границами\n";
                std::cout << "  7. Избранное\n";
                std::cout << "  8. История поисков\n";
                std::cout << "  9. История буфера\n";
                std::cout << "  0. Настройки\n> ";
                printMenu = false;
            } else {
                std::cout << "> ";
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
                std::cout << "\n";
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
            std::cout << "\n";

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
            std::cout << "\n";
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
            std::cout << "  " << i++ << ". [" << kv.first << "] "
                      << FormatSet(kv.second) << "\n";
        }
        std::cout << "  По умолчанию: " << settings::DefaultExtensionSet << "\n\n";
    }

    static void EditExtensionSets() {
        ShowExtensionSets();
        bool printMenu = true;
        while (true) {
            if (printMenu) {
                std::cout << "Что сделать?\n";
                std::cout << "  1. Добавить сет\n";
                std::cout << "  2. Изменить сет\n";
                std::cout << "  3. Удалить сет\n";
                std::cout << "  4. Сменить сет по умолчанию\n";
                std::cout << "  0. Назад\n> ";
                printMenu = false;
            } else {
                std::cout << "> ";
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
                std::cout << "\nДобавлено.\n\n";
            }
            else if (n == 2 && !keys.empty()) {
                std::cout << "Введите номер сета: ";
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
                std::cout << "\nИзменено.\n\n";
            }
            else if (n == 3 && !keys.empty()) {
                std::cout << "Введите номер сета: ";
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                settings::ExtensionSets.erase(keys[k - 1]);
                std::cout << "\nУдалено.\n\n";
            }
            else if (n == 4 && !keys.empty()) {
                std::cout << "Введите номер сета: ";
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                settings::DefaultExtensionSet = keys[k - 1];
                std::cout << "\nПо умолчанию: " << keys[k - 1] << "\n\n";
            }
        }
    }

    // ============ 2. ИСКЛЮЧЁННЫЕ ПАПКИ ============

    static void ShowExcludeDirSets() {
        std::cout << "\n";
        int i = 1;
        for (const auto& kv : settings::ExcludeDirSets) {
            std::cout << "  " << i++ << ". [" << kv.first << "] "
                      << FormatSet(kv.second) << "\n";
        }
        std::cout << "  По умолчанию: " << settings::DefaultExcludeDirSet << "\n\n";
    }

    static void EditExcludeDirSets() {
        ShowExcludeDirSets();
        bool printMenu = true;
        while (true) {
            if (printMenu) {
                std::cout << "Что сделать?\n";
                std::cout << "  1. Добавить сет\n";
                std::cout << "  2. Изменить сет\n";
                std::cout << "  3. Удалить сет\n";
                std::cout << "  4. Сменить сет по умолчанию\n";
                std::cout << "  0. Назад\n> ";
                printMenu = false;
            } else {
                std::cout << "> ";
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
                std::cout << "\nДобавлено.\n\n";
            }
            else if (n == 2 && !keys.empty()) {
                std::cout << "Введите номер сета: ";
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
                std::cout << "\nИзменено.\n\n";
            }
            else if (n == 3 && !keys.empty()) {
                std::cout << "Введите номер сета: ";
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                settings::ExcludeDirSets.erase(keys[k - 1]);
                std::cout << "\nУдалено.\n\n";
            }
            else if (n == 4 && !keys.empty()) {
                std::cout << "Введите номер сета: ";
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                settings::DefaultExcludeDirSet = keys[k - 1];
                std::cout << "\nПо умолчанию: " << keys[k - 1] << "\n\n";
            }
        }
    }

    // ============ 3. ИСКЛЮЧЁННЫЕ ФАЙЛЫ ============

    static void ShowExcludeFileSets() {
        std::cout << "\n";
        int i = 1;
        for (const auto& kv : settings::ExcludeFileSets) {
            std::cout << "  " << i++ << ". [" << kv.first << "] "
                      << FormatSet(kv.second) << "\n";
        }
        std::cout << "  По умолчанию: " << settings::DefaultExcludeFileSet << "\n\n";
    }

    static void EditExcludeFileSets() {
        ShowExcludeFileSets();
        bool printMenu = true;
        while (true) {
            if (printMenu) {
                std::cout << "Что сделать?\n";
                std::cout << "  1. Добавить сет\n";
                std::cout << "  2. Изменить сет\n";
                std::cout << "  3. Удалить сет\n";
                std::cout << "  4. Сменить сет по умолчанию\n";
                std::cout << "  0. Назад\n> ";
                printMenu = false;
            } else {
                std::cout << "> ";
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
                std::cout << "\nДобавлено.\n\n";
            }
            else if (n == 2 && !keys.empty()) {
                std::cout << "Введите номер сета: ";
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
                std::cout << "\nИзменено.\n\n";
            }
            else if (n == 3 && !keys.empty()) {
                std::cout << "Введите номер сета: ";
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                settings::ExcludeFileSets.erase(keys[k - 1]);
                std::cout << "\nУдалено.\n\n";
            }
            else if (n == 4 && !keys.empty()) {
                std::cout << "Введите номер сета: ";
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                settings::DefaultExcludeFileSet = keys[k - 1];
                std::cout << "\nПо умолчанию: " << keys[k - 1] << "\n\n";
            }
        }
    }

    // ============ 4. МАКСИМАЛЬНЫЙ РАЗМЕР ============

    static void ShowMaxFileSizeSets() {
        std::cout << "\n";
        int i = 1;
        for (const auto& kv : settings::MaxFileSizeSets) {
            std::cout << "  " << i++ << ". [" << kv.first << "] "
                    << (kv.second < 0 ? std::string("без лимита")
                                        : std::to_string(kv.second) + " МБ")
                    << "\n";
        }
        std::cout << "  По умолчанию: " << settings::DefaultMaxFileSizeSet << "\n\n";
    }

    static void EditMaxFileSizeSets() {
        ShowMaxFileSizeSets();
        bool printMenu = true;
        while (true) {
            if (printMenu) {
                std::cout << "Что сделать?\n";
                std::cout << "  1. Добавить сет\n";
                std::cout << "  2. Изменить сет\n";
                std::cout << "  3. Удалить сет\n";
                std::cout << "  4. Сменить сет по умолчанию\n";
                std::cout << "  0. Назад\n> ";
                printMenu = false;
            } else {
                std::cout << "> ";
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
                std::cout << "Размер в МБ (-1 — без лимита, 0 — 0 МБ): ";
                int val = ReadInt(-1, 100000);
                settings::MaxFileSizeSets[name] = val;
                std::cout << "\nДобавлено.\n\n";
            }
            else if (n == 2 && !keys.empty()) {
                std::cout << "Введите номер сета: ";
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
                std::cout << "\nИзменено.\n\n";
            }
            else if (n == 3 && !keys.empty()) {
                std::cout << "Введите номер сета: ";
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                settings::MaxFileSizeSets.erase(keys[k - 1]);
                std::cout << "\nУдалено.\n\n";
            }
            else if (n == 4 && !keys.empty()) {
                std::cout << "Введите номер сета: ";
                int k = ReadInt(1, (int)keys.size());
                if (GoBack) { GoBack = false; continue; }
                settings::DefaultMaxFileSizeSet = keys[k - 1];
                std::cout << "\nПо умолчанию: " << keys[k - 1] << "\n\n";
            }
        }
    }

    // ============ 5. ВЫВОД ============

    static void EditOutputSettings() {
        bool printMenu = true;
        while (true) {
            if (printMenu) {
                std::cout << "\n=== ВЫВОД ===\n\n";
                std::cout << "  1. Показывать первых: " << settings::ShowFirstN << "\n";
                std::cout << "  2. Цветной: " << (settings::ColoredOutput ? "да" : "нет") << "\n";
                std::cout << "  3. Файл результатов: " << settings::ResultsFile << "\n";
                std::cout << "  0. Назад\n> ";
                printMenu = false;
            } else {
                std::cout << "> ";
            }

            std::string input;
            std::getline(std::cin, input);
            size_t s = input.find_first_not_of(" \t\r\n");
            size_t e = input.find_last_not_of(" \t\r\n");
            if (s == std::string::npos) continue;
            input = input.substr(s, e - s + 1);

            int n = -1;
            try { n = std::stoi(input); } catch (...) { continue; }
            if (n < 0 || n > 3) continue;

            if (n == 0) return;
            printMenu = true;

            if (n == 1) {
                std::cout << "Сколько (0 — назад): ";
                int v = ReadInt(1, 100000);
                if (GoBack) { GoBack = false; continue; }
                settings::ShowFirstN = v;
                std::cout << "\nУстановлено.\n\n";
            }
            else if (n == 2) {
                settings::ColoredOutput = !settings::ColoredOutput;
                std::cout << "\nИзменено.\n\n";
            }
            else if (n == 3) {
                auto v = ReadLine("Имя файла (0 — назад): ");
                if (GoBack) { GoBack = false; continue; }
                settings::ResultsFile = v;
                std::cout << "\nУстановлено.\n\n";
            }
        }
    }

    // ============ 6. ЯЗЫКИ ============

    static void ShowLanguages() {
        std::cout << "\n";
        int i = 1;
        for (const auto& l : settings::FavoriteLanguages) {
            std::cout << "  " << i++ << ". [" << l.Name << "] "
                      << "prefix=" << l.Prefix
                      << ", signs=" << l.Signs << "\n";
        }
        std::cout << "  По умолчанию: " << settings::DefaultLanguage << "\n\n";
    }

    static void EditLanguages() {
        ShowLanguages();
        bool printMenu = true;
        while (true) {
            if (printMenu) {
                std::cout << "Что сделать?\n";
                std::cout << "  1. Добавить язык\n";
                std::cout << "  2. Изменить язык\n";
                std::cout << "  3. Удалить язык\n";
                std::cout << "  4. Сменить язык по умолчанию\n";
                std::cout << "  0. Назад\n> ";
                printMenu = false;
            } else {
                std::cout << "> ";
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
                std::cout << "\nДобавлено.\n\n";
            }
            else if (n == 2 && sz > 0) {
                std::cout << "Введите номер языка: ";
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
                std::cout << "\nИзменено.\n\n";
            }
            else if (n == 3 && sz > 0) {
                std::cout << "Введите номер языка: ";
                int k = ReadInt(1, sz);
                if (GoBack) { GoBack = false; continue; }
                settings::FavoriteLanguages.erase(
                    settings::FavoriteLanguages.begin() + (k - 1));
                std::cout << "\nУдалено.\n\n";
            }
            else if (n == 4 && sz > 0) {
                std::cout << "Введите номер языка: ";
                int k = ReadInt(1, sz);
                if (GoBack) { GoBack = false; continue; }
                settings::DefaultLanguage = settings::FavoriteLanguages[k - 1].Name;
                std::cout << "\nПо умолчанию: "
                          << settings::DefaultLanguage << "\n\n";
            }
        }
    }

    // ============ ГЛАВНОЕ МЕНЮ НАСТРОЕК ============

    void EditSettings() {
        bool printMenu = true;
        while (true) {
            if (printMenu) {
                std::cout << "\n=== НАСТРОЙКИ ===\n\n";
                std::cout << "  1. Расширения по языкам\n";
                std::cout << "  2. Исключённые папки\n";
                std::cout << "  3. Исключённые файлы\n";
                std::cout << "  4. Максимальный размер файла\n";
                std::cout << "  5. Вывод\n";
                std::cout << "  6. Языки\n";
                std::cout << "  0. Сохранить и выйти\n> ";
                printMenu = false;
            } else {
                std::cout << "> ";
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
                    std::cout << "\nСохранено в settings.json.\n\n";
                } else {
                    std::cout << "\nОшибка сохранения.\n\n";
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
                        const regex_builder::SearchRequest& req) {
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

                std::cout << "\n";
                std::cout << "Измените параметры поиска:\n";
                std::cout << "  1. Начать поиск\n";
                std::cout << "  2. Директория: " << dirLabel << "\n";
                std::cout << "  3. Расширения: " << FormatSet(opt.Extensions) << "\n";
                std::cout << "  4. Исключённые папки: " << FormatSet(opt.ExcludeDirs) << "\n";
                std::cout << "  5. Исключённые файлы: " << FormatSet(opt.ExcludeFiles) << "\n";
                std::cout << "  6. Максимальный размер: "
                    << (opt.MaxFileSizeMb < 0 ? std::string("без лимита") : std::to_string(opt.MaxFileSizeMb) + " МБ")
                    << "\n";
                std::cout << "  0. Отмена\n> ";
                printMenu = false;
            } else {
                std::cout << "> ";
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
            if (!valid) continue;

            printMenu = true;

            if (choice == "2") {
                auto dir = ReadLine("Директория (Enter — текущая, 0 — назад): ");
                if (GoBack) { GoBack = false; printMenu = true; continue; }
                if (!dir.empty()) {
                    fs::path dirPath = fs::u8path(dir);
                    if (fs::exists(dirPath) && fs::is_directory(dirPath))
                        opt.Directory = dirPath;
                    else
                        std::cout << colors::Error << "Директория не найдена: " << dir << colors::Reset << "\n";
                }
            }
            else if (choice == "3") {
                if (!settings::ExtensionSets.empty()) {
                    std::cout << "\nВыберите сет расширений:\n";
                    int i = 1;
                    std::vector<std::string> keys;
                    for (const auto& kv : settings::ExtensionSets) {
                        std::cout << "  " << i++ << ". " << kv.first
                                  << " — " << FormatSet(kv.second) << "\n";
                        keys.push_back(kv.first);
                    }
                    std::cout << "  0. Отмена\n> ";
                    int n = ReadInt(0, (int)keys.size());
                    if (GoBack) { GoBack = false; printMenu = true; continue; }
                    if (n > 0) opt.Extensions = settings::ExtensionSets[keys[n - 1]];
                }
            }
            else if (choice == "4") {
                if (!settings::ExcludeDirSets.empty()) {
                    std::cout << "\nВыберите сет папок:\n";
                    int i = 1;
                    std::vector<std::string> keys;
                    for (const auto& kv : settings::ExcludeDirSets) {
                        std::cout << "  " << i++ << ". " << kv.first
                                  << " — " << FormatSet(kv.second) << "\n";
                        keys.push_back(kv.first);
                    }
                    std::cout << "  0. Отмена\n> ";
                    int n = ReadInt(0, (int)keys.size());
                    if (GoBack) { GoBack = false; printMenu = true; continue; }
                    if (n > 0) opt.ExcludeDirs = settings::ExcludeDirSets[keys[n - 1]];
                }
            }
            else if (choice == "5") {
                if (!settings::ExcludeFileSets.empty()) {
                    std::cout << "\nВыберите сет файлов:\n";
                    int i = 1;
                    std::vector<std::string> keys;
                    for (const auto& kv : settings::ExcludeFileSets) {
                        std::cout << "  " << i++ << ". " << kv.first
                                  << " — " << FormatSet(kv.second) << "\n";
                        keys.push_back(kv.first);
                    }
                    std::cout << "  0. Отмена\n> ";
                    int n = ReadInt(0, (int)keys.size());
                    if (GoBack) { GoBack = false; printMenu = true; continue; }
                    if (n > 0) opt.ExcludeFiles = settings::ExcludeFileSets[keys[n - 1]];
                }
            }
            else if (choice == "6") {
                std::cout << "Максимум МБ (-1 — без лимита, 0 — 0 МБ): ";
                int mb = ReadInt(-1, 10000);
                opt.MaxFileSizeMb = mb;
            }
        }
    }
}