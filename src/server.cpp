/**
 * TransferServer HTTP 服务
 *
 * 通过 HTTP 接口下发转发配置，控制解密 / 转码流水线。
 *
 * 接口：
 *   POST /transfer/start  启动转发（JSON body 或 query 参数）
 *         JSON 示例：
 *         {
 *           "url": "tcp://127.0.0.1:12345",
 *           "dst_ip": "192.168.1.1",
 *           "dst_port": 554,
 *           "decrypt": true,
 *           "transcode": true,
 *           "transcode_format": "h264",
 *           "vkek": "1234567887654321",
 *           "vkek_version": "1.0"
 *         }
 *   POST /transfer/stop   停止转发
 *   GET  /transfer/status 查询状态
 *
 * 端口：默认 8080，可通过首个参数指定，如 ./TransferServer 9000
 */
#include "TransferServer.h"
#include "logger.h"
#include "util.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <csignal>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

static volatile sig_atomic_t g_quit = 0;
static void sigint_handler(int) { g_quit = 1; }

// ==================== URL / query 解析 ====================

static std::string urldecode(const std::string &s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int v = 0;
            sscanf(s.c_str() + i + 1, "%2x", &v);
            out += static_cast<char>(v);
            i += 2;
        } else if (s[i] == '+') {
            out += ' ';
        } else {
            out += s[i];
        }
    }
    return out;
}

static std::map<std::string, std::string> parseQuery(const std::string &s)
{
    std::map<std::string, std::string> m;
    size_t i = 0;
    while (i < s.size()) {
        size_t eq = s.find('=', i);
        if (eq == std::string::npos) break;
        size_t amp = s.find('&', eq + 1);
        if (amp == std::string::npos) amp = s.size();
        std::string key = urldecode(s.substr(i, eq - i));
        std::string val = urldecode(s.substr(eq + 1, amp - eq - 1));
        if (!key.empty()) m[key] = val;
        i = amp + 1;
    }
    return m;
}

// ==================== 极简 JSON 解析（仅一层扁平对象） ====================

struct JsonValue {
    enum Type { Null, String, Number, Bool } type = Null;
    std::string str;
    double num = 0;
    bool boolean = false;
};

static void skipWs(const std::string &s, size_t &i)
{
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
}

static bool parseJsonString(const std::string &s, size_t &i, std::string &out)
{
    if (i >= s.size() || s[i] != '"') return false;
    ++i;
    out.clear();
    while (i < s.size()) {
        char c = s[i];
        if (c == '"') { ++i; return true; }
        if (c == '\\' && i + 1 < s.size()) {
            char n = s[++i];
            switch (n) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                default: out += n; break;
            }
            ++i;
        } else {
            out += c;
            ++i;
        }
    }
    return false;
}

static bool parseJsonNumber(const std::string &s, size_t &i, double &out)
{
    size_t start = i;
    if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
    bool hasDigit = false;
    while (i < s.size() && isdigit((unsigned char)s[i])) { ++i; hasDigit = true; }
    if (i < s.size() && s[i] == '.') {
        ++i;
        while (i < s.size() && isdigit((unsigned char)s[i])) { ++i; hasDigit = true; }
    }
    if (!hasDigit) { i = start; return false; }
    out = atof(s.substr(start, i - start).c_str());
    return true;
}

static bool parseJsonValue(const std::string &s, size_t &i, JsonValue &v)
{
    skipWs(s, i);
    if (i >= s.size()) return false;
    char c = s[i];
    if (c == '"') {
        v.type = JsonValue::String;
        return parseJsonString(s, i, v.str);
    }
    if (c == '-' || c == '+' || isdigit((unsigned char)c)) {
        v.type = JsonValue::Number;
        return parseJsonNumber(s, i, v.num);
    }
    if (s.compare(i, 4, "true") == 0) { v.type = JsonValue::Bool; v.boolean = true; i += 4; return true; }
    if (s.compare(i, 5, "false") == 0) { v.type = JsonValue::Bool; v.boolean = false; i += 5; return true; }
    if (s.compare(i, 4, "null") == 0) { v.type = JsonValue::Null; i += 4; return true; }
    return false;
}

static bool parseJsonObject(const std::string &s, std::map<std::string, JsonValue> &out)
{
    size_t i = 0;
    skipWs(s, i);
    if (i >= s.size() || s[i] != '{') return false;
    ++i;
    skipWs(s, i);
    if (i < s.size() && s[i] == '}') { ++i; return true; }
    while (i < s.size()) {
        skipWs(s, i);
        std::string key;
        if (!parseJsonString(s, i, key)) return false;
        skipWs(s, i);
        if (i >= s.size() || s[i] != ':') return false;
        ++i;
        JsonValue v;
        if (!parseJsonValue(s, i, v)) return false;
        out[key] = v;
        skipWs(s, i);
        if (i < s.size() && s[i] == ',') { ++i; continue; }
        if (i < s.size() && s[i] == '}') { ++i; return true; }
        return false;
    }
    return false;
}

