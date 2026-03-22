#pragma once
/**
 * TxPoWMiner — minimal TxPoW miner for Minima.
 * Java ref: src/org/minima/system/txpow/TxPoWMiner.java
 *
 * Mining algorithm:
 *   1. Set nonce = 0
 *   2. powHash = SHA2-256(serialised TxHeader)
 *   3. If powHash < blockDifficulty → found! 
 *   4. Else: nonce++ and repeat
 *
 * blockDifficulty: 32-byte big-endian; lower = harder.
 * For transactions: use txnDifficulty from TxBody.
 */
#include "../objects/TxPoW.hpp"
#include "../types/MiniData.hpp"
#include "../types/MiniNumber.hpp"
#include "../crypto/Hash.hpp"
#include <cstdint>
#include <atomic>
#include <vector>

namespace minima {
namespace mining {

// Compare two 32-byte big-endian values: return true if a < b
inline bool lessThan(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    size_t len = std::min(a.size(), b.size());
    for (size_t i = 0; i < len; ++i) {
        if (a[i] < b[i]) return true;
        if (a[i] > b[i]) return false;
    }
    return false;
}

/**
 * Mine a block TxPoW (blockDifficulty).
 * @param txpow     Modified in place (nonce updated)
 * @param maxIter   0 = unlimited
 * @param stop      Optional cancel flag
 * @return true if valid nonce found
 */
inline bool mineBlock(TxPoW& txpow,
                      uint64_t maxIter = 0,
                      const std::atomic<bool>* stop = nullptr)
{
    const std::vector<uint8_t>& diff = txpow.header().blockDifficulty.bytes();

    for (uint64_t n = 0; ; ++n) {
        txpow.header().nonce = MiniNumber(static_cast<int64_t>(n));

        // PoW = SHA2-256(TxHeader serialised bytes)
        auto headerBytes = txpow.header().serialise();
        auto powHashData = crypto::Hash::sha2_256(headerBytes);
        const auto& powHash = powHashData.bytes();

        if (lessThan(powHash, diff)) return true;

        if (maxIter > 0 && n >= maxIter - 1) return false;
        if (stop && (n & 0x3FF) == 0 && stop->load()) return false;
    }
}

/**
 * Mine a transaction TxPoW (txnDifficulty).
 */
inline bool mineTxn(TxPoW& txpow,
                    uint64_t maxIter = 0,
                    const std::atomic<bool>* stop = nullptr)
{
    const std::vector<uint8_t>& diff = txpow.body().txnDifficulty.bytes();

    for (uint64_t n = 0; ; ++n) {
        txpow.header().nonce = MiniNumber(static_cast<int64_t>(n));
        auto headerBytes = txpow.header().serialise();
        auto powHashData = crypto::Hash::sha2_256(headerBytes);
        const auto& powHash = powHashData.bytes();

        if (lessThan(powHash, diff)) return true;
        if (maxIter > 0 && n >= maxIter - 1) return false;
        if (stop && (n & 0x3FF) == 0 && stop->load()) return false;
    }
}

/**
 * Build a simple difficulty target.
 * leadingZeroBytes = 0 → easiest (any hash valid), 31 → hardest
 */
inline MiniData makeDifficulty(int leadingZeroBytes) {
    std::vector<uint8_t> d(32, 0xFF);
    for (int i = 0; i < leadingZeroBytes && i < 32; ++i) d[i] = 0x00;
    return MiniData(d);
}

} // namespace mining
} // namespace minima
