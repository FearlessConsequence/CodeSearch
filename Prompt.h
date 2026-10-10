#pragma once
#include "RegexBuilder.h"
#include "FileSearcher.h"

namespace prompt {

    regex_builder::SearchRequest Run();
    bool AskSearchOptions(file_searcher::SearchOptions& options,
        const regex_builder::SearchRequest& req);
    void EditSettings();
    int ReadInt(int minVal, int maxVal);
}