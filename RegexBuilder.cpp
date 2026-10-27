#include "RegexBuilder.h"
#include <cctype>
#include "Settings.h"
#include <regex>
#include <sstream>
#include <algorithm>

namespace regex_builder {

    const std::string PERCENT_PLACEHOLDER = "\x01" "PERCENT" "\x01";
    const std::string STAR_PLACEHOLDER    = "\x01" "STAR" "\x01";
    const std::string CARET_PLACEHOLDER   = "\x01" "CARET" "\x01";
    const std::string DOLLAR_PLACEHOLDER  = "\x01" "DOLLAR" "\x01";

    std::string Escape(const std::string& s) {
        static const std::string special = R"(\.^$|()[]{}*+?)";
        std::string result;
        for (char c : s) {
            if (special.find(c) != std::string::npos) result += '\\';
            result += c;
        }
        return result;
    }

    std::string EscapeXyz(const std::string& s) {
        std::string trimmed = s;
        while (!trimmed.empty() && std::isspace((unsigned char)trimmed.front())) trimmed.erase(trimmed.begin());
        while (!trimmed.empty() && std::isspace((unsigned char)trimmed.back())) trimmed.pop_back();
        if (trimmed == "}") return R"(^\s*\})";

        std::string temp = s;
        size_t pos;
        while ((pos = temp.find('%')) != std::string::npos) temp.replace(pos, 1, PERCENT_PLACEHOLDER);
        while ((pos = temp.find('*')) != std::string::npos) temp.replace(pos, 1, STAR_PLACEHOLDER);
        while ((pos = temp.find('^')) != std::string::npos) temp.replace(pos, 1, CARET_PLACEHOLDER);
        while ((pos = temp.find('$')) != std::string::npos) temp.replace(pos, 1, DOLLAR_PLACEHOLDER);

        std::string escaped = Escape(temp);

        while ((pos = escaped.find(PERCENT_PLACEHOLDER)) != std::string::npos) escaped.replace(pos, PERCENT_PLACEHOLDER.length(), R"(\s*)");
        while ((pos = escaped.find(STAR_PLACEHOLDER)) != std::string::npos) escaped.replace(pos, STAR_PLACEHOLDER.length(), R"([\s\S]*?)");
        while ((pos = escaped.find(CARET_PLACEHOLDER)) != std::string::npos) escaped.replace(pos, CARET_PLACEHOLDER.length(), R"(^)");
        while ((pos = escaped.find(DOLLAR_PLACEHOLDER)) != std::string::npos) escaped.replace(pos, DOLLAR_PLACEHOLDER.length(), R"($)");
        return escaped;
    }

    std::string WrapWord(const std::string& s) {
        std::string escaped = Escape(s);
        if (!s.empty() && std::isalpha((unsigned char)s[0])) return R"(\b)" + escaped + R"(\b)";
        return escaped;
    }

    static std::vector<std::string> SplitByCommaWithQuotes(const std::string& s) {
        std::vector<std::string> result;
        std::string current;
        bool inQuote = false;
        char quoteChar = 0;
        for (char c : s) {
            if (inQuote) {
                if (c == quoteChar) inQuote = false;
                else current += c;
            } else {
                if (c == '"' || c == '\'') { inQuote = true; quoteChar = c; }
                else if (c == ',') {
                    size_t a = current.find_first_not_of(" \t");
                    size_t b = current.find_last_not_of(" \t");
                    if (a != std::string::npos) result.push_back(current.substr(a, b - a + 1));
                    current.clear();
                } else current += c;
            }
        }
        size_t a = current.find_first_not_of(" \t");
        size_t b = current.find_last_not_of(" \t");
        if (a != std::string::npos) result.push_back(current.substr(a, b - a + 1));
        return result;
    }

    static std::string BuildWord(const SearchRequest& req) {
        std::string word = Escape(req.Word);
        if (req.WordWholeOnly) return R"(\b)" + word + R"(\b)";
        return word;
    }

    static std::string BuildEmpty(const std::string& x) {
        std::string lower = x;
        for (auto& c : lower) c = std::tolower((unsigned char)c);
        if (lower == "catch") return EscapeXyz(x) + R"(\s*(\([^)]*\))?\s*\{\s*\})";
        return EscapeXyz(x) + R"(\s*\{\s*\})";
    }

    static std::string BuildStructure(const SearchRequest& req) {
        switch (req.Structure) {
            case StructureType::XEmpty:
                return BuildEmpty(req.X);
            case StructureType::XAnywhereBeforeYBeforeZ:
                return EscapeXyz(req.X) + "[\\s\\S]*?(" +
                       EscapeXyz(req.Y) + ")[\\s\\S]*?" +
                       EscapeXyz(req.Z);
        }
        return "";
    }

