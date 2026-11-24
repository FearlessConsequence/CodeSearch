#pragma once
#include <string>
#include <map>
#include <iostream>

namespace theme {

    // 1. Роли
    enum class Role {
        Title,       // Заголовки === ... ===
        Question,    // Вопросы меню (Что искать?)
        MenuNum,     // Цифры пунктов (1., 2.)
        MenuItem,    // Текст пунктов (Начать поиск) — ДОБАВЛЕНО
        Match,       // САМОЕ ВАЖНОЕ: совпадение (if (true))
        FileName,    // Имя файла и время
        LineNumber,  // Номер строки (57)
        StatValue,   // Цифры в итогах (Найдено: 1)
        Dim,         // Второстепенное (скобки, двоеточия)
        Error,       // Ошибки — ДОБАВЛЕНО
        Default,     // Обычный текст (белый)
        Warning,     // Предупреждения — ДОБАВЛЕНО
        Reset        // Сброс цвета
    };

    enum class ThemeType { 
        Default, 
        Matrix, 
        HighContrast 
    };

    // 2. Глобальный словарь текущей темы
    inline std::map<Role, std::string> current_theme;

    // 3. Парсер строк из JSON
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

    // 4. Установка дефолтной темы
    inline void SetDefaultTheme() {
        current_theme[Role::Reset]      = "\033[0m";
        current_theme[Role::Title]      = "\033[1;36m"; // Жирный голубой
        current_theme[Role::Question]   = "\033[36m";   // Голубой
        current_theme[Role::MenuNum]    = "\033[33m";   // Желтый
        current_theme[Role::MenuItem]   = "\033[32m";   // Зеленый
        current_theme[Role::Match]      = "\033[35m";   // Розовый
        current_theme[Role::FileName]   = "\033[34m";   // Синий
        current_theme[Role::LineNumber] = "\033[31m";   // Красный
        current_theme[Role::StatValue]  = "\033[36m";   // Голубой
        current_theme[Role::Dim]        = "\033[2m";    // Тусклый
        current_theme[Role::Error]      = "\033[31m";   // Красный
        current_theme[Role::Warning]    = "\033[33m";   // Желтый
        current_theme[Role::Default]    = "";
    }

    inline void SetNewTheme() {
        current_theme.clear(); // Очищаем на случай, если SetDefaultTheme уже вызывался

        // 1. Базовые элементы интерфейса
        current_theme[Role::Title]      = "\033[1;33m"; // Жирный желтый (для === ЗАГОЛОВКИ ===)
        current_theme[Role::MenuNum]    = "\033[35m";   // Маджента/Розовый (для цифр 1., 2., 0.)
        current_theme[Role::Question]   = "\033[36m";   // Бирюзовый (для "Что искать? >")
        
        // 2. Контент
        current_theme[Role::Match]      = "\033[32m";   // Ярко-зеленый (для найденных совпадений)
        current_theme[Role::FileName]   = "\033[31m";   // Красный (для имен файлов)
        current_theme[Role::LineNumber] = "\033[34m";   // Синий (для номеров строк)

        // 3. Статусы (используем ваши роли)
        current_theme[Role::Error]      = "\033[1;31m"; // Жирный красный
        current_theme[Role::Warning]    = "\033[33m";   // Желтый

        // 4. Технические
        current_theme[Role::Reset]      = "\033[0m";
        current_theme[Role::Default]    = ""; // Пустая строка или \033[37m (белый)
    }


    // Тестовая тема (Кислотная/Матрица), чтобы проверить работу системы
    inline void SetTestTheme() {
        current_theme[Role::Reset]      = "\033[0m";
        current_theme[Role::Title]      = "\033[1;32m"; // Жирный зелёный
        current_theme[Role::Question]   = "\033[32m";   // Зелёный
        current_theme[Role::MenuNum]    = "\033[35m";   // Розовый
        current_theme[Role::MenuItem]   = "\033[37m";   // Белый
        current_theme[Role::Match]      = "\033[1;33m"; // Жирный жёлтый
        current_theme[Role::FileName]   = "\033[36m";   // Голубой
        current_theme[Role::LineNumber] = "\033[31m";   // Красный
        current_theme[Role::StatValue]  = "\033[1;32m"; // Жирный зелёный
        current_theme[Role::Dim]        = "\033[2m";    // Тусклый
        current_theme[Role::Error]      = "\033[31m";   // Красный
        current_theme[Role::Warning]    = "\033[33m";   // Жёлтый
        current_theme[Role::Default]    = "";           // Белый (сброс)
    }

    // 5. Получение цвета
    inline const std::string& GetColor(Role role) {
        static const std::string empty = "";
        if (role == Role::Reset) return current_theme[Role::Reset];
        auto it = current_theme.find(role);
        return (it != current_theme.end()) ? it->second : empty;
    }
}

// 6. Перегрузка оператора << 
inline std::ostream& operator<<(std::ostream& os, theme::Role role) {
    os << theme::GetColor(role);
    return os;
}