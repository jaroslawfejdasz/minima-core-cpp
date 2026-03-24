#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"

#include "../../src/chain/BlockStore.hpp"
#include "../../src/chain/ChainState.hpp"
#include "../../src/chain/UTxOSet.hpp"
#include "../../src/chain/ChainProcessor.hpp"
#include "../../src/objects/TxPoW.hpp"
#include "../../src/objects/Coin.hpp"
#include "../../src/objects/Transaction.hpp"
#include "../../src/objects/Address.hpp"
#include "../../src/crypto/Hash.hpp"

using namespace minima;
using namespace minima::chain;

// ─── helpers ────────────────────────────────────────────────────────────────

static MiniData makeHash(uint8_t b) {
    return MiniData(std::vector<uint8_t>(32, b));
}
static MiniData nativeToken() {
    return MiniData(std::vector<uint8_t>{0x00});
}

// Coin with explicit coinID (for inputs seeded into UTxO)
static Coin makeSeedCoin(const MiniData& addrHash, double amount, uint8_t coinSeed) {
    Address addr(addrHash);
    Coin c;
    c.setAddress(addr);
    c.setTokenID(nativeToken());
    c.setAmount(MiniNumber(amount));
    c.setMmrEntry(MiniNumber(int64_t(0)));
    c.setCreated(MiniNumber(int64_t(1)));
    c.setCoinID(MiniData(std::vector<uint8_t>(32, coinSeed)));
    return c;
}

// Coin with EMPTY coinID (output — coinID assigned by applyCoins)
static Coin makeOutputCoin(const MiniData& addrHash, double amount) {
    Address addr(addrHash);
    Coin c;
    c.setAddress(addr);
    c.setTokenID(nativeToken());
    c.setAmount(MiniNumber(amount));
    c.setMmrEntry(MiniNumber(int64_t(0)));
    c.setCreated(MiniNumber(int64_t(1)));
    // coinID = empty → assigned by ChainProcessor::applyCoins
    return c;
}

// Build TxPoW with easy difficulty + one input + one output
static TxPoW makeTxPoW(const Coin& inputCoin, const Coin& outputCoin, int64_t blockNum) {
    TxPoW t;
    t.header().blockNumber = MiniNumber(blockNum);
    t.header().timeMilli   = MiniNumber(int64_t(1000000 + blockNum));
    t.body().txnDifficulty = MiniData(std::vector<uint8_t>(32, 0xFF)); // easy
    t.body().txn.addInput(inputCoin);
    t.body().txn.addOutput(outputCoin);
    return t;
}

// Compute the coinID that applyCoins will assign to output[0] of a TxPoW
static MiniData computeExpectedCoinID(const TxPoW& t, uint32_t outputIdx = 0) {
    return Transaction::computeCoinID(t.computeID(), outputIdx);
}

// ─── BlockStore ─────────────────────────────────────────────────────────────

TEST_CASE("BlockStore: add and retrieve") {
    BlockStore store;
    TxPoW b;
    b.header().blockNumber = MiniNumber(int64_t(1));
    MiniData id = b.computeID();
    store.add(id, b);
    CHECK(store.has(id));
    auto opt = store.get(id);
    REQUIRE(opt.has_value());
    CHECK(opt->header().blockNumber == MiniNumber(int64_t(1)));
}

TEST_CASE("BlockStore: missing key returns nullopt") {
    BlockStore store;
    CHECK_FALSE(store.has(makeHash(0xAB)));
    CHECK_FALSE(store.get(makeHash(0xAB)).has_value());
}

TEST_CASE("BlockStore: multiple blocks") {
    BlockStore store;
    for (int i = 1; i <= 10; ++i) {
        TxPoW b;
        b.header().blockNumber = MiniNumber(int64_t(i));
        store.add(b.computeID(), b);
    }
    CHECK(store.size() == 10);
}

TEST_CASE("BlockStore: getByNumber") {
    BlockStore store;
    TxPoW b;
    b.header().blockNumber = MiniNumber(int64_t(7));
    MiniData id = b.computeID();
    store.add(id, b);
    auto opt = store.getByNumber(7);
    REQUIRE(opt.has_value());
    CHECK(opt->header().blockNumber == MiniNumber(int64_t(7)));
}

