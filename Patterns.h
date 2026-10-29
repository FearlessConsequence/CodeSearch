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
                return R"((?:^|[^.\d])((?:(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)\.){3}(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d))(?!\.\d))";
            case FormatType::Guid:
                return R"((?:^|[^\w-])([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})(?![\w-]))";
            case FormatType::Date:
                return R"((?:^|[^\d])((?:\d{4}-(?:0[1-9]|1[0-2])-(?:0[1-9]|[12]\d|3[01])|(?:0[1-9]|[12]\d|3[01])\.(?:0[1-9]|1[0-2])\.\d{4}|\d{4}/(?:0[1-9]|1[0-2])/(?:0[1-9]|[12]\d|3[01])))(?!\d))";
            default:
                return "";
        }
    }
}