static std::string jsonValueToString(const JsonValue &v)
{
    switch (v.type) {
        case JsonValue::String: return v.str;
        case JsonValue::Number: return std::to_string(static_cast<int>(v.num));
        case JsonValue::Bool: return v.boolean ? "true" : "false";
        default: return "";
    }
}

// ==================== HTTP 基础 ====================

static bool readRequest(int fd, std::string &method, std::string &path, std::string &body)
{
    char buf[4096];
    std::string raw;
    int contentLength = -1;
    bool gotHeaders = false;

    while (true) {
        ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) return false;
        buf[n] = '\0';
        raw += buf;

        if (!gotHeaders) {
            size_t sep = raw.find("\r\n\r\n");
            if (sep == std::string::npos) {
                if (raw.size() > 8192) return false;
                continue;
            }
            std::string headers = raw.substr(0, sep);
            body = raw.substr(sep + 4);

            size_t sp1 = headers.find(' ');
            if (sp1 == std::string::npos) return false;
            method = headers.substr(0, sp1);
            size_t sp2 = headers.find(' ', sp1 + 1);
            if (sp2 == std::string::npos) return false;
            path = headers.substr(sp1 + 1, sp2 - sp1 - 1);

            size_t cl = headers.find("Content-Length:");
            if (cl != std::string::npos) {
                cl += 14;
                while (cl < headers.size() && headers[cl] == ' ') cl++;
                contentLength = std::stoi(headers.substr(cl));
            }
            gotHeaders = true;
        }

        if (contentLength < 0 || static_cast<int>(body.size()) >= contentLength) break;
        if (raw.size() > 65536) return false;
    }
    return true;
}

static void sendResponse(int fd, int code, const std::string &body,
                         const std::string &contentType = "application/json")
{
    const char *msg = (code == 200) ? "OK"
                    : (code == 400) ? "Bad Request"
                    : (code == 500) ? "Internal Server Error"
                    : "Error";
    std::ostringstream out;
    out << "HTTP/1.1 " << code << " " << msg << "\r\n"
        << "Content-Type: " << contentType << "; charset=utf-8\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n\r\n"
        << body;
    std::string s = out.str();
    send(fd, s.data(), s.size(), MSG_NOSIGNAL);
}

static std::string jsonOk(const std::string &key, const std::string &val)
{
    return "{\"ok\":true,\"" + key + "\":\"" + val + "\"}";
}
static std::string jsonErr(const std::string &msg)
{
    return "{\"ok\":false,\"error\":\"" + msg + "\"}";
}

// ==================== 配置解析 ====================

static bool toBool(const std::string &v, bool def = false)
{
    if (v.empty()) return def;
    std::string s = v;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)::tolower(c); });
    return s == "true" || s == "1" || s == "yes" || s == "on";
}

static int toInt(const std::string &v, int def = 0)
{
    if (v.empty()) return def;
    return atoi(v.c_str());
}

static bool parseVkek(const std::string &s, unsigned char out[16])
{
    memset(out, 0, 16);
    if (s.empty()) return false;

    // 32 个十六进制字符 -> 解码为 16 字节
    bool hexLike = s.size() == 32 &&
        std::all_of(s.begin(), s.end(), [](unsigned char c) { return isxdigit(c) != 0; });
    if (hexLike) {
        for (int i = 0; i < 16; ++i) {
            unsigned int b = 0;
            sscanf(s.c_str() + i * 2, "%2x", &b);
            out[i] = static_cast<unsigned char>(b);
        }
        return true;
    }

    // 否则按原始字节拷贝（最多 16 字节）
    size_t n = std::min(s.size(), static_cast<size_t>(16));
    memcpy(out, s.data(), n);
    return true;
}

// 由统一参数表构建 TransferConfig；失败返回非空错误信息
static std::string buildConfig(const std::map<std::string, std::string> &p, TransferConfig &cfg)
{
    auto has = [&p](const std::string &k) { return p.count(k) > 0 && !p.at(k).empty(); };
    auto get = [&p](const std::string &k) { return p.count(k) ? p.at(k) : std::string(); };

    std::string url = get("url");
    if (url.empty()) return "missing url";
    StreamUrl recv = parseStreamUrl(url);
    if (!recv.ok) return "invalid url, expect tcp://ip or udp://ip (port optional)";
    cfg.protocol = (recv.protocol == "tcp" || recv.protocol == "TCP") ? Protocol::TCP : Protocol::UDP;
    cfg.recvIp   = recv.ip;
    cfg.recvPort = static_cast<uint16_t>(recv.port);

    std::string dstIp = get("dst_ip").empty() ? get("dstIp") : get("dst_ip");
    std::string dstPort = get("dst_port").empty() ? get("dstPort") : get("dst_port");
    if (dstIp.empty()) return "missing dst_ip";
    if (dstPort.empty()) return "missing dst_port";
    cfg.dstIp   = dstIp;
    cfg.dstPort = toInt(dstPort);

    std::string outProto = get("out_protocol").empty() ? get("outProtocol") : get("out_protocol");
    if (!outProto.empty()) {
        cfg.outProtocol = (outProto == "tcp" || outProto == "TCP") ? Protocol::TCP : Protocol::UDP;
    }

    cfg.decrypt   = toBool(get("decrypt"), false);
    cfg.transcode = toBool(get("transcode"), false);

    std::string fmt = get("transcode_format");
    if (!fmt.empty()) {
        cfg.transcodeFormat = fmt;
    }

    cfg.vkekVersion = get("vkek_version").empty() ? "1.0" : get("vkek_version");
    std::string vkekStr = get("vkek");
    if (!vkekStr.empty()) {
        parseVkek(vkekStr, cfg.vkek);
    }

    cfg.fps     = has("fps") ? toInt(get("fps"), 25) : 25;
    cfg.bitrate = has("bitrate") ? toInt(get("bitrate"), 2000000) : 2000000;
    cfg.width   = has("width") ? toInt(get("width"), 1920) : 1920;
    cfg.height  = has("height") ? toInt(get("height"), 1080) : 1080;

    std::string lib = get("decoder_lib_path");
    if (!lib.empty()) cfg.decoderLibPath = lib;

    std::string ssrc = get("ssrc");
    if (!ssrc.empty()) cfg.ssrc = static_cast<uint32_t>(strtoul(ssrc.c_str(), nullptr, 0));

    return "";
}

