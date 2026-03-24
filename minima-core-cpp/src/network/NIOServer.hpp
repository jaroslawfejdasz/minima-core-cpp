#pragma once
/**
 * NIOServer — TCP server for Minima P2P connections.
 *
 * Listens on port (default 9001) for incoming peer connections.
 * Each connection runs NetworkProtocol on its own thread.
 * Outgoing connections (connect to peer) are also managed here.
 *
 * Java ref: NIOManager.java — manages both server socket and peer connections.
 *
 * Thread model:
 *   - acceptThread: accept() loop on server socket
 *   - per peer: connThread — read/write loop using ProtocolConnection
 *
 * Thread-safe peer list via mutex.
 */
#include "NIOMessage.hpp"
#include "NetworkProtocol.hpp"
#include "../objects/IBD.hpp"
#include "../objects/TxPoW.hpp"
#include "../types/MiniNumber.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>

namespace minima::network {

// ── PeerConn — one live TCP connection ───────────────────────────────────────

struct PeerConn {
    int                   fd      { -1 };
    std::string           host;
    uint16_t              port    { 0 };
    bool                  inbound { false };
    std::atomic<bool>     running { false };
    std::thread           thread;
    std::unique_ptr<ProtocolConnection> proto;

    PeerConn(int fd_, const std::string& h, uint16_t p, bool in,
             MiniNumber myTip,
             std::function<void(NetworkEvent)> cb)
        : fd(fd_), host(h), port(p), inbound(in)
        , proto(std::make_unique<ProtocolConnection>(myTip, std::move(cb)))
    {}

    // Not copyable
    PeerConn(const PeerConn&) = delete;
    PeerConn& operator=(const PeerConn&) = delete;
};

// ── NIOServer ────────────────────────────────────────────────────────────────

class NIOServer {
public:
    // Callbacks called from peer threads — must be thread-safe
    using OnEvent = std::function<void(int peerId, NetworkEvent)>;
    using OnLog   = std::function<void(const std::string&)>;

    explicit NIOServer(uint16_t listenPort = 9001,
                       OnEvent  onEvent = nullptr,
                       OnLog    onLog   = nullptr);

    ~NIOServer();

    // Start accepting connections
    bool start();

    // Connect to a remote peer (outgoing)
    bool connect(const std::string& host, uint16_t port);

    // Stop everything
    void stop();

    // Update tip advertised in future greetings
    void setMyTip(const MiniNumber& tip);

    // Send to all READY peers
    void broadcast(const NIOMsg& msg);

    int peerCount() const;
    bool isRunning() const { return m_running.load(); }

    uint16_t listenPort() const { return m_listenPort; }

private:
    uint16_t           m_listenPort;
    int                m_serverFd { -1 };
    std::atomic<bool>  m_running  { false };
    std::thread        m_acceptThread;

    mutable std::mutex              m_peersMu;
    std::unordered_map<int, std::unique_ptr<PeerConn>> m_peers;
    int                             m_nextId { 0 };

    std::atomic<int64_t>  m_myTip { -1 };
    OnEvent               m_onEvent;
    OnLog                 m_onLog;

    void _acceptLoop();
    void _connLoop(int peerId);
    int  _addPeer(int fd, const std::string& host, uint16_t port, bool inbound);
    void _removePeer(int peerId);
    void _log(const std::string& msg);

