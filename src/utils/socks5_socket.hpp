#pragma once


#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <stdexcept>
#include <vector>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

namespace utils {

class Socks5Error : public std::runtime_error {
public:
    explicit Socks5Error(const std::string& msg) : std::runtime_error(msg) {}
};


struct TcpConn {
    SOCKET s = INVALID_SOCKET;

    explicit TcpConn(const std::string& host, uint16_t port) {
        struct addrinfo hints{}, *res = nullptr;
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        char port_str[8];
        snprintf(port_str, sizeof(port_str), "%u", port);
        if (getaddrinfo(host.c_str(), port_str, &hints, &res) != 0 || !res)
            throw Socks5Error("DNS resolution failed for proxy host");
        s = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (s == INVALID_SOCKET) { freeaddrinfo(res); throw Socks5Error("socket() failed"); }
        DWORD tv = 10000;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
        if (::connect(s, res->ai_addr, (int)res->ai_addrlen) != 0) {
            freeaddrinfo(res); closesocket(s); s = INVALID_SOCKET;
            throw Socks5Error("TCP connect to SOCKS5 proxy failed");
        }
        freeaddrinfo(res);
    }
    ~TcpConn() { if (s != INVALID_SOCKET) closesocket(s); }

    void send_all(const uint8_t* buf, int len) {
        int sent = 0;
        while (sent < len) {
            int r = ::send(s, (const char*)buf + sent, len - sent, 0);
            if (r <= 0) throw Socks5Error("send() failed on SOCKS5 control stream");
            sent += r;
        }
    }
    void recv_all(uint8_t* buf, int len) {
        int got = 0;
        while (got < len) {
            int r = ::recv(s, (char*)buf + got, len - got, 0);
            if (r <= 0) throw Socks5Error("recv() failed on SOCKS5 control stream");
            got += r;
        }
    }
};



inline SOCKET socks5_udp_associate(
    const std::string& proxy_host,
    uint16_t           proxy_port,
    const std::string& username,
    const std::string& password,
    sockaddr_in&       out_relay_addr,
    SOCKET&            out_control_socket)
{
    
    TcpConn tc(proxy_host, proxy_port);
    bool use_auth = !username.empty() && !password.empty();
    {
        std::vector<uint8_t> hello;
        hello.push_back(0x05);
        if (use_auth) { hello.push_back(0x02); hello.push_back(0x00); hello.push_back(0x02); }
        else          { hello.push_back(0x01); hello.push_back(0x00); }
        tc.send_all(hello.data(), (int)hello.size());
        uint8_t resp[2]; tc.recv_all(resp, 2);
        if (resp[0] != 0x05) throw Socks5Error("SOCKS5 server returned unexpected version");
        if (resp[1] == 0xFF) throw Socks5Error("SOCKS5 server rejected all auth methods");
    }

    
    if (use_auth) {
        std::vector<uint8_t> auth;
        auth.push_back(0x01);
        auth.push_back((uint8_t)username.size());
        for (char c : username) auth.push_back((uint8_t)c);
        auth.push_back((uint8_t)password.size());
        for (char c : password) auth.push_back((uint8_t)c);
        tc.send_all(auth.data(), (int)auth.size());
        uint8_t resp[2]; tc.recv_all(resp, 2);
        if (resp[1] != 0x00) throw Socks5Error("SOCKS5 username/password authentication failed");
    }

    
    {
        uint8_t req[10] = { 0x05, 0x03, 0x00, 0x01,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        tc.send_all(req, 10);
        uint8_t resp[4]; tc.recv_all(resp, 4);
        if (resp[0] != 0x05) throw Socks5Error("SOCKS5 bad version in UDP ASSOCIATE response");
        if (resp[1] != 0x00) throw Socks5Error("SOCKS5 UDP ASSOCIATE failed, code=" + std::to_string(resp[1]));
        if (resp[3] == 0x01) {
            uint8_t tail[6]; tc.recv_all(tail, 6);
            out_relay_addr = {};
            out_relay_addr.sin_family = AF_INET;
            memcpy(&out_relay_addr.sin_addr, tail, 4);
            out_relay_addr.sin_port = *(uint16_t*)(tail + 4); 
        } else {
            throw Socks5Error("SOCKS5 UDP ASSOCIATE returned unsupported address type");
        }
    }

    
    out_control_socket = tc.s;
    tc.s = INVALID_SOCKET; 

    
    SOCKET udp = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp == INVALID_SOCKET) throw Socks5Error("Failed to create UDP socket for SOCKS5");
    u_long nb = 1;
    ioctlsocket(udp, FIONBIO, &nb);
    sockaddr_in bind_addr{}; bind_addr.sin_family = AF_INET;
    ::bind(udp, (sockaddr*)&bind_addr, sizeof(bind_addr));
    return udp;
}


inline std::vector<uint8_t> socks5_udp_header(const sockaddr_in& target) {
    std::vector<uint8_t> h;
    h.push_back(0x00); h.push_back(0x00); 
    h.push_back(0x00);                     
    h.push_back(0x01);                     
    uint8_t ip[4]; memcpy(ip, &target.sin_addr, 4);
    h.insert(h.end(), ip, ip + 4);
    h.push_back(((uint8_t*)&target.sin_port)[0]);
    h.push_back(((uint8_t*)&target.sin_port)[1]);
    return h;
}


inline int socks5_udp_parse_header(const uint8_t* buf, int len, sockaddr_in& out_src) {
    if (len < 10) return -1;
    if (buf[0] != 0x00 || buf[1] != 0x00) return -1; 
    if (buf[2] != 0x00) return -1;                    
    if (buf[3] != 0x01) return -1;                    
    out_src.sin_family = AF_INET;
    memcpy(&out_src.sin_addr, buf + 4, 4);
    out_src.sin_port = *(uint16_t*)(buf + 8);
    return 10;
}

} 
