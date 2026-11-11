/**
 * TransferServer HTTP 服务
 *
 * 通过 HTTP 接口下发转发配置，控制解密 / 转码流水线，支持多路并发转发。
 *
 * 接口：
 *   POST /transfer/start  启动一路转发（JSON body 或 query 参数），返回 id 与 recv_port
 *         JSON 示例：
 *         {
 *           "src_protocol": "tcp",
 *           "dst_ip": "192.168.1.1",
 *           "dst_port": 554,
 *           "decrypt": true,
 *           "transcode": true,
 *           "transcode_format": "h264",
 *           "vkek": "AQIDBAUGBwgJCgsMDQ4PEA==",
 *           "vkek_version": "1.0"
 *         }
 *   POST /transfer/save   保存接收流与解密/转码后流到文件（不对外推送）
 *   POST /transfer/stop   停止转发（可选 ?id=N 停止指定路，缺省停止全部）
 *   GET  /transfer/status 查询状态（可选 ?id=N 查询指定路，缺省返回全部）
 *   GET  /transfer/transcode 查询当前运行的转码服务（仅转码路）
 *
 * 端口：默认 8080，可通过首个参数指定，如 ./TransferServer 9000
 */
#include "TransferServer.h"
#include "logger.h"

#include "httplib.h"
#include "json.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

using nlohmann::json;

// ==================== 配置读取 ====================

// 收流空闲超时（秒），0 表示不超时；由 config.ini 读取
static int g_idleTimeoutSec = 0;

// 从 config.ini 读取整型配置项（key=value，支持 # 注释），读不到返回默认值
static int readConfigInt(const std::string &path, const std::string &key, int defaultValue)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) return defaultValue;

    auto trim = [](std::string s) {
        size_t b = s.find_first_not_of(" \t\r\n");
        size_t e = s.find_last_not_of(" \t\r\n");
        if (b == std::string::npos) return std::string();
        return s.substr(b, e - b + 1);
    };

    std::string line;
    while (std::getline(ifs, line)) {
        size_t pos = line.find('#');
        if (pos != std::string::npos) line = line.substr(0, pos);
        pos = line.find('=');
        if (pos == std::string::npos) continue;
        if (trim(line.substr(0, pos)) == key) {
            std::string v = trim(line.substr(pos + 1));
            try { return std::stoi(v); } catch (...) { return defaultValue; }
        }
    }
    return defaultValue;
}

// ==================== 参数解析 ====================

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