// ==================== 主流程 ====================

int main(int argc, char *argv[])
{
    int httpPort = 8080;
    if (argc >= 2) httpPort = atoi(argv[1]);

    signal(SIGINT, sigint_handler);

    std::unique_ptr<TransferServer> transfer;
    std::mutex transferMutex;

    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0) {
        BLOG(error) << "socket failed";
        return 1;
    }
    int opt = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<uint16_t>(httpPort));

    if (bind(listenFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        BLOG(error) << "bind failed on port " << httpPort;
        close(listenFd);
        return 1;
    }
    if (listen(listenFd, 8) < 0) {
        BLOG(error) << "listen failed";
        close(listenFd);
        return 1;
    }
    BLOG(info) << "TransferServer HTTP listening on port " << httpPort;

    while (!g_quit) {
        struct timeval tv = {1, 0};
        fd_set rd;
        FD_ZERO(&rd);
        FD_SET(listenFd, &rd);
        int r = select(listenFd + 1, &rd, nullptr, nullptr, &tv);
        if (r <= 0) continue;

        int client = accept(listenFd, nullptr, nullptr);
        if (client < 0) continue;

        std::string method, path, body;
        if (!readRequest(client, method, path, body)) {
            close(client);
            continue;
        }

        size_t q = path.find('?');
        std::string pathOnly = (q != std::string::npos) ? path.substr(0, q) : path;
        std::string query = (q != std::string::npos && q + 1 < path.size()) ? path.substr(q + 1) : "";

        if (pathOnly == "/transfer/start" && (method == "POST" || method == "GET")) {
            // 参数来源：query 优先合并，JSON body 覆盖
            std::map<std::string, std::string> params = parseQuery(query);
            if (!body.empty()) {
                std::map<std::string, JsonValue> json;
                if (parseJsonObject(body, json)) {
                    for (const auto &kv : json) {
                        params[kv.first] = jsonValueToString(kv.second);
                    }
                }
            }

            TransferConfig cfg;
            std::string err = buildConfig(params, cfg);
            if (!err.empty()) {
                sendResponse(client, 400, jsonErr(err));
                close(client);
                continue;
            }

            std::lock_guard<std::mutex> lk(transferMutex);
            if (transfer && transfer->isRunning()) {
                sendResponse(client, 400, jsonErr("transfer already running"));
                close(client);
                continue;
            }

            transfer = std::make_unique<TransferServer>();
            if (!transfer->start(cfg)) {
                sendResponse(client, 500, jsonErr("transfer start failed"));
                transfer.reset();
            } else {
                uint16_t recvPort = transfer->getRecvPort();
                std::string body = "{\"ok\":true,\"status\":\"started\",\"recv_port\":"
                    + std::to_string(recvPort) + "}";
                sendResponse(client, 200, body);
            }
        } else if (pathOnly == "/transfer/stop" && (method == "POST" || method == "GET")) {
            std::lock_guard<std::mutex> lk(transferMutex);
            if (transfer) {
                transfer->stop();
                transfer.reset();
                sendResponse(client, 200, jsonOk("status", "stopped"));
            } else {
                sendResponse(client, 200, jsonOk("status", "not_running"));
            }
        } else if (pathOnly == "/transfer/status" && method == "GET") {
            std::lock_guard<std::mutex> lk(transferMutex);
            bool running = transfer && transfer->isRunning();
            sendResponse(client, 200, running ? jsonOk("status", "running") : jsonOk("status", "stopped"));
        } else {
            sendResponse(client, 404, jsonErr("not found"));
        }
        close(client);
    }

    {
        std::lock_guard<std::mutex> lk(transferMutex);
        if (transfer) transfer->stop();
    }
    close(listenFd);
    BLOG(info) << "TransferServer HTTP stopped";
    return 0;
}
