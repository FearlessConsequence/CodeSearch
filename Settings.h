#pragma once
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include "include/json.hpp"

using json = nlohmann::json;

namespace settings {

    inline bool Save(const std::string& path);

    struct LanguageInfo { std::string Name, Prefix, Signs; };

    struct HistoryEntry {
        std::string Type, Language, Pattern, Description;
        int ReqType = 1;
        std::string Word;
        bool WordWholeOnly = true;
        int FormatType = 1;
        std::string X, Y, Z;
        int StructureType = 1;
        std::string Exclude;
        int CombinationType = 1;
        int XButNotYModeValue = 1;
        int QuantityType = 1;
        int NumberCompareValue = 1;
        int MinCount = 0, MaxCount = 0;
        int CommentType = 1;
        std::string CommentWord;
        int EmptyMetricValue = 1;
        int EmptyMaxCount = 0;
    };

    inline std::vector<HistoryEntry> History;
    inline int MaxHistory = 20;

    struct FavoriteEntry {
        std::string Name, Type, Language, Pattern, Description;
        int ReqType = 1;
        std::string Word;
        bool WordWholeOnly = true;
        int FormatType = 1;
        std::string X, Y, Z;
        int StructureType = 1;
        std::string Exclude;
        int CombinationType = 1;
        int XButNotYModeValue = 1;
        int QuantityType = 1;
        int NumberCompareValue = 1;
        int MinCount = 0, MaxCount = 0;
        int CommentType = 1;
        std::string CommentWord;
        int EmptyMetricValue = 1;
        int EmptyMaxCount = 0;
    };

    inline std::vector<FavoriteEntry> Favorites;
    inline int MaxFavorites = 50;

    struct ClipboardEntry { std::string Time, Text, Source; };
    inline std::vector<ClipboardEntry> ClipboardHistory;
    inline int MaxClipboardEntries = 50;

    inline std::map<std::string, std::vector<std::string>> ExtensionSets;
    inline std::string DefaultExtensionSet = "";
    inline std::vector<std::string> FallbackExtensions = {".cs"};

    inline std::map<std::string, std::vector<std::string>> ExcludeDirSets;
    inline std::string DefaultExcludeDirSet = "";
    inline std::vector<std::string> FallbackExcludeDirs = {"bin", "obj", ".git", "node_modules", ".vs"};

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

        inline void SetDefaults() {
        if (ExtensionSets.empty()) {
            ExtensionSets["C#"]      = {".cs"};
            ExtensionSets["C++"]     = {".cpp", ".h", ".hpp"};
            ExtensionSets["C++, C#"] = {".cpp", ".h", ".hpp", ".cs"};
            ExtensionSets["Python"]  = {".py"};
            ExtensionSets["Web"]     = {".js", ".ts", ".html", ".css"};
            if (DefaultExtensionSet.empty()) DefaultExtensionSet = "C#";
        }
        if (ExcludeDirSets.empty()) {
            ExcludeDirSets["Default"] = {"bin", "obj", ".git", ".vs"};
            ExcludeDirSets["None"]    = {};
            ExcludeDirSets["Python"]  = {"__pycache__", ".venv", "venv"};
            ExcludeDirSets["Web"]     = {"node_modules", "dist", "build"};
            if (DefaultExcludeDirSet.empty()) DefaultExcludeDirSet = "Default";
        }
        if (ExcludeFileSets.empty()) {
            ExcludeFileSets["None"] = {};
            if (DefaultExcludeFileSet.empty()) DefaultExcludeFileSet = "None";
        }
        if (MaxFileSizeSets.empty()) {
            MaxFileSizeSets["Все читать"] = -1;
            MaxFileSizeSets["До 10 МБ"]   = 10;
            MaxFileSizeSets["До 100 МБ"]  = 100;
            MaxFileSizeSets["До 200 МБ"]  = 200;
            if (DefaultMaxFileSizeSet.empty()) DefaultMaxFileSizeSet = "До 10 МБ";
        }
        if (FavoriteLanguages.empty()) {
            FavoriteLanguages.push_back({"C#",         "//", "=;(){}[\\]<>?!."});
            FavoriteLanguages.push_back({"C++",        "//", "=;(){}[\\]<>?!."});
            FavoriteLanguages.push_back({"Python",     "#",  "=:(){}[\\]<>?!."});
            FavoriteLanguages.push_back({"JavaScript", "//", "=;(){}[\\]<>?!."});
            if (DefaultLanguage.empty()) DefaultLanguage = "C#";
        }
    }

