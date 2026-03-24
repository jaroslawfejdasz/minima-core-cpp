#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/crypto/RSA.hpp"
#include "../../src/crypto/Hash.hpp"

using namespace minima;
using namespace minima::crypto;

TEST_CASE("RSA: keygen produces non-empty keys") {
    auto kp = RSA::generateKeyPair();
    CHECK_FALSE(kp.privateKey.bytes().empty());
    CHECK_FALSE(kp.publicKey.bytes().empty());
    CHECK(kp.privateKey.bytes().size() > 100);
    CHECK(kp.publicKey.bytes().size() > 100);
}

TEST_CASE("RSA: sign produces 128-byte signature (RSA-1024)") {
    auto kp = RSA::generateKeyPair();
    MiniData data(std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04});
    MiniData sig = RSA::sign(kp.privateKey, data);
    CHECK(sig.bytes().size() == 128);
}

TEST_CASE("RSA: sign + verify roundtrip") {
    auto kp = RSA::generateKeyPair();
    std::string msg = "Hello Minima KISS VM";
    MiniData data(std::vector<uint8_t>(msg.begin(), msg.end()));
    MiniData sig = RSA::sign(kp.privateKey, data);
    CHECK(RSA::verify(kp.publicKey, data, sig));
}

TEST_CASE("RSA: wrong pubkey fails verification") {
    auto kp1 = RSA::generateKeyPair();
    auto kp2 = RSA::generateKeyPair();
    MiniData data(std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF});
    MiniData sig = RSA::sign(kp1.privateKey, data);
    CHECK_FALSE(RSA::verify(kp2.publicKey, data, sig));
}

TEST_CASE("RSA: tampered data fails verification") {
    auto kp = RSA::generateKeyPair();
    MiniData data(std::vector<uint8_t>{0x01, 0x02, 0x03});
    MiniData sig = RSA::sign(kp.privateKey, data);
    MiniData tampered(std::vector<uint8_t>{0x01, 0x02, 0xFF});
    CHECK_FALSE(RSA::verify(kp.publicKey, tampered, sig));
}

TEST_CASE("RSA: tampered signature fails verification") {
    auto kp = RSA::generateKeyPair();
    MiniData data(std::vector<uint8_t>{0xAB, 0xCD, 0xEF});
    MiniData sig = RSA::sign(kp.privateKey, data);
    auto bytes = sig.bytes();
    bytes[10] ^= 0xFF;
    CHECK_FALSE(RSA::verify(kp.publicKey, data, MiniData(bytes)));
}

TEST_CASE("RSA: empty signature returns false") {
    auto kp = RSA::generateKeyPair();
    MiniData data(std::vector<uint8_t>{0x01});
    CHECK_FALSE(RSA::verify(kp.publicKey, data, MiniData{}));
}

TEST_CASE("RSA: sign SHA3-hashed data (CHECKSIG pattern)") {
    auto kp = RSA::generateKeyPair();
    std::string txdata = "minima:transaction:abc123";
    MiniData raw(std::vector<uint8_t>(txdata.begin(), txdata.end()));
    MiniData hashed = Hash::sha3_256(raw.bytes());
    MiniData sig = RSA::sign(kp.privateKey, hashed);
    CHECK(RSA::verify(kp.publicKey, hashed, sig));
    CHECK_FALSE(RSA::verify(kp.publicKey, raw, sig));
}
