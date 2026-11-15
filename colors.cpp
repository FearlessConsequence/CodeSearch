#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <set>

#ifdef _WIN32
#include <windows.h>
#endif

struct Color {
    int code;
    int r, g, b;
    std::string hex;
};

std::string ToHex(int r, int g, int b) {
    char buf[10];
    snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
    return std::string(buf);
}

// Функция, которая применяет алгоритм "Ближайший сосед" к любому списку
void ApplyTSP(std::vector<Color>& list) {
    if (list.empty()) return;
    std::vector<Color> sorted;
    std::vector<bool> visited(list.size(), false);
    
    // Находим самый темный цвет, чтобы начать с него (или черный)
    int current = 0; 
    double min_l = 999;
    for (size_t i = 0; i < list.size(); ++i) {
        double l = 0.299 * list[i].r + 0.587 * list[i].g + 0.114 * list[i].b;
        if (l < min_l) {
            min_l = l;
            current = i;
        }
    }

    sorted.push_back(list[current]);
    visited[current] = true;

    for (size_t step = 1; step < list.size(); ++step) {
        int next_color = -1;
        double min_dist = 999999.0;

        for (size_t i = 0; i < list.size(); ++i) {
            if (visited[i]) continue;
            
            double dr = list[current].r - list[i].r;
            double dg = list[current].g - list[i].g;
            double db = list[current].b - list[i].b;
            double dist = 2.0 * dr * dr + 4.0 * dg * dg + 3.0 * db * db; 

            if (dist < min_dist) {
                min_dist = dist;
                next_color = i;
            }
        }

        if (next_color != -1) {
            current = next_color;
            visited[current] = true;
            sorted.push_back(list[current]);
        }
    }
    list = sorted; // Заменяем исходный массив на отсортированный
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode))
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif

    std::vector<Color> colors;
    for (int i = 0; i < 256; ++i) {
        int r, g, b;
        if (i < 16) {
            int base[16][3] = {
                {0,0,0}, {128,0,0}, {0,128,0}, {128,128,0}, {0,0,128}, {128,0,128}, {0,128,128}, {192,192,192},
                {128,128,128}, {255,0,0}, {0,255,0}, {255,255,0}, {0,0,255}, {255,0,255}, {0,255,255}, {255,255,255}
            };
            r = base[i][0]; g = base[i][1]; b = base[i][2];
        } else if (i < 232) {
            int levels[6] = {0, 95, 135, 175, 215, 255};
            int idx = i - 16;
            r = levels[idx / 36];
            g = levels[(idx % 36) / 6];
            b = levels[idx % 6];
        } else {
            int v = 8 + (i - 232) * 10;
            r = v; g = v; b = v;
        }
        colors.push_back({i, r, g, b, ToHex(r, g, b)});
    }

    // Твой огромный список изгнанных
    std::set<int> excludedCodes = {
        138, 174, 210, 209, 173, 180, 144, 181, 104, 68, 67, 103, 97, 61, 60, 96,
        176, 140, 139, 175, 205, 169, 133, 132, 131, 95, 101, 65, 66, 137, 136, 100, 64, 58,
        130, 94, 29, 148, 184, 220, 226, 190, 154, 98, 62, 56, 92, 128, 163, 127, 91, 55, 54, 90, 126, 162,
        134, 164, 219, 207, 206,
        49, 48, 47, 50, 51,
        55, 56, 58, 60, 61, 62, 64, 65, 66, 67, 68, 90, 91, 92, 94, 95, 96, 97, 98, 100, 101, 103, 104, 
        126, 127, 128, 130, 131, 132, 133, 134, 136, 137, 138, 139, 140, 144, 148, 154, 162, 163, 164, 
        169, 173, 174, 175, 176, 180, 181, 184, 190, 205, 206, 207, 209, 210, 219, 220, 226, 208, 202, 204, 203
    };
    
    std::vector<Color> baseColors;
    std::vector<Color> gradColors;
    std::vector<Color> excludedColors;

    for (int i = 0; i < 256; ++i) {
        if (i < 16) {
            baseColors.push_back(colors[i]);
        } else if (excludedCodes.count(i)) {
            excludedColors.push_back(colors[i]);
        } else {
            gradColors.push_back(colors[i]);
        }
    }

    // 1. Строим идеальный градиент для оставшихся
    ApplyTSP(gradColors);

    // 2. Строим градиент для изгнанных цветов
    ApplyTSP(excludedColors);

    // 3. РУЧНАЯ КОРРЕКТИРОВКА (208 перед 202, 204 после 203)
    auto get_color = [&](int code) {
        return *std::find_if(colors.begin(), colors.end(), [code](const Color& c){ return c.code == code; });
    };
    
    auto remove_code = [&](int code) {
        excludedColors.erase(std::remove_if(excludedColors.begin(), excludedColors.end(), [code](const Color& c) { return c.code == code; }), excludedColors.end());
    };

    // Вынимаем эти 4 цвета из списка
    remove_code(208); remove_code(202); remove_code(204); remove_code(203);

    // Вставляем 208 перед 202
    auto it202 = std::find_if(excludedColors.begin(), excludedColors.end(), [](const Color& c){ return c.code == 202; });
    if (it202 != excludedColors.end()) {
        excludedColors.insert(it202, get_color(208));
    }

    // Вставляем 204 после 203
    auto it203 = std::find_if(excludedColors.begin(), excludedColors.end(), [](const Color& c){ return c.code == 203; });
    if (it203 != excludedColors.end()) {
        excludedColors.insert(it203 + 1, get_color(204));
    }

    // 4. ВЫВОД
    std::cout << "=== БАЗОВЫЕ 16 ЦВЕТОВ ===\n";
    std::cout << "=====================================================\n";
    for (const auto& c : baseColors) {
        double luminance = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;
        int fg_color = (luminance > 128) ? 0 : 15;
        std::cout << "\033[48;5;" << c.code << "m" << "\033[38;5;" << fg_color << "m"
                  << "  " << std::setw(3) << c.code << "  | HEX: " << c.hex << "  \033[0m\n";
    }

    std::cout << "\n=== ОСНОВНОЙ ГРАДИЕНТ ===\n";
    std::cout << "=====================================================\n";
    for (const auto& c : gradColors) {
        double luminance = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;
        int fg_color = (luminance > 128) ? 0 : 15;
        std::cout << "\033[48;5;" << c.code << "m" << "\033[38;5;" << fg_color << "m"
                  << "  " << std::setw(3) << c.code << "  | HEX: " << c.hex << "  \033[0m\n";
    }

    std::cout << "\n=== ИЗГНАННЫЕ ЦВЕТА (Выстроены в градиент) ===\n";
    std::cout << "=====================================================\n";
    for (const auto& c : excludedColors) {
        double luminance = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;
        int fg_color = (luminance > 128) ? 0 : 15;
        std::cout << "\033[48;5;" << c.code << "m" << "\033[38;5;" << fg_color << "m"
                  << "  " << std::setw(3) << c.code << "  | HEX: " << c.hex << "  \033[0m\n";
    }

    return 0;
}