    inline bool Load(const std::string& path = "settings.json") {
        std::ifstream in(path);
        if (!in) { std::cerr << "Не найден " << path << ". Создан с дефолтами.\n"; SetDefaults(); Save(path); return true; }
        json j;
        try { in >> j; } catch (const std::exception& e) { std::cerr << "Ошибка парсинга JSON: " << e.what() << "\n"; return false; }
        try {
            if (j.contains("extension_sets")) { ExtensionSets.clear(); for (auto& [k, v] : j["extension_sets"].items()) ExtensionSets[k] = v.get<std::vector<std::string>>(); }
            if (j.contains("default_extension_set")) DefaultExtensionSet = j["default_extension_set"];
            if (j.contains("fallback_extensions")) FallbackExtensions = j["fallback_extensions"];
            if (j.contains("exclude_dir_sets")) { ExcludeDirSets.clear(); for (auto& [k, v] : j["exclude_dir_sets"].items()) ExcludeDirSets[k] = v.get<std::vector<std::string>>(); }
            if (j.contains("default_exclude_dir_set")) DefaultExcludeDirSet = j["default_exclude_dir_set"];
            if (j.contains("fallback_exclude_dirs")) FallbackExcludeDirs = j["fallback_exclude_dirs"];
            if (j.contains("exclude_file_sets")) { ExcludeFileSets.clear(); for (auto& [k, v] : j["exclude_file_sets"].items()) ExcludeFileSets[k] = v.get<std::vector<std::string>>(); }
            if (j.contains("default_exclude_file_set")) DefaultExcludeFileSet = j["default_exclude_file_set"];
            if (j.contains("fallback_exclude_files")) FallbackExcludeFiles = j["fallback_exclude_files"];
            if (j.contains("max_file_size_sets")) { MaxFileSizeSets.clear(); for (auto& [k, v] : j["max_file_size_sets"].items()) MaxFileSizeSets[k] = v.get<int>(); }
            if (j.contains("default_max_file_size_set")) DefaultMaxFileSizeSet = j["default_max_file_size_set"];
            if (j.contains("fallback_max_file_size_mb")) FallbackMaxFileSizeMb = j["fallback_max_file_size_mb"];
            if (j.contains("output")) {
                auto& out = j["output"];
                if (out.contains("show_first_n")) ShowFirstN = out["show_first_n"];
                if (out.contains("colored_output")) ColoredOutput = out["colored_output"];
                if (out.contains("results_file")) ResultsFile = out["results_file"];
            }
            if (j.contains("favorite_languages")) {
                FavoriteLanguages.clear();
                for (auto& lang : j["favorite_languages"]) {
                    LanguageInfo info;
                    info.Name = lang.value("name", "");
                    info.Prefix = lang.value("prefix", "//");
                    info.Signs = lang.value("signs", "");
                    if (!info.Name.empty()) FavoriteLanguages.push_back(info);
                }
            }
            if (j.contains("default_language")) DefaultLanguage = j["default_language"];
        } catch (const std::exception& e) { std::cerr << "Ошибка чтения JSON: " << e.what() << "\n"; return false; }
        SetDefaults();
        return true;
    }

    inline LanguageInfo GetLanguage(const std::string& name) {
        for (const auto& lang : FavoriteLanguages) if (lang.Name == name) return lang;
        if (!FavoriteLanguages.empty()) return FavoriteLanguages[0];
        return { "C#", "//", "=;:(){}[]<>?!." };
    }

