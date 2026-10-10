#include "Prompt.h"
#include "Colors.h"
#include "Settings.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace prompt {

    // ============ ВСПОМОГАТЕЛЬНОЕ ============

    int ReadInt(int minVal, int maxVal) {
        while (true) {
            std::string input;
            std::getline(std::cin, input);

            size_t start = input.find_first_not_of(" \t\r\n");
            size_t end = input.find_last_not_of(" \t\r\n");
            if (start == std::string::npos) {
                std::cout << "Введите число от " << minVal << " до " << maxVal << ": ";
                continue;
            }
            input = input.substr(start, end - start + 1);

            try {
                int value = std::stoi(input);
                if (value >= minVal && value <= maxVal)
                    return value;
            }
            catch (...) {}

            std::cout << "Введите число от " << minVal << " до " << maxVal << ": ";
        }
    }

    static std::string ReadLine(const std::string& promptText) {
        std::cout << promptText;
        std::string input;
        std::getline(std::cin, input);

        size_t start = input.find_first_not_of(" \t\r\n");
        size_t end = input.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        return input.substr(start, end - start + 1);
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

    // ============ ТИП 1 ============

    static void AskWord(regex_builder::SearchRequest& req) {
        req.Word = ReadLine("Введите слово: ");
        auto w = ReadLine("Искать как отдельное слово? (y/n, Enter — да): ");
        std::transform(w.begin(), w.end(), w.begin(), ::tolower);
        req.WordWholeOnly = (w != "n");
    }

    // ============ ТИП 2 ============

    static void AskFormat(regex_builder::SearchRequest& req) {
        std::cout << "Что искать?\n";
        std::cout << "  1. Email\n";
        std::cout << "  2. URL\n";
        std::cout << "  3. IP-адрес\n";
        std::cout << "  4. GUID\n";
        std::cout << "  5. Дата (2024-05-12)\n";
        std::cout << "> ";
        req.Format = (patterns::FormatType)ReadInt(1, 5);
    }

    // ============ ТИП 3 ============

    static void AskStructure(regex_builder::SearchRequest& req) {
        std::cout << "Что искать?\n";
        std::cout << "  1. X, внутри которого пусто\n";
        std::cout << "  2. X, потом что угодно, потом Y, потом что угодно, потом Z\n";
        std::cout << "> ";
        req.Structure = (regex_builder::StructureType)ReadInt(1, 2);

        if (req.Structure == regex_builder::StructureType::XEmpty) {
            req.X = ReadLine("Что должно быть пустым (catch, метод, блок): ");
        }
        else {
            while (true) {
                std::cout << colors::Dim
                          << "(% — любые пробелы, * — любые символы)"
                          << colors::Reset << "\n";

                req.X = ReadLine("X: ");
                req.Y = ReadLine("Y: ");
                req.Z = ReadLine("Z: ");

                bool ok = true;

                if (req.X.empty()) {
                    std::cout << colors::Error
                              << "X не может быть пустым."
                              << colors::Reset << "\n\n";
                    ok = false;
                }
                if (req.Y.empty()) {
                    std::cout << colors::Error
                              << "Y не может быть пустым."
                              << colors::Reset << "\n\n";
                    ok = false;
                }
                if (req.Z.empty()) {
                    std::cout << colors::Error
                              << "Z не может быть пустым."
                              << colors::Reset << "\n\n";
                    ok = false;
                }

                if (ok) break;

                std::cout << "Попробуйте снова:\n";
            }
        }
    }

    // ============ ТИП 4 ============

    static void AskCombination(regex_builder::SearchRequest& req) {
        std::cout << "Что искать?\n";
        std::cout << "  1. X, но не Y\n";
        std::cout << "  2. X или Y или Z\n";
        std::cout << "> ";
        req.Combination = (regex_builder::CombinationType)ReadInt(1, 2);

        if (req.Combination == regex_builder::CombinationType::XOrY) {
            req.X = ReadLine("Варианты через запятую: ");
        }
        else {
            req.X = ReadLine("X: ");
            req.Y = ReadLine("Y (исключить): ");
            req.Exclude = req.Y;
        }
    }

    // ============ ТИП 5 ============

    static void AskQuantity(regex_builder::SearchRequest& req) {
        std::cout << "Что ограничить?\n";
        std::cout << "  1. Числа\n";
        std::cout << "  2. Длина строки (от скольки символов)\n";
        std::cout << "  3. Длина списка аргументов\n";
        std::cout << "> ";
        req.Quantity = (regex_builder::QuantityType)ReadInt(1, 3);

        if (req.Quantity == regex_builder::QuantityType::Numbers) {
            std::cout << "Как сравнивать количество цифр?\n";
            std::cout << "  1. Больше N\n";
            std::cout << "  2. Равно N\n";
            std::cout << "  3. Меньше N\n";
            std::cout << "  4. В диапазоне от A до B\n";
            std::cout << "> ";
            req.NumberCompareValue = (regex_builder::NumberCompare)ReadInt(1, 4);

            if (req.NumberCompareValue == regex_builder::NumberCompare::Between) {
                std::cout << "Минимум цифр: ";
                req.MinCount = ReadInt(1, 10000);
                std::cout << "Максимум цифр: ";
                req.MaxCount = ReadInt(req.MinCount, 10000);
            }
            else {
                std::cout << "Количество цифр: ";
                req.MinCount = ReadInt(1, 10000);
            }
        }
        else if (req.Quantity == regex_builder::QuantityType::LineLength) {
            std::cout << "Как сравнивать длину?\n";
            std::cout << "  1. Больше N\n";
            std::cout << "  2. Равно N\n";
            std::cout << "  3. Меньше N\n";
            std::cout << "  4. В диапазоне от A до B\n";
            std::cout << "> ";
            req.NumberCompareValue = (regex_builder::NumberCompare)ReadInt(1, 4);

            if (req.NumberCompareValue == regex_builder::NumberCompare::Between) {
                std::cout << "Минимум символов: ";
                req.MinCount = ReadInt(1, 10000);
                std::cout << "Максимум символов: ";
                req.MaxCount = ReadInt(req.MinCount, 10000);
            }
            else {
                std::cout << "Количество символов: ";
                req.MinCount = ReadInt(1, 10000);
            }
        }
        else if (req.Quantity == regex_builder::QuantityType::ArgsLength) {
            std::cout << "Как сравнивать количество аргументов?\n";
            std::cout << "  1. Больше N\n";
            std::cout << "  2. Равно N\n";
            std::cout << "  3. Меньше N\n";
            std::cout << "  4. В диапазоне от A до B\n";
            std::cout << "> ";
            req.NumberCompareValue = (regex_builder::NumberCompare)ReadInt(1, 4);

            if (req.NumberCompareValue == regex_builder::NumberCompare::Between) {
                std::cout << "Минимум аргументов: ";
                req.MinCount = ReadInt(1, 10000);
                std::cout << "Максимум аргументов: ";
                req.MaxCount = ReadInt(req.MinCount, 10000);
            }
            else {
                std::cout << "Количество аргументов: ";
                req.MinCount = ReadInt(1, 10000);
            }
        }
    }

    // ============ ТИП 6 ============

    static void AskComment(regex_builder::SearchRequest& req) {
        req.Language = settings::DefaultLanguage;

        std::cout << "Что искать?\n";
        std::cout << "  1. TODO / FIXME / HACK\n";
        std::cout << "  2. Комментарий в начале строки\n";
        std::cout << "  3. Комментарий в конце строки\n";
        std::cout << "  4. Закомментированный код\n";
        std::cout << "> ";
        req.Comment = (regex_builder::CommentType)ReadInt(1, 4);

        if (req.Comment == regex_builder::CommentType::Todo) {
            req.CommentWord = ReadLine("Слово (Enter — TODO/FIXME/HACK): ");
        }
    }

    // ============ ГЛАВНЫЙ МЕТОД ============

    regex_builder::SearchRequest Run() {
        regex_builder::SearchRequest req;

        std::cout << "=== Генератор регулярных выражений ===\n\n";

        std::cout << "Что искать?\n";
        std::cout << "  1. Слово с границами (name, но не username)\n";
        std::cout << "  2. Формат (email, URL, IP, GUID, дата)\n";
        std::cout << "  3. Структура\n";
        std::cout << "  4. Комбинирование (но не, или)\n";
        std::cout << "  5. Количество (длина, число цифр)\n";
        std::cout << "  6. Комментарии (TODO, закомментированный код)\n";
        std::cout << "> ";

        req.Type = (regex_builder::SearchType)ReadInt(1, 6);
        std::cout << "\n";

        switch (req.Type) {
            case regex_builder::SearchType::Word:        AskWord(req); break;
            case regex_builder::SearchType::Format:      AskFormat(req); break;
            case regex_builder::SearchType::Structure:   AskStructure(req); break;
            case regex_builder::SearchType::Combination: AskCombination(req); break;
            case regex_builder::SearchType::Quantity:    AskQuantity(req); break;
            case regex_builder::SearchType::Comment:     AskComment(req); break;
        }

        return req;
    }

    // ============ ПАРАМЕТРЫ ПОИСКА ============

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
        while (true) {
            ShowExtensionSets();

            std::cout << "Что сделать?\n";
            std::cout << "  1. Добавить сет\n";
            std::cout << "  2. Изменить сет\n";
            std::cout << "  3. Удалить сет\n";
            std::cout << "  4. Сменить сет по умолчанию\n";
            std::cout << "  0. Назад\n";
            std::cout << "> ";

            int n = ReadInt(0, 4);
            if (n == 0) return;

            std::vector<std::string> keys;
            for (const auto& kv : settings::ExtensionSets)
                keys.push_back(kv.first);

            if (n == 1) {
                std::string name = ReadLine("Имя нового сета: ");
                if (name.empty()) continue;
                auto raw = ReadLine("Расширения через запятую: ");
                settings::ExtensionSets[name] = SplitByComma(raw);
            }
            else if (n == 2 && !keys.empty()) {
                std::cout << "Какой сет изменить: ";
                int k = ReadInt(1, (int)keys.size());
                std::string oldName = keys[k - 1];
                auto oldVals = settings::ExtensionSets[oldName];

                auto newName = ReadLine("Имя сета (Enter — оставить): ");
                auto raw = ReadLine("Расширения через запятую (Enter — оставить): ");

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
            }
            else if (n == 3 && !keys.empty()) {
                std::cout << "Какой сет удалить: ";
                int k = ReadInt(1, (int)keys.size());
                settings::ExtensionSets.erase(keys[k - 1]);
            }
            else if (n == 4 && !keys.empty()) {
                std::cout << "Какой по умолчанию: ";
                int k = ReadInt(1, (int)keys.size());
                settings::DefaultExtensionSet = keys[k - 1];
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
        while (true) {
            ShowExcludeDirSets();

            std::cout << "Что сделать?\n";
            std::cout << "  1. Добавить сет\n";
            std::cout << "  2. Изменить сет\n";
            std::cout << "  3. Удалить сет\n";
            std::cout << "  4. Сменить сет по умолчанию\n";
            std::cout << "  0. Назад\n";
            std::cout << "> ";

            int n = ReadInt(0, 4);
            if (n == 0) return;

            std::vector<std::string> keys;
            for (const auto& kv : settings::ExcludeDirSets)
                keys.push_back(kv.first);

            if (n == 1) {
                std::string name = ReadLine("Имя нового сета: ");
                if (name.empty()) continue;
                auto raw = ReadLine("Папки через запятую: ");
                settings::ExcludeDirSets[name] = SplitByComma(raw);
            }
            else if (n == 2 && !keys.empty()) {
                std::cout << "Какой сет изменить: ";
                int k = ReadInt(1, (int)keys.size());
                std::string oldName = keys[k - 1];
                auto oldVals = settings::ExcludeDirSets[oldName];

                auto newName = ReadLine("Имя сета (Enter — оставить): ");
                auto raw = ReadLine("Папки через запятую (Enter — оставить): ");

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
            }
            else if (n == 3 && !keys.empty()) {
                std::cout << "Какой сет удалить: ";
                int k = ReadInt(1, (int)keys.size());
                settings::ExcludeDirSets.erase(keys[k - 1]);
            }
            else if (n == 4 && !keys.empty()) {
                std::cout << "Какой по умолчанию: ";
                int k = ReadInt(1, (int)keys.size());
                settings::DefaultExcludeDirSet = keys[k - 1];
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
        while (true) {
            ShowExcludeFileSets();

            std::cout << "Что сделать?\n";
            std::cout << "  1. Добавить сет\n";
            std::cout << "  2. Изменить сет\n";
            std::cout << "  3. Удалить сет\n";
            std::cout << "  4. Сменить сет по умолчанию\n";
            std::cout << "  0. Назад\n";
            std::cout << "> ";

            int n = ReadInt(0, 4);
            if (n == 0) return;

            std::vector<std::string> keys;
            for (const auto& kv : settings::ExcludeFileSets)
                keys.push_back(kv.first);

            if (n == 1) {
                std::string name = ReadLine("Имя нового сета: ");
                if (name.empty()) continue;
                auto raw = ReadLine("Файлы через запятую: ");
                settings::ExcludeFileSets[name] = SplitByComma(raw);
            }
            else if (n == 2 && !keys.empty()) {
                std::cout << "Какой сет изменить: ";
                int k = ReadInt(1, (int)keys.size());
                std::string oldName = keys[k - 1];
                auto oldVals = settings::ExcludeFileSets[oldName];

                auto newName = ReadLine("Имя сета (Enter — оставить): ");
                auto raw = ReadLine("Файлы через запятую (Enter — оставить): ");

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
            }
            else if (n == 3 && !keys.empty()) {
                std::cout << "Какой сет удалить: ";
                int k = ReadInt(1, (int)keys.size());
                settings::ExcludeFileSets.erase(keys[k - 1]);
            }
            else if (n == 4 && !keys.empty()) {
                std::cout << "Какой по умолчанию: ";
                int k = ReadInt(1, (int)keys.size());
                settings::DefaultExcludeFileSet = keys[k - 1];
            }
        }
    }

    // ============ 4. МАКСИМАЛЬНЫЙ РАЗМЕР ============

    static void ShowMaxFileSizeSets() {
        std::cout << "\n";
        int i = 1;
        for (const auto& kv : settings::MaxFileSizeSets) {
            std::cout << "  " << i++ << ". [" << kv.first << "] "
                      << kv.second << " МБ\n";
        }
        std::cout << "  По умолчанию: " << settings::DefaultMaxFileSizeSet << "\n\n";
    }

    static void EditMaxFileSizeSets() {
        while (true) {
            ShowMaxFileSizeSets();

            std::cout << "Что сделать?\n";
            std::cout << "  1. Добавить сет\n";
            std::cout << "  2. Изменить сет\n";
            std::cout << "  3. Удалить сет\n";
            std::cout << "  4. Сменить сет по умолчанию\n";
            std::cout << "  0. Назад\n";
            std::cout << "> ";

            int n = ReadInt(0, 4);
            if (n == 0) return;

            std::vector<std::string> keys;
            for (const auto& kv : settings::MaxFileSizeSets)
                keys.push_back(kv.first);

            if (n == 1) {
                std::string name = ReadLine("Имя нового сета: ");
                if (name.empty()) continue;
                std::cout << "Размер в МБ (-1 — без лимита): ";
                settings::MaxFileSizeSets[name] = ReadInt(-1, 100000);
            }
            else if (n == 2 && !keys.empty()) {
                std::cout << "Какой сет изменить: ";
                int k = ReadInt(1, (int)keys.size());
                std::string oldName = keys[k - 1];
                int oldVal = settings::MaxFileSizeSets[oldName];

                auto newName = ReadLine("Имя сета (Enter — оставить): ");
                auto raw = ReadLine("Размер в МБ (Enter — оставить): ");

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
            }
            else if (n == 3 && !keys.empty()) {
                std::cout << "Какой сет удалить: ";
                int k = ReadInt(1, (int)keys.size());
                settings::MaxFileSizeSets.erase(keys[k - 1]);
            }
            else if (n == 4 && !keys.empty()) {
                std::cout << "Какой по умолчанию: ";
                int k = ReadInt(1, (int)keys.size());
                settings::DefaultMaxFileSizeSet = keys[k - 1];
            }
        }
    }

    // ============ 5. ВЫВОД ============

    static void EditOutputSettings() {
        while (true) {
            std::cout << "\n=== ВЫВОД ===\n\n";
            std::cout << "  1. Показывать первых: " << settings::ShowFirstN << "\n";
            std::cout << "  2. Цветной: " << (settings::ColoredOutput ? "да" : "нет") << "\n";
            std::cout << "  3. Файл результатов: " << settings::ResultsFile << "\n";
            std::cout << "  0. Назад\n";
            std::cout << "> ";

            int n = ReadInt(0, 3);
            if (n == 0) return;

            if (n == 1) {
                std::cout << "Сколько: ";
                settings::ShowFirstN = ReadInt(1, 100000);
            }
            else if (n == 2) {
                settings::ColoredOutput = !settings::ColoredOutput;
            }
            else if (n == 3) {
                std::cout << "Имя файла: ";
                settings::ResultsFile = ReadLine("");
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
        while (true) {
            ShowLanguages();

            std::cout << "Что сделать?\n";
            std::cout << "  1. Добавить язык\n";
            std::cout << "  2. Изменить язык\n";
            std::cout << "  3. Удалить язык\n";
            std::cout << "  4. Сменить язык по умолчанию\n";
            std::cout << "  0. Назад\n";
            std::cout << "> ";

            int n = ReadInt(0, 4);
            if (n == 0) return;

            int sz = (int)settings::FavoriteLanguages.size();

            if (n == 1) {
                settings::LanguageInfo info;
                info.Name = ReadLine("Имя языка: ");
                if (info.Name.empty()) continue;
                info.Prefix = ReadLine("Префикс комментария: ");
                info.Signs = ReadLine("Признаки кода: ");
                settings::FavoriteLanguages.push_back(info);
            }
            else if (n == 2 && sz > 0) {
                std::cout << "Какой язык изменить: ";
                int k = ReadInt(1, sz);
                auto& lang = settings::FavoriteLanguages[k - 1];
                std::string oldName = lang.Name;

                auto name = ReadLine("Имя (Enter — оставить): ");
                auto prefix = ReadLine("Префикс (Enter — оставить): ");
                auto signs = ReadLine("Признаки (Enter — оставить): ");

                if (!name.empty()) lang.Name = name;
                if (!prefix.empty()) lang.Prefix = prefix;
                if (!signs.empty()) lang.Signs = signs;

                if (settings::DefaultLanguage == oldName && !name.empty())
                    settings::DefaultLanguage = name;
            }
            else if (n == 3 && sz > 0) {
                std::cout << "Какой язык удалить: ";
                int k = ReadInt(1, sz);
                settings::FavoriteLanguages.erase(
                    settings::FavoriteLanguages.begin() + (k - 1));
            }
            else if (n == 4 && sz > 0) {
                std::cout << "Какой по умолчанию: ";
                int k = ReadInt(1, sz);
                settings::DefaultLanguage = settings::FavoriteLanguages[k - 1].Name;
            }
        }
    }

    // ============ ГЛАВНОЕ МЕНЮ НАСТРОЕК ============

    void EditSettings() {
        while (true) {
            std::cout << "\n=== НАСТРОЙКИ ===\n\n";
            std::cout << "  1. Расширения по языкам\n";
            std::cout << "  2. Исключённые папки\n";
            std::cout << "  3. Исключённые файлы\n";
            std::cout << "  4. Максимальный размер файла\n";
            std::cout << "  5. Вывод\n";
            std::cout << "  6. Языки\n";
            std::cout << "  0. Сохранить и выйти\n";
            std::cout << "> ";

            int n = ReadInt(0, 6);

            if (n == 0) {
                if (settings::Save()) {
                    std::cout << "\nСохранено в settings.json.\n\n";
                } else {
                    std::cout << "\nОшибка сохранения.\n\n";
                }
                return;
            }

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

    // ============ ПАРАМЕТРЫ ПОИСКА ============

    bool AskSearchOptions(file_searcher::SearchOptions& opt,
                          const regex_builder::SearchRequest& req) {
        opt.Directory = fs::current_path();

        auto extIt = settings::ExtensionSets.find(req.Language);
        if (extIt != settings::ExtensionSets.end())
            opt.Extensions = extIt->second;
        else
            opt.Extensions = GetDefaultExtensions();

        opt.ExcludeDirs = GetDefaultExcludeDirs();
        opt.ExcludeFiles = GetDefaultExcludeFiles();
        opt.MaxFileSizeMb = GetDefaultMaxFileSize();
        opt.ShowFirstN = settings::ShowFirstN;

        while (true) {
            auto currentDir = fs::current_path();
            std::string dirLabel;
            if (opt.Directory == currentDir) {
                dirLabel = "[текущая]";
            } else {
                auto u8 = opt.Directory.u8string();
                dirLabel = std::string(u8.begin(), u8.end());
            }

            std::cout << "\n";
            std::cout << "Измените параметры поиска:\n";
            std::cout << "  1. Начать поиск\n";
            std::cout << "  2. Директория: " << dirLabel << "\n";
            std::cout << "  3. Расширения: " << FormatSet(opt.Extensions) << "\n";
            std::cout << "  4. Исключённые папки: " << FormatSet(opt.ExcludeDirs) << "\n";
            std::cout << "  5. Исключённые файлы: " << FormatSet(opt.ExcludeFiles) << "\n";
            std::cout << "  6. Максимальный размер: " << opt.MaxFileSizeMb << " МБ\n";
            std::cout << "  7. Границы слов: " << (opt.WordBoundaries ? "да" : "нет") << "\n";
            std::cout << "  0. Отмена\n";
            std::cout << "> ";

            std::string choice;
            std::getline(std::cin, choice);
            size_t s = choice.find_first_not_of(" \t\r\n");
            size_t e = choice.find_last_not_of(" \t\r\n");
            if (s == std::string::npos) continue;
            choice = choice.substr(s, e - s + 1);

            if (choice == "0") return false;
            if (choice == "1") return true;

            if (choice == "2") {
                auto dir = ReadLine("Директория (Enter — текущая): ");
                if (!dir.empty()) {
                    fs::path dirPath = fs::u8path(dir);
                    if (fs::exists(dirPath))
                        opt.Directory = dirPath;
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
                    if (n > 0) opt.ExcludeFiles = settings::ExcludeFileSets[keys[n - 1]];
                }
            }
            else if (choice == "6") {
                std::cout << "Максимум МБ (-1 — без лимита): ";
                int mb = ReadInt(-1, 10000);
                opt.MaxFileSizeMb = mb;
            }
            else if (choice == "7") {
                opt.WordBoundaries = !opt.WordBoundaries;
            }
        }
    }
}