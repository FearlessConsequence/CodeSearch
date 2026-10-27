#pragma once
#include <string>
#include "RegexBuilder.h"
#include "FileSearcher.h"


namespace prompt {

    extern bool GoBack;
    extern bool WantHistory;
    extern bool WantFavorites; 
    extern bool WantClipboardHistory;
    
    regex_builder::SearchRequest Run();
    bool AskSearchOptions(file_searcher::SearchOptions& options,
                          const regex_builder::SearchRequest& req);

    void EditSettings();

    int ReadInt(int minVal, int maxVal);
    std::string ReadLine(const std::string& promptText);
}