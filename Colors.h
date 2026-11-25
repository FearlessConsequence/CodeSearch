#pragma once
#include <string>
#include <map>
#include <iostream>

namespace theme {

    enum class Role {
        Title, Question, MenuNum, MenuItem, Match, FileName, LineNumber, StatValue, Dim, Error, Warning, Default, Reset
    };

    inline std::map<Role, std::string> current_theme;
    inline std::map<std::string, std::map<Role, std::string>> AllThemes;

    // Парсер (оставляем как был)
    inline std::string ParseColorString(const std::string& colorName) {
        if (colorName.empty()) return "";
        std::string ansi = "\033[";
        bool first = true;
        size_t start = 0, end;
        while ((end = colorName.find(' ', start)) != std::string::npos) {
            std::string token = colorName.substr(start, end - start);
            if (token == "bold") ansi += first ? "1" : ";1";
            else if (token == "dim") ansi += first ? "2" : ";2";
            else if (token == "red") ansi += first ? "31" : ";31";
            else if (token == "green") ansi += first ? "32" : ";32";
            else if (token == "yellow") ansi += first ? "33" : ";33";
            else if (token == "blue") ansi += first ? "34" : ";34";
            else if (token == "magenta") ansi += first ? "35" : ";35";
            else if (token == "cyan") ansi += first ? "36" : ";36";
            else if (token == "white") ansi += first ? "37" : ";37";
            first = false;
            start = end + 1;
        }
        std::string token = colorName.substr(start);
        if (token == "bold") ansi += first ? "1" : ";1";
        else if (token == "dim") ansi += first ? "2" : ";2";
        else if (token == "red") ansi += first ? "31" : ";31";
        else if (token == "green") ansi += first ? "32" : ";32";
        else if (token == "yellow") ansi += first ? "33" : ";33";
        else if (token == "blue") ansi += first ? "34" : ";34";
        else if (token == "magenta") ansi += first ? "35" : ";35";
        else if (token == "cyan") ansi += first ? "36" : ";36";
        else if (token == "white") ansi += first ? "37" : ";37";
        return ansi + "m";
    }

