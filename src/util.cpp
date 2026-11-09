#include "util.h"

#include <regex>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstddef>

/** 使用正则从 url 中分离 protocol、ip、port、sid、core（port 可省略，省略时 port=0 由服务分配） */
StreamUrl parseStreamUrl(const std::string& url) {
    StreamUrl out;
    // 匹配: tcp://127.0.0.1 或 tcp://127.0.0.1:12345 或 tcp://127.0.0.1:12345?sid=54321&core=0
    std::regex re(R"((tcp|udp)://([^:/]+)(?::(\d+))?(?:\?(.*))?)", std::regex::icase);
    std::smatch m;
    if (!std::regex_match(url, m, re)) {
        return out;
    }
    out.protocol = m[1].str();
    out.ip = m[2].str();
    out.port = (m[3].matched && !m[3].str().empty()) ? std::stoi(m[3].str()) : 0;
    out.ok = true;

    if (m[4].matched && !m[4].str().empty()) {
        std::string query = m[4].str();
        // 解析 query: sid=54321&core=0
        std::regex kv(R"(([^=&]+)=([^&]*))");
        for (auto it = std::sregex_iterator(query.begin(), query.end(), kv);
             it != std::sregex_iterator(); ++it) {
            std::string key = (*it)[1].str();
            std::string val = (*it)[2].str();
            if (key == "sid") out.sid = val;
            else if (key == "core") out.core = val;
        }
    }
    return out;
}
