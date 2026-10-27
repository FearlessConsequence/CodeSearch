#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include "RegexBuilder.h"

namespace file_searcher {

    namespace fs = std::filesystem;

    struct SearchOptions {
        fs::path Directory;
        std::vector<std::string> Extensions;
        std::vector<std::string> ExcludeDirs;
        std::vector<std::string> ExcludeFiles;
        int MaxFileSizeMb = 200;
        int ShowFirstN = 50;
    };

    struct SearchResult {
        fs::path File;
        int Line = 0;
        std::string Text;
    };

    std::vector<SearchResult> Search(
        const regex_builder::SearchRequest& req,
        const std::string& pattern,
        const SearchOptions& options);
}