#pragma once
/**
 * MegaMMR — Global UTxO state as a single Merkle Mountain Range.
 * Port of com.minima.database.MegaMMR (Minima Java).
 *
 * Purpose: fast sync for new nodes — download current UTxO MMR state
 * and verify root matches chain-tip header.mmrRoot.
 *
 * Wire format (MEGAMMRSYNC_RESP):
 *   [uint32]  leaf_count
 *   for each leaf: [uint8 spent] [MiniData hash (serialised)]
 *   [uint32]  index_count
 *   for each index: [MiniData coinID] [uint64 LE entry]
 */
#include "MMRSet.hpp"
#include "MMRData.hpp"
#include "MMREntry.hpp"
#include "MMRProof.hpp"
#include "../types/MiniData.hpp"
#include "../types/MiniNumber.hpp"
#include "../crypto/Hash.hpp"
#include "../serialization/DataStream.hpp"
#include "../objects/Coin.hpp"
#include <unordered_map>
#include <vector>
#include <optional>
#include <functional>
#include <stdexcept>

namespace minima {

class MegaMMR {
public:
    MegaMMR() = default;

    // ── Coin → leaf hash ─────────────────────────────────────────────────
    /**
     * hash = SHA3_256(coinID || address || amount_string)
     * Matches Java MMRSet.getMMRData(Coin).
     */
    static MMRData coinToMMRData(const Coin& coin) {
        std::vector<uint8_t> buf;
        const auto& cid  = coin.coinID().bytes();
        const auto& addr = coin.address().hash().bytes();
        std::string amt  = coin.amount().toString();
        buf.insert(buf.end(), cid.begin(),  cid.end());
        buf.insert(buf.end(), addr.begin(), addr.end());
        buf.insert(buf.end(), amt.begin(),  amt.end());
        MiniData h = crypto::Hash::sha3_256(buf.data(), buf.size());
        return MMRData(h, coin.amount(), false);
    }

    // ── Mutation ─────────────────────────────────────────────────────────
    uint64_t addCoin(const Coin& coin) {
        MMRData  d   = coinToMMRData(coin);
        MMREntry e   = m_mmr.addLeaf(d);
        uint64_t idx = e.getEntry();
        m_idx[coin.coinID()] = idx;
        return idx;
    }

    bool spendCoin(const MiniData& coinID) {
        auto it = m_idx.find(coinID);
        if (it == m_idx.end()) return false;
        auto optE = m_mmr.getEntry(0, it->second);
        if (!optE) return false;
        MMRData d = optE->getData();
        d.setSpent(true);
        m_mmr.updateLeaf(it->second, d);
        return true;
    }

    void processBlock(const std::vector<MiniData>& spentIDs,
                      const std::vector<Coin>& newCoins) {
        for (const auto& id : spentIDs) spendCoin(id);
        for (const auto& c  : newCoins) addCoin(c);
    }

    // ── Query ─────────────────────────────────────────────────────────────
    MMRData  getRoot()    const { return m_mmr.getRoot(); }
    uint64_t leafCount()  const { return m_mmr.getLeafCount(); }

    bool hasCoin(const MiniData& id) const { return m_idx.count(id) > 0; }

    std::optional<uint64_t> getLeafEntry(const MiniData& id) const {
        auto it = m_idx.find(id);
        if (it == m_idx.end()) return std::nullopt;
        return it->second;
    }

    std::optional<MMRProof> getCoinProof(const MiniData& id) const {
        auto it = m_idx.find(id);
        if (it == m_idx.end()) return std::nullopt;
        return m_mmr.getProof(it->second);
    }

    bool verifyCoinProof(const Coin& coin, MMRProof proof) const {
        proof.setData(coinToMMRData(coin));
        return proof.verifyProof(m_mmr.getRoot());
    }

    void reset() { m_mmr = MMRSet(); m_idx.clear(); }

    // ── Wire ─────────────────────────────────────────────────────────────
    std::vector<uint8_t> serialise() const {
        DataStream ds;
        uint64_t n = m_mmr.getLeafCount();
        ds.writeUInt32(static_cast<uint32_t>(n));
        for (uint64_t i = 0; i < n; ++i) {
            auto optE = m_mmr.getEntry(0, i);
            if (optE) {
                ds.writeUInt8(optE->getData().isSpent() ? 1 : 0);
                ds.writeBytes(optE->getData().getData().serialise());
            } else {
                ds.writeUInt8(0);
                ds.writeBytes(MiniData(std::vector<uint8_t>(32, 0)).serialise());
            }
        }
        ds.writeUInt32(static_cast<uint32_t>(m_idx.size()));
        for (const auto& [cid, entry] : m_idx) {
            ds.writeBytes(cid.serialise());
            for (int b = 0; b < 8; ++b)
                ds.writeUInt8(static_cast<uint8_t>((entry >> (8*b)) & 0xFF));
        }
        return ds.buffer();
    }

    static MegaMMR deserialise(const uint8_t* data, size_t& offset) {
        MegaMMR mega;
        uint32_t n = readU32(data, offset);
        for (uint32_t i = 0; i < n; ++i) {
            bool spent = (data[offset++] != 0);
            MiniData hash = MiniData::deserialise(data, offset);
            MMRData d(hash); d.setSpent(spent);
            mega.m_mmr.addLeaf(d);
        }
        uint32_t ic = readU32(data, offset);
        for (uint32_t i = 0; i < ic; ++i) {
            MiniData cid = MiniData::deserialise(data, offset);
            uint64_t e = 0;
            for (int b = 0; b < 8; ++b) e |= (uint64_t(data[offset++]) << (8*b));
            mega.m_idx[cid] = e;
        }
        return mega;
    }

private:
    struct CIDHash {
        size_t operator()(const MiniData& d) const noexcept {
            size_t s = 0;
            for (uint8_t b : d.bytes())
                s ^= std::hash<uint8_t>{}(b) + 0x9e3779b9 + (s<<6) + (s>>2);
            return s;
        }
    };
    struct CIDEq {
        bool operator()(const MiniData& a, const MiniData& b) const noexcept {
            return a == b;
        }
    };

    static uint32_t readU32(const uint8_t* d, size_t& o) {
        // big-endian (matches DataStream::writeUInt32)
        uint32_t v = (uint32_t(d[o])<<24) | (uint32_t(d[o+1])<<16) |
                     (uint32_t(d[o+2])<< 8) |  uint32_t(d[o+3]);
        o += 4; return v;
    }

    MMRSet m_mmr;
    std::unordered_map<MiniData, uint64_t, CIDHash, CIDEq> m_idx;
};

} // namespace minima