    // Заполнение всех тем
    inline void InitThemes() {
        AllThemes.clear();
        
        // 1. Default (наша тестовая)
        AllThemes["default"] = {
            {Role::Title, "\033[1;36m"}, {Role::Question, "\033[36m"}, {Role::MenuNum, "\033[33m"},
            {Role::MenuItem, "\033[32m"}, {Role::Match, "\033[35m"}, {Role::FileName, "\033[34m"},
            {Role::LineNumber, "\033[31m"}, {Role::StatValue, "\033[36m"}, {Role::Dim, "\033[2m"},
            {Role::Error, "\033[31m"}, {Role::Warning, "\033[33m"}, {Role::Default, ""}, {Role::Reset, "\033[0m"}
        };

        // 2. GraphitePhlegmatic (afd7d7, b2b2b2, ff875f, 87afaf, dadada)
        AllThemes["GraphitePhlegmatic"] = {
            {Role::Title, "\033[38;2;175;215;215m"}, {Role::Question, "\033[38;2;178;178;178m"},
            {Role::MenuNum, "\033[38;2;255;135;95m"}, {Role::MenuItem, "\033[38;2;135;175;175m"},
            {Role::Match, "\033[38;2;218;218;218m"}, {Role::FileName, "\033[38;2;135;175;175m"},
            {Role::LineNumber, "\033[38;2;255;135;95m"}, {Role::StatValue, "\033[38;2;175;215;215m"},
            {Role::Dim, "\033[2m"}, {Role::Error, "\033[38;2;255;135;95m"}, {Role::Warning, "\033[38;2;178;178;178m"},
            {Role::Default, ""}, {Role::Reset, "\033[0m"}
        };

        // 3. PredatorBlood (ffaf5f, 005f5f, 008787, 870000, 585858, af005f)
        AllThemes["PredatorBlood"] = {
            {Role::Title, "\033[38;2;0;135;135m"},         // Теперь циановый (был оранжевый)
            {Role::Question, "\033[38;2;0;95;95m"},
            {Role::MenuNum, "\033[38;2;255;175;95m"},    // Теперь оранжевый (был циановым)
            {Role::MenuItem, "\033[38;2;255;175;95m"},   // Оранжевый
            {Role::Match, "\033[38;2;88;88;88m"}, 
            {Role::FileName, "\033[38;2;175;0;95m"},
            {Role::LineNumber, "\033[38;2;135;0;0m"}, 
            {Role::StatValue, "\033[38;2;255;175;95m"},
            {Role::Dim, "\033[2m"}, 
            {Role::Error, "\033[38;2;135;0;0m"}, 
            {Role::Warning, "\033[38;2;255;175;95m"},
            {Role::Default, ""}, 
            {Role::Reset, "\033[0m"}
        };

        // 4. SwampFlower (8787d7, 005f5f, 5f8787, 00875f, 5f8700, 5fafff)
        AllThemes["SwampFlower"] = {
            {Role::Title, "\033[38;2;135;135;215m"}, {Role::Question, "\033[38;2;0;95;95m"},
            {Role::MenuNum, "\033[38;2;95;135;135m"}, {Role::MenuItem, "\033[38;2;0;135;95m"},
            {Role::Match, "\033[38;2;95;135;0m"}, {Role::FileName, "\033[38;2;95;175;255m"},
            {Role::LineNumber, "\033[38;2;0;135;95m"}, {Role::StatValue, "\033[38;2;135;135;215m"},
            {Role::Dim, "\033[2m"}, {Role::Error, "\033[38;2;0;135;95m"}, {Role::Warning, "\033[38;2;95;135;0m"},
            {Role::Default, ""}, {Role::Reset, "\033[0m"}
        };

        // 5. InfectedFlowers (af5faf, afd7d7, bcbcbc, 8787d7, 87afaf, afd7af)
        AllThemes["InfectedFlowers"] = {
            {Role::Title, "\033[38;2;135;175;175m"},       // Цвет от старого Match
            {Role::Question, "\033[38;2;175;215;215m"},
            {Role::MenuNum, "\033[38;2;188;188;188m"},
            {Role::MenuItem, "\033[38;2;135;135;215m"},
            {Role::Match, "\033[38;2;175;95;175m"},        // Цвет от старых Title/StatValue
            {Role::FileName, "\033[38;2;175;215;175m"},
            {Role::LineNumber, "\033[38;2;188;188;188m"}, 
            {Role::StatValue, "\033[38;2;135;175;175m"},   // Цвет от старого Match
            {Role::Dim, "\033[2m"}, 
            {Role::Error, "\033[38;2;175;95;175m"}, 
            {Role::Warning, "\033[38;2;135;135;215m"},
            {Role::Default, ""}, 
            {Role::Reset, "\033[0m"}
        };

        // 6. WitchVial (af5faf, 5f8787, 5f87af, 8787af, af87af, d7af87)
        AllThemes["WitchVial"] = {
            {Role::Title, "\033[38;2;175;95;175m"}, {Role::Question, "\033[38;2;95;135;135m"},
            {Role::MenuNum, "\033[38;2;95;135;175m"}, {Role::MenuItem, "\033[38;2;135;135;175m"},
            {Role::Match, "\033[38;2;175;135;175m"}, {Role::FileName, "\033[38;2;215;175;135m"},
            {Role::LineNumber, "\033[38;2;95;135;175m"}, {Role::StatValue, "\033[38;2;175;95;175m"},
            {Role::Dim, "\033[2m"}, {Role::Error, "\033[38;2;175;95;175m"}, {Role::Warning, "\033[38;2;95;135;135m"},
            {Role::Default, ""}, {Role::Reset, "\033[0m"}
        };

        // 7. OldHag (af5faf, 87875f, eeeeee, 8787af, d7af87, 5f8787)
        AllThemes["OldHag"] = {
            {Role::Title, "\033[38;2;175;95;175m"}, {Role::Question, "\033[38;2;135;135;95m"},
            {Role::MenuNum, "\033[38;2;238;238;238m"}, {Role::MenuItem, "\033[38;2;135;135;175m"},
            {Role::Match, "\033[38;2;215;175;135m"}, {Role::FileName, "\033[38;2;95;135;135m"},
            {Role::LineNumber, "\033[38;2;238;238;238m"}, {Role::StatValue, "\033[38;2;175;95;175m"},
            {Role::Dim, "\033[2m"}, {Role::Error, "\033[38;2;175;95;175m"}, {Role::Warning, "\033[38;2;135;135;95m"},
            {Role::Default, ""}, {Role::Reset, "\033[0m"}
        };

        // 8. DarkSea (af5faf, 005f87, 0087af, 87ffff, 5fafff, eeeeee)
        AllThemes["DarkSea"] = {
            {Role::Title, "\033[38;2;0;135;175m"},       // Синий (был циан)
            {Role::Question, "\033[38;2;0;95;135m"},     // Тёмно-синий
            {Role::MenuNum, "\033[38;2;175;95;175m"},    // Розовый (цифры)
            {Role::MenuItem, "\033[38;2;175;95;175m"},   // Розовый (текст)
            {Role::Match, "\033[38;2;0;255;255m"},       // Яркий циан (был синий)
            {Role::FileName, "\033[38;2;238;238;238m"},  // Светло-серый
            {Role::LineNumber, "\033[38;2;0;135;175m"},  // Синий
            {Role::StatValue, "\033[38;2;0;255;255m"},   // Циан
            {Role::Dim, "\033[2m"},
            {Role::Error, "\033[38;2;175;95;175m"},      // Розовый
            {Role::Warning, "\033[38;2;0;95;135m"},      // Тёмно-синий
            {Role::Default, ""},
            {Role::Reset, "\033[0m"}
        };

        // 9. DustyPastels (87afd7, ffd7ff, ffffd7, ffd7af)
        AllThemes["DustyPastels"] = {
            {Role::Title, "\033[38;2;135;175;215m"}, {Role::Question, "\033[38;2;255;215;255m"},
            {Role::MenuNum, "\033[38;2;255;255;215m"}, {Role::MenuItem, "\033[38;2;255;215;175m"},
            {Role::Match, "\033[38;2;255;215;255m"}, {Role::FileName, "\033[38;2;135;175;215m"},
            {Role::LineNumber, "\033[38;2;255;255;215m"}, {Role::StatValue, "\033[38;2;135;175;215m"},
            {Role::Dim, "\033[2m"}, {Role::Error, "\033[38;2;255;215;175m"}, {Role::Warning, "\033[38;2;255;255;215m"},
            {Role::Default, ""}, {Role::Reset, "\033[0m"}
        };

        // 10. CaramelPastels (87d787, afd7d7, ffffd7, ffd75f, 5f87d7)
        AllThemes["CaramelPastels"] = {
            {Role::Title, "\033[38;2;135;215;135m"}, {Role::Question, "\033[38;2;175;215;215m"},
            {Role::MenuNum, "\033[38;2;255;255;215m"}, {Role::MenuItem, "\033[38;2;255;215;95m"},
            {Role::Match, "\033[38;2;95;135;215m"}, {Role::FileName, "\033[38;2;175;215;215m"},
            {Role::LineNumber, "\033[38;2;255;215;95m"}, {Role::StatValue, "\033[38;2;135;215;135m"},
            {Role::Dim, "\033[2m"}, {Role::Error, "\033[38;2;255;215;95m"}, {Role::Warning, "\033[38;2;255;255;215m"},
            {Role::Default, ""}, {Role::Reset, "\033[0m"}
        };

        // 11. ForestCamo (005f87, 005f5f, 8787af, d7af87, 87875f)
        AllThemes["ForestCamo"] = {
            {Role::Title, "\033[38;2;0;95;135m"}, 
            {Role::Question, "\033[38;2;0;95;95m"},
            {Role::MenuNum, "\033[38;2;135;135;175m"}, 
            {Role::MenuItem, "\033[38;2;215;175;135m"},
            {Role::Match, "\033[38;2;135;135;95m"}, 
            {Role::FileName, "\033[38;2;0;95;135m"},
            {Role::LineNumber, "\033[38;2;135;135;175m"}, // Вернули прежний цвет
            {Role::StatValue, "\033[38;2;135;135;95m"},   // Взяли цвет у Match
            {Role::Dim, "\033[2m"}, 
            {Role::Error, "\033[38;2;215;175;135m"}, 
            {Role::Warning, "\033[38;2;135;135;95m"},
            {Role::Default, ""}, 
            {Role::Reset, "\033[0m"}
        };

        // 12. NeonDawn (af5faf, 87afaf, 00d7af, ff875f, ffd787)
        AllThemes["NeonDawn"] = {
            {Role::Title, "\033[38;2;175;95;175m"}, 
            {Role::Question, "\033[38;2;135;175;175m"},
            {Role::MenuNum, "\033[38;2;0;215;175m"}, 
            {Role::MenuItem, "\033[38;2;255;135;95m"},
            {Role::Match, "\033[38;2;255;215;135m"}, 
            {Role::FileName, "\033[38;2;0;215;175m"},
            {Role::LineNumber, "\033[38;2;255;135;95m"}, 
            {Role::StatValue, "\033[38;2;175;95;175m"},
            {Role::Dim, "\033[2m"}, 
            {Role::Error, "\033[38;2;255;135;95m"}, 
            {Role::Warning, "\033[38;2;255;215;135m"},
            {Role::Default, ""}, 
            {Role::Reset, "\033[0m"}
        };

        // 13. LilacHaze (585858, af005f)
        AllThemes["LilacHaze"] = {
            {Role::Title, "\033[38;2;88;88;88m"}, {Role::Question, "\033[38;2;175;0;95m"},
            {Role::MenuNum, "\033[38;2;88;88;88m"}, {Role::MenuItem, "\033[38;2;175;0;95m"},
            {Role::Match, "\033[38;2;175;0;95m"}, {Role::FileName, "\033[38;2;88;88;88m"},
            {Role::LineNumber, "\033[38;2;175;0;95m"}, {Role::StatValue, "\033[38;2;88;88;88m"},
            {Role::Dim, "\033[2m"}, {Role::Error, "\033[38;2;175;0;95m"}, {Role::Warning, "\033[38;2;88;88;88m"},
            {Role::Default, ""}, {Role::Reset, "\033[0m"}
        };

        // 14. DarkAristocracy (5f5f00, 875f00, af5f00, d7875f)
        AllThemes["DarkAristocracy"] = {
            {Role::Title, "\033[38;2;95;95;0m"}, {Role::Question, "\033[38;2;135;95;0m"},
            {Role::MenuNum, "\033[38;2;175;95;0m"}, {Role::MenuItem, "\033[38;2;215;135;95m"},
            {Role::Match, "\033[38;2;175;95;0m"}, {Role::FileName, "\033[38;2;135;95;0m"},
            {Role::LineNumber, "\033[38;2;215;135;95m"}, {Role::StatValue, "\033[38;2;95;95;0m"},
            {Role::Dim, "\033[2m"}, {Role::Error, "\033[38;2;175;95;0m"}, {Role::Warning, "\033[38;2;215;135;95m"},
            {Role::Default, ""}, {Role::Reset, "\033[0m"}
        };

        // 15. EmeraldDew (00875f, 5faf5f, 00afff, afd7d7, eeeeee)
        AllThemes["EmeraldDew"] = {
            {Role::Title, "\033[38;2;0;135;95m"}, 
            {Role::Question, "\033[38;2;95;175;95m"},
            {Role::MenuNum, "\033[38;2;0;175;255m"}, 
            {Role::MenuItem, "\033[38;2;175;215;215m"},
            {Role::Match, "\033[38;2;238;238;238m"}, 
            {Role::FileName, "\033[38;2;0;175;255m"},
            {Role::LineNumber, "\033[38;2;95;175;95m"}, 
            {Role::StatValue, "\033[38;2;0;135;95m"},
            {Role::Dim, "\033[2m"}, 
            {Role::Error, "\033[38;2;0;135;95m"}, 
            {Role::Warning, "\033[38;2;95;175;95m"},
            {Role::Default, ""}, 
            {Role::Reset, "\033[0m"}
        };
    }

    // Применение темы по имени
    inline void ApplyTheme(const std::string& themeName) {
        current_theme.clear();
        auto it = AllThemes.find(themeName);
        if (it != AllThemes.end()) {
            current_theme = it->second;
        } else {
            current_theme = AllThemes["default"]; // Если не нашли, ставим дефолт
        }
    }

    inline const std::string& GetColor(Role role) {
        static const std::string empty = "";
        if (role == Role::Reset) return current_theme[Role::Reset];
        auto it = current_theme.find(role);
        return (it != current_theme.end()) ? it->second : empty;
    }
}

inline std::ostream& operator<<(std::ostream& os, theme::Role role) {
    os << theme::GetColor(role);
    return os;
}