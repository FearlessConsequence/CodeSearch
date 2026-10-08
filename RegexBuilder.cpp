#include "RegexBuilder.h"
#include <regex>
#include <sstream>

namespace regex_builder {

    const std::string PERCENT_PLACEHOLDER = "\x01PERCENT\x01";
    const std::string STAR_PLACEHOLDER = "\x01STAR\x01";

    std::string Escape(const std::string& s) {
        static const std::string special = R"(\.^$|()[]{}*+?)";
        std::string result;
        for (char c : s) {
            if (special.find(c) != std::string::npos) {
                result += '\\';
            }
            result += c;
        }
        return result;
    }

        std::string EscapeXyz(const std::string& s) {
        // Если чистая закрывающая скобка — искать в начале строки
        std::string trimmed = s;
        while (!trimmed.empty() && std::isspace((unsigned char)trimmed.front()))
            trimmed.erase(trimmed.begin());
        while (!trimmed.empty() && std::isspace((unsigned char)trimmed.back()))
            trimmed.pop_back();

        if (trimmed == "}") {
            return R"(^\s*\})";
        }

        // Заменить % / * / ^ на плейсхолдеры
        std::string temp = s;
        size_t pos;
        while ((pos = temp.find('%')) != std::string::npos)
            temp.replace(pos, 1, PERCENT_PLACEHOLDER);
        while ((pos = temp.find('*')) != std::string::npos)
            temp.replace(pos, 1, STAR_PLACEHOLDER);
        while ((pos = temp.find('^')) != std::string::npos)
            temp.replace(pos, 1, STAR_PLACEHOLDER);

        // Экранировать
        std::string escaped = Escape(temp);

        // Вернуть плейсхолдеры
        while ((pos = escaped.find(PERCENT_PLACEHOLDER)) != std::string::npos)
            escaped.replace(pos, PERCENT_PLACEHOLDER.length(), R"(\s*)");
        while ((pos = escaped.find(STAR_PLACEHOLDER)) != std::string::npos)
            escaped.replace(pos, STAR_PLACEHOLDER.length(), R"([\s\S]*?)");

        return escaped;
    }

    std::string WrapWord(const std::string& s) {
        std::string escaped = Escape(s);
        if (!s.empty() && std::isalpha((unsigned char)s[0])) {
            return R"(\b)" + escaped + R"(\b)";
        }
        return escaped;
    }

    static std::string BuildWord(const SearchRequest& req) {
        std::string word = Escape(req.Word);
        if (req.WordWholeOnly)
            return R"(\b)" + word + R"(\b)";
        return word;
    }

    static std::string BuildEmpty(const std::string& x) {
        // Проверка на "catch" без учёта регистра
        std::string lower = x;
        for (auto& c : lower) c = std::tolower((unsigned char)c);
        if (lower == "catch")
            return R"(catch\s*(\([^)]*\))?\s*\{\s*\})";
        return EscapeXyz(x) + R"(\s*\{\s*\})";
    }

    static std::string BuildStructure(const SearchRequest& req) {
        switch (req.Structure) {
            case StructureType::XEmpty:
                return BuildEmpty(req.X);
            case StructureType::XAnywhereBeforeYBeforeZ:
                return EscapeXyz(req.X) + "[\\s\\S]*?" +
                       EscapeXyz(req.Y) + "[\\s\\S]*?" +
                       EscapeXyz(req.Z);
        }
        return "";
    }

    static std::string BuildCombination(const SearchRequest& req) {
        switch (req.Combination) {
            case CombinationType::XButNotY:
                return "(?!.*" + Escape(req.Exclude) + ").*" + Escape(req.X);

            case CombinationType::XOrY: {
                std::stringstream ss(req.X);
                std::string item;
                std::string joined;
                bool first = true;
                while (std::getline(ss, item, ',')) {
                    // обрезать пробелы
                    while (!item.empty() && std::isspace((unsigned char)item.front()))
                        item.erase(item.begin());
                    while (!item.empty() && std::isspace((unsigned char)item.back()))
                        item.pop_back();
                    if (item.empty()) continue;
                    if (!first) joined += "|";
                    joined += WrapWord(item);
                    first = false;
                }
                return R"(\b()" + joined + R"()\w*)";
            }
        }
        return "";
    }

    static std::string BuildNumbers(const SearchRequest& req) {
        switch (req.NumberCompareValue) {
            case NumberCompare::Equal:
                return R"(\b\d{)" + std::to_string(req.MinCount) + R"(}\b)";

            case NumberCompare::Greater:
                return R"(\b\d{)" + std::to_string(req.MinCount + 1) + R"(,}\b)";

            case NumberCompare::Less: {
                int maxN = std::max(1, req.MinCount - 1);
                return R"(\b\d{1,)" + std::to_string(maxN) + R"(}\b)";
            }

            case NumberCompare::Between:
                return R"(\b\d{)" + std::to_string(req.MinCount) + "," +
                       std::to_string(req.MaxCount) + R"(}\b)";
        }
        return "";
    }

    static std::string BuildQuantity(const SearchRequest& req) {
        switch (req.Quantity) {
            case QuantityType::Numbers:
                return BuildNumbers(req);

            case QuantityType::LineLength:
                return "^{." + std::to_string(req.MinCount + 1) + ",}$";

            case QuantityType::ArgsLength: {
                int commas = std::max(0, req.MinCount - 1);
                return R"(\w+\s+\w+\s+\w+\s*\([^)]*\w+\s+\w+(\s*,\s*\w+\s+\w+){)" +
                       std::to_string(commas) + ",}";
            }
        }
        return "";
    }

    static std::string BuildTodo(const SearchRequest& req, const std::string& prefix) {
        if (!req.CommentWord.empty())
            return prefix + R"(\s*)" + Escape(req.CommentWord) + ".*";
        return prefix + R"(\s*(TODO|FIXME|HACK).*)";
    }

    static std::string BuildCommentedCode(const SearchRequest& req, const std::string& prefix) {
        if (!req.CommentedCodeWords.empty()) {
            std::string lookaheads;
            for (const auto& w : req.CommentedCodeWords) {
                lookaheads += "(?=.*" + WrapWord(w) + ")";
            }
            return R"(^\s*)" + prefix + R"(\s*)" + lookaheads + ".*";
        }
        std::string signs = patterns::CodeSigns(req.Language);
        return R"(^\s*)" + prefix + R"(\s*.*)" + signs;
    }

    static std::string BuildComment(const SearchRequest& req) {
        std::string prefix = patterns::CommentPrefix(req.Language);
        std::string firstChar = patterns::CommentPrefixFirstChar(req.Language);

        switch (req.Comment) {
            case CommentType::Todo:
                return BuildTodo(req, prefix);

            case CommentType::StartLine:
                return R"(^\s*)" + prefix + ".*";

            case CommentType::EndLine:
                return R"(^\s*[^)" + firstChar + R"(\s].*)" + prefix + ".*";

            case CommentType::CommentedCode:
                return BuildCommentedCode(req, prefix);

            case CommentType::All:
                return prefix + ".*";
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