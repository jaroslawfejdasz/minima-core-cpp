#pragma once
/**
 * WOTS.hpp — Winternitz One-Time Signature Scheme (Minima variant)
 *
 * Java reference: src/org/minima/utils/Crypto.java
 *                 src/org/minima/objects/keys/SingleKey.java
 *
 * Parameters (match Minima Java exactly):
 *   Hash        : SHA3-256 (32-byte output)
 *   w           : 16  (Winternitz parameter — each digit is a nibble, 0–15)
 *   msg_digits  : 64  (256 bits / 4 bits per digit)
 *   csum_digits : 3   (checksum nibbles: ceil(log16(64 * 15 + 1)) = 3)
 *   total_digits: 67
 *
 * Key sizes:
 *   Private key : 67 × 32 bytes = 2144 bytes
 *   Public key  : 32 bytes  (SHA3 of all 67 final chain values)
 *
 * Signature    : 67 × 32 bytes = 2144 bytes
 *
 * Signing for digit d (0-15):
 *   sig_chain[i] = hash_chain(privKey[i], d)      (apply SHA3 d times)
 *   verify:       hash_chain(sig_chain[i], 15 - d) == pubKey_chain[i]
 *
 * Public key derivation:
 *   pubKey_chain[i] = hash_chain(privKey[i], 15)
 *   pubKey = SHA3( concat(pubKey_chain[0..66]) )
 */

#include "../types/MiniData.hpp"
#include "Hash.hpp"
#include <array>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace minima::crypto {

// ── Constants ─────────────────────────────────────────────────────────────────

static constexpr int WOTS_W            = 16;    // Winternitz param (nibble alphabet)
static constexpr int WOTS_MSG_DIGITS   = 64;    // 32-byte msg → 64 nibbles
static constexpr int WOTS_CSUM_DIGITS  = 3;     // checksum nibbles
static constexpr int WOTS_TOTAL_DIGITS = WOTS_MSG_DIGITS + WOTS_CSUM_DIGITS;  // 67
static constexpr int WOTS_HASH_LEN     = 32;    // SHA3-256 output bytes
static constexpr int WOTS_PRIVKEY_LEN  = WOTS_TOTAL_DIGITS * WOTS_HASH_LEN;  // 2144
static constexpr int WOTS_SIG_LEN      = WOTS_TOTAL_DIGITS * WOTS_HASH_LEN;  // 2144

// ── WOTS helper functions ─────────────────────────────────────────────────────

/**
 * hash_chain(seed, steps) — apply SHA3-256 `steps` times.
 * steps=0 → return seed unchanged.
 */
inline std::array<uint8_t, WOTS_HASH_LEN>
hash_chain(const uint8_t* seed, int steps) {
    std::array<uint8_t, WOTS_HASH_LEN> cur;
    std::copy(seed, seed + WOTS_HASH_LEN, cur.begin());
    for (int i = 0; i < steps; i++) {
        auto h = Hash::sha3_256(std::vector<uint8_t>(cur.begin(), cur.end()));
        const auto& hb = h.bytes();
        std::copy(hb.begin(), hb.end(), cur.begin());
    }
    return cur;
}

/**
 * Expand 32-byte seed → 67 independent 32-byte chain seeds via SHA3.
 * chain_seed[i] = SHA3(seed || i)
 */
inline std::vector<std::array<uint8_t, WOTS_HASH_LEN>>
expand_seed(const MiniData& seed) {
    if (seed.size() != 32)
        throw std::invalid_argument("WOTS seed must be 32 bytes");

    std::vector<std::array<uint8_t, WOTS_HASH_LEN>> chains(WOTS_TOTAL_DIGITS);
    const auto& sb = seed.bytes();
    for (int i = 0; i < WOTS_TOTAL_DIGITS; i++) {
        // input = seed + 4-byte big-endian index
        std::vector<uint8_t> input(sb.begin(), sb.end());
        input.push_back((i >> 24) & 0xFF);
        input.push_back((i >> 16) & 0xFF);
        input.push_back((i >>  8) & 0xFF);
        input.push_back( i        & 0xFF);
        auto h = Hash::sha3_256(input);
        const auto& hb = h.bytes();
        std::copy(hb.begin(), hb.end(), chains[i].begin());
    }
    return chains;
}

// ── Message → digit array ─────────────────────────────────────────────────────

/**
 * Convert 32-byte message to 64 nibbles + 3 checksum nibbles = 67 digits.
 */