// ─── ChainState ──────────────────────────────────────────────────────────────

TEST_CASE("ChainState: initial state") {
    ChainState cs;
    CHECK(cs.getHeight() == 0);
    CHECK(cs.isGenesis());
}

TEST_CASE("ChainState: update tip") {
    ChainState cs;
    MiniData h = makeHash(0xBE);
    cs.setTip(h, 42);
    CHECK(cs.getHeight() == 42);
    CHECK(cs.getTip() == h);
}

// ─── UTxOSet ─────────────────────────────────────────────────────────────────

TEST_CASE("UTxOSet: add and retrieve coin") {
    UTxOSet utxo;
    Coin c = makeSeedCoin(makeHash(0x01), 100.0, 0xA1);
    utxo.addCoin(c);
    REQUIRE(utxo.hasCoin(c.coinID()));
    const Coin* pc = utxo.getCoin(c.coinID());
    REQUIRE(pc != nullptr);
    CHECK(pc->amount().getAsDouble() == doctest::Approx(100.0));
}

TEST_CASE("UTxOSet: spend coin removes it") {
    UTxOSet utxo;
    Coin c = makeSeedCoin(makeHash(0x02), 50.0, 0xA2);
    utxo.addCoin(c);
    utxo.spendCoin(c.coinID());
    CHECK_FALSE(utxo.hasCoin(c.coinID()));
    CHECK(utxo.getCoin(c.coinID()) == nullptr);
}

TEST_CASE("UTxOSet: getBalance — single coin") {
    UTxOSet utxo;
    MiniData addr = makeHash(0x03);
    Coin c = makeSeedCoin(addr, 250.0, 0xA3);
    utxo.addCoin(c);
    MiniNumber bal = utxo.getBalance(addr, nativeToken());
    CHECK(bal.getAsDouble() == doctest::Approx(250.0));
}

TEST_CASE("UTxOSet: getBalance — sum multiple coins") {
    UTxOSet utxo;
    MiniData addr = makeHash(0x04);
    Coin c1 = makeSeedCoin(addr, 100.0, 0xA4);
    Coin c2 = makeSeedCoin(addr, 200.0, 0xA5);
    utxo.addCoin(c1);
    utxo.addCoin(c2);
    CHECK(utxo.getBalance(addr, nativeToken()).getAsDouble() == doctest::Approx(300.0));
}

TEST_CASE("UTxOSet: getBalance — unknown address returns zero") {
    UTxOSet utxo;
    MiniNumber bal = utxo.getBalance(makeHash(0xAA), nativeToken());
    CHECK(bal.getAsDouble() == doctest::Approx(0.0));
}

// ─── ChainProcessor — processBlock (no-validation) ──────────────────────────

TEST_CASE("ChainProcessor: processBlock stores genesis") {
    ChainProcessor cp;
    TxPoW genesis;
    genesis.header().blockNumber = MiniNumber(int64_t(0));
    CHECK(cp.processBlock(genesis));
    CHECK(cp.getHeight() == 0);
    CHECK(cp.blockStore().size() == 1);
}

TEST_CASE("ChainProcessor: processBlock rejects duplicate") {
    ChainProcessor cp;
    TxPoW b;
    b.header().blockNumber = MiniNumber(int64_t(1));
    CHECK(cp.processBlock(b));
    CHECK_FALSE(cp.processBlock(b));
    CHECK(cp.blockStore().size() == 1);
}

TEST_CASE("ChainProcessor: processBlock updates tip") {
    ChainProcessor cp;
    for (int i = 0; i <= 5; ++i) {
        TxPoW b;
        b.header().blockNumber = MiniNumber(int64_t(i));
        cp.processBlock(b);
    }
    CHECK(cp.getHeight() == 5);
}

TEST_CASE("ChainProcessor: processBlock adds UTxO outputs with derived coinID") {
    ChainProcessor cp;
    MiniData addr = makeHash(0x10);
    Coin out = makeOutputCoin(addr, 1000.0);

    TxPoW b;
    b.header().blockNumber = MiniNumber(int64_t(1));
    b.body().txn.addOutput(out);

    MiniData expectedCoinID = computeExpectedCoinID(b, 0);
    cp.processBlock(b);

    CHECK(cp.utxoSet().hasCoin(expectedCoinID));
    CHECK(cp.utxoSet().getBalance(addr, nativeToken()).getAsDouble() == doctest::Approx(1000.0));
}

