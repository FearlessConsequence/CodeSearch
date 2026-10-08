#pragma once
#include <string>
#include <vector>
#include "Patterns.h"

namespace regex_builder {

    enum class SearchType {
        Word = 1,
        Format = 2,
        Structure = 3,
        Combination = 4,
        Quantity = 5,
        Comment = 6
    };

    enum class StructureType {
        XEmpty = 1,
        XAnywhereBeforeYBeforeZ = 2
    };

    enum class CombinationType {
        XButNotY = 1,
        XOrY = 2
    };

    enum class QuantityType {
        Numbers = 1,
        LineLength = 2,
        ArgsLength = 3
    };

    enum class NumberCompare {
        Equal = 1,
        Greater = 2,
        Less = 3,
        Between = 4
    };

    enum class CommentType {
        Todo = 1,
        StartLine = 2,
        EndLine = 3,
        CommentedCode = 4,
        All = 5
    };

    struct SearchRequest {
        patterns::Language Language = patterns::Language::CSharp;
        SearchType Type = SearchType::Word;

        // Тип 1
        std::string Word;
        bool WordWholeOnly = true;

        // Тип 2
        patterns::FormatType Format = patterns::FormatType::Email;

        // Тип 3
        std::string X, Y, Z;
        StructureType Structure = StructureType::XEmpty;

        // Тип 4
        std::string Exclude;
        CombinationType Combination = CombinationType::XButNotY;

        // Тип 5
        QuantityType Quantity = QuantityType::Numbers;
        NumberCompare NumberCompareValue = NumberCompare::Greater;
        int MinCount = 0;
        int MaxCount = 0;

        // Тип 6
        CommentType Comment = CommentType::Todo;
        std::string CommentWord;
        std::vector<std::string> CommentedCodeWords;
    };

    std::string Build(const SearchRequest& req);
    std::string Escape(const std::string& s);
    std::string EscapeXyz(const std::string& s);
    std::string WrapWord(const std::string& s);
}