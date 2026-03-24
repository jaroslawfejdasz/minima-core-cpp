#pragma once
/**
 * TreeKey.hpp — Merkle tree over WOTS leaf keys (quantum-resistant key scheme)
 *
 * Java reference: src/org/minima/objects/keys/TreeKey.java
 *                 src/org/minima/objects/keys/MultiKey.java
 *
 * Structure:
 *   depth = 8  →  2^8 = 256 leaf positions
 *   Each leaf is a WOTS key pair derived from: seed_i = SHA3(master_seed || i)
 *   Merkle tree root = SHA3 of pairwise SHA3 up the tree
 *
 * Public key  : 32-byte Merkle root (serialised as MiniData)
 * Private key : 32-byte master seed (kept secret; leaf keys are derived on demand)
 *
 * Signature (per-use):
 *   [4-byte leaf index][2144-byte WOTS sig][8 × 32-byte Merkle auth path]
 *   Total: 4 + 2144 + 256 = 2404 bytes
 *
 * Usage model (one-time-per-leaf, like Minima Java):
 *   - keyUse counter tracks next leaf index
 *   - Once all 256 leaves are used, the key is exhausted (need new TreeKey)
 *
 * Security property:
 *   - Each leaf WOTS key is used ONCE (hence "one-time signature")
 *   - Reusing a leaf breaks WOTS security
 *   - TreeKey adds stateful leaf selection + Merkle inclusion proof
 */

#include "WOTS.hpp"
#include "Hash.hpp"
#include "../types/MiniData.hpp"
#include <vector>
#include <array>
#include <cstdint>
#include <stdexcept>

namespace minima::crypto {

// ── Constants ─────────────────────────────────────────────────────────────────

static constexpr int TREEKEY_DEPTH      = 8;
static constexpr int TREEKEY_LEAVES     = 1 << TREEKEY_DEPTH;  // 256
static constexpr int TREEKEY_SIG_LEAF_BYTES  = 4;              // leaf index
static constexpr int TREEKEY_SIG_AUTH_BYTES  = TREEKEY_DEPTH * WOTS_HASH_LEN; // 256
static constexpr int TREEKEY_SIG_TOTAL  =
    TREEKEY_SIG_LEAF_BYTES + WOTS_SIG_LEN + TREEKEY_SIG_AUTH_BYTES; // 2404

// ── Merkle helpers ────────────────────────────────────────────────────────────

/**
 * Compute SHA3(left || right) for a Merkle parent node.
 */
inline std::array<uint8_t, WOTS_HASH_LEN>
merkle_parent(const uint8_t* left, const uint8_t* right) {
    std::vector<uint8_t> input(left, left + WOTS_HASH_LEN);
    input.insert(input.end(), right, right + WOTS_HASH_LEN);
    auto h = Hash::sha3_256(input);
    std::array<uint8_t, WOTS_HASH_LEN> out;
    std::copy(h.bytes().begin(), h.bytes().end(), out.begin());
    return out;
}

// ── TreeKey ───────────────────────────────────────────────────────────────────

class TreeKey {
public:
    // ── Generation ───────────────────────────────────────────────────────────

    /**
     * Generate a fresh TreeKey from a 32-byte master seed.
     *
     * Precomputes the full Merkle tree over all 256 WOTS leaf public keys.
     * O(256 × 67 × 15) SHA3 calls — ~255k hashes — takes ~10ms on modern HW.
     */
    static TreeKey generate(const MiniData& masterSeed) {
        if (masterSeed.size() != 32)
            throw std::invalid_argument("TreeKey master seed must be 32 bytes");

        TreeKey tk;
        tk.m_seed   = masterSeed;
        tk.m_keyUse = 0;

        // Derive 256 WOTS public keys
        std::vector<std::array<uint8_t, WOTS_HASH_LEN>> leaves(TREEKEY_LEAVES);
        for (int i = 0; i < TREEKEY_LEAVES; i++) {
            MiniData leafSeed = tk._leafSeed(i);
            auto kp = WOTS::generateKeyPair(leafSeed);
            const auto& pb = kp.publicKey.bytes();
            std::copy(pb.begin(), pb.end(), leaves[i].begin());
        }

        // Build Merkle tree bottom-up
        // tree[0 .. TREEKEY_LEAVES-1] = leaf hashes
        // tree[TREEKEY_LEAVES .. 2*TREEKEY_LEAVES-2] = internal nodes
        // root = tree[TREEKEY_LEAVES - 1]  (standard 1-indexed: root at index 1)
        //
        // We use a flat array of size 2 * TREEKEY_LEAVES:
        //   index 1 = root
        //   index 2,3 = level 1 nodes
        //   index TREEKEY_LEAVES .. 2*TREEKEY_LEAVES-1 = leaves
        tk.m_tree.resize(2 * TREEKEY_LEAVES);
        for (int i = 0; i < TREEKEY_LEAVES; i++)
            tk.m_tree[TREEKEY_LEAVES + i] = leaves[i];

        for (int i = TREEKEY_LEAVES - 1; i >= 1; i--) {
            tk.m_tree[i] = merkle_parent(
                tk.m_tree[2*i].data(),
                tk.m_tree[2*i+1].data());
        }

        tk.m_root = MiniData(std::vector<uint8_t>(
            tk.m_tree[1].begin(), tk.m_tree[1].end()));

        return tk;
    }

