#pragma once
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include <netinet/in.h> // sockaddr_in

enum class Protocol { TCP, UDP };

class NetworkReceiver : public std::enable_shared_from_this<NetworkReceiver> {
public:
    using RtpCallback = std::function<void(const uint8_t*, size_t)>;

    NetworkReceiver(Protocol protocol, const std::string& ip, uint16_t port);
    ~NetworkReceiver();

    void setCallback(RtpCallback cb);

    /** 设置接收缓冲区大小（字节），在 start() 前调用；0 表示使用系统默认 */
    void setRecvBufSize(int size) { _recvBufSize = size; }
    /** 获取当前 socket 的 SO_RCVBUF 实际值；仅在 start() 后有效，否则返回 -1 */
    int getRecvBufSize() const;

    /** 获取实际监听的端口（当构造时端口传 0 时，由内核自动分配） */
    uint16_t getPort() const { return _port; }

    bool start();
    void stop();

private:
    bool createSocket();

    void tcpRecvLoop();
    void udpRecvLoop();

private:
    Protocol _protocol;
    std::string _ip;
    uint16_t _port;

    int _sock{-1};  // 接收套接字
    int _recvBufSize{0};  // 期望的 SO_RCVBUF，0=默认
    std::atomic<bool> _running{false};
    std::thread _recvThread;
    RtpCallback _callback;

    sockaddr_in _clientAddr{};
    socklen_t _clientAddrLen{sizeof(_clientAddr)};
};
