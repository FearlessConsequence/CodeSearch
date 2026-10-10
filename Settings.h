#pragma once
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include "include/json.hpp"

using json = nlohmann::json;

namespace settings {

    // ============ ЯЗЫК ============

    struct LanguageInfo {
        std::string Name;
        std::string Prefix;
        std::string Signs;
    };

    // ============ ИСТОРИЯ ============

    struct HistoryEntry {
        std::string Type;
        std::string Language;
        std::string Pattern;
        std::string Description;
    };

    inline std::vector<HistoryEntry> History;
    inline int MaxHistory = 20;

    // ============ ВСЕ НАСТРОЙКИ ============

    inline std::map<std::string, std::vector<std::string>> ExtensionSets;
    inline std::string DefaultExtensionSet = "";
    inline std::vector<std::string> FallbackExtensions = {".cs"};

    inline std::map<std::string, std::vector<std::string>> ExcludeDirSets;
    inline std::string DefaultExcludeDirSet = "";
    inline std::vector<std::string> FallbackExcludeDirs = {
        "bin", "obj", ".git", "node_modules", ".vs"
    };

    inline std::map<std::string, std::vector<std::string>> ExcludeFileSets;
    inline std::string DefaultExcludeFileSet = "";
    inline std::vector<std::string> FallbackExcludeFiles = {};

    inline std::map<std::string, int> MaxFileSizeSets;
    inline std::string DefaultMaxFileSizeSet = "";
    inline int FallbackMaxFileSizeMb = 200;

    inline int ShowFirstN = 50;
    inline bool ColoredOutput = true;
    inline std::string ResultsFile = "search_results.txt";

    inline std::vector<LanguageInfo> FavoriteLanguages;
    inline std::string DefaultLanguage = "C#";

    // ============ ЗАГРУЗКА ============

    inline bool Load(const std::string& path = "settings.json") {
        std::ifstream in(path);
        if (!in) {
            std::cerr << "Не найден " << path << ". Используются дефолты.\n";
            return false;
        }

        json j;
        try {
            in >> j;
        }
        catch (const std::exception& e) {
            std::cerr << "Ошибка парсинга JSON: " << e.what() << "\n";
            return false;
        }

        try {
            if (j.contains("extension_sets")) {
                ExtensionSets.clear();
                for (auto& [k, v] : j["extension_sets"].items()) {
                    ExtensionSets[k] = v.get<std::vector<std::string>>();
                }
            }
            if (j.contains("default_extension_set"))
                DefaultExtensionSet = j["default_extension_set"];
            if (j.contains("fallback_extensions"))
                FallbackExtensions = j["fallback_extensions"];

            if (j.contains("exclude_dir_sets")) {
                ExcludeDirSets.clear();
                for (auto& [k, v] : j["exclude_dir_sets"].items()) {
                    ExcludeDirSets[k] = v.get<std::vector<std::string>>();
                }
            }
            if (j.contains("default_exclude_dir_set"))
                DefaultExcludeDirSet = j["default_exclude_dir_set"];
            if (j.contains("fallback_exclude_dirs"))
                FallbackExcludeDirs = j["fallback_exclude_dirs"];

            if (j.contains("exclude_file_sets")) {
                ExcludeFileSets.clear();
                for (auto& [k, v] : j["exclude_file_sets"].items()) {
                    ExcludeFileSets[k] = v.get<std::vector<std::string>>();
                }
            }
            if (j.contains("default_exclude_file_set"))
                DefaultExcludeFileSet = j["default_exclude_file_set"];
            if (j.contains("fallback_exclude_files"))
                FallbackExcludeFiles = j["fallback_exclude_files"];

            if (j.contains("max_file_size_sets")) {
                MaxFileSizeSets.clear();
                for (auto& [k, v] : j["max_file_size_sets"].items()) {
                    MaxFileSizeSets[k] = v.get<int>();
                }
            }
            if (j.contains("default_max_file_size_set"))
                DefaultMaxFileSizeSet = j["default_max_file_size_set"];
            if (j.contains("fallback_max_file_size_mb"))
                FallbackMaxFileSizeMb = j["fallback_max_file_size_mb"];

            if (j.contains("output")) {
                auto& out = j["output"];
                if (out.contains("show_first_n"))
                    ShowFirstN = out["show_first_n"];
                if (out.contains("colored_output"))
                    ColoredOutput = out["colored_output"];
                if (out.contains("results_file"))
                    ResultsFile = out["results_file"];
            }

            if (j.contains("favorite_languages")) {
                FavoriteLanguages.clear();
                for (auto& lang : j["favorite_languages"]) {
                    LanguageInfo info;
                    info.Name   = lang.value("name", "");
                    info.Prefix = lang.value("prefix", "//");
                    info.Signs  = lang.value("signs", "");
                    if (!info.Name.empty())
                        FavoriteLanguages.push_back(info);
                }
            }
            if (j.contains("default_language"))
                DefaultLanguage = j["default_language"];
        }
        catch (const std::exception& e) {
            std::cerr << "Ошибка чтения JSON: " << e.what() << "\n";
            return false;
        }

        return true;
    }

