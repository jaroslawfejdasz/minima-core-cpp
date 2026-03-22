#pragma once
/**
 * TreeKey — Merkle tree of Winternitz OTS keys
 * Java-exact port of org.minima.objects.keys.TreeKey
 * 
 * A TreeKey has:
 *   - depth D leaves (keys)
 *   - each leaf = a WOTS keypair (derived from master seed + leaf index)
 *   - root = Merkle root of all leaf public keys (SHA3-256 tree)
 *   - sign(data): picks the next unused leaf, signs, produces SignatureProof
 *   - verify: checks WOTS sig + Merkle proof
 * 
 * Default: depth=12 → 4096 one-time signatures per TreeKey
 */

#include "Winternitz.hpp"
#include "Hash.hpp"
#include "../types/MiniData.hpp"
#include <cstdint>
#include <vector>
#include <array>
#include <cstring>
#include <stdexcept>

namespace minima::crypto {

// ── Merkle authentication path ────────────────────────────────────────────
struct MerklePath {
    std::vector<MiniData> nodes;  // sibling hashes from leaf to root

    bool empty() const { return nodes.empty(); }
};

// ── Result of a single WOTS sign + Merkle proof ──────────────────────────
struct SignatureProof {
    MiniData rootPublicKey;   // 32-byte Merkle root
    int      leafIndex{0};    // which leaf was used
    MiniData publicKey;       // leaf WOTS public key (2880 bytes)
    MiniData signature;       // WOTS signature (2880 bytes)
    MerklePath proof;         // Merkle auth path

    bool isValid() const {
        return !rootPublicKey.empty() &&
               !publicKey.empty() &&
               !signature.empty();
    }
};

// ── TreeKey ───────────────────────────────────────────────────────────────
class TreeKey {
public:
    static constexpr int DEFAULT_DEPTH = 12;  // 2^12 = 4096 keys

    // ── Construction ─────────────────────────────────────────────────────

    TreeKey() = default;

    /**
     * Create a TreeKey from a 32-byte master seed.
     * Derives leaf seeds as SHA3-256(masterSeed || leaf_index_4B).
     */
    explicit TreeKey(const MiniData& masterSeed, int depth = DEFAULT_DEPTH)
        : mDepth(depth), mMasterSeed(masterSeed), mKeyUsed(0) {
        buildMerkleTree();
    }

    // ── Public API ───────────────────────────────────────────────────────

    /** The Merkle root = public identity of this TreeKey */
    MiniData getPublicKey() const { return mRoot; }

    int depth()    const { return mDepth; }
    int keyUsed()  const { return mKeyUsed; }
    int keysLeft() const { return (1 << mDepth) - mKeyUsed; }

    bool canSign()  const { return keysLeft() > 0; }

    /**
     * Sign data with the next available leaf.
     * Returns a SignatureProof containing:
     *   - WOTS signature
     *   - leaf public key
     *   - Merkle authentication path
     *   - the root public key
     */
    SignatureProof sign(const MiniData& data) {
        if (!canSign()) throw std::runtime_error("TreeKey exhausted");

        int leaf = mKeyUsed++;

        // Derive leaf seed
        auto leafSeed = deriveLeafSeed(mMasterSeed, leaf);

        // WOTS sign
        Winternitz::SigData wotsig =
            Winternitz::sign(leafSeed, data.bytes());

        Winternitz::PubKey wotpub =
            Winternitz::generatePublicKey(leafSeed);

        // Build Merkle auth path
        MerklePath path = buildAuthPath(leaf);

        SignatureProof sp;
        sp.rootPublicKey = mRoot;
        sp.leafIndex     = leaf;
        sp.publicKey     = MiniData(wotpub);
        sp.signature     = MiniData(wotsig);
        sp.proof         = path;
        return sp;
    }

    // ── Verification (static) ────────────────────────────────────────────