    inline LanguageInfo CurrentLanguage() { return GetLanguage(DefaultLanguage); }

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
        j["output"] = { {"show_first_n", ShowFirstN}, {"colored_output", ColoredOutput}, {"results_file", ResultsFile} };
        json langs = json::array();
        for (const auto& l : FavoriteLanguages) langs.push_back({ {"name", l.Name}, {"prefix", l.Prefix}, {"signs", l.Signs} });
        j["favorite_languages"] = langs;
        j["default_language"] = DefaultLanguage;
        std::ofstream out(path);
        if (!out) return false;
        out << j.dump(2);
        return true;
    }

    inline bool LoadHistory(const std::string& path = "history.json") {
        std::ifstream in(path);
        if (!in) return false;
        try {
            json j; in >> j;
            History.clear();
            for (auto& e : j["history"]) {
                HistoryEntry h;
                h.Type = e.value("type", "");
                h.Language = e.value("language", "");
                h.Pattern = e.value("pattern", "");
                h.Description = e.value("description", "");
                h.ReqType = e.value("req_type", 1);
                h.Word = e.value("word", "");
                h.WordWholeOnly = e.value("word_whole", true);
                h.FormatType = e.value("format_type", 1);
                h.X = e.value("x", "");
                h.Y = e.value("y", "");
                h.Z = e.value("z", "");
                h.StructureType = e.value("structure_type", 1);
                h.Exclude = e.value("exclude", "");
                h.CombinationType = e.value("combination_type", 1);
                h.XButNotYModeValue = e.value("x_not_y_mode", 1);
                h.QuantityType = e.value("quantity_type", 1);
                h.NumberCompareValue = e.value("number_compare", 1);
                h.MinCount = e.value("min_count", 0);
                h.MaxCount = e.value("max_count", 0);
                h.CommentType = e.value("comment_type", 1);
                h.CommentWord = e.value("comment_word", "");
                h.EmptyMetricValue = e.value("empty_metric", 1);
                h.EmptyMaxCount    = e.value("empty_max_count", 0);
                History.push_back(h);
            }
            MaxHistory = j.value("max", 20);
            return true;
        } catch (...) { return false; }
    }

    inline bool SaveHistory(const std::string& path = "history.json") {
        json j, arr = json::array();
        for (const auto& e : History) {
            arr.push_back({
                {"type", e.Type}, {"language", e.Language}, {"pattern", e.Pattern}, {"description", e.Description},
                {"req_type", e.ReqType}, {"word", e.Word}, {"word_whole", e.WordWholeOnly},
                {"format_type", e.FormatType}, {"x", e.X}, {"y", e.Y}, {"z", e.Z},
                {"structure_type", e.StructureType}, {"exclude", e.Exclude}, {"combination_type", e.CombinationType},
                {"x_not_y_mode", e.XButNotYModeValue},
                {"quantity_type", e.QuantityType}, {"number_compare", e.NumberCompareValue},
                {"min_count", e.MinCount}, {"max_count", e.MaxCount},
                {"comment_type", e.CommentType}, {"comment_word", e.CommentWord},
                {"empty_metric", e.EmptyMetricValue}, {"empty_max_count", e.EmptyMaxCount}
            });
        }
        j["history"] = arr;
        j["max"] = MaxHistory;
        std::ofstream out(path);
        if (!out) return false;
        out << j.dump(2);
        return true;
    }

    inline bool HistoryEquals(const HistoryEntry& a, const HistoryEntry& b) {
        return a.Pattern == b.Pattern && a.ReqType == b.ReqType && a.Language == b.Language &&
               a.Word == b.Word && a.X == b.X && a.Y == b.Y && a.Z == b.Z &&
               a.QuantityType == b.QuantityType && a.MinCount == b.MinCount && a.MaxCount == b.MaxCount;
    }

    inline void AddToHistory(const HistoryEntry& e) {
        for (auto it = History.begin(); it != History.end(); ++it) {
            if (HistoryEquals(*it, e)) { History.erase(it); break; }
        }
        History.insert(History.begin(), e);
        while ((int)History.size() > MaxHistory) History.pop_back();
    }

    inline bool LoadFavorites(const std::string& path = "favorites.json") {
        std::ifstream in(path);
        if (!in) return false;
        try {
            json j; in >> j;
            Favorites.clear();
            for (auto& e : j["favorites"]) {
                FavoriteEntry f;
                f.Name = e.value("name", "");
                f.Type = e.value("type", "");
                f.Language = e.value("language", "");
                f.Pattern = e.value("pattern", "");
                f.Description = e.value("description", "");
                f.ReqType = e.value("req_type", 1);
                f.Word = e.value("word", "");
                f.WordWholeOnly = e.value("word_whole", true);
                f.FormatType = e.value("format_type", 1);
                f.X = e.value("x", "");
                f.Y = e.value("y", "");
                f.Z = e.value("z", "");
                f.StructureType = e.value("structure_type", 1);
                f.Exclude = e.value("exclude", "");
                f.CombinationType = e.value("combination_type", 1);
                f.XButNotYModeValue = e.value("x_not_y_mode", 1);
                f.QuantityType = e.value("quantity_type", 1);
                f.NumberCompareValue = e.value("number_compare", 1);
                f.MinCount = e.value("min_count", 0);
                f.MaxCount = e.value("max_count", 0);
                f.CommentType = e.value("comment_type", 1);
                f.CommentWord = e.value("comment_word", "");
                f.EmptyMetricValue = e.value("empty_metric", 1);
                f.EmptyMaxCount    = e.value("empty_max_count", 0);
                Favorites.push_back(f);
            }
            MaxFavorites = j.value("max", 50);
            return true;
        } catch (...) { return false; }
    }

    inline bool SaveFavorites(const std::string& path = "favorites.json") {
        json j, arr = json::array();
        for (const auto& f : Favorites) {
            arr.push_back({
                {"name", f.Name}, {"type", f.Type}, {"language", f.Language}, {"pattern", f.Pattern}, {"description", f.Description},
                {"req_type", f.ReqType}, {"word", f.Word}, {"word_whole", f.WordWholeOnly},
                {"format_type", f.FormatType}, {"x", f.X}, {"y", f.Y}, {"z", f.Z},
                {"structure_type", f.StructureType}, {"exclude", f.Exclude}, {"combination_type", f.CombinationType},
                {"x_not_y_mode", f.XButNotYModeValue},
                {"quantity_type", f.QuantityType}, {"number_compare", f.NumberCompareValue},
                {"min_count", f.MinCount}, {"max_count", f.MaxCount},
                {"comment_type", f.CommentType}, {"comment_word", f.CommentWord},
                {"empty_metric", f.EmptyMetricValue}, {"empty_max_count", f.EmptyMaxCount}
            });
        }
        j["favorites"] = arr;
        j["max"] = MaxFavorites;
        std::ofstream out(path);
        if (!out) return false;
        out << j.dump(2);
        return true;
    }

    inline void AddToFavorites(const FavoriteEntry& e) {
        for (auto it = Favorites.begin(); it != Favorites.end(); ++it) {
            if (it->Pattern == e.Pattern && it->ReqType == e.ReqType && it->Language == e.Language) { Favorites.erase(it); break; }
        }
        Favorites.insert(Favorites.begin(), e);
        while ((int)Favorites.size() > MaxFavorites) Favorites.pop_back();
    }

    inline bool LoadClipboardHistory(const std::string& path = "clipboard_history.json") {
        std::ifstream in(path);
        if (!in) return false;
        try {
            json j; in >> j;
            ClipboardHistory.clear();
            for (auto& e : j["entries"]) {
                ClipboardEntry c;
                c.Time = e.value("time", "");
                c.Text = e.value("text", "");
                c.Source = e.value("source", "");
                ClipboardHistory.push_back(c);
            }
            MaxClipboardEntries = j.value("max", 50);
            return true;
        } catch (...) { return false; }
    }

    inline bool SaveClipboardHistory(const std::string& path = "clipboard_history.json") {
        json j, arr = json::array();
        for (const auto& c : ClipboardHistory) arr.push_back({ {"time", c.Time}, {"text", c.Text}, {"source", c.Source} });
        j["entries"] = arr;
        j["max"] = MaxClipboardEntries;
        std::ofstream out(path);
        if (!out) return false;
        out << j.dump(2);
        return true;
    }

    inline std::string NowString() {
        std::time_t t = std::time(nullptr);
        std::tm* tm = std::localtime(&t);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
        return std::string(buf);
    }

    inline bool ParseTime(const std::string& s, std::tm& out) {
        std::istringstream ss(s);
        ss >> std::get_time(&out, "%Y-%m-%d %H:%M:%S");
        return !ss.fail();
    }

    inline void CleanClipboardHistory(int hoursKeep = 24) {
        std::time_t now = std::time(nullptr);
        std::vector<ClipboardEntry> kept;
        for (const auto& c : ClipboardHistory) {
            std::tm tm{};
            if (!ParseTime(c.Time, tm)) continue;
            std::time_t t = std::mktime(&tm);
            if (std::difftime(now, t) / 3600.0 <= hoursKeep) kept.push_back(c);
        }
        ClipboardHistory = kept;
    }

    inline void AddClipboardEntry(const std::string& text, const std::string& source) {
        if (text.empty()) return;
        for (auto it = ClipboardHistory.begin(); it != ClipboardHistory.end(); ++it)
            if (it->Text == text) { ClipboardHistory.erase(it); break; }
        ClipboardEntry c;
        c.Time = NowString();
        c.Text = text;
        c.Source = source;
        ClipboardHistory.insert(ClipboardHistory.begin(), c);
        while ((int)ClipboardHistory.size() > MaxClipboardEntries) ClipboardHistory.pop_back();
    }
}