    // ============ ЯЗЫК ============

    inline LanguageInfo GetLanguage(const std::string& name) {
        for (const auto& lang : FavoriteLanguages) {
            if (lang.Name == name) return lang;
        }
        if (!FavoriteLanguages.empty())
            return FavoriteLanguages[0];
        return { "C#", "//", "=;:(){}[]<>?!." };
    }

    inline LanguageInfo CurrentLanguage() {
        return GetLanguage(DefaultLanguage);
    }

    // ============ СОХРАНЕНИЕ НАСТРОЕК ============

    inline bool Save(const std::string& path = "settings.json") {
        json j;

        j["extension_sets"] = ExtensionSets;
        j["default_extension_set"] = DefaultExtensionSet;
        j["fallback_extensions"] = FallbackExtensions;

        j["exclude_dir_sets"] = ExcludeDirSets;
        j["default_exclude_dir_set"] = DefaultExcludeDirSet;
        j["fallback_exclude_dirs"] = FallbackExcludeDirs;

        j["exclude_file_sets"] = ExcludeFileSets;
        j["default_exclude_file_set"] = DefaultExcludeFileSet;
        j["fallback_exclude_files"] = FallbackExcludeFiles;

        j["max_file_size_sets"] = MaxFileSizeSets;
        j["default_max_file_size_set"] = DefaultMaxFileSizeSet;
        j["fallback_max_file_size_mb"] = FallbackMaxFileSizeMb;

        j["output"] = {
            { "show_first_n", ShowFirstN },
            { "colored_output", ColoredOutput },
            { "results_file", ResultsFile }
        };

        json langs = json::array();
        for (const auto& l : FavoriteLanguages) {
            langs.push_back({
                { "name", l.Name },
                { "prefix", l.Prefix },
                { "signs", l.Signs }
            });
        }
        j["favorite_languages"] = langs;
        j["default_language"] = DefaultLanguage;

        std::ofstream out(path);
        if (!out) return false;
        out << j.dump(2);
        return true;
    }

    // ============ ИСТОРИЯ: ЗАГРУЗКА ============

    inline bool LoadHistory(const std::string& path = "history.json") {
        std::ifstream in(path);
        if (!in) return false;
        try {
            json j;
            in >> j;
            History.clear();
            for (auto& e : j["history"]) {
                HistoryEntry h;
                h.Type = e.value("type", "");
                h.Language = e.value("language", "");
                h.Pattern = e.value("pattern", "");
                h.Description = e.value("description", "");
                History.push_back(h);
            }
            MaxHistory = j.value("max", 20);
            return true;
        } catch (...) { return false; }
    }

    // ============ ИСТОРИЯ: СОХРАНЕНИЕ ============

    inline bool SaveHistory(const std::string& path = "history.json") {
        json j;
        json arr = json::array();
        for (const auto& e : History) {
            arr.push_back({
                {"type", e.Type},
                {"language", e.Language},
                {"pattern", e.Pattern},
                {"description", e.Description}
            });
        }
        j["history"] = arr;
        j["max"] = MaxHistory;

        std::ofstream out(path);
        if (!out) return false;
        out << j.dump(2);
        return true;
    }

    // ============ ИСТОРИЯ: ДОБАВИТЬ ============

    inline void AddToHistory(const HistoryEntry& e) {
        for (auto it = History.begin(); it != History.end(); ++it) {
            if (it->Pattern == e.Pattern) {
                History.erase(it);
                break;
            }
        }
        History.insert(History.begin(), e);
        while ((int)History.size() > MaxHistory)
            History.pop_back();
    }

}