inline std::array<int, WOTS_TOTAL_DIGITS>
msg_to_digits(const MiniData& msg) {
    if (msg.size() != 32)
        throw std::invalid_argument("WOTS msg must be 32 bytes");

    std::array<int, WOTS_TOTAL_DIGITS> digits{};
    const auto& mb = msg.bytes();

    // 64 nibbles from message
    for (int i = 0; i < 32; i++) {
        digits[2*i]   = (mb[i] >> 4) & 0x0F;
        digits[2*i+1] =  mb[i]       & 0x0F;
    }

    // Checksum: sum of (15 - digit_i) for i in 0..63
    int csum = 0;
    for (int i = 0; i < WOTS_MSG_DIGITS; i++)
        csum += (WOTS_W - 1) - digits[i];

    // Encode checksum in WOTS_CSUM_DIGITS nibbles (big-endian)
    for (int i = WOTS_CSUM_DIGITS - 1; i >= 0; i--) {
        digits[WOTS_MSG_DIGITS + i] = csum & 0x0F;
        csum >>= 4;
    }

    return digits;
}

// ── WOTS Key pair ─────────────────────────────────────────────────────────────

struct WOTSKeyPair {
    MiniData privateKey;  // 2144 bytes (67 × 32)
    MiniData publicKey;   // 32 bytes (SHA3 of all 67 top-chain values)
};

// ── WOTS class ────────────────────────────────────────────────────────────────

class WOTS {
public:
    /**
     * Generate a WOTS key pair from a 32-byte seed.
     *
     * Private key: concatenation of 67 chain seeds (each 32 bytes).
     * Public key:  SHA3( concat of hash_chain(seed_i, 15) for i in 0..66 )
     */
    static WOTSKeyPair generateKeyPair(const MiniData& seed) {
        auto chains = expand_seed(seed);

        // Build private key blob
        std::vector<uint8_t> priv;
        priv.reserve(WOTS_PRIVKEY_LEN);

        // Build public key pre-image
        std::vector<uint8_t> pubConcat;
        pubConcat.reserve(WOTS_TOTAL_DIGITS * WOTS_HASH_LEN);

        for (int i = 0; i < WOTS_TOTAL_DIGITS; i++) {
            const auto& c = chains[i];
            priv.insert(priv.end(), c.begin(), c.end());

            auto top = hash_chain(c.data(), WOTS_W - 1);  // iterate 15 times
            pubConcat.insert(pubConcat.end(), top.begin(), top.end());
        }

        MiniData pubKey = Hash::sha3_256(pubConcat);
        return { MiniData(priv), pubKey };
    }

    /**
     * Sign a 32-byte message with a WOTS private key.
     *
     * For each digit d[i]:
     *   sig[i] = hash_chain(privKey[i], d[i])
     *
     * Returns 2144-byte signature (67 × 32).
     */
    static MiniData sign(const MiniData& privateKey, const MiniData& msg) {
        if (privateKey.size() != (size_t)WOTS_PRIVKEY_LEN)
            throw std::invalid_argument("WOTS private key must be 2144 bytes");

        auto digits = msg_to_digits(msg);
        const auto& pk = privateKey.bytes();

        std::vector<uint8_t> sig;
        sig.reserve(WOTS_SIG_LEN);

        for (int i = 0; i < WOTS_TOTAL_DIGITS; i++) {
            auto s = hash_chain(pk.data() + i * WOTS_HASH_LEN, digits[i]);
            sig.insert(sig.end(), s.begin(), s.end());
        }

        return MiniData(sig);
    }

    /**
     * Verify a WOTS signature against a public key and message.
     *
     * For each digit d[i]:
     *   candidate[i] = hash_chain(sig[i], 15 - d[i])
     * Check: SHA3(concat candidate) == publicKey
     */
    static bool verify(const MiniData& publicKey,
                       const MiniData& msg,
                       const MiniData& signature) {
        if (signature.size() != (size_t)WOTS_SIG_LEN) return false;
        if (publicKey.size() != WOTS_HASH_LEN)          return false;

        auto digits = msg_to_digits(msg);
        const auto& sb = signature.bytes();

        std::vector<uint8_t> pubConcat;
        pubConcat.reserve(WOTS_TOTAL_DIGITS * WOTS_HASH_LEN);

        for (int i = 0; i < WOTS_TOTAL_DIGITS; i++) {
            int remaining = (WOTS_W - 1) - digits[i];  // 15 - d
            auto top = hash_chain(sb.data() + i * WOTS_HASH_LEN, remaining);
            pubConcat.insert(pubConcat.end(), top.begin(), top.end());
        }

        MiniData candidate = Hash::sha3_256(pubConcat);
        return candidate == publicKey;
    }
};

} // namespace minima::crypto
