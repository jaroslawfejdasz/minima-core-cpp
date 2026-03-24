#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"

#include "../../src/mempool/Mempool.hpp"
#include "../../src/objects/TxPoW.hpp"
#include "../../src/objects/Coin.hpp"
#include "../../src/objects/Address.hpp"
#include "../../src/objects/Witness.hpp"
#include "../../src/crypto/Hash.hpp"

using namespace minima;
using namespace minima::mempool;

// ─── helpers ────────────────────────────────────────────────────────────────

static MiniData nativeToken() {
    return MiniData(std::vector<uint8_t>{0x00});
}

// Coin with given explicit coinID seed byte
static Coin makeCoin(uint8_t seed, double amount = 100.0) {
    static const std::string script = "RETURN TRUE";
    MiniData addrHash = crypto::Hash::sha3_256(
        std::vector<uint8_t>(script.begin(), script.end()));
    Address addr(addrHash);
    Coin c;
    c.setAddress(addr);
    c.setTokenID(nativeToken());
    c.setAmount(MiniNumber(amount));
    c.setMmrEntry(MiniNumber(int64_t(0)));
    c.setCreated(MiniNumber(int64_t(1)));
    c.setCoinID(MiniData(std::vector<uint8_t>(32, seed)));
    return c;
}

// Build a simple TxPoW spending one coin
static TxPoW makeTx(const Coin& input, int64_t blockNum = 1) {
    TxPoW t;
    t.header().blockNumber = MiniNumber(blockNum);
    t.header().timeMilli   = MiniNumber(int64_t(1000000 + blockNum));
    t.body().txnDifficulty = MiniData(std::vector<uint8_t>(32, 0xFF));
    t.body().txn.addInput(input);
    return t;
}

// ─── Basic add/contains/remove ───────────────────────────────────────────────

TEST_CASE("Mempool: add and contains") {
    Mempool mp;
    Coin c = makeCoin(0x01);
    TxPoW tx = makeTx(c, 1);
    MiniData id = tx.computeID();

    CHECK(mp.add(tx));
    CHECK(mp.contains(id));
    CHECK(mp.size() == 1);
}

TEST_CASE("Mempool: remove returns true and clears") {
    Mempool mp;
    TxPoW tx = makeTx(makeCoin(0x02), 1);
    MiniData id = tx.computeID();
    mp.add(tx);
    CHECK(mp.remove(id));
    CHECK_FALSE(mp.contains(id));
    CHECK(mp.size() == 0);
}

TEST_CASE("Mempool: remove non-existent returns false") {
    Mempool mp;
    MiniData bogus(std::vector<uint8_t>(32, 0xFF));
    CHECK_FALSE(mp.remove(bogus));
}

TEST_CASE("Mempool: duplicate add returns false") {
    Mempool mp;
    TxPoW tx = makeTx(makeCoin(0x03), 1);
    CHECK(mp.add(tx));
    CHECK_FALSE(mp.add(tx));    // duplicate
    CHECK(mp.size() == 1);
}

// ─── Double-spend detection ──────────────────────────────────────────────────

TEST_CASE("Mempool: rejects double-spend (same input in pool)") {
    Mempool mp;
    Coin shared = makeCoin(0x10);   // same input coin

    TxPoW tx1 = makeTx(shared, 1);
    TxPoW tx2 = makeTx(shared, 2); // different tx, same input

    CHECK(mp.add(tx1));
    CHECK_FALSE(mp.add(tx2));   // double-spend
    CHECK(mp.size() == 1);
}

TEST_CASE("Mempool: different inputs — both accepted") {
    Mempool mp;
    TxPoW tx1 = makeTx(makeCoin(0x11), 1);
    TxPoW tx2 = makeTx(makeCoin(0x12), 2);

    CHECK(mp.add(tx1));
    CHECK(mp.add(tx2));
    CHECK(mp.size() == 2);
}

TEST_CASE("Mempool: after remove, same input can be re-added") {
    Mempool mp;
    Coin c = makeCoin(0x13);
    TxPoW tx1 = makeTx(c, 1);
    MiniData id1 = tx1.computeID();

    mp.add(tx1);
    mp.remove(id1);

    TxPoW tx2 = makeTx(c, 2);  // different tx, same coin
    CHECK(mp.add(tx2));
    CHECK(mp.size() == 1);
}

// ─── getPending ──────────────────────────────────────────────────────────────

TEST_CASE("Mempool: getPending returns oldest first") {
    Mempool mp;
    std::vector<MiniData> ids;
    for (int i = 0; i < 5; ++i) {
        TxPoW tx = makeTx(makeCoin((uint8_t)(0x20 + i)), (int64_t)i);
        ids.push_back(tx.computeID());
        mp.add(tx);
    }
    auto pending = mp.getPending(5);
    REQUIRE(pending.size() == 5);
    // Verify order = insertion order
    for (size_t i = 0; i < pending.size(); ++i)
        CHECK(pending[i].computeID().bytes() == ids[i].bytes());
}

