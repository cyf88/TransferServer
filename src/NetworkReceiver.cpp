#include "NetworkReceiver.h"
#include "logger.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

NetworkReceiver::NetworkReceiver(Protocol protocol, const std::string &ip, uint16_t port)
    : _protocol(protocol)
    , _ip(ip)
    , _port(port) {}

NetworkReceiver::~NetworkReceiver() {
    stop();
    if (_sock > 0)
        close(_sock);
}

// 设置数据回调
void NetworkReceiver::setCallback(RtpCallback cb) {
    _callback = std::move(cb);
}

int NetworkReceiver::getRecvBufSize() const {
    if (_sock < 0)
        return -1;
    int val = 0;
    socklen_t len = sizeof(val);
    if (getsockopt(_sock, SOL_SOCKET, SO_RCVBUF, &val, &len) != 0)
        return -1;
    return val;
}

// 创建 Socket 并绑定 / 监听
bool NetworkReceiver::createSocket() {
    BLOG(info) << "[NetworkReceiver] createSocket() called, protocol=" << (_protocol == Protocol::TCP ? "TCP" : "UDP");
    if (_protocol == Protocol::TCP) {
        _sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    } else {
        _sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }

    if (_sock < 0) {
        BLOG(error) << "[NetworkReceiver] create socket failed, errno=" << errno << " " << strerror(errno);
        return false;
    }
    BLOG(info) << "[NetworkReceiver] socket created, fd=" << _sock;
    setRecvBufSize(1024 * 1024 * 5);
    BLOG(info) << "[NetworkReceiver] recv buf size: " << _recvBufSize;

    // 设置地址可复用
    int opt = 1;
    setsockopt(_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));


    // 设置接收缓冲区大小
    if (_recvBufSize > 0) {
        if (setsockopt(_sock, SOL_SOCKET, SO_RCVBUF, &_recvBufSize, sizeof(_recvBufSize)) < 0) {
            BLOG(warning) << "[NetworkReceiver] SO_RCVBUF set failed: " << strerror(errno);
        }
    }

    struct timeval tv;
	tv.tv_sec =  10;
	tv.tv_usec = 0;
	if (setsockopt(_sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(timeval)) < 0) {
		BLOG(error) << "[NetworkReceiver] SO_RCVTIMEO set failed: " << strerror(errno);
		return false;
	}

    // 填充 sockaddr_in
    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    // addr.sin_addr.s_addr = inet_addr(_ip.c_str());
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (_protocol == Protocol::TCP) {
        if (bind(_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            BLOG(error) << "TCP bind failed";
            return false;
        }
        if (listen(_sock, 5) < 0) {
            BLOG(error) << "TCP listen failed";
            return false;
        }
    } else {
        // UDP 直接 bind
        if (bind(_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            BLOG(error) << "UDP bind failed";
            return false;
        }
    }

    // 当端口传入 0 时，由内核自动分配，读取实际端口
    if (_port == 0) {
        sockaddr_in local {};
        socklen_t localLen = sizeof(local);
        if (getsockname(_sock, (struct sockaddr *)&local, &localLen) == 0) {
            _port = ntohs(local.sin_port);
            BLOG(info) << "[NetworkReceiver] allocated port: " << _port;
        }
    }

    // 读取并打印实际 SO_RCVBUF（内核可能取整或加倍）
    int actual = 0;
    socklen_t len = sizeof(actual);
    if (getsockopt(_sock, SOL_SOCKET, SO_RCVBUF, &actual, &len) == 0) {
        BLOG(info) << "[NetworkReceiver] SO_RCVBUF=" << actual << " (requested " << _recvBufSize << ")";
    }

    return true;
}

// 启动接收线程
bool NetworkReceiver::start() {
    if (!createSocket())
        return false;

    _running = true;
    auto self = shared_from_this();

    if (_protocol == Protocol::TCP) {
        _recvThread = std::thread([self]() { self->tcpRecvLoop(); });
    } else {
        _recvThread = std::thread([self]() { self->udpRecvLoop(); });
    }

    return true;
}

// 停止接收：先关闭 socket 使 accept() 返回，再 join 线程
void NetworkReceiver::stop() {
    _running = false;
    if (_sock >= 0) {
        shutdown(_sock, SHUT_RDWR);
        close(_sock);
        _sock = -1;
    }


}

// TCP 接收循环：只 accept 一次，连接断开后退出
void NetworkReceiver::tcpRecvLoop() {
    BLOG(info) << "[NetworkReceiver] TCP waiting for client...";
    
    // accept() 可能被信号中断（EINTR），需要重试
    int clientSock = -1;
    while (_running && _sock >= 0 && clientSock < 0) {
        clientSock = accept(_sock, (struct sockaddr *)&_clientAddr, &_clientAddrLen);
        if (clientSock < 0) {
            if (!_running || _sock < 0) {
                BLOG(info) << "[NetworkReceiver] TCP socket closed before accept";
                return;
            }
            if (errno == EINTR) {
                // 被信号中断，重试
                BLOG(debug) << "[NetworkReceiver] accept interrupted by signal, retrying...";
                continue;
            }
            BLOG(error) << "[NetworkReceiver] TCP accept failed: " << strerror(errno) << " (errno=" << errno << ")";
            return;
        }
    }
    
    if (clientSock < 0) {
        return;  // 退出条件不满足或 accept 失败
    }
    BLOG(info) << "[NetworkReceiver] TCP client connected from " 
               << inet_ntoa(_clientAddr.sin_addr) << ":" << ntohs(_clientAddr.sin_port);
    
    struct timeval timeout;
    timeout.tv_sec = 50;
    timeout.tv_usec = 0;
    if (setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        BLOG(warning) << "[NetworkReceiver] Setsockopt SO_RCVTIMEO failed: " << strerror(errno);
    }

    uint8_t buf[1500];
    while (_running) {
        int len = recv(clientSock, buf, sizeof(buf), 0);
        if (len == 0) {
            BLOG(info) << "[NetworkReceiver] Client disconnected";
            break;
        } else if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                BLOG(debug) << "[NetworkReceiver] recv timeout, continue";
                continue;
            } else {
                BLOG(warning) << "[NetworkReceiver] recv error: " << strerror(errno);
                break;
            }
        } else {
            if (_callback)
                _callback(buf, len);
        }
    }
    close(clientSock);
    BLOG(info) << "[NetworkReceiver] tcpRecvLoop end";
}

// UDP 接收循环
void NetworkReceiver::udpRecvLoop() {
    uint8_t buf[1500];
    while (_running) {
        int len = recvfrom(_sock, buf, sizeof(buf), 0, (struct sockaddr *)&_clientAddr, &_clientAddrLen);
        if (len <= 0)
            continue;
        if (_callback)
            _callback(buf, len);
    }
}