// base64 解码到字节缓冲区；包含非法字符时返回 false
static bool base64Decode(const std::string &in, unsigned char *out, size_t outCap, size_t &outLen)
{
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint32_t val = 0;
    int valb = -8;
    outLen = 0;
    for (unsigned char c : in) {
        if (c == '=') break;
        const char *hit = strchr(tbl, static_cast<int>(c));
        if (!hit) return false;
        val = (val << 6) | static_cast<uint32_t>(hit - tbl);
        valb += 6;
        if (valb >= 0) {
            if (outLen >= outCap) return false;
            out[outLen++] = static_cast<unsigned char>((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    return true;
}

static bool parseVkek(const std::string &s, unsigned char out[16])
{
    memset(out, 0, 16);
    if (s.empty()) return false;

    // base64 解码为字节（最多取 16 字节）
    unsigned char buf[64] = {0};
    size_t n = 0;
    if (!base64Decode(s, buf, sizeof(buf), n) || n == 0) {
        BLOG(warning) << "invalid vkek base64: " << s;
        return false;
    }
    size_t copyLen = std::min(n, static_cast<size_t>(16));
    memcpy(out, buf, copyLen);
    return true;
}

// 任意 JSON 值统一转为字符串，供 toBool / toInt 等复用
static std::string jsonToString(const json &v)
{
    if (v.is_string()) return v.get<std::string>();
    if (v.is_boolean()) return v.get<bool>() ? "true" : "false";
    if (v.is_number_integer()) return std::to_string(v.get<int64_t>());
    if (v.is_number_unsigned()) return std::to_string(v.get<uint64_t>());
    if (v.is_number_float()) return std::to_string(v.get<double>());
    return "";
}

// 合并 query 参数与 JSON body（body 覆盖 query）
static json collectParams(const httplib::Request &req)
{
    json p = json::object();
    for (const auto &kv : req.params) {
        p[kv.first] = kv.second;
    }
    if (!req.body.empty()) {
        try {
            json body = json::parse(req.body);
            if (body.is_object()) {
                for (auto it = body.begin(); it != body.end(); ++it) {
                    p[it.key()] = it.value();
                }
            }
        } catch (const json::parse_error &) {
            // 非法 body 忽略，必填项由 buildConfig 校验
        }
    }
    return p;
}

// 统一获取 JSON 字段并转为字符串
static std::string getParam(const json &p, const std::string &k)
{
    auto it = p.find(k);
    return it == p.end() ? std::string() : jsonToString(*it);
}

static bool hasParam(const json &p, const std::string &k)
{
    return p.contains(k);
}

// 解析 src_protocol 与解密/转码等公共参数；失败返回非空错误信息
static std::string parseCommonParams(const json &p, TransferConfig &cfg)
{
    std::string proto = getParam(p, "src_protocol");
    if (proto.empty()) return "missing src_protocol";
    if (proto == "tcp" || proto == "TCP") {
        cfg.protocol = Protocol::TCP;
    } else if (proto == "udp" || proto == "UDP") {
        cfg.protocol = Protocol::UDP;
    } else {
        return "invalid src_protocol, expect tcp or udp";
    }
    cfg.recvIp   = "";
    cfg.recvPort = 0; // 始终由内核自动分配

    cfg.decrypt   = toBool(getParam(p, "decrypt"), false);
    cfg.transcode = toBool(getParam(p, "transcode"), false);

    std::string fmt = getParam(p, "transcode_format");
    if (!fmt.empty()) cfg.transcodeFormat = fmt;

    std::string vkekVersion = getParam(p, "vkek_version");
    cfg.vkekVersion = vkekVersion.empty() ? "1.0" : vkekVersion;
    std::string vkekStr = getParam(p, "vkek");
    if (!vkekStr.empty()) parseVkek(vkekStr, cfg.vkek);

    cfg.fps     = hasParam(p, "fps") ? toInt(getParam(p, "fps"), 25) : 25;
    cfg.bitrate = hasParam(p, "bitrate") ? toInt(getParam(p, "bitrate"), 2000000) : 2000000;
    cfg.idleTimeoutSec = g_idleTimeoutSec;

    return "";
}

// 由统一参数对象构建转发 TransferConfig；失败返回非空错误信息
static std::string buildConfig(const json &p, TransferConfig &cfg)
{
    std::string err = parseCommonParams(p, cfg);
    if (!err.empty()) return err;

    std::string dstIp = getParam(p, "dst_ip").empty() ? getParam(p, "dstIp") : getParam(p, "dst_ip");
    std::string dstPort = getParam(p, "dst_port").empty() ? getParam(p, "dstPort") : getParam(p, "dst_port");
    if (dstIp.empty()) return "missing dst_ip";
    if (dstPort.empty()) return "missing dst_port";
    cfg.dstIp   = dstIp;
    cfg.dstPort = toInt(dstPort);

    std::string outProto = getParam(p, "out_protocol").empty() ? getParam(p, "outProtocol") : getParam(p, "out_protocol");
    if (!outProto.empty()) {
        cfg.outProtocol = (outProto == "tcp" || outProto == "TCP") ? Protocol::TCP : Protocol::UDP;
    }

    std::string ssrc = getParam(p, "ssrc");
    if (!ssrc.empty()) cfg.ssrc = static_cast<uint32_t>(strtoul(ssrc.c_str(), nullptr, 0));

    return "";
}

// 构建“保存到文件”配置（不对外推送）
static std::string buildSaveConfig(const json &p, TransferConfig &cfg)
{
    std::string err = parseCommonParams(p, cfg);
    if (!err.empty()) return err;

    cfg.saveToFile   = true;
    cfg.recvFilePath = getParam(p, "recv_file").empty() ? getParam(p, "recvFile") : getParam(p, "recv_file");
    cfg.outFilePath  = getParam(p, "out_file").empty() ? getParam(p, "outFile") : getParam(p, "out_file");
    if (cfg.recvFilePath.empty()) return "missing recv_file";
    if (cfg.outFilePath.empty()) return "missing out_file";

    return "";
}

static void reply(httplib::Response &res, int status, const json &body)
{
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

// 输入编码类型码 -> 字符串
static const char *inputFormatString(int codecid)
{
    switch (codecid) {
        case PSI_STREAM_VIDEO_SVAC: return "svac";
        case PSI_STREAM_H264:       return "h264";
        case PSI_STREAM_H265:       return "h265";
        default:                    return "unknown";
    }
}

// ==================== 主流程 ====================

int main(int argc, char *argv[])
{
    int httpPort = 8080;
    if (argc >= 2) httpPort = atoi(argv[1]);

    g_idleTimeoutSec = readConfigInt("config.ini", "idle_timeout", 0);
    BLOG(info) << "config.ini idle_timeout=" << g_idleTimeoutSec;

    std::map<uint32_t, std::unique_ptr<TransferServer>> transfers;
    std::mutex transfersMutex;
    std::atomic<uint32_t> nextId{1};

    httplib::Server svr;

    auto onStart = [&](const httplib::Request &req, httplib::Response &res) {
        BLOG(info) << "[HTTP] " << req.method << " " << req.path;
        TransferConfig cfg;
        std::string err = buildConfig(collectParams(req), cfg);
        if (!err.empty()) {
            BLOG(warning) << "[HTTP] start 参数错误: " << err;
            reply(res, 400, {{"ok", false}, {"error", err}});
            return;
        }
        BLOG(info) << "[HTTP] start: src_protocol=" << (cfg.protocol == Protocol::TCP ? "tcp" : "udp")
                   << " dst=" << cfg.dstIp << ":" << cfg.dstPort
                   << " decrypt=" << (cfg.decrypt ? "on" : "off")
                   << " transcode=" << (cfg.transcode ? cfg.transcodeFormat : "off");

        uint32_t id = nextId.fetch_add(1);
        auto transfer = std::make_unique<TransferServer>();
        if (!transfer->start(cfg)) {
            reply(res, 500, {{"ok", false}, {"error", "transfer start failed"}});
            return;
        }

        uint16_t recvPort = transfer->getRecvPort();
        {
            std::lock_guard<std::mutex> lk(transfersMutex);
            transfers[id] = std::move(transfer);
        }

        reply(res, 200, {{"ok", true}, {"status", "started"}, {"id", id}, {"recv_port", recvPort}});
    };

    auto onStop = [&](const httplib::Request &req, httplib::Response &res) {
        BLOG(info) << "[HTTP] " << req.method << " " << req.path;
        bool hasId = req.has_param("id");
        uint32_t id = hasId
            ? static_cast<uint32_t>(strtoul(req.get_param_value("id").c_str(), nullptr, 0))
            : 0;

        std::lock_guard<std::mutex> lk(transfersMutex);
        if (hasId) {
            auto it = transfers.find(id);
            if (it == transfers.end()) {
                reply(res, 404, {{"ok", false}, {"error", "transfer not found"}});
                return;
            }
            it->second->stop();
            transfers.erase(it);
            reply(res, 200, {{"ok", true}, {"status", "stopped"}});
        } else {
            for (auto &kv : transfers) {
                if (kv.second) kv.second->stop();
            }
            transfers.clear();
            reply(res, 200, {{"ok", true}, {"status", "stopped"}});
        }
    };

    auto onStatus = [&](const httplib::Request &req, httplib::Response &res) {
        BLOG(info) << "[HTTP] " << req.method << " " << req.path;
        bool hasId = req.has_param("id");
        uint32_t id = hasId
            ? static_cast<uint32_t>(strtoul(req.get_param_value("id").c_str(), nullptr, 0))
            : 0;

        std::lock_guard<std::mutex> lk(transfersMutex);
        if (hasId) {
            auto it = transfers.find(id);
            if (it == transfers.end()) {
                reply(res, 404, {{"ok", false}, {"error", "transfer not found"}});
                return;
            }
            bool running = it->second && it->second->isRunning();
            reply(res, 200, {{"ok", true}, {"status", running ? "running" : "stopped"}});
        } else {
            json arr = json::array();
            for (const auto &kv : transfers) {
                arr.push_back({
                    {"id", kv.first},
                    {"status", (kv.second && kv.second->isRunning()) ? "running" : "stopped"},
                    {"recv_port", kv.second ? kv.second->getRecvPort() : 0},
                });
            }
            reply(res, 200, {{"ok", true}, {"count", transfers.size()}, {"transfers", arr}});
        }
    };

    auto onTranscode = [&](const httplib::Request &req, httplib::Response &res) {
        BLOG(info) << "[HTTP] " << req.method << " " << req.path;
        std::lock_guard<std::mutex> lk(transfersMutex);
        json arr = json::array();
        for (const auto &kv : transfers) {
            const TransferServer *t = kv.second.get();
            if (!t || !t->isRunning()) continue;
            const TransferConfig &c = t->config();
            if (!c.transcode) continue; // 只返回转码路
            arr.push_back({
                {"id", kv.first},
                {"transcode_format", c.transcodeFormat},
                {"input_format", inputFormatString(t->inputCodecid())},
                {"decrypt", c.decrypt},
                {"protocol", c.protocol == Protocol::TCP ? "tcp" : "udp"},
                {"recv_ip", c.recvIp},
                {"recv_port", t->getRecvPort()},
                {"out_protocol", c.outProtocol == Protocol::TCP ? "tcp" : "udp"},
                {"dst_ip", c.dstIp},
                {"dst_port", c.dstPort},
                {"fps", c.fps},
                {"bitrate", c.bitrate},
                {"width", t->width()},
                {"height", t->height()},
            });
        }
        reply(res, 200, {{"ok", true}, {"count", arr.size()}, {"transfers", arr}});
    };

    auto onSave = [&](const httplib::Request &req, httplib::Response &res) {
        BLOG(info) << "[HTTP] " << req.method << " " << req.path;
        TransferConfig cfg;
        std::string err = buildSaveConfig(collectParams(req), cfg);
        if (!err.empty()) {
            BLOG(warning) << "[HTTP] save 参数错误: " << err;
            reply(res, 400, {{"ok", false}, {"error", err}});
            return;
        }
        BLOG(info) << "[HTTP] save: src_protocol=" << (cfg.protocol == Protocol::TCP ? "tcp" : "udp")
                   << " recv_file=" << cfg.recvFilePath
                   << " out_file=" << cfg.outFilePath
                   << " decrypt=" << (cfg.decrypt ? "on" : "off")
                   << " transcode=" << (cfg.transcode ? cfg.transcodeFormat : "off");

        uint32_t id = nextId.fetch_add(1);
        auto transfer = std::make_unique<TransferServer>();
        if (!transfer->start(cfg)) {
            reply(res, 500, {{"ok", false}, {"error", "transfer start failed"}});
            return;
        }

        uint16_t recvPort = transfer->getRecvPort();
        {
            std::lock_guard<std::mutex> lk(transfersMutex);
            transfers[id] = std::move(transfer);
        }

        reply(res, 200, {
            {"ok", true},
            {"status", "started"},
            {"id", id},
            {"recv_port", recvPort},
            {"recv_file", cfg.recvFilePath},
            {"out_file", cfg.outFilePath},
        });
    };

    svr.Get("/transfer/start", onStart);
    svr.Post("/transfer/start", onStart);
    svr.Get("/transfer/save", onSave);
    svr.Post("/transfer/save", onSave);
    svr.Get("/transfer/stop", onStop);
    svr.Post("/transfer/stop", onStop);
    svr.Get("/transfer/status", onStatus);
    svr.Get("/transfer/transcode", onTranscode);

    BLOG(info) << "TransferServer HTTP listening on port " << httpPort;
    if (!svr.listen("0.0.0.0", httpPort)) {
        BLOG(error) << "listen failed on port " << httpPort;
        return 1;
    }
    return 0;
}