TEST_CASE("Mempool: getPending respects limit") {
    Mempool mp;
    for (int i = 0; i < 10; ++i)
        mp.add(makeTx(makeCoin((uint8_t)(0x30 + i)), (int64_t)i));
    auto pending = mp.getPending(3);
    CHECK(pending.size() == 3);
}

TEST_CASE("Mempool: getPending on empty pool") {
    Mempool mp;
    auto pending = mp.getPending(100);
    CHECK(pending.empty());
}

// ─── onBlockAccepted ─────────────────────────────────────────────────────────

TEST_CASE("Mempool: onBlockAccepted removes conflicting txns") {
    Mempool mp;
    Coin shared = makeCoin(0x40);
    Coin other  = makeCoin(0x41);

    TxPoW tx1 = makeTx(shared, 1);  // spends shared
    TxPoW tx2 = makeTx(other,  2);  // spends different coin
    mp.add(tx1);
    mp.add(tx2);
    REQUIRE(mp.size() == 2);

    // Block accepts a tx that spends the same shared coin
    TxPoW block;
    block.header().blockNumber = MiniNumber(int64_t(3));
    block.body().txnDifficulty = MiniData(std::vector<uint8_t>(32, 0xFF));
    block.body().txn.addInput(shared);

    mp.onBlockAccepted(block);

    // tx1 conflicts → evicted; tx2 unaffected
    CHECK_FALSE(mp.contains(tx1.computeID()));
    CHECK(mp.contains(tx2.computeID()));
    CHECK(mp.size() == 1);
}

TEST_CASE("Mempool: onBlockAccepted with no conflicts leaves pool intact") {
    Mempool mp;
    for (int i = 0; i < 3; ++i)
        mp.add(makeTx(makeCoin((uint8_t)(0x50 + i)), (int64_t)i));
    REQUIRE(mp.size() == 3);

    // Block spends an unrelated coin
    TxPoW block;
    block.header().blockNumber = MiniNumber(int64_t(10));
    block.body().txn.addInput(makeCoin(0xFF));

    mp.onBlockAccepted(block);
    CHECK(mp.size() == 3);  // unchanged
}

TEST_CASE("Mempool: onBlockAccepted clears multiple conflicting txns") {
    Mempool mp;
    Coin c1 = makeCoin(0x60);
    Coin c2 = makeCoin(0x61);
    Coin c3 = makeCoin(0x62);

    mp.add(makeTx(c1, 1));
    mp.add(makeTx(c2, 2));
    mp.add(makeTx(c3, 3));
    REQUIRE(mp.size() == 3);

    // Block spends c1 and c2
    TxPoW block;
    block.header().blockNumber = MiniNumber(int64_t(4));
    block.body().txn.addInput(c1);
    block.body().txn.addInput(c2);

    mp.onBlockAccepted(block);
    CHECK(mp.size() == 1);  // only c3 tx remains
}

// ─── Capacity / eviction ─────────────────────────────────────────────────────

TEST_CASE("Mempool: capacity cap evicts oldest (FIFO)") {
    Mempool mp(3);  // tiny pool

    TxPoW tx0 = makeTx(makeCoin(0x70), 0);
    TxPoW tx1 = makeTx(makeCoin(0x71), 1);
    TxPoW tx2 = makeTx(makeCoin(0x72), 2);
    TxPoW tx3 = makeTx(makeCoin(0x73), 3); // triggers eviction of tx0

    MiniData id0 = tx0.computeID();
    MiniData id3 = tx3.computeID();

    mp.add(tx0);
    mp.add(tx1);
    mp.add(tx2);
    mp.add(tx3);  // evicts tx0

    CHECK(mp.size() == 3);
    CHECK_FALSE(mp.contains(id0));  // evicted
    CHECK(mp.contains(id3));        // new one
}

TEST_CASE("Mempool: after eviction, evicted coin can be re-added") {
    Mempool mp(1);  // single slot

    Coin c0 = makeCoin(0x80);
    Coin c1 = makeCoin(0x81);

    TxPoW tx0 = makeTx(c0, 1);
    TxPoW tx1 = makeTx(c1, 2);  // evicts tx0

    mp.add(tx0);
    mp.add(tx1);  // tx0 evicted, c0 freed

    // Now c0 can be added again
    TxPoW tx0b = makeTx(c0, 3);
    // tx1 is in pool, tx0b uses different coin — should succeed
    // But pool is full → evicts tx1 first
    CHECK(mp.add(tx0b));
    CHECK(mp.size() == 1);
}
