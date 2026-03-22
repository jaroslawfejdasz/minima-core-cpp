#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/crypto/TreeKey.hpp"
#include "../../src/types/MiniData.hpp"
#include <vector>
#include <cstring>

using namespace minima::crypto;

static MiniData makeSeed(uint8_t fill) {
    return MiniData(std::vector<uint8_t>(32, fill));
}

static MiniData makeMsg(uint8_t fill) {
    // 32-byte message (SHA3 hash of fill byte repeated)
    std::vector<uint8_t> data(32, fill);
    auto h = Hash::sha3_256(data.data(), data.size());
    return MiniData(std::vector<uint8_t>(h.begin(), h.end()));
}

// We use depth=4 (16 leaves) for speed in tests
static constexpr int TEST_DEPTH = 4;

TEST_CASE("TreeKey: construction produces non-empty root") {
    TreeKey tk(makeSeed(0x01), TEST_DEPTH);
    auto root = tk.getPublicKey();
    CHECK(root.bytes().size() == 32u);
    bool allZero = true;
    for (auto b : root.bytes()) if (b) { allZero = false; break; }
    CHECK_FALSE(allZero);
}

TEST_CASE("TreeKey: same seed produces same root") {
    TreeKey tk1(makeSeed(0x42), TEST_DEPTH);
    TreeKey tk2(makeSeed(0x42), TEST_DEPTH);
    CHECK(tk1.getPublicKey().bytes() == tk2.getPublicKey().bytes());
}

TEST_CASE("TreeKey: different seeds produce different roots") {
    TreeKey tk1(makeSeed(0x01), TEST_DEPTH);
    TreeKey tk2(makeSeed(0x02), TEST_DEPTH);
    CHECK(tk1.getPublicKey().bytes() != tk2.getPublicKey().bytes());
}

TEST_CASE("TreeKey: depth and key count") {
    TreeKey tk(makeSeed(0x55), TEST_DEPTH);
    CHECK(tk.depth()    == TEST_DEPTH);
    CHECK(tk.keyUsed()  == 0);
    CHECK(tk.keysLeft() == (1 << TEST_DEPTH));
    CHECK(tk.canSign());
}

TEST_CASE("TreeKey: sign produces valid SignatureProof") {
    TreeKey tk(makeSeed(0xAB), TEST_DEPTH);
    auto msg   = makeMsg(0xCD);
    auto proof = tk.sign(msg);

    CHECK(proof.isValid());
    CHECK(proof.leafIndex == 0);
    CHECK(proof.signature.bytes().size() ==
          static_cast<size_t>(Winternitz::SIG_SIZE));
    CHECK(proof.publicKey.bytes().size() ==
          static_cast<size_t>(Winternitz::PUBKEY_SIZE));
    CHECK_FALSE(proof.proof.empty());
    CHECK(proof.proof.nodes.size() == static_cast<size_t>(TEST_DEPTH));
}

TEST_CASE("TreeKey: sign advances key counter") {
    TreeKey tk(makeSeed(0x11), TEST_DEPTH);
    CHECK(tk.keyUsed() == 0);
    tk.sign(makeMsg(0x22));
    CHECK(tk.keyUsed() == 1);
    tk.sign(makeMsg(0x33));
    CHECK(tk.keyUsed() == 2);
}

TEST_CASE("TreeKey: verify succeeds for valid proof") {
    TreeKey tk(makeSeed(0xDE), TEST_DEPTH);
    auto root  = tk.getPublicKey();
    auto msg   = makeMsg(0xAD);
    auto proof = tk.sign(msg);

    CHECK(TreeKey::verify(root, msg, proof));
}

TEST_CASE("TreeKey: verify fails for wrong message") {
    TreeKey tk(makeSeed(0xBE), TEST_DEPTH);
    auto root   = tk.getPublicKey();
    auto msg1   = makeMsg(0x10);
    auto msg2   = makeMsg(0x20);
    auto proof  = tk.sign(msg1);

    CHECK(      TreeKey::verify(root, msg1, proof));
    CHECK_FALSE(TreeKey::verify(root, msg2, proof));
}

TEST_CASE("TreeKey: verify fails for tampered signature") {
    TreeKey tk(makeSeed(0xCA), TEST_DEPTH);
    auto root  = tk.getPublicKey();
    auto msg   = makeMsg(0xFE);
    auto proof = tk.sign(msg);

    // Tamper with the WOTS signature
    auto sigBytes = proof.signature.bytes();
    sigBytes[500] ^= 0xFF;
    proof.signature = MiniData(sigBytes);

    CHECK_FALSE(TreeKey::verify(root, msg, proof));
}

TEST_CASE("TreeKey: verify fails for wrong root") {
    TreeKey tk1(makeSeed(0x01), TEST_DEPTH);
    TreeKey tk2(makeSeed(0x02), TEST_DEPTH);
    auto msg   = makeMsg(0x77);
    auto proof = tk1.sign(msg);

    // Use tk2's root to verify tk1's proof
    CHECK_FALSE(TreeKey::verify(tk2.getPublicKey(), msg, proof));
}

TEST_CASE("TreeKey: sequential signs use different leaves") {
    TreeKey tk(makeSeed(0x88), TEST_DEPTH);
    auto msg1 = makeMsg(0x01);
    auto msg2 = makeMsg(0x02);

    auto p1 = tk.sign(msg1);
    auto p2 = tk.sign(msg2);

    CHECK(p1.leafIndex == 0);
    CHECK(p2.leafIndex == 1);
    CHECK(p1.publicKey.bytes() != p2.publicKey.bytes());
}

TEST_CASE("TreeKey: each proof verifies with correct message") {
    TreeKey tk(makeSeed(0x99), TEST_DEPTH);
    auto root = tk.getPublicKey();

    for (int i = 0; i < 4; ++i) {
        auto msg   = makeMsg(static_cast<uint8_t>(i + 1));
        auto proof = tk.sign(msg);
        CHECK(TreeKey::verify(root, msg, proof));
    }
}

TEST_CASE("TreeKey: exhaustion throws") {
    TreeKey tk(makeSeed(0xFF), 1);  // depth=1 → only 2 keys
    tk.sign(makeMsg(0x01));
    tk.sign(makeMsg(0x02));
    CHECK_THROWS(tk.sign(makeMsg(0x03)));
}