    static std::string BuildCombination(const SearchRequest& req) {
        switch (req.Combination) {
            case CombinationType::XButNotY:
                return EscapeXyz(req.X);
            case CombinationType::XOrY: {
                auto items = SplitByCommaWithQuotes(req.X);
                std::string joined;
                for (size_t i = 0; i < items.size(); ++i) {
                    if (i > 0) joined += "|";
                    std::string esc = EscapeXyz(items[i]);
                    bool leftBound = !items[i].empty() && (std::isalnum((unsigned char)items[i][0]) || items[i][0] == '_');
                    bool rightBound = !items[i].empty() && (std::isalnum((unsigned char)items[i].back()) || items[i].back() == '_');
                    if (leftBound) esc = R"(\b)" + esc;
                    if (rightBound) esc = esc + R"(\b)";
                    joined += esc;
                }
                if (joined.empty()) return "";
                return R"((?:)" + joined + R"())";
            }
        }
        return "";
    }

    static std::string BuildNumbers(const SearchRequest& req) {
        switch (req.NumberCompareValue) {
            case NumberCompare::Equal:   return R"(\b\d{)" + std::to_string(req.MinCount) + R"(}\b)";
            case NumberCompare::Greater: return R"(\b\d{)" + std::to_string(req.MinCount + 1) + R"(,}\b)";
            case NumberCompare::Less:    return R"(\b\d{1,)" + std::to_string(std::max(1, req.MinCount - 1)) + R"(}\b)";
            case NumberCompare::Between: return R"(\b\d{)" + std::to_string(req.MinCount) + "," + std::to_string(req.MaxCount) + R"(}\b)";
        }
        return "";
    }

    static std::string BuildQuantity(const SearchRequest& req) {
        switch (req.Quantity) {
            case QuantityType::Numbers: return BuildNumbers(req);
            case QuantityType::LineLength:
                switch (req.NumberCompareValue) {
                    case NumberCompare::Greater: return "^.{" + std::to_string(req.MinCount + 1) + ",}$";
                    case NumberCompare::Equal:   return "^.{" + std::to_string(req.MinCount) + "}$";
                    case NumberCompare::Less:    return "^.{1," + std::to_string(std::max(1, req.MinCount - 1)) + "}$";
                    case NumberCompare::Between: return "^.{" + std::to_string(req.MinCount) + "," + std::to_string(req.MaxCount) + "}$";
                }
                return "";
            case QuantityType::ArgsLength: return "[args-length]";
        }
        return "";
    }

    static std::string BuildTodo(const SearchRequest& req, const std::string& prefix) {
        if (!req.CommentWord.empty()) return prefix + R"(\s*)" + Escape(req.CommentWord) + ".*";
        return prefix + R"(\s*(TODO|FIXME|HACK).*)";
    }

    static std::string BuildCommentedCode(const SearchRequest& req, const std::string& prefix) {
        auto lang = settings::GetLanguage(req.Language);
        if (!req.CommentedCodeWords.empty()) {
            std::string lookaheads;
            for (const auto& w : req.CommentedCodeWords) lookaheads += "(?=.*" + WrapWord(w) + ")";
            return R"(^\s*)" + prefix + R"(\s*)" + lookaheads + ".*";
        }
        return R"(^\s*)" + prefix + R"(\s*(?!(TODO|FIXME|HACK|NOTE)\b).*[)" + lang.Signs + R"(])";
    }

    static std::string BuildComment(const SearchRequest& req) {
        auto lang = settings::GetLanguage(req.Language);
        std::string prefix = lang.Prefix;
        std::string firstChar = prefix.substr(0, 1);
        switch (req.Comment) {
            case CommentType::Todo:          return BuildTodo(req, prefix);
            case CommentType::StartLine:     return R"(^\s*)" + prefix + ".*";
            case CommentType::EndLine:       return R"(^\s*[^)" + firstChar + R"(\s].*)" + prefix + ".*";
            case CommentType::CommentedCode: return BuildCommentedCode(req, prefix);
        }
        return "";
    }

    std::string Build(const SearchRequest& req) {
        switch (req.Type) {
            case SearchType::Word:        return BuildWord(req);
            case SearchType::Format:      return patterns::ForFormat(req.Format);
            case SearchType::Structure:   return BuildStructure(req);
            case SearchType::Combination: return BuildCombination(req);
            case SearchType::Quantity:    return BuildQuantity(req);
            case SearchType::Comment:     return BuildComment(req);
        }
        return "";
    }
}