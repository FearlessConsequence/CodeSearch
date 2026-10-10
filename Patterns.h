#pragma once
#include <string>
#include "Settings.h"

namespace patterns {

    enum class FormatType {
        Email = 1,
        Url = 2,
        Ip = 3,
        Guid = 4,
        Date = 5
    };

    inline std::string ForFormat(FormatType fmt) {
        switch (fmt) {
            case FormatType::Email:
                return R"([\w.+-]+@[\w-]+\.[\w.-]+)";
            case FormatType::Url:
                return R"(https?://[^\s"']+)";
            case FormatType::Ip:
                return R"((?<![\d.])\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}(?![\d.]))";
            case FormatType::Guid:
                return R"((?<![0-9a-fA-F-])[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}(?![0-9a-fA-F-]))";
            case FormatType::Date:
                return R"(\b(\d{4}-\d{1,2}-\d{1,2}|\d{1,2}\.\d{1,2}\.\d{4}|\d{4}/\d{1,2}/\d{1,2})\b)";
            default:
                return "";
        }
    }

}