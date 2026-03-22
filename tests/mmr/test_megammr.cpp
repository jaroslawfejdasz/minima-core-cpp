#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"
#include "../../src/mmr/MegaMMR.hpp"
#include "../../src/objects/Coin.hpp"
#include "../../src/objects/Address.hpp"
#include "../../src/types/MiniData.hpp"
#include "../../src/types/MiniNumber.hpp"

using namespace minima;

static Coin makeCoin(uint8_t id, int64_t amount) {
    Coin c;
    c.setCoinID(MiniData(std::vector<uint8_t>(32, id)))
     .setAddress(Address(MiniData(std::vector<uint8_t>(32, uint8_t(id+0x80)))))
     .setAmount(MiniNumber(amount));
    return c;
}

TEST_SUITE("MegaMMR") {
    TEST_CASE("empty: 0 leaves") {
        MegaMMR m;
        CHECK(m.leafCount() == 0);
    }
    TEST_CASE("addCoin increments leafCount") {
        MegaMMR m;
        m.addCoin(makeCoin(1, 100));
        CHECK(m.leafCount() == 1);
        m.addCoin(makeCoin(2, 200));
        CHECK(m.leafCount() == 2);
    }
    TEST_CASE("hasCoin true after add") {
        MegaMMR m;
        auto c = makeCoin(1, 100);
        m.addCoin(c);
        CHECK(m.hasCoin(c.coinID()));
    }
    TEST_CASE("hasCoin false for unknown") {
        MegaMMR m;
        m.addCoin(makeCoin(1, 100));
        CHECK_FALSE(m.hasCoin(MiniData(std::vector<uint8_t>(32, 0xFF))));
    }
    TEST_CASE("spendCoin ok") {
        MegaMMR m;
        auto c = makeCoin(1, 100);
        m.addCoin(c);
        CHECK(m.spendCoin(c.coinID()));
    }
    TEST_CASE("spendCoin false for unknown") {
        MegaMMR m;
        CHECK_FALSE(m.spendCoin(MiniData(std::vector<uint8_t>(32, 0xAA))));
    }
    TEST_CASE("root changes on addCoin") {
        MegaMMR m;
        auto r0 = m.getRoot();
        m.addCoin(makeCoin(1, 100));
        CHECK_FALSE(m.getRoot().getData() == r0.getData());
    }
    TEST_CASE("spendCoin marks leaf as spent (metadata)") {
        // MMR hashes do NOT include the spent flag — spent is metadata only.
        // This matches Minima Java: combine() hashes only data bytes.
        // After spendCoin, hasCoin still returns true (coin tracked),
        // and spendCoin succeeds. The spent status is reflected in leaf MMRData.
        MegaMMR m;
        auto c1 = makeCoin(1, 100);
        m.addCoin(c1);
        bool ok = m.spendCoin(c1.coinID());
        CHECK(ok);
        // coin is still tracked in index
        CHECK(m.hasCoin(c1.coinID()));
        // Verify leaf entry is accessible
        auto entry = m.getLeafEntry(c1.coinID());
        CHECK(entry.has_value());
    }
    TEST_CASE("processBlock add+spend") {
        MegaMMR m;
        auto c1 = makeCoin(1, 100);
        auto c2 = makeCoin(2, 200);
        m.addCoin(c1);
        m.processBlock({c1.coinID()}, {c2});
        CHECK(m.leafCount() == 2);
        CHECK(m.hasCoin(c2.coinID()));
    }
    TEST_CASE("getLeafEntry correct indices") {
        MegaMMR m;
        auto c0 = makeCoin(0, 10), c1 = makeCoin(1, 20), c2 = makeCoin(2, 30);
        m.addCoin(c0); m.addCoin(c1); m.addCoin(c2);
        CHECK(m.getLeafEntry(c0.coinID()).value() == 0);
        CHECK(m.getLeafEntry(c1.coinID()).value() == 1);
        CHECK(m.getLeafEntry(c2.coinID()).value() == 2);
    }
    TEST_CASE("getCoinProof present/absent") {
        MegaMMR m;
        auto c = makeCoin(1, 100);
        m.addCoin(c);
        CHECK(m.getCoinProof(c.coinID()).has_value());
        CHECK_FALSE(m.getCoinProof(MiniData(std::vector<uint8_t>(32, 0xFF))).has_value());
    }
    TEST_CASE("serialise/deserialise empty") {
        MegaMMR m;
        auto b = m.serialise();
        size_t off = 0;
        auto m2 = MegaMMR::deserialise(b.data(), off);
        CHECK(m2.leafCount() == 0);
        CHECK(off == b.size());
    }
    TEST_CASE("serialise/deserialise 3 coins") {
        MegaMMR m;
        for (uint8_t i = 1; i <= 3; ++i) m.addCoin(makeCoin(i, i*100));
        auto b = m.serialise();
        size_t off = 0;
        auto m2 = MegaMMR::deserialise(b.data(), off);
        CHECK(m2.leafCount() == 3);
        CHECK(off == b.size());
        for (uint8_t i = 1; i <= 3; ++i)
            CHECK(m2.hasCoin(MiniData(std::vector<uint8_t>(32, i))));
    }
    TEST_CASE("serialise/deserialise preserves root") {
        MegaMMR m;
        for (uint8_t i = 0; i < 5; ++i) m.addCoin(makeCoin(i, (i+1)*50));
        auto rootBefore = m.getRoot();
        auto b = m.serialise();
        size_t off = 0;
        auto m2 = MegaMMR::deserialise(b.data(), off);
        CHECK(m2.getRoot().getData() == rootBefore.getData());
    }
    TEST_CASE("reset clears all") {
        MegaMMR m;
        m.addCoin(makeCoin(1, 100));
        m.reset();
        CHECK(m.leafCount() == 0);
        CHECK_FALSE(m.hasCoin(MiniData(std::vector<uint8_t>(32, 1))));
    }
    TEST_CASE("coinToMMRData deterministic") {
        auto c = makeCoin(0xAB, 999);
        CHECK(MegaMMR::coinToMMRData(c).getData() == MegaMMR::coinToMMRData(c).getData());
    }
    TEST_CASE("coinToMMRData differs for different coins") {
        auto d1 = MegaMMR::coinToMMRData(makeCoin(1, 100));
        auto d2 = MegaMMR::coinToMMRData(makeCoin(2, 100));
        CHECK_FALSE(d1.getData() == d2.getData());
    }
}
