#include "remote_source.h"
#include "runtime/log.h"
#include "runtime/env.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace nj::runtime::input {
namespace {

void close_udp(int fd) {
#ifdef _WIN32
    closesocket(fd);
#else
    close(fd);
#endif
}

// Prise UDP non bloquante sur 127.0.0.1:port (seulement la machine elle-meme) ; -1 si le port est pris.
int open_loopback_udp(unsigned short port) {
#ifdef _WIN32
    static const bool wsa_ready = [] {
        WSADATA w;
        return WSAStartup(MAKEWORD(2, 2), &w) == 0;
    }();
    (void)wsa_ready;
#endif
    const int fd = (int)socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return -1;
    sockaddr_in where{};
    where.sin_family = AF_INET;
    where.sin_port = htons(port);
    where.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fd, reinterpret_cast<const sockaddr*>(&where), sizeof where) != 0) {
        close_udp(fd);
        return -1;
    }
#ifdef _WIN32
    u_long on = 1;
    ioctlsocket(fd, FIONBIO, &on);
#else
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
#endif
    return fd;
}

}  // namespace

// Manette a distance (essais) : NJ_REMOTEPAD=<port UDP> (8771 par defaut, ou si <= 1024).
bool RemoteSource::enabled() { return NJ_ENV("NJ_REMOTEPAD") != nullptr; }

RemoteSource::RemoteSource() {
    const char* e = NJ_ENV("NJ_REMOTEPAD");
    const int asked = e ? std::atoi(e) : 0;
    const unsigned short port = (unsigned short)(asked > 1024 && asked < 65536 ? asked : 8771);
    m_socket = open_loopback_udp(port);
    if (m_socket < 0)
        NJ_ERR << "[RemotePad] port UDP " << port << " indisponible" << std::endl;
    else
        NJ_LOG << "[RemotePad] manette a distance sur 127.0.0.1:" << port << std::endl;
}

RemoteSource::~RemoteSource() {
    if (m_socket >= 0)
        close_udp(m_socket);
}

void RemoteSource::poll(PadState pads[4]) {
    if (m_socket < 0)
        return;
    unsigned char buf[64];
    for (;;) {   // garder le dernier paquet en attente
        const int n = (int)recv(m_socket, (char*)buf, sizeof buf, 0);
        if (n < 12)
            break;
        if (std::memcmp(buf, "NWRP", 4) != 0)
            continue;
        m_state.buttons = (uint16_t)((buf[4] << 8) | buf[5]);
        m_state.stick_x = (int8_t)buf[6];
        m_state.stick_y = (int8_t)buf[7];
        m_state.substick_x = (int8_t)buf[8];
        m_state.substick_y = (int8_t)buf[9];
        m_state.trigger_l = buf[10];
        m_state.trigger_r = buf[11];
        m_last = std::chrono::steady_clock::now();
        m_have = true;
    }
    if (!m_have || std::chrono::steady_clock::now() - m_last > std::chrono::milliseconds(300))
        return;
    PadState& p = pads[0];
    p.buttons |= m_state.buttons;
    if (m_state.stick_x || m_state.stick_y) {
        p.stick_x = m_state.stick_x;
        p.stick_y = m_state.stick_y;
    }
    if (m_state.substick_x || m_state.substick_y) {
        p.substick_x = m_state.substick_x;
        p.substick_y = m_state.substick_y;
    }
    if (m_state.trigger_l > p.trigger_l) p.trigger_l = m_state.trigger_l;
    if (m_state.trigger_r > p.trigger_r) p.trigger_r = m_state.trigger_r;
    // Le jeu fabrique L et R depuis les gachettes analogiques (pad.cpp) : bouton presse = a fond.
    if (m_state.buttons & (1 << 6)) p.trigger_l = 255;
    if (m_state.buttons & (1 << 5)) p.trigger_r = 255;
}

}  // namespace nj::runtime::input
