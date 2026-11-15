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
    std::string EscapeXyzWithBounds(const std::string& s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        std::string trimmed = s.substr(a, b - a + 1);

        std::string esc = EscapeXyz(trimmed);

        // B2: если % на границе, а следующий/предыдущий значимый символ — словесный,
        // вставляем \b после/перед всеми \s* от ведущих/хвостовых %, чтобы граница
        // слова сохранялась даже при пустом матче \s*.
        if (trimmed.front() == '%') {
            size_t k = 0;
            while (k < trimmed.size() && trimmed[k] == '%') ++k;
            if (k < trimmed.size() &&
                (std::isalnum((unsigned char)trimmed[k]) || trimmed[k] == '_'))
                esc.insert(k * 3, R"(\b)");  // "\\s*" = 3 символа
        }
        if (trimmed.back() == '%') {
            size_t k = 0;
            while (k < trimmed.size() && trimmed[trimmed.size() - 1 - k] == '%') ++k;
            if (k < trimmed.size()) {
                char prev = trimmed[trimmed.size() - 1 - k];
                if (std::isalnum((unsigned char)prev) || prev == '_')
                    esc.insert(esc.size() - k * 3, R"(\b)");
            }
        }

        bool leftBound  = std::isalnum((unsigned char)trimmed.front()) || trimmed.front() == '_';
        bool rightBound = std::isalnum((unsigned char)trimmed.back())  || trimmed.back()  == '_';
        if (leftBound)  esc = R"(\b)" + esc;
        if (rightBound) esc = esc + R"(\b)";
        return esc;
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
        if (!req.WordWholeOnly) return word;

        bool leftBound  = !req.Word.empty() &&
                        (std::isalnum((unsigned char)req.Word.front()) || req.Word.front() == '_');
        bool rightBound = !req.Word.empty() &&
                        (std::isalnum((unsigned char)req.Word.back())  || req.Word.back()  == '_');

        if (leftBound)  word = R"(\b)" + word;
        if (rightBound) word = word + R"(\b)";
        return word;
    }

    static std::string BuildEmpty(const SearchRequest& req) {
        std::string metric;
        switch (req.EmptyMetricValue) {
            case EmptyMetric::StrictlyEmpty: metric = "строго пусто"; break;
            case EmptyMetric::MaxChars:
                metric = "≤ " + std::to_string(req.EmptyMaxCount) + " непроб. символов"; break;
            case EmptyMetric::MaxLines:
                metric = "≤ " + std::to_string(req.EmptyMaxCount) + " строк"; break;
        }
        return "[empty-block: " + req.X + ", " + metric + "]";
    }

    static std::string BuildStructure(const SearchRequest& req) {
        switch (req.Structure) {
            case StructureType::XEmpty:
                return BuildEmpty(req);
            case StructureType::XAnywhereBeforeYBeforeZ: {
                std::string x = EscapeXyzWithBounds(req.X);
                std::string y = EscapeXyzWithBounds(req.Y);
                std::string z = EscapeXyzWithBounds(req.Z);
                if (x.empty() || y.empty() || z.empty()) return "";
                return x + "[\\s\\S]*?(" + y + ")[\\s\\S]*?" + z;
            }
        }
        return "";
    }

    static std::string BuildCombination(const SearchRequest& req) {
        switch (req.Combination) {
            case CombinationType::XButNotY: {
                std::string mode;
                switch (req.XButNotYModeValue) {
                    case XButNotYMode::ByPosition: mode = "по позиции"; break;
                    case XButNotYMode::ByLine:     mode = "по строке"; break;
                }
                return "[X но не Y (" + mode + "): " + req.X + " / " + req.Y + "]";
            }
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
            case NumberCompare::Less:
                if (req.MinCount <= 1) return R"((?!))";
                return R"(\b\d{1,)" + std::to_string(req.MinCount - 1) + R"(}\b)";
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
                    case NumberCompare::Less:
                        if (req.MinCount <= 1) return R"((?!))";
                        return "^.{1," + std::to_string(req.MinCount - 1) + "}$";
                    case NumberCompare::Between: return "^.{" + std::to_string(req.MinCount) + "," + std::to_string(req.MaxCount) + "}$";
                }
                return "";
            case QuantityType::ArgsLength: return "[args-length]";
        }
        return "";
    }

    static std::string BuildTodo(const SearchRequest& req, const std::string& prefix) {
        if (!req.CommentWord.empty()) return prefix + R"(\s*)" + Escape(req.CommentWord) + ".*";
        return prefix + R"(\s*([Tt][Oo][Dd][Oo]|[Ff][Ii][Xx][Mm][Ee]|[Hh][Aa][Cc][Kk]).*)";
    }

    static std::string BuildComment(const SearchRequest& req) {
        std::string mode;
        switch (req.Comment) {
            case CommentType::StartLine:     mode = "в начале строки"; break;
            case CommentType::EndLine:       mode = "в конце строки";  break;
            case CommentType::CommentedCode: mode = "все комментарии"; break;
        }
        std::string word = req.CommentWord.empty() ? "(все)" : req.CommentWord;
        return "[comments: " + mode + ", язык: " + req.Language + ", слово: " + word + "]";
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