    /**
     * Verify a SignatureProof against a root public key.
     * 1. Verify WOTS sig → recovers leaf pubkey
     * 2. Verify Merkle path from leaf pubkey to root
     */
    static bool verify(const MiniData& rootPubKey,
                       const MiniData& data,
                       const SignatureProof& proof) {
        // Step 1: WOTS verify
        bool wotsOk = Winternitz::verify(
            proof.publicKey.bytes(),
            data.bytes(),
            proof.signature.bytes());
        if (!wotsOk) return false;

        // Step 2: Merkle path verify
        // Start from hash of leaf pubkey
        auto current = Hash::sha3_256(
            proof.publicKey.bytes().data(),
            proof.publicKey.bytes().size());

        int idx = proof.leafIndex;
        for (const auto& sibling : proof.proof.nodes) {
            std::vector<uint8_t> combined;
            if (idx & 1) {
                // current is right child
                const auto& sb = sibling.bytes();
                combined.insert(combined.end(), sb.begin(), sb.end());
                combined.insert(combined.end(), current.begin(), current.end());
            } else {
                // current is left child
                combined.insert(combined.end(), current.begin(), current.end());
                const auto& sb = sibling.bytes();
                combined.insert(combined.end(), sb.begin(), sb.end());
            }
            current = Hash::sha3_256(combined.data(), combined.size());
            idx >>= 1;
        }

        // current should equal root
        std::vector<uint8_t> rootBytes = rootPubKey.bytes();
        return std::vector<uint8_t>(current.begin(), current.end()) == rootBytes;
    }

private:
    int      mDepth{DEFAULT_DEPTH};
    MiniData mMasterSeed;
    int      mKeyUsed{0};
    MiniData mRoot;

    // Flattened Merkle tree: index 0 = root, leaves at [numLeaves..2*numLeaves-1]
    // tree[i] = SHA3-256(32 bytes)
    using Hash32 = std::array<uint8_t, 32>;
    std::vector<Hash32> mTree;

    void buildMerkleTree() {
        int numLeaves = 1 << mDepth;
        mTree.resize(2 * numLeaves);

        // Fill leaves: hash of each WOTS pubkey
        for (int i = 0; i < numLeaves; ++i) {
            auto seed = deriveLeafSeed(mMasterSeed, i);
            auto pub  = Winternitz::generatePublicKey(seed);
            mTree[numLeaves + i] =
                Hash::sha3_256(pub.data(), pub.size());
        }

        // Build internal nodes bottom-up
        for (int i = numLeaves - 1; i >= 1; --i) {
            std::vector<uint8_t> combined;
            combined.insert(combined.end(),
                mTree[2*i].begin(),   mTree[2*i].end());
            combined.insert(combined.end(),
                mTree[2*i+1].begin(), mTree[2*i+1].end());
            mTree[i] = Hash::sha3_256(combined.data(), combined.size());
        }

        mRoot = MiniData(
            std::vector<uint8_t>(mTree[1].begin(), mTree[1].end()));
    }

    MerklePath buildAuthPath(int leaf) const {
        int numLeaves = 1 << mDepth;
        int idx = numLeaves + leaf;
        MerklePath path;
        while (idx > 1) {
            int sibling = (idx & 1) ? idx - 1 : idx + 1;
            path.nodes.emplace_back(
                std::vector<uint8_t>(mTree[sibling].begin(), mTree[sibling].end()));
            idx >>= 1;
        }
        return path;
    }

    static std::vector<uint8_t> deriveLeafSeed(const MiniData& master, int index) {
        const auto& mb = master.bytes();
        std::vector<uint8_t> data;
        data.reserve(mb.size() + 4);
        data.insert(data.end(), mb.begin(), mb.end());
        data.push_back(static_cast<uint8_t>((index >> 24) & 0xFF));
        data.push_back(static_cast<uint8_t>((index >> 16) & 0xFF));
        data.push_back(static_cast<uint8_t>((index >>  8) & 0xFF));
        data.push_back(static_cast<uint8_t>( index        & 0xFF));
        auto h = Hash::sha3_256(data.data(), data.size());
        return std::vector<uint8_t>(h.begin(), h.end());
    }
};

} // namespace minima::crypto
