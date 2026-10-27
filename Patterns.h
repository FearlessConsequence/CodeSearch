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
                return R"(\b\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\b)";
            case FormatType::Guid:
                return R"(\b[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\b)";
            case FormatType::Date:
                return R"(\b(\d{4}-\d{1,2}-\d{1,2}|\d{1,2}\.\d{1,2}\.\d{4}|\d{4}/\d{1,2}/\d{1,2})\b)";
            default:
                return "";
        }
    }

}