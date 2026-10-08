#pragma once
#include <string>
#include <vector>
#include <map>

namespace settings {

    // ============ СЕТЫ РАСШИРЕНИЙ ============

    inline std::map<std::string, std::vector<std::string>> ExtensionSets = {
        // Раскомментируй:
        // {"C#",     {".cs"}},
        // {"C++",    {".cpp", ".h", ".hpp"}},
        // {"Java",   {".java"}},
        // {"Python", {".py"}},
        // {"Web",    {".js", ".ts", ".html", ".css"}},
    };

    inline std::string DefaultExtensionSet = "";

    inline std::vector<std::string> FallbackExtensions = {".cs"};


    // ============ СЕТЫ ИСКЛЮЧЁННЫХ ПАПОК ============

    inline std::map<std::string, std::vector<std::string>> ExcludeDirSets = {
        // {"Default", {"bin", "obj", ".git", ".vs"}},
        // {"Web",     {"node_modules", "dist", "build"}},
        // {"Python",  {"__pycache__", ".venv", "venv"}},
        // {"None",    {}}
    };

    inline std::string DefaultExcludeDirSet = "";

    inline std::vector<std::string> FallbackExcludeDirs = {
        "bin", "obj", ".git", "node_modules", ".vs"
    };


    // ============ СЕТЫ ИСКЛЮЧЁННЫХ ФАЙЛОВ ============

    inline std::map<std::string, std::vector<std::string>> ExcludeFileSets = {
        // {"C#",   {"*.Designer.cs", "*.g.cs"}},
        // {"None", {}}
    };

    inline std::string DefaultExcludeFileSet = "";

    inline std::vector<std::string> FallbackExcludeFiles = {};


    // ============ СЕТЫ МАКСИМАЛЬНОГО РАЗМЕРА ============

    inline std::map<std::string, int> MaxFileSizeSets = {
        // {"Все читать", -1},
        // {"До 10 МБ",   10},
        // {"До 100 МБ",  100}
    };

    inline std::string DefaultMaxFileSizeSet = "";

    inline int FallbackMaxFileSizeMb = 200;


    // ============ ВЫВОД ============

    inline int ShowFirstN = 50;
    inline bool ColoredOutput = true;
    inline std::string ResultsFile = "search_results.txt";
}