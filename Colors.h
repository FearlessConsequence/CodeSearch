#pragma once
#include <string>

namespace colors {

    // Базовые цвета
    const std::string Green    = "\033[32m";
    const std::string Yellow   = "\033[33m";
    const std::string Cyan     = "\033[96m";
    const std::string Magenta  = "\033[35m";
    const std::string Burgundy = "\033[38;5;88m";
    const std::string Red      = "\033[31m";
    const std::string Bold     = "\033[1m";
    const std::string Dim      = "\033[2m";
    const std::string Reset    = "\033[0m";

    // Назначение
    const std::string File       = Yellow;
    const std::string LineNumber = Green;
    const std::string MatchText  = Magenta;
    const std::string Totals     = Cyan;
    const std::string MenuHeader = "";      // без цвета
    const std::string MenuNumber = Burgundy;
    const std::string Warning    = Yellow;
    const std::string Error      = Red;
}