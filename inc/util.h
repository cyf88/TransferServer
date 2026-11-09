#ifndef UTIL_H
#define UTIL_H


#include <string>


/** 流 URL 解析结果，如 tcp://127.0.0.1:12345?sid=54321&core=0 */
struct StreamUrl {
    std::string protocol;  // "tcp" | "udp"
    std::string ip;
    int port = 0;
    std::string sid;
    std::string core;     // 可选，未指定时为空
    bool ok = false;
};

/** 使用正则从 url 中分离 protocol、ip、port、sid、core */
StreamUrl parseStreamUrl(const std::string& url);





#endif // UTIL_H