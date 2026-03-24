/**
 * test_checksig_treekey.cpp
 *
 * Integration tests: TreeKey signatures through KISS VM functions.
 *
 * Verifies that CHECKSIG, SIGNEDBY, and MULTISIG correctly handle
 * TreeKey (32-byte Merkle root pubkey, 2404-byte signature) alongside
 * the existing RSA path — auto-dispatched by pubkey size.
 *
 * Test plan:
 *   CHECKSIG + TreeKey:
 *     1. valid sig → TRUE
 *     2. wrong data → FALSE
 *     3. tampered sig → FALSE
 *     4. wrong pubkey (different TreeKey) → FALSE
 *
 *   SIGNEDBY + TreeKey:
 *     5. valid sig in witness, no txpowID → TRUE (unit test fallback)
 *     6. valid sig in witness, with txpowID → TRUE
 *     7. wrong txpowID → FALSE
 *     8. pubkey not in witness → FALSE
 *
 *   MULTISIG + TreeKey:
 *     9.  require 1 of 1 TreeKey → TRUE
 *    10.  require 2 of 2 TreeKey → TRUE
 *    11.  require 2 of 2, one bad sig → FALSE
 *    12.  require 1 of 1 TreeKey, wrong pubkey → FALSE
 *
 *   Mixed RSA + TreeKey:
 *    13.  MULTISIG require 2: one RSA + one TreeKey, both valid → TRUE
 *    14.  CHECKSIG RSA pubkey still works → TRUE
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"

#include "../../src/crypto/WOTS.hpp"
#include "../../src/crypto/TreeKey.hpp"
#include "../../src/crypto/RSA.hpp"
#include "../../src/crypto/Hash.hpp"
#include "../../src/kissvm/Contract.hpp"
#include "../../src/objects/Transaction.hpp"
#include "../../src/objects/Witness.hpp"
#include "../../src/objects/Coin.hpp"
#include "../../src/objects/Address.hpp"
#include "../../src/types/MiniData.hpp"

using namespace minima;
using namespace minima::crypto;
using namespace minima::kissvm;

// ── Test helpers ──────────────────────────────────────────────────────────────

static MiniData make_seed(uint8_t fill) {
    return MiniData(std::vector<uint8_t>(32, fill));
}

static MiniData make_data(uint8_t fill = 0xAB) {
    return MiniData(std::vector<uint8_t>(32, fill));
}

static MiniData corrupt_byte(const MiniData& d, size_t offset) {
    auto b = d.bytes();
    b[offset % b.size()] ^= 0xFF;
    return MiniData(b);
}

// Build a minimal Contract from a script, with no real coin context.
// Used for testing CHECKSIG(pubkey, data, sig) inline args (stateless).
static Contract makeContract(const std::string& script,
                              const Transaction& txn,
                              const Witness& w,
                              int coinIndex = 0) {
    static const std::string trivialScript = "RETURN TRUE";
    Contract c(script, txn, w, coinIndex);
    c.setBlockNumber(MiniNumber(int64_t(1)));
    c.setCoinAge(MiniNumber::ZERO);
    return c;
}

// Build a Transaction + Witness with a single dummy coin, ready for SIGNEDBY/MULTISIG tests.
struct TxContext {
    Transaction txn;
    Witness     witness;

    static TxContext make() {
        static const std::string trivialScript = "RETURN TRUE";
        MiniData addr = Hash::sha3_256(
            std::vector<uint8_t>(trivialScript.begin(), trivialScript.end()));
        Coin coin;
        coin.setAddress(Address(addr));
        coin.setTokenID(MiniData(std::vector<uint8_t>{0x00}));
        coin.setAmount(MiniNumber(100.0));
        coin.setCoinID(MiniData(std::vector<uint8_t>(32, 0x01)));

        TxContext ctx;
        ctx.txn.addInput(coin);

        ScriptProof sp;
        sp.script  = MiniString(trivialScript);
        sp.address = addr;
        ctx.witness.addScript(sp);

        return ctx;
    }

    void addTreeKeySig(const TreeKey& tk, const MiniData& signedData) {
        SignatureProof sp;
        sp.pubKey    = tk.publicKey();
        sp.signature = tk.signAt(signedData, 0);
        witness.addSignature(sp);
    }

    void addTreeKeySigAt(const TreeKey& tk, const MiniData& signedData, int leaf) {
        SignatureProof sp;
        sp.pubKey    = tk.publicKey();
        sp.signature = tk.signAt(signedData, leaf);
        witness.addSignature(sp);
    }

    void addRSASig(const RSAKeyPair& kp, const MiniData& signedData) {
        SignatureProof sp;
        sp.pubKey    = kp.publicKey;
        sp.signature = RSA::sign(kp.privateKey, signedData);
        witness.addSignature(sp);
    }
};

// Build and execute a contract, returns true/false
static bool runScript(const std::string& script,
                      const Transaction& txn,
                      const Witness& w,
                      const MiniData& txpowID = MiniData{}) {
    Contract c(script, txn, w, 0);
    c.setBlockNumber(MiniNumber(int64_t(1)));
    c.setCoinAge(MiniNumber::ZERO);
    if (txpowID.size() > 0) c.setTxPoWID(txpowID);
    c.execute();
    return c.isTrue();
}

// ─────────────────────────────────────────────────────────────────────────────
// CHECKSIG + TreeKey
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("CHECKSIG — TreeKey dispatch") {

TEST_CASE("1. CHECKSIG TreeKey: valid sig → TRUE") {
    auto tk  = TreeKey::generate(make_seed(0x11));
    auto msg = make_data(0xAA);
    auto sig = tk.signAt(msg, 0);

    std::string script =
        "LET v = CHECKSIG("
        + tk.publicKey().toHexString() + ","
        + msg.toHexString() + ","
        + sig.toHexString() +
        ") RETURN v";

    auto ctx = TxContext::make();
    CHECK(runScript(script, ctx.txn, ctx.witness));
}

TEST_CASE("2. CHECKSIG TreeKey: wrong data → FALSE") {
    auto tk  = TreeKey::generate(make_seed(0x11));
    auto msg = make_data(0xAA);
    auto sig = tk.signAt(msg, 0);
    auto bad = make_data(0x00);  // different message

    std::string script =
        "LET v = CHECKSIG("
        + tk.publicKey().toHexString() + ","
        + bad.toHexString() + ","
        + sig.toHexString() +
        ") RETURN v";

    auto ctx = TxContext::make();
    CHECK_FALSE(runScript(script, ctx.txn, ctx.witness));
}

TEST_CASE("3. CHECKSIG TreeKey: tampered sig → FALSE") {
    auto tk  = TreeKey::generate(make_seed(0x11));
    auto msg = make_data(0xAA);
    auto sig = tk.signAt(msg, 0);
    auto bad_sig = corrupt_byte(sig, 200);

    std::string script =
        "LET v = CHECKSIG("
        + tk.publicKey().toHexString() + ","
        + msg.toHexString() + ","
        + bad_sig.toHexString() +
        ") RETURN v";

    auto ctx = TxContext::make();
    CHECK_FALSE(runScript(script, ctx.txn, ctx.witness));
}

TEST_CASE("4. CHECKSIG TreeKey: wrong pubkey → FALSE") {
    auto tk1 = TreeKey::generate(make_seed(0x11));
    auto tk2 = TreeKey::generate(make_seed(0x22));
    auto msg = make_data(0xAA);
    auto sig = tk1.signAt(msg, 0);

    std::string script =
        "LET v = CHECKSIG("
        + tk2.publicKey().toHexString() + ","  // tk2's key
        + msg.toHexString() + ","
        + sig.toHexString() +               // tk1's sig
        ") RETURN v";

    auto ctx = TxContext::make();
    CHECK_FALSE(runScript(script, ctx.txn, ctx.witness));
}

} // TEST_SUITE CHECKSIG TreeKey

// ─────────────────────────────────────────────────────────────────────────────
// SIGNEDBY + TreeKey
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("SIGNEDBY — TreeKey dispatch") {

TEST_CASE("5. SIGNEDBY: valid sig in witness, no txpowID → TRUE (unit fallback)") {
    auto tk  = TreeKey::generate(make_seed(0x33));
    auto ctx = TxContext::make();
    // For SIGNEDBY without txpowID we sign an empty txpowID effectively
    // But the fallback: if txdata.size()==0, presence is accepted
    ctx.addTreeKeySig(tk, make_data(0x00));  // placeholder sig

    std::string script =
        "LET v = SIGNEDBY(" + tk.publicKey().toHexString() + ") RETURN v";
    // No txpowID set → presence fallback → TRUE
    CHECK(runScript(script, ctx.txn, ctx.witness));
}

TEST_CASE("6. SIGNEDBY: valid sig with matching txpowID → TRUE") {
    auto tk     = TreeKey::generate(make_seed(0x44));
    MiniData txid = make_data(0xBB);
    auto ctx    = TxContext::make();
    ctx.addTreeKeySig(tk, txid);  // sign the txpowID

    std::string script =
        "LET v = SIGNEDBY(" + tk.publicKey().toHexString() + ") RETURN v";
    CHECK(runScript(script, ctx.txn, ctx.witness, txid));
}

TEST_CASE("7. SIGNEDBY: sig over wrong txpowID → FALSE") {
    auto tk      = TreeKey::generate(make_seed(0x55));
    MiniData txid = make_data(0xBB);
    MiniData bad  = make_data(0xCC);  // different txpowID
    auto ctx     = TxContext::make();
    ctx.addTreeKeySig(tk, txid);  // signed with txid

    std::string script =
        "LET v = SIGNEDBY(" + tk.publicKey().toHexString() + ") RETURN v";
    CHECK_FALSE(runScript(script, ctx.txn, ctx.witness, bad));
}

TEST_CASE("8. SIGNEDBY: pubkey not in witness → FALSE") {
    auto tk  = TreeKey::generate(make_seed(0x66));
    auto ctx = TxContext::make();
    // No signature added to witness

    std::string script =
        "LET v = SIGNEDBY(" + tk.publicKey().toHexString() + ") RETURN v";
    CHECK_FALSE(runScript(script, ctx.txn, ctx.witness));
}

} // TEST_SUITE SIGNEDBY TreeKey

// ─────────────────────────────────────────────────────────────────────────────
// MULTISIG + TreeKey
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("MULTISIG — TreeKey dispatch") {

TEST_CASE("9. MULTISIG 1-of-1 TreeKey: valid → TRUE") {
    auto tk   = TreeKey::generate(make_seed(0x77));
    MiniData txid = make_data(0x01);
    auto ctx  = TxContext::make();
    ctx.addTreeKeySig(tk, txid);

    std::string script =
        "LET v = MULTISIG(1," + tk.publicKey().toHexString() + ") RETURN v";
    CHECK(runScript(script, ctx.txn, ctx.witness, txid));
}

TEST_CASE("10. MULTISIG 2-of-2 TreeKey: both valid → TRUE") {
    auto tk1  = TreeKey::generate(make_seed(0x11));
    auto tk2  = TreeKey::generate(make_seed(0x22));
    MiniData txid = make_data(0x02);
    auto ctx  = TxContext::make();
    ctx.addTreeKeySig(tk1, txid);
    ctx.addTreeKeySig(tk2, txid);

    std::string script =
        "LET v = MULTISIG(2,"
        + tk1.publicKey().toHexString() + ","
        + tk2.publicKey().toHexString() +
        ") RETURN v";
    CHECK(runScript(script, ctx.txn, ctx.witness, txid));
}

TEST_CASE("11. MULTISIG 2-of-2: one sig wrong txpowID → FALSE") {
    auto tk1  = TreeKey::generate(make_seed(0x11));
    auto tk2  = TreeKey::generate(make_seed(0x22));
    MiniData txid = make_data(0x02);
    MiniData bad  = make_data(0xFF);
    auto ctx  = TxContext::make();
    ctx.addTreeKeySig(tk1, txid);
    ctx.addTreeKeySig(tk2, bad);  // tk2 signs wrong data

    std::string script =
        "LET v = MULTISIG(2,"
        + tk1.publicKey().toHexString() + ","
        + tk2.publicKey().toHexString() +
        ") RETURN v";
    CHECK_FALSE(runScript(script, ctx.txn, ctx.witness, txid));
}

TEST_CASE("12. MULTISIG 1-of-1: wrong pubkey → FALSE") {
    auto tk1  = TreeKey::generate(make_seed(0xAA));
    auto tk2  = TreeKey::generate(make_seed(0xBB));
    MiniData txid = make_data(0x03);
    auto ctx  = TxContext::make();
    ctx.addTreeKeySig(tk1, txid);  // witness has tk1's sig

    std::string script =
        "LET v = MULTISIG(1," + tk2.publicKey().toHexString() + ") RETURN v";
    CHECK_FALSE(runScript(script, ctx.txn, ctx.witness, txid));
}

} // TEST_SUITE MULTISIG TreeKey

// ─────────────────────────────────────────────────────────────────────────────
// Mixed RSA + TreeKey
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("Mixed RSA + TreeKey dispatch") {

TEST_CASE("13. MULTISIG 2-of-2: one RSA + one TreeKey, both valid → TRUE") {
    auto kp  = RSA::generateKeyPair();
    auto tk  = TreeKey::generate(make_seed(0xCC));
    MiniData txid = make_data(0x04);
    auto ctx = TxContext::make();
    ctx.addRSASig(kp, txid);
    ctx.addTreeKeySig(tk, txid);

    std::string script =
        "LET v = MULTISIG(2,"
        + kp.publicKey.toHexString() + ","
        + tk.publicKey().toHexString() +
        ") RETURN v";
    CHECK(runScript(script, ctx.txn, ctx.witness, txid));
}

TEST_CASE("14. CHECKSIG RSA pubkey still works after TreeKey integration") {
    auto kp  = RSA::generateKeyPair();
    auto msg = make_data(0xEF);
    auto sig = RSA::sign(kp.privateKey, msg);

    std::string script =
        "LET v = CHECKSIG("
        + kp.publicKey.toHexString() + ","
        + msg.toHexString() + ","
        + sig.toHexString() +
        ") RETURN v";

    auto ctx = TxContext::make();
    CHECK(runScript(script, ctx.txn, ctx.witness));
}

} // TEST_SUITE Mixed
