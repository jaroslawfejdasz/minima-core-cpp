#pragma once
/**
 * Node — top-level Minima C++ experimental node.
 *
 * Wires together:
 *   - NIOServer      (P2P TCP, inbound + outbound connections)
 *   - ChainProcessor (block validation, UTxO state, mempool)
 *
 * Lifecycle: Node::start() → signal → Node::stop()
 *
 * Java ref: Main.java + TxPoWProcessor.java + NIOManager.java
 */
#include "../chain/ChainProcessor.hpp"
#include "../network/NIOServer.hpp"
#include "../objects/IBD.hpp"
#include "../types/MiniNumber.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <memory>
#include <cstdio>

namespace minima {

struct NodeConfig {
    uint16_t    listenPort  { 9001 };
    std::string connectHost;           // peer to dial on startup (optional)
    uint16_t    connectPort { 9001 };
    bool        verbose     { true };
};

// ─────────────────────────────────────────────────────────────────────────────

class Node {
public:
    explicit Node(const NodeConfig& cfg = {});

    bool start();
    void stop();

    bool connectPeer(const std::string& host, uint16_t port);

    MiniNumber tipBlock() const {
        std::lock_guard<std::mutex> lk(m_mu);
        return m_tip;
    }
    int peerCount() const { return m_server ? m_server->peerCount() : 0; }

private:
    NodeConfig                          m_cfg;
    mutable std::mutex                  m_mu;
    chain::ChainProcessor               m_processor;  // owns BlockStore + UTxOSet + Mempool
    MiniNumber                          m_tip { int64_t(-1) };
    std::unique_ptr<network::NIOServer> m_server;
    std::atomic<bool>                   m_running { false };

    void _log(const std::string& msg);
    void _onEvent(int peerId, network::NetworkEvent ev);
    void _applyIBD(const IBD& ibd);
};

// ── Implementation ────────────────────────────────────────────────────────────

inline Node::Node(const NodeConfig& cfg) : m_cfg(cfg) {}

inline void Node::_log(const std::string& msg) {
    if (m_cfg.verbose) { printf("[Node] %s\n", msg.c_str()); fflush(stdout); }
}

inline bool Node::start() {
    _log("Starting Minima C++ node...");
    m_running = true;

    auto evCb  = [this](int id, network::NetworkEvent ev) { _onEvent(id, std::move(ev)); };
    auto logCb = [this](const std::string& m) { _log(m); };

    m_server = std::make_unique<network::NIOServer>(m_cfg.listenPort, evCb, logCb);
    m_server->setMyTip(m_tip);

    if (!m_server->start()) {
        _log("Failed to start server on port " + std::to_string(m_cfg.listenPort));
        return false;
    }

    if (!m_cfg.connectHost.empty()) {
        _log("Dialing " + m_cfg.connectHost + ":" + std::to_string(m_cfg.connectPort));
        m_server->connect(m_cfg.connectHost, m_cfg.connectPort);
    }

    _log("Ready.  tip=" + m_tip.toString() +
         "  port=" + std::to_string(m_cfg.listenPort));
    return true;
}

inline void Node::stop() {
    if (!m_running.exchange(false)) return;
    _log("Stopping...");
    if (m_server) m_server->stop();
    _log("Stopped.");
}

inline bool Node::connectPeer(const std::string& host, uint16_t port) {
    return m_server && m_server->connect(host, port);
}

inline void Node::_onEvent(int peerId, network::NetworkEvent ev) {
    using Kind = network::NetworkEvent::Kind;
    switch (ev.kind) {
        case Kind::CONNECTED:
            _log("Peer " + std::to_string(peerId) + " handshake complete");
            break;

        case Kind::GREETING_RECV:
            if (ev.greeting)
                _log("Peer " + std::to_string(peerId) +
                     "  remote_tip=" + ev.greeting->topBlock().toString() +
                     "  version="    + std::string(ev.greeting->version().str()));
            break;

        case Kind::IBD_RECV:
            if (ev.ibd) {
                _log("IBD from peer " + std::to_string(peerId) +
                     "  blocks=" + std::to_string(ev.ibd->txBlocks().size()));
                _applyIBD(*ev.ibd);
            }
            break;

        case Kind::TXPOW_RECV: {
            // detail contains "txpow received" — actual TxPoW is in the stream
            // (NetworkProtocol deserialises it but doesn't expose it yet;
            //  that's a future enhancement — for now log the event)
            _log("TxPoW received from peer " + std::to_string(peerId));
            break;
        }

        case Kind::TXPOWID_RECV:
            if (ev.txpowId)
                _log("TxPoW ID from peer " + std::to_string(peerId) +
                     ": " + ev.txpowId->toHexString(false).substr(0, 16) + "...");
            break;

        case Kind::PULSE_RECV:
            if (ev.blockHeight)
                _log("Pulse from peer " + std::to_string(peerId) +
                     "  height=" + ev.blockHeight->toString());
            break;

        case Kind::ERROR:
            _log("Error from peer " + std::to_string(peerId) + ": " + ev.detail);
            break;

        default: break;
    }
}

inline void Node::_applyIBD(const IBD& ibd) {
    std::lock_guard<std::mutex> lk(m_mu);
    int applied = 0;
    for (const auto& tb : ibd.txBlocks()) {
        if (m_processor.processBlock(tb.txpow())) {
            m_tip = tb.txpow().header().blockNumber;
            applied++;
        }
    }
    if (applied > 0) {
        _log("IBD applied: " + std::to_string(applied) +
             " blocks, new tip=" + m_tip.toString());
        m_server->setMyTip(m_tip);
    }
}

} // namespace minima
