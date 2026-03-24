#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/crypto/RSA.hpp"
#include "../../src/crypto/Hash.hpp"
#include "../../src/kissvm/Contract.hpp"
#include "../../src/objects/Transaction.hpp"
#include "../../src/objects/Witness.hpp"
#include "../../src/objects/Coin.hpp"
#include "../../src/objects/Address.hpp"

using namespace minima;
using namespace minima::crypto;
using namespace minima::kissvm;

// ── helpers ──────────────────────────────────────────────────────────────────

static std::pair<Transaction, Witness> buildCtxForKey(
    const MiniData& pubkey, const MiniData& sig)
{
    static const std::string rtScript = "RETURN TRUE";
    MiniData rtAddr = Hash::sha3_256(
        std::vector<uint8_t>(rtScript.begin(), rtScript.end()));
    Address addr(rtAddr);
    Coin coin; coin.setAddress(addr);
    coin.setTokenID(MiniData(std::vector<uint8_t>{0x00}));
    coin.setAmount(MiniNumber(100.0));
    coin.setCoinID(MiniData(std::vector<uint8_t>(32, 0x01)));
    Transaction txn; txn.addInput(coin);
    Witness w; ScriptProof sp;
    sp.script = MiniString(rtScript); sp.address = rtAddr;
    w.addScript(sp);
    SignatureProof sigp; sigp.pubKey = pubkey; sigp.signature = sig;
    w.addSignature(sigp);
    return {txn, w};
}

static bool runChecksig(const std::string& pubHex,
                        const std::string& dataHex,
                        const std::string& sigHex,
                        const RSAKeyPair& kp,
                        const MiniData& sig)
{
    std::string script = "LET v = CHECKSIG(" + pubHex
                       + "," + dataHex
                       + "," + sigHex
                       + ") RETURN v";
    auto [txn, w] = buildCtxForKey(kp.publicKey, sig);
    Contract c(script, txn, w, 0);
    c.setBlockNumber(MiniNumber(int64_t(1)));
    c.setCoinAge(MiniNumber::ZERO);
    c.execute();
    return c.isTrue();
}

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_CASE("CHECKSIG integration: valid RSA-1024 signature") {
    auto kp = RSA::generateKeyPair();
    MiniData data(std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF});
    MiniData sig = RSA::sign(kp.privateKey, data);
    REQUIRE(RSA::verify(kp.publicKey, data, sig));

    CHECK(runChecksig(kp.publicKey.toHexString(),
                      data.toHexString(),
                      sig.toHexString(),
                      kp, sig));
}

TEST_CASE("CHECKSIG integration: wrong pubkey returns FALSE") {
    auto kp1 = RSA::generateKeyPair();
    auto kp2 = RSA::generateKeyPair();
    MiniData data(std::vector<uint8_t>{0x01, 0x02});
    MiniData sig = RSA::sign(kp1.privateKey, data);

    // Script uses kp2.pubkey, but sig made with kp1
    std::string script = "LET v = CHECKSIG(" + kp2.publicKey.toHexString()
                       + "," + data.toHexString()
                       + "," + sig.toHexString()
                       + ") RETURN v";
    auto [txn, w] = buildCtxForKey(kp1.publicKey, sig);
    Contract c(script, txn, w, 0);
    c.setBlockNumber(MiniNumber(int64_t(1)));
    c.setCoinAge(MiniNumber::ZERO);
    c.execute();
    CHECK_FALSE(c.isTrue());
}

TEST_CASE("CHECKSIG integration: tampered data returns FALSE") {
    auto kp = RSA::generateKeyPair();
    MiniData original(std::vector<uint8_t>{0x10, 0x20, 0x30});
    MiniData sig = RSA::sign(kp.privateKey, original);
    MiniData tampered(std::vector<uint8_t>{0x10, 0x20, 0xFF}); // changed

    CHECK_FALSE(runChecksig(kp.publicKey.toHexString(),
                            tampered.toHexString(),
                            sig.toHexString(),
                            kp, sig));
}

TEST_CASE("CHECKSIG integration: tampered signature returns FALSE") {
    auto kp = RSA::generateKeyPair();
    MiniData data(std::vector<uint8_t>{0xAA, 0xBB});
    MiniData sig = RSA::sign(kp.privateKey, data);
    auto bytes = sig.bytes(); bytes[20] ^= 0xFF;
    MiniData badSig(bytes);

    CHECK_FALSE(runChecksig(kp.publicKey.toHexString(),
                            data.toHexString(),
                            badSig.toHexString(),
                            kp, badSig));
}

TEST_CASE("CHECKSIG integration: SHA3-hashed data (real KISS VM pattern)") {
    // Real pattern: sign SHA3 of transaction data
    auto kp = RSA::generateKeyPair();
    std::string txdata = "minima:tx:deadbeef";
    MiniData raw(std::vector<uint8_t>(txdata.begin(), txdata.end()));
    MiniData hashed = Hash::sha3_256(raw.bytes());
    MiniData sig = RSA::sign(kp.privateKey, hashed);
    REQUIRE(RSA::verify(kp.publicKey, hashed, sig));

    // Script: hash inline then check
    std::string script = "LET h = SHA3(" + raw.toHexString() + ")"
                         " LET v = CHECKSIG(" + kp.publicKey.toHexString()
                       + ",h," + sig.toHexString()
                       + ") RETURN v";
    auto [txn, w] = buildCtxForKey(kp.publicKey, sig);
    Contract c(script, txn, w, 0);
    c.setBlockNumber(MiniNumber(int64_t(1)));
    c.setCoinAge(MiniNumber::ZERO);
    c.execute();
    CHECK(c.isTrue());
}

TEST_CASE("SIGNEDBY integration: pubkey in witness → TRUE") {
    auto kp = RSA::generateKeyPair();
    MiniData data(std::vector<uint8_t>{0x01});
    MiniData sig = RSA::sign(kp.privateKey, data);

    std::string script = "RETURN SIGNEDBY(" + kp.publicKey.toHexString() + ")";
    auto [txn, w] = buildCtxForKey(kp.publicKey, sig);
    Contract c(script, txn, w, 0);
    c.setBlockNumber(MiniNumber(int64_t(1)));
    c.setCoinAge(MiniNumber::ZERO);
    c.execute();
    CHECK(c.isTrue());
}

TEST_CASE("SIGNEDBY integration: unknown pubkey → FALSE") {
    auto kp1 = RSA::generateKeyPair();
    auto kp2 = RSA::generateKeyPair();
    MiniData sig = RSA::sign(kp1.privateKey, MiniData(std::vector<uint8_t>{0x01}));

    // Ask if kp2 signed — it didn't
    std::string script = "RETURN SIGNEDBY(" + kp2.publicKey.toHexString() + ")";
    auto [txn, w] = buildCtxForKey(kp1.publicKey, sig);
    Contract c(script, txn, w, 0);
    c.setBlockNumber(MiniNumber(int64_t(1)));
    c.setCoinAge(MiniNumber::ZERO);
    c.execute();
    CHECK_FALSE(c.isTrue());
}