    // ── Key access ────────────────────────────────────────────────────────────

    /** The 32-byte Merkle root = TreeKey public key */
    const MiniData& publicKey() const { return m_root; }

    /** Current leaf index (how many signatures have been used) */
    int keyUse() const { return m_keyUse; }

    /** True if all leaves have been used */
    bool isExhausted() const { return m_keyUse >= TREEKEY_LEAVES; }

    // ── Signing ───────────────────────────────────────────────────────────────

    /**
     * Sign a 32-byte message using the next available leaf.
     *
     * Signature layout:
     *   [0..3]    : leaf index (big-endian uint32)
     *   [4..2147] : WOTS signature (2144 bytes)
     *   [2148..2403] : Merkle auth path (8 × 32 bytes, from leaf to root)
     *
     * Increments keyUse counter after signing.
     * Throws if key is exhausted.
     */
    MiniData sign(const MiniData& msg) {
        if (isExhausted())
            throw std::runtime_error("TreeKey exhausted: all 256 leaves used");

        int leafIdx = m_keyUse++;
        return signAt(msg, leafIdx);
    }

    /**
     * Sign at a specific leaf index (does NOT increment keyUse).
     * Used internally and for testing.
     */
    MiniData signAt(const MiniData& msg, int leafIdx) const {
        if (leafIdx < 0 || leafIdx >= TREEKEY_LEAVES)
            throw std::invalid_argument("leaf index out of range");

        MiniData leafSeed = _leafSeed(leafIdx);
        auto kp = WOTS::generateKeyPair(leafSeed);
        MiniData wotsSig = WOTS::sign(kp.privateKey, msg);

        // Build auth path (8 nodes, from leaf level up to root)
        std::vector<uint8_t> authPath;
        authPath.reserve(TREEKEY_SIG_AUTH_BYTES);

        int pos = TREEKEY_LEAVES + leafIdx;
        for (int level = 0; level < TREEKEY_DEPTH; level++) {
            int sibling = (pos % 2 == 0) ? pos + 1 : pos - 1;
            const auto& sib = m_tree[sibling];
            authPath.insert(authPath.end(), sib.begin(), sib.end());
            pos /= 2;
        }

        // Assemble signature
        std::vector<uint8_t> sig;
        sig.reserve(TREEKEY_SIG_TOTAL);

        // leaf index (4 bytes, big-endian)
        sig.push_back((leafIdx >> 24) & 0xFF);
        sig.push_back((leafIdx >> 16) & 0xFF);
        sig.push_back((leafIdx >>  8) & 0xFF);
        sig.push_back( leafIdx        & 0xFF);

        // WOTS sig
        const auto& wb = wotsSig.bytes();
        sig.insert(sig.end(), wb.begin(), wb.end());

        // auth path
        sig.insert(sig.end(), authPath.begin(), authPath.end());

        return MiniData(sig);
    }

    // ── Verification (static) ─────────────────────────────────────────────────

