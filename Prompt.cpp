#include "Prompt.h"
#include "Colors.h"
#include "Settings.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace prompt {

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
        if (items.empty()) return "[нет]";
        std::string result;
        for (size_t i = 0; i < items.size(); ++i) {
            if (i > 0) result += ", ";
            result += items[i];
        }
        return result;
    }

    static std::string FormatSize(int mb) {
        if (mb < 0) return "[нет лимита]";
        return std::to_string(mb) + " МБ";
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
            // 3.2 — X, Y, Z обязательны
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
        else if (req.Quantity == regex_builder::QuantityType::ArgsLength) {
            std::cout << "Минимальное количество аргументов: ";
            req.MinCount = ReadInt(1, 10000);
        }
        else {
            std::cout << "Минимум: ";
            req.MinCount = ReadInt(1, 10000);
        }
    }

    // ============ ТИП 6 ============

    static void AskComment(regex_builder::SearchRequest& req) {
        std::cout << "Выберите язык:\n";
        std::cout << "  1. C#\n";
        std::cout << "  2. Python\n";
        std::cout << "  3. JavaScript\n";
        std::cout << "  4. Другой\n";
        std::cout << "> ";
        req.Language = (patterns::Language)ReadInt(1, 4);
        std::cout << "\n";

        std::cout << "Что искать?\n";
        std::cout << "  1. TODO / FIXME / HACK\n";
        std::cout << "  2. Комментарий в начале строки\n";
        std::cout << "  3. Комментарий в конце строки\n";
        std::cout << "  4. Закомментированный код\n";
        std::cout << "  5. Все комментарии\n";
        std::cout << "> ";
        req.Comment = (regex_builder::CommentType)ReadInt(1, 5);

        if (req.Comment == regex_builder::CommentType::Todo) {
            req.CommentWord = ReadLine("Слово (Enter — TODO/FIXME/HACK): ");
        }
        else if (req.Comment == regex_builder::CommentType::CommentedCode) {
            auto raw = ReadLine("Слова через запятую (Enter — признаки кода): ");
            if (!raw.empty()) {
                std::stringstream ss(raw);
                std::string item;
                while (std::getline(ss, item, ',')) {
                    while (!item.empty() && std::isspace((unsigned char)item.front()))
                        item.erase(item.begin());
                    while (!item.empty() && std::isspace((unsigned char)item.back()))
                        item.pop_back();
                    if (!item.empty())
                        req.CommentedCodeWords.push_back(item);
                }
            }
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

    bool AskSearchOptions(file_searcher::SearchOptions& opt) {
        opt.Directory = fs::current_path();
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
            std::cout << "  6. Максимальный размер: " << FormatSize(opt.MaxFileSizeMb) << "\n";
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