    static void _setNonBlocking(int fd);
    static bool _recvAll(int fd, std::vector<uint8_t>& buf, int timeoutMs = 100);
};

// ── Implementation ────────────────────────────────────────────────────────────

inline NIOServer::NIOServer(uint16_t listenPort, OnEvent onEvent, OnLog onLog)
    : m_listenPort(listenPort)
    , m_onEvent(std::move(onEvent))
    , m_onLog(std::move(onLog))
{}

inline NIOServer::~NIOServer() { stop(); }

inline void NIOServer::_log(const std::string& msg) {
    if (m_onLog) m_onLog(msg);
    else         fprintf(stderr, "[NIO] %s\n", msg.c_str());
}

inline void NIOServer::_setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

inline bool NIOServer::start() {
    m_serverFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverFd < 0) {
        _log("socket() failed");
        return false;
    }
    int opt = 1;
    setsockopt(m_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(m_listenPort);

    if (::bind(m_serverFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        _log("bind() failed on port " + std::to_string(m_listenPort));
        ::close(m_serverFd);
        m_serverFd = -1;
        return false;
    }
    ::listen(m_serverFd, 16);

    m_running = true;
    m_acceptThread = std::thread([this]{ _acceptLoop(); });
    _log("Listening on port " + std::to_string(m_listenPort));
    return true;
}

inline void NIOServer::stop() {
    if (!m_running.exchange(false)) return;

    if (m_serverFd >= 0) {
        ::shutdown(m_serverFd, SHUT_RDWR);
        ::close(m_serverFd);
        m_serverFd = -1;
    }
    if (m_acceptThread.joinable()) m_acceptThread.join();

    std::lock_guard<std::mutex> lk(m_peersMu);
    for (auto& [id, peer] : m_peers) {
        peer->running = false;
        if (peer->fd >= 0) {
            ::shutdown(peer->fd, SHUT_RDWR);
            ::close(peer->fd);
            peer->fd = -1;
        }
        if (peer->thread.joinable()) peer->thread.join();
    }
    m_peers.clear();
    _log("Stopped.");
}

inline void NIOServer::setMyTip(const MiniNumber& tip) {
    m_myTip = tip.getAsLong();
}

inline int NIOServer::peerCount() const {
    std::lock_guard<std::mutex> lk(m_peersMu);
    return (int)m_peers.size();
}

inline void NIOServer::broadcast(const NIOMsg& msg) {
    auto encoded = msg.encode();
    std::lock_guard<std::mutex> lk(m_peersMu);
    for (auto& [id, peer] : m_peers) {
        if (peer->proto && peer->proto->isReady()) {
            peer->proto->outBuffer.insert(
                peer->proto->outBuffer.end(),
                encoded.begin(), encoded.end());
        }
    }
}

inline bool NIOServer::connect(const std::string& host, uint16_t port) {
    addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0) {
        _log("getaddrinfo failed for " + host);
        return false;
    }

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { freeaddrinfo(res); return false; }

    // 5s connect timeout via non-blocking + select
    _setNonBlocking(fd);
    int r = ::connect(fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    if (r < 0 && errno != EINPROGRESS) {
        ::close(fd);
        _log("connect() failed to " + host + ":" + portStr);
        return false;
    }

    // Wait for connection (up to 5s)
    fd_set wfds; FD_ZERO(&wfds); FD_SET(fd, &wfds);
    timeval tv{ 5, 0 };
    if (select(fd + 1, nullptr, &wfds, nullptr, &tv) <= 0) {
        ::close(fd);
        _log("connect() timeout to " + host + ":" + portStr);
        return false;
    }
    int err = 0; socklen_t len = sizeof(err);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
    if (err != 0) { ::close(fd); return false; }

    // Back to blocking
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    _log("Connected to " + host + ":" + portStr);
    _addPeer(fd, host, port, false);
    return true;
}

inline void NIOServer::_acceptLoop() {
    while (m_running) {
        sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);

        // accept with timeout (use select to allow clean shutdown)
        fd_set rfds; FD_ZERO(&rfds); FD_SET(m_serverFd, &rfds);
        timeval tv{ 0, 200000 };  // 200ms
        if (select(m_serverFd + 1, &rfds, nullptr, nullptr, &tv) <= 0) continue;

        int clientFd = ::accept(m_serverFd, (sockaddr*)&clientAddr, &addrLen);
        if (clientFd < 0) {
            if (m_running) _log("accept() failed");
            break;
        }

        char hostBuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, hostBuf, sizeof(hostBuf));
        uint16_t clientPort = ntohs(clientAddr.sin_port);
        _log("Inbound connection from " + std::string(hostBuf) + ":" +
             std::to_string(clientPort));

        _addPeer(clientFd, hostBuf, clientPort, true);
    }
}

inline int NIOServer::_addPeer(int fd, const std::string& host,
                                uint16_t port, bool inbound) {
    std::lock_guard<std::mutex> lk(m_peersMu);
    int id = m_nextId++;
    MiniNumber tip(m_myTip.load());

    auto eventCb = [this, id](NetworkEvent ev) {
        if (m_onEvent) m_onEvent(id, std::move(ev));
    };

    auto peer = std::make_unique<PeerConn>(fd, host, port, inbound, tip, eventCb);
    peer->running = true;

    m_peers[id] = std::move(peer);

    // Launch connection thread (capture id, raw pointer safe because we own it)
    m_peers[id]->thread = std::thread([this, id] { _connLoop(id); });
    return id;
}

inline void NIOServer::_connLoop(int peerId) {
    PeerConn* peer = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_peersMu);
        auto it = m_peers.find(peerId);
        if (it == m_peers.end()) return;
        peer = it->second.get();
    }

    auto& conn = *peer->proto;

    // Flush initial greeting
    if (!conn.outBuffer.empty()) {
        auto b = conn.outBuffer; conn.outBuffer.clear();
        send(peer->fd, b.data(), b.size(), MSG_NOSIGNAL);
    }

    char buf[65536];
    while (peer->running && m_running) {
        // Read with 200ms timeout
        fd_set rfds; FD_ZERO(&rfds); FD_SET(peer->fd, &rfds);
        timeval tv{ 0, 200000 };
        int sel = select(peer->fd + 1, &rfds, nullptr, nullptr, &tv);

        if (sel < 0) { peer->running = false; break; }

        if (sel > 0) {
            ssize_t n = recv(peer->fd, buf, sizeof(buf), 0);
            if (n <= 0) { peer->running = false; break; }
            conn.receive(std::vector<uint8_t>(buf, buf + n));
        }

        // Flush outgoing
        if (!conn.outBuffer.empty()) {
            std::vector<uint8_t> out = std::move(conn.outBuffer);
            conn.outBuffer.clear();
            size_t sent = 0;
            while (sent < out.size()) {
                ssize_t n = send(peer->fd, out.data() + sent,
                                 out.size() - sent, MSG_NOSIGNAL);
                if (n <= 0) { peer->running = false; break; }
                sent += (size_t)n;
            }
        }

        if (conn.isClosed()) { peer->running = false; break; }
    }

    ::close(peer->fd);
    peer->fd = -1;
    _log("Peer " + peer->host + ":" + std::to_string(peer->port) + " disconnected");
    _removePeer(peerId);
}

inline void NIOServer::_removePeer(int peerId) {
    std::lock_guard<std::mutex> lk(m_peersMu);
    m_peers.erase(peerId);
}

} // namespace minima::network