    /**
     * Verify a TreeKey signature.
     *
     * @param publicKey  32-byte Merkle root
     * @param msg        32-byte message that was signed
     * @param signature  2404-byte signature
     */
    static bool verify(const MiniData& publicKey,
                       const MiniData& msg,
                       const MiniData& signature) {
        if (signature.size() != (size_t)TREEKEY_SIG_TOTAL) return false;
        if (publicKey.size() != WOTS_HASH_LEN)              return false;

        const auto& sb = signature.bytes();

        // Extract leaf index
        uint32_t leafIdx = ((uint32_t)sb[0] << 24)
                         | ((uint32_t)sb[1] << 16)
                         | ((uint32_t)sb[2] <<  8)
                         |  (uint32_t)sb[3];
        if (leafIdx >= (uint32_t)TREEKEY_LEAVES) return false;

        // Extract WOTS signature (bytes 4..2147)
        MiniData wotsSig(std::vector<uint8_t>(
            sb.begin() + 4,
            sb.begin() + 4 + WOTS_SIG_LEN));

        // Extract auth path (bytes 2148..2403)
        const uint8_t* authPtr = sb.data() + 4 + WOTS_SIG_LEN;

        // Recover WOTS public key from signature
        // We need to compute candidate_wots_pubkey from wotsSig + msg
        // WOTS::verify computes SHA3(concat of hash_chain(sig[i], 15-d[i]))
        // We need the intermediate public key (leaf node), not just boolean.
        // So we inline the WOTS candidate computation:
        auto digits = msg_to_digits(msg);
        const auto& ws = wotsSig.bytes();

        std::vector<uint8_t> pubConcat;
        pubConcat.reserve(WOTS_TOTAL_DIGITS * WOTS_HASH_LEN);
        for (int i = 0; i < WOTS_TOTAL_DIGITS; i++) {
            int remaining = (WOTS_W - 1) - digits[i];
            auto top = hash_chain(ws.data() + i * WOTS_HASH_LEN, remaining);
            pubConcat.insert(pubConcat.end(), top.begin(), top.end());
        }
        MiniData leafPubKey = Hash::sha3_256(pubConcat);

        // Walk the auth path to reconstruct the Merkle root
        std::array<uint8_t, WOTS_HASH_LEN> node;
        std::copy(leafPubKey.bytes().begin(), leafPubKey.bytes().end(), node.begin());

        uint32_t pos = leafIdx;
        for (int level = 0; level < TREEKEY_DEPTH; level++) {
            const uint8_t* sibling = authPtr + level * WOTS_HASH_LEN;
            if (pos % 2 == 0) {
                // node is left child
                node = merkle_parent(node.data(), sibling);
            } else {
                // node is right child
                node = merkle_parent(sibling, node.data());
            }
            pos /= 2;
        }

        // Compare reconstructed root with public key
        return std::equal(node.begin(), node.end(), publicKey.bytes().begin());
    }

    // ── Serialisation ─────────────────────────────────────────────────────────

    /**
     * Serialise TreeKey for persistence.
     * Format: [32-byte seed][4-byte keyUse]
     * (The full tree is re-derived on deserialise — not stored)
     */
    std::vector<uint8_t> serialise() const {
        std::vector<uint8_t> out(m_seed.bytes());
        out.push_back((m_keyUse >> 24) & 0xFF);
        out.push_back((m_keyUse >> 16) & 0xFF);
        out.push_back((m_keyUse >>  8) & 0xFF);
        out.push_back( m_keyUse        & 0xFF);
        return out;
    }

    static TreeKey deserialise(const std::vector<uint8_t>& data) {
        if (data.size() < 36)
            throw std::invalid_argument("TreeKey serialised data too short");
        MiniData seed(std::vector<uint8_t>(data.begin(), data.begin() + 32));
        uint32_t use = ((uint32_t)data[32] << 24)
                     | ((uint32_t)data[33] << 16)
                     | ((uint32_t)data[34] <<  8)
                     |  (uint32_t)data[35];
        TreeKey tk = generate(seed);
        tk.m_keyUse = (int)use;
        return tk;
    }

private:
    MiniData  m_seed;
    MiniData  m_root;
    int       m_keyUse { 0 };

    // Flat Merkle tree: index 1=root, 2-3=level1, ..., 256-511=leaves
    std::vector<std::array<uint8_t, WOTS_HASH_LEN>> m_tree;

    // Derive deterministic leaf seed from master seed + index
    MiniData _leafSeed(int i) const {
        std::vector<uint8_t> input(m_seed.bytes());
        input.push_back((i >> 24) & 0xFF);
        input.push_back((i >> 16) & 0xFF);
        input.push_back((i >>  8) & 0xFF);
        input.push_back( i        & 0xFF);
        return Hash::sha3_256(input);
    }
};

} // namespace minima::crypto