TEST_CASE("ChainProcessor: processBlock spends inputs") {
    ChainProcessor cp;
    MiniData addr = makeHash(0x11);

    // Block 1: create coin
    Coin out = makeOutputCoin(addr, 500.0);
    TxPoW b1;
    b1.header().blockNumber = MiniNumber(int64_t(1));
    b1.body().txn.addOutput(out);
    MiniData expectedCoinID = computeExpectedCoinID(b1, 0);
    cp.processBlock(b1);
    REQUIRE(cp.utxoSet().hasCoin(expectedCoinID));

    // Block 2: spend it — need coin with the derived coinID
    Coin spendCoin = makeOutputCoin(addr, 500.0);
    spendCoin.setCoinID(expectedCoinID);  // use derived ID

    TxPoW b2;
    b2.header().blockNumber = MiniNumber(int64_t(2));
    b2.body().txn.addInput(spendCoin);
    cp.processBlock(b2);
    CHECK_FALSE(cp.utxoSet().hasCoin(expectedCoinID));
}

// ─── ChainProcessor::process() — full validation pipeline ───────────────────

TEST_CASE("ChainProcessor::process: accepts valid txpow with easy difficulty") {
    ChainProcessor cp;
    MiniData addr = makeHash(0x40);

    Coin inCoin = makeSeedCoin(addr, 100.0, 0xC1);
    cp.utxoSet().addCoin(inCoin);

    Coin outCoin = makeOutputCoin(addr, 100.0);
    TxPoW t = makeTxPoW(inCoin, outCoin, 1);
    MiniData expectedCoinID = computeExpectedCoinID(t, 0);

    auto r = cp.process(t);
    CHECK(r.accepted);
    if (r.accepted) {
        CHECK_FALSE(cp.utxoSet().hasCoin(inCoin.coinID()));
        CHECK(cp.utxoSet().hasCoin(expectedCoinID));
        CHECK(cp.getHeight() == 1);
    }
}

TEST_CASE("ChainProcessor::process: rejects duplicate txpow") {
    ChainProcessor cp;
    MiniData addr = makeHash(0x41);

    Coin inCoin = makeSeedCoin(addr, 100.0, 0xD1);
    cp.utxoSet().addCoin(inCoin);

    Coin outCoin = makeOutputCoin(addr, 100.0);
    TxPoW t = makeTxPoW(inCoin, outCoin, 1);

    auto r1 = cp.process(t);
    REQUIRE(r1.accepted);

    auto r2 = cp.process(t);
    CHECK_FALSE(r2.accepted);
    CHECK(r2.reason.find("duplicate") != std::string::npos);
}

TEST_CASE("ChainProcessor::process: rejects hard difficulty") {
    ChainProcessor cp;
    MiniData addr = makeHash(0x42);

    Coin inCoin = makeSeedCoin(addr, 100.0, 0xE1);
    cp.utxoSet().addCoin(inCoin);

    Coin outCoin = makeOutputCoin(addr, 100.0);
    TxPoW t;
    t.header().blockNumber = MiniNumber(int64_t(1));
    t.header().timeMilli   = MiniNumber(int64_t(1000001));
    t.body().txnDifficulty = MiniData(std::vector<uint8_t>(32, 0x00)); // impossible
    t.body().txn.addInput(inCoin);
    t.body().txn.addOutput(outCoin);

    auto r = cp.process(t);
    CHECK_FALSE(r.accepted);
    CHECK(r.reason.find("validation failed") != std::string::npos);
}

TEST_CASE("ChainProcessor::process: rejects double spend") {
    ChainProcessor cp;
    MiniData addr = makeHash(0x50);

    Coin inCoin = makeSeedCoin(addr, 100.0, 0xF1);
    cp.utxoSet().addCoin(inCoin);

    // First spend
    Coin outA = makeOutputCoin(addr, 100.0);
    TxPoW t1 = makeTxPoW(inCoin, outA, 1);
    auto r1 = cp.process(t1);
    REQUIRE(r1.accepted);

    // Second spend of same input (already gone from UTxO)
    Coin outB = makeOutputCoin(addr, 100.0);
    TxPoW t2 = makeTxPoW(inCoin, outB, 2);
    auto r2 = cp.process(t2);
    CHECK_FALSE(r2.accepted);
}

