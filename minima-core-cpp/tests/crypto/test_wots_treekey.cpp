/**
 * test_wots_treekey.cpp
 *
 * Tests for WOTS (Winternitz One-Time Signature) and TreeKey (Merkle key).
 *
 * Test plan:
 *   WOTS:
 *     1. keygen + sign + verify: valid signature verifies
 *     2. wrong message: verify returns false
 *     3. tampered signature byte: verify returns false
 *     4. sign deterministic: same seed+msg → same signature
 *     5. digit encoding: all-zero msg → all digits zero
 *     6. digit encoding: all-0xFF msg → all msg digits = 15
 *     7. checksum correctness: all-zero msg → csum = 64*15 = 960
 *     8. different seeds → different public keys
 *
 *   TreeKey:
 *     1. generate + sign + verify: first leaf, valid
 *     2. sign + verify: last leaf (255), valid
 *     3. wrong message: verify returns false
 *     4. tampered auth path: verify returns false
 *     5. tampered wots sig byte: verify returns false
 *     6. keyUse increments on sign()
 *     7. exhausted key throws on sign()
 *     8. different master seeds → different public keys
 *     9. serialise/deserialise roundtrip: keyUse preserved, can still verify
 *    10. sign same leaf twice (signAt): same result (deterministic)
 *    11. public key stability: same seed → same root
 *    12. signature size is exactly TREEKEY_SIG_TOTAL bytes
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/crypto/WOTS.hpp"
#include "../../src/crypto/TreeKey.hpp"

using namespace minima;
using namespace minima::crypto;

// ── Helpers ───────────────────────────────────────────────────────────────────

static MiniData make_seed(uint8_t fill = 0x42) {
    return MiniData(std::vector<uint8_t>(32, fill));
}

static MiniData make_msg(uint8_t fill = 0xAB) {
    return MiniData(std::vector<uint8_t>(32, fill));
}

// Corrupt one byte in a MiniData copy
static MiniData corrupt(const MiniData& d, size_t offset, uint8_t xor_val = 0xFF) {
    auto b = d.bytes();
    b[offset % b.size()] ^= xor_val;
    return MiniData(b);
}

// ─────────────────────────────────────────────────────────────────────────────
// WOTS Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("WOTS — Winternitz One-Time Signature") {

TEST_CASE("1. keygen + sign + verify: valid signature") {
    auto kp  = WOTS::generateKeyPair(make_seed());
    auto msg = make_msg();
    auto sig = WOTS::sign(kp.privateKey, msg);

    CHECK(sig.size() == (size_t)WOTS_SIG_LEN);
    CHECK(WOTS::verify(kp.publicKey, msg, sig));
}

TEST_CASE("2. wrong message: verify returns false") {
    auto kp  = WOTS::generateKeyPair(make_seed());
    auto msg = make_msg(0xAB);
    auto sig = WOTS::sign(kp.privateKey, msg);

    auto wrong_msg = make_msg(0x00);
    CHECK_FALSE(WOTS::verify(kp.publicKey, wrong_msg, sig));
}

TEST_CASE("3. tampered signature: verify returns false") {
    auto kp  = WOTS::generateKeyPair(make_seed());
    auto msg = make_msg();
    auto sig = WOTS::sign(kp.privateKey, msg);

    auto bad_sig = corrupt(sig, 100);
    CHECK_FALSE(WOTS::verify(kp.publicKey, msg, bad_sig));
}

TEST_CASE("4. signing is deterministic: same seed+msg → same sig") {
    auto kp   = WOTS::generateKeyPair(make_seed());
    auto msg  = make_msg();
    auto sig1 = WOTS::sign(kp.privateKey, msg);
    auto sig2 = WOTS::sign(kp.privateKey, msg);
    CHECK(sig1 == sig2);
}

TEST_CASE("5. digit encoding: all-zero msg → msg digits all zero") {
    MiniData zero_msg(std::vector<uint8_t>(32, 0x00));
    auto digits = msg_to_digits(zero_msg);
    for (int i = 0; i < WOTS_MSG_DIGITS; i++)
        CHECK(digits[i] == 0);
    // checksum = 64 * 15 = 960 = 0x3C0
    // in 3 nibbles big-endian: 0x03, 0x0C, 0x00
    CHECK(digits[WOTS_MSG_DIGITS + 0] == 0x03);
    CHECK(digits[WOTS_MSG_DIGITS + 1] == 0x0C);
    CHECK(digits[WOTS_MSG_DIGITS + 2] == 0x00);
}

TEST_CASE("6. digit encoding: all-0xFF msg → msg digits all 15") {
    MiniData ff_msg(std::vector<uint8_t>(32, 0xFF));
    auto digits = msg_to_digits(ff_msg);
    for (int i = 0; i < WOTS_MSG_DIGITS; i++)
        CHECK(digits[i] == 15);
    // checksum = 64 * (15-15) = 0 → csum digits all zero
    for (int i = WOTS_MSG_DIGITS; i < WOTS_TOTAL_DIGITS; i++)
        CHECK(digits[i] == 0);
}

TEST_CASE("7. different seeds → different public keys") {
    auto kp1 = WOTS::generateKeyPair(make_seed(0x11));
    auto kp2 = WOTS::generateKeyPair(make_seed(0x22));
    CHECK_FALSE(kp1.publicKey == kp2.publicKey);
}

TEST_CASE("8. verify with wrong public key → false") {
    auto kp1 = WOTS::generateKeyPair(make_seed(0x11));
    auto kp2 = WOTS::generateKeyPair(make_seed(0x22));
    auto msg = make_msg();
    auto sig = WOTS::sign(kp1.privateKey, msg);
    CHECK_FALSE(WOTS::verify(kp2.publicKey, msg, sig));
}

} // TEST_SUITE WOTS

// ─────────────────────────────────────────────────────────────────────────────
// TreeKey Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("TreeKey — Merkle tree over WOTS leaves") {

TEST_CASE("1. generate + sign + verify: first leaf (index 0)") {
    auto tk  = TreeKey::generate(make_seed());
    auto msg = make_msg();
    auto sig = tk.signAt(msg, 0);

    CHECK(sig.size() == (size_t)TREEKEY_SIG_TOTAL);
    CHECK(TreeKey::verify(tk.publicKey(), msg, sig));
}

TEST_CASE("2. sign + verify: last leaf (index 255)") {
    auto tk  = TreeKey::generate(make_seed());
    auto msg = make_msg(0xCD);
    auto sig = tk.signAt(msg, 255);
    CHECK(TreeKey::verify(tk.publicKey(), msg, sig));
}

TEST_CASE("3. wrong message: verify returns false") {
    auto tk  = TreeKey::generate(make_seed());
    auto msg = make_msg(0xAB);
    auto sig = tk.signAt(msg, 0);
    CHECK_FALSE(TreeKey::verify(tk.publicKey(), make_msg(0x00), sig));
}

TEST_CASE("4. tampered auth path: verify returns false") {
    auto tk  = TreeKey::generate(make_seed());
    auto msg = make_msg();
    auto sig = tk.signAt(msg, 3);
    // Corrupt a byte in the auth path region (starts at byte 4 + WOTS_SIG_LEN = 2148)
    auto bad = corrupt(sig, 2148 + 10);
    CHECK_FALSE(TreeKey::verify(tk.publicKey(), msg, bad));
}

TEST_CASE("5. tampered WOTS sig: verify returns false") {
    auto tk  = TreeKey::generate(make_seed());
    auto msg = make_msg();
    auto sig = tk.signAt(msg, 7);
    // Corrupt byte inside WOTS sig region (bytes 4..2147)
    auto bad = corrupt(sig, 500);
    CHECK_FALSE(TreeKey::verify(tk.publicKey(), msg, bad));
}

TEST_CASE("6. keyUse increments on sign()") {
    auto tk = TreeKey::generate(make_seed());
    CHECK(tk.keyUse() == 0);
    tk.sign(make_msg(0x01));
    CHECK(tk.keyUse() == 1);
    tk.sign(make_msg(0x02));
    CHECK(tk.keyUse() == 2);
}

TEST_CASE("7. exhausted key throws on sign()") {
    // Create a key and manually mark it as used up (tweak a copy via ser/deser)
    auto tk  = TreeKey::generate(make_seed());
    auto raw = tk.serialise();
    // Patch keyUse to 256 = 0x00000100
    raw[32] = 0x00; raw[33] = 0x00; raw[34] = 0x01; raw[35] = 0x00;
    auto exhausted = TreeKey::deserialise(raw);
    CHECK(exhausted.isExhausted());
    CHECK_THROWS_AS(exhausted.sign(make_msg()), std::runtime_error);
}

TEST_CASE("8. different master seeds → different public keys") {
    auto tk1 = TreeKey::generate(make_seed(0x11));
    auto tk2 = TreeKey::generate(make_seed(0x22));
    CHECK_FALSE(tk1.publicKey() == tk2.publicKey());
}

TEST_CASE("9. serialise/deserialise: keyUse preserved, verify still works") {
    auto tk = TreeKey::generate(make_seed(0xBB));
    auto msg0 = make_msg(0x01);
    auto sig0 = tk.sign(msg0);   // leaf 0, keyUse → 1
    auto msg1 = make_msg(0x02);
    auto sig1 = tk.sign(msg1);   // leaf 1, keyUse → 2

    auto raw = tk.serialise();
    auto tk2 = TreeKey::deserialise(raw);

    CHECK(tk2.keyUse() == 2);
    CHECK(tk2.publicKey() == tk.publicKey());

    // Old sigs still verify against same public key
    CHECK(TreeKey::verify(tk2.publicKey(), msg0, sig0));
    CHECK(TreeKey::verify(tk2.publicKey(), msg1, sig1));
}

TEST_CASE("10. signAt is deterministic: same leaf + msg → same sig") {
    auto tk   = TreeKey::generate(make_seed());
    auto msg  = make_msg();
    auto sig1 = tk.signAt(msg, 42);
    auto sig2 = tk.signAt(msg, 42);
    CHECK(sig1 == sig2);
}

TEST_CASE("11. public key stability: same seed → same root") {
    auto tk1 = TreeKey::generate(make_seed(0xCC));
    auto tk2 = TreeKey::generate(make_seed(0xCC));
    CHECK(tk1.publicKey() == tk2.publicKey());
}

TEST_CASE("12. signature size is exactly TREEKEY_SIG_TOTAL") {
    auto tk  = TreeKey::generate(make_seed());
    auto sig = tk.signAt(make_msg(), 0);
    CHECK(sig.size() == (size_t)TREEKEY_SIG_TOTAL);
    // TREEKEY_SIG_TOTAL = 4 + 2144 + 256 = 2404
    CHECK(TREEKEY_SIG_TOTAL == 2404);
}

} // TEST_SUITE TreeKey
