#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/kissvm/functions/Functions.hpp"
#include "../../src/kissvm/Contract.hpp"
#include "../../src/kissvm/Value.hpp"
#include "../../src/crypto/RSA.hpp"
#include "../../src/crypto/Hash.hpp"
#include "../../src/types/MiniData.hpp"
#include "../../src/objects/Coin.hpp"
#include "../../src/objects/Address.hpp"
#include "../../src/objects/Transaction.hpp"
#include "../../src/objects/Witness.hpp"

using namespace minima;
using namespace minima::crypto;
using namespace minima::kissvm;

static Contract makeCtx() {
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
    static Contract ctx(rtScript, txn, w, 0);
    ctx.setBlockNumber(MiniNumber(int64_t(1)));
    ctx.setCoinAge(MiniNumber::ZERO);
    return ctx;
}

// Direct call to CHECKSIG function — no parser involved
TEST_CASE("CHECKSIG fn: valid signature returns TRUE") {
    auto kp = RSA::generateKeyPair();
    MiniData data(std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF});
    MiniData sig = RSA::sign(kp.privateKey, data);
    REQUIRE(RSA::verify(kp.publicKey, data, sig));

    std::vector<Value> args = { Value::hex(kp.publicKey), Value::hex(data), Value::hex(sig) };
    auto ctx = makeCtx();
    Value result = fn::CHECKSIG(args, ctx);
    MESSAGE("result: " << result.toString());
    CHECK(result.asBoolean());
}

TEST_CASE("CHECKSIG fn: wrong pubkey returns FALSE") {
    auto kp1 = RSA::generateKeyPair();
    auto kp2 = RSA::generateKeyPair();
    MiniData data(std::vector<uint8_t>{0x01, 0x02});
    MiniData sig = RSA::sign(kp1.privateKey, data);

    std::vector<Value> args = { Value::hex(kp2.publicKey), Value::hex(data), Value::hex(sig) };
    auto ctx = makeCtx();
    Value result = fn::CHECKSIG(args, ctx);
    CHECK_FALSE(result.asBoolean());
}

// Via KISS VM script — parser path
TEST_CASE("CHECKSIG via script parser: valid RSA sig") {
    auto kp = RSA::generateKeyPair();
    MiniData data(std::vector<uint8_t>{0xCA, 0xFE});
    MiniData sig = RSA::sign(kp.privateKey, data);
    REQUIRE(RSA::verify(kp.publicKey, data, sig));

    std::string pubHex  = kp.publicKey.toHexString();  // "0x..."
    std::string dataHex = data.toHexString();
    std::string sigHex  = sig.toHexString();

    MESSAGE("pubHex: " << pubHex.substr(0,10) << "...");
    MESSAGE("dataHex: " << dataHex);
    MESSAGE("sigHex len: " << sigHex.size());

    std::string script = "LET v = CHECKSIG(" + pubHex
                       + "," + dataHex
                       + "," + sigHex
                       + ") RETURN v";

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
    SignatureProof sigp; sigp.pubKey = kp.publicKey; sigp.signature = sig;
    w.addSignature(sigp);

    try {
        Contract c(script, txn, w, 0);
        c.setBlockNumber(MiniNumber(int64_t(1)));
        c.setCoinAge(MiniNumber::ZERO);
        Value result = c.execute();
        MESSAGE("result type: " << (int)result.type() << " val: " << result.toString());
        CHECK(c.isTrue());
    } catch (const std::exception& e) {
        MESSAGE("exception: " << e.what());
        FAIL("exception thrown");
    }
}