TEST_CASE("ChainProcessor: bootstrap genesis then process child block") {
    ChainProcessor cp;
    MiniData addr = makeHash(0x60);

    // Genesis via processBlock (coinbase, no validation)
    Coin genesis_out = makeOutputCoin(addr, 1000000.0);
    TxPoW genesis;
    genesis.header().blockNumber = MiniNumber(int64_t(0));
    genesis.body().txn.addOutput(genesis_out);
    MiniData genesis_coinID = computeExpectedCoinID(genesis, 0);
    cp.processBlock(genesis);
    REQUIRE(cp.utxoSet().hasCoin(genesis_coinID));

    // Block 1 via process() — spend genesis output
    Coin spender = makeOutputCoin(addr, 1000000.0);
    spender.setCoinID(genesis_coinID);  // use derived ID

    Coin child_out = makeOutputCoin(addr, 1000000.0);

    TxPoW blk1;
    blk1.header().blockNumber       = MiniNumber(int64_t(1));
    blk1.header().timeMilli         = MiniNumber(int64_t(1000001));
    blk1.body().txnDifficulty       = MiniData(std::vector<uint8_t>(32, 0xFF));
    blk1.body().txn.addInput(spender);
    blk1.body().txn.addOutput(child_out);
    MiniData child_coinID = computeExpectedCoinID(blk1, 0);

    auto r = cp.process(blk1);
    CHECK(r.accepted);
    if (r.accepted) {
        CHECK_FALSE(cp.utxoSet().hasCoin(genesis_coinID));
        CHECK(cp.utxoSet().hasCoin(child_coinID));
        CHECK(cp.getHeight() == 1);
    }
}

TEST_CASE("ChainProcessor: multi-block chain (5 blocks)") {
    ChainProcessor cp;
    MiniData addr = makeHash(0x70);

    // Seed genesis coin via processBlock
    Coin coin0 = makeOutputCoin(addr, 1000.0);
    TxPoW genesis;
    genesis.header().blockNumber = MiniNumber(int64_t(0));
    genesis.body().txn.addOutput(coin0);
    MiniData prevCoinID = computeExpectedCoinID(genesis, 0);
    cp.processBlock(genesis);

    // 5 blocks via process()
    for (int i = 1; i <= 5; ++i) {
        // Build spender coin from prev coinID
        Coin spender = makeOutputCoin(addr, 1000.0);
        spender.setCoinID(prevCoinID);

        Coin nextOut = makeOutputCoin(addr, 1000.0);

        TxPoW blk;
        blk.header().blockNumber  = MiniNumber(int64_t(i));
        blk.header().timeMilli    = MiniNumber(int64_t(1000000 + i));
        blk.body().txnDifficulty  = MiniData(std::vector<uint8_t>(32, 0xFF));
        blk.body().txn.addInput(spender);
        blk.body().txn.addOutput(nextOut);

        MiniData nextCoinID = computeExpectedCoinID(blk, 0);
        auto r = cp.process(blk);
        CHECK(r.accepted);
        prevCoinID = nextCoinID;
    }

    CHECK(cp.getHeight() == 5);
    CHECK(cp.utxoSet().hasCoin(prevCoinID));
    CHECK(cp.utxoSet().size() == 1); // only last coin
}

TEST_CASE("ChainProcessor: balance check — rejects output > input") {
    ChainProcessor cp;
    MiniData addr = makeHash(0x80);

    Coin inCoin = makeSeedCoin(addr, 100.0, 0x81);
    cp.utxoSet().addCoin(inCoin);

    // Output > input — inflation attack
    Coin outCoin = makeOutputCoin(addr, 200.0);
    TxPoW t = makeTxPoW(inCoin, outCoin, 1);

    auto r = cp.process(t);
    CHECK_FALSE(r.accepted);
}
