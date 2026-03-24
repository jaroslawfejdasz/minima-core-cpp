#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../vendor/doctest/doctest.h"

#include "../../src/chain/BlockStore.hpp"
#include "../../src/chain/ChainState.hpp"
#include "../../src/chain/UTxOSet.hpp"
#include "../../src/chain/ChainProcessor.hpp"
#include "../../src/objects/TxPoW.hpp"
#include "../../src/objects/Coin.hpp"
#include "../../src/objects/Transaction.hpp"
#include "../../src/objects/Witness.hpp"
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

// SHA3("RETURN TRUE") — precomputed for test address
static MiniData returnTrueAddress() {
    static const std::string script = "RETURN TRUE";
    static MiniData addr = crypto::Hash::sha3_256(
        std::vector<uint8_t>(script.begin(), script.end()));
    return addr;
}

// Witness with "RETURN TRUE" script for the canonical address
static Witness returnTrueWitness() {
    Witness w;
    ScriptProof sp;
    sp.script  = MiniString("RETURN TRUE");
    sp.address = returnTrueAddress();
    w.addScript(sp);
    return w;
}

// Coin with explicit coinID (for seeding UTxO — inputs)
static Coin makeSeedCoin(double amount, uint8_t coinSeed) {
    Address addr(returnTrueAddress());
    Coin c;
    c.setAddress(addr);
    c.setTokenID(nativeToken());
    c.setAmount(MiniNumber(amount));
    c.setMmrEntry(MiniNumber(int64_t(0)));
    c.setCreated(MiniNumber(int64_t(1)));
    c.setCoinID(MiniData(std::vector<uint8_t>(32, coinSeed)));
    return c;
}

// Coin with EMPTY coinID (output — assigned by applyCoins)
static Coin makeOutputCoin(double amount) {
    Address addr(returnTrueAddress());
    Coin c;
    c.setAddress(addr);
    c.setTokenID(nativeToken());
    c.setAmount(MiniNumber(amount));
    c.setMmrEntry(MiniNumber(int64_t(0)));
    c.setCreated(MiniNumber(int64_t(1)));
    return c;
}

// Build TxPoW with easy difficulty, proper witness, input + output
static TxPoW makeTxPoW(const Coin& inputCoin, const Coin& outputCoin, int64_t blockNum) {
    TxPoW t;
    t.header().blockNumber = MiniNumber(blockNum);
    t.header().timeMilli   = MiniNumber(int64_t(1000000 + blockNum));
    t.body().txnDifficulty = MiniData(std::vector<uint8_t>(32, 0xFF));
    t.body().txn.addInput(inputCoin);
    t.body().txn.addOutput(outputCoin);
    t.body().witness = returnTrueWitness();
    return t;
}

// Compute coinID that applyCoins will derive for output[idx]
static MiniData expectedCoinID(const TxPoW& t, uint32_t idx = 0) {
    return Transaction::computeCoinID(t.computeID(), idx);
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
    Coin c = makeSeedCoin(100.0, 0xA1);
    utxo.addCoin(c);
    REQUIRE(utxo.hasCoin(c.coinID()));
    const Coin* pc = utxo.getCoin(c.coinID());
    REQUIRE(pc != nullptr);
    CHECK(pc->amount().getAsDouble() == doctest::Approx(100.0));
}

TEST_CASE("UTxOSet: spend coin removes it") {
    UTxOSet utxo;
    Coin c = makeSeedCoin(50.0, 0xA2);
    utxo.addCoin(c);
    utxo.spendCoin(c.coinID());
    CHECK_FALSE(utxo.hasCoin(c.coinID()));
    CHECK(utxo.getCoin(c.coinID()) == nullptr);
}

TEST_CASE("UTxOSet: getBalance — single coin") {
    UTxOSet utxo;
    Coin c = makeSeedCoin(250.0, 0xA3);
    utxo.addCoin(c);
    MiniNumber bal = utxo.getBalance(returnTrueAddress(), nativeToken());
    CHECK(bal.getAsDouble() == doctest::Approx(250.0));
}

TEST_CASE("UTxOSet: getBalance — sum multiple coins") {
    UTxOSet utxo;
    Coin c1 = makeSeedCoin(100.0, 0xA4);
    Coin c2 = makeSeedCoin(200.0, 0xA5);
    utxo.addCoin(c1);
    utxo.addCoin(c2);
    CHECK(utxo.getBalance(returnTrueAddress(), nativeToken()).getAsDouble()
          == doctest::Approx(300.0));
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

TEST_CASE("ChainProcessor: processBlock assigns derived coinID to outputs") {
    ChainProcessor cp;
    Coin out = makeOutputCoin(1000.0);
    TxPoW b;
    b.header().blockNumber = MiniNumber(int64_t(1));
    b.body().txn.addOutput(out);
    MiniData cid = expectedCoinID(b, 0);
    cp.processBlock(b);
    CHECK(cp.utxoSet().hasCoin(cid));
    CHECK(cp.utxoSet().getBalance(returnTrueAddress(), nativeToken()).getAsDouble()
          == doctest::Approx(1000.0));
}

TEST_CASE("ChainProcessor: processBlock spends inputs") {
    ChainProcessor cp;
    // Block 1: create coin
    Coin out = makeOutputCoin(500.0);
    TxPoW b1;
    b1.header().blockNumber = MiniNumber(int64_t(1));
    b1.body().txn.addOutput(out);
    MiniData cid = expectedCoinID(b1, 0);
    cp.processBlock(b1);
    REQUIRE(cp.utxoSet().hasCoin(cid));

    // Block 2: spend it
    Coin spender = makeOutputCoin(500.0);
    spender.setCoinID(cid);
    TxPoW b2;
    b2.header().blockNumber = MiniNumber(int64_t(2));
    b2.body().txn.addInput(spender);
    cp.processBlock(b2);
    CHECK_FALSE(cp.utxoSet().hasCoin(cid));
}

// ─── ChainProcessor::process() — full validation pipeline ───────────────────

TEST_CASE("ChainProcessor::process: accepts valid txpow") {
    ChainProcessor cp;
    Coin inCoin = makeSeedCoin(100.0, 0xC1);
    cp.utxoSet().addCoin(inCoin);

    Coin outCoin = makeOutputCoin(100.0);
    TxPoW t = makeTxPoW(inCoin, outCoin, 1);
    MiniData cid = expectedCoinID(t, 0);

    auto r = cp.process(t);
    CHECK(r.accepted);
    if (r.accepted) {
        CHECK_FALSE(cp.utxoSet().hasCoin(inCoin.coinID()));
        CHECK(cp.utxoSet().hasCoin(cid));
        CHECK(cp.getHeight() == 1);
    }
}

TEST_CASE("ChainProcessor::process: rejects duplicate") {
    ChainProcessor cp;
    Coin inCoin = makeSeedCoin(100.0, 0xD1);
    cp.utxoSet().addCoin(inCoin);

    TxPoW t = makeTxPoW(inCoin, makeOutputCoin(100.0), 1);
    auto r1 = cp.process(t);
    REQUIRE(r1.accepted);

    auto r2 = cp.process(t);
    CHECK_FALSE(r2.accepted);
    CHECK(r2.reason.find("duplicate") != std::string::npos);
}

TEST_CASE("ChainProcessor::process: rejects impossible PoW difficulty") {
    ChainProcessor cp;
    Coin inCoin = makeSeedCoin(100.0, 0xE1);
    cp.utxoSet().addCoin(inCoin);

    TxPoW t;
    t.header().blockNumber = MiniNumber(int64_t(1));
    t.header().timeMilli   = MiniNumber(int64_t(1000001));
    t.body().txnDifficulty = MiniData(std::vector<uint8_t>(32, 0x00)); // impossible
    t.body().txn.addInput(inCoin);
    t.body().txn.addOutput(makeOutputCoin(100.0));
    t.body().witness = returnTrueWitness();

    auto r = cp.process(t);
    CHECK_FALSE(r.accepted);
    CHECK(r.reason.find("validation failed") != std::string::npos);
}

TEST_CASE("ChainProcessor::process: rejects double spend") {
    ChainProcessor cp;
    Coin inCoin = makeSeedCoin(100.0, 0xF1);
    cp.utxoSet().addCoin(inCoin);

    auto r1 = cp.process(makeTxPoW(inCoin, makeOutputCoin(100.0), 1));
    REQUIRE(r1.accepted);

    // Same input, different output — input already spent
    auto r2 = cp.process(makeTxPoW(inCoin, makeOutputCoin(100.0), 2));
    CHECK_FALSE(r2.accepted);
}

TEST_CASE("ChainProcessor::process: rejects inflation (output > input)") {
    ChainProcessor cp;
    Coin inCoin = makeSeedCoin(100.0, 0x81);
    cp.utxoSet().addCoin(inCoin);

    auto r = cp.process(makeTxPoW(inCoin, makeOutputCoin(200.0), 1));
    CHECK_FALSE(r.accepted);
}

TEST_CASE("ChainProcessor: bootstrap genesis then process child block") {
    ChainProcessor cp;

    // Genesis via processBlock (coinbase, no validation)
    Coin genesis_out = makeOutputCoin(1000000.0);
    TxPoW genesis;
    genesis.header().blockNumber = MiniNumber(int64_t(0));
    genesis.body().txn.addOutput(genesis_out);
    MiniData genesis_cid = expectedCoinID(genesis, 0);
    cp.processBlock(genesis);
    REQUIRE(cp.utxoSet().hasCoin(genesis_cid));

    // Block 1 via process() — spend genesis output
    Coin spender = makeOutputCoin(1000000.0);
    spender.setCoinID(genesis_cid);

    TxPoW blk1;
    blk1.header().blockNumber  = MiniNumber(int64_t(1));
    blk1.header().timeMilli    = MiniNumber(int64_t(1000001));
    blk1.body().txnDifficulty  = MiniData(std::vector<uint8_t>(32, 0xFF));
    blk1.body().txn.addInput(spender);
    blk1.body().txn.addOutput(makeOutputCoin(1000000.0));
    blk1.body().witness = returnTrueWitness();

    MiniData child_cid = expectedCoinID(blk1, 0);
    auto r = cp.process(blk1);
    CHECK(r.accepted);
    if (r.accepted) {
        CHECK_FALSE(cp.utxoSet().hasCoin(genesis_cid));
        CHECK(cp.utxoSet().hasCoin(child_cid));
        CHECK(cp.getHeight() == 1);
    }
}

TEST_CASE("ChainProcessor: multi-block chain (5 blocks)") {
    ChainProcessor cp;

    // Genesis coin
    Coin coin0 = makeOutputCoin(1000.0);
    TxPoW genesis;
    genesis.header().blockNumber = MiniNumber(int64_t(0));
    genesis.body().txn.addOutput(coin0);
    MiniData prevCID = expectedCoinID(genesis, 0);
    cp.processBlock(genesis);

    // 5 blocks, each spending previous output
    for (int i = 1; i <= 5; ++i) {
        Coin spender = makeOutputCoin(1000.0);
        spender.setCoinID(prevCID);

        TxPoW blk;
        blk.header().blockNumber  = MiniNumber(int64_t(i));
        blk.header().timeMilli    = MiniNumber(int64_t(1000000 + i));
        blk.body().txnDifficulty  = MiniData(std::vector<uint8_t>(32, 0xFF));
        blk.body().txn.addInput(spender);
        blk.body().txn.addOutput(makeOutputCoin(1000.0));
        blk.body().witness = returnTrueWitness();

        MiniData nextCID = expectedCoinID(blk, 0);
        auto r = cp.process(blk);
        CHECK(r.accepted);
        prevCID = nextCID;
    }

    CHECK(cp.getHeight() == 5);
    CHECK(cp.utxoSet().hasCoin(prevCID));
    CHECK(cp.utxoSet().size() == 1);
}

// ─── ChainProcessor ↔ Mempool integration ────────────────────────────────────

TEST_CASE("ChainProcessor: process() clears conflicting mempool entries") {
    ChainProcessor cp;

    // Seed UTxO with two coins
    Coin c1 = makeSeedCoin(100.0, 0x90);
    Coin c2 = makeSeedCoin(100.0, 0x91);
    cp.utxoSet().addCoin(c1);
    cp.utxoSet().addCoin(c2);

    // Add a pending tx spending c1 to mempool
    TxPoW pending;
    pending.header().blockNumber = MiniNumber(int64_t(99));
    pending.header().timeMilli   = MiniNumber(int64_t(1099000));
    pending.body().txnDifficulty = MiniData(std::vector<uint8_t>(32, 0xFF));
    pending.body().txn.addInput(c1);
    pending.body().witness = returnTrueWitness();
    MiniData pendingID = pending.computeID();
    cp.mempool().add(pending);
    REQUIRE(cp.mempool().contains(pendingID));

    // Now process a block that also spends c1
    TxPoW blk = makeTxPoW(c1, makeOutputCoin(100.0), 1);
    auto r = cp.process(blk);
    CHECK(r.accepted);

    // Mempool entry must be evicted (conflicts with accepted block)
    CHECK_FALSE(cp.mempool().contains(pendingID));
}

TEST_CASE("ChainProcessor: process() leaves non-conflicting mempool entries") {
    ChainProcessor cp;

    Coin c1 = makeSeedCoin(100.0, 0x92);
    Coin c2 = makeSeedCoin(100.0, 0x93);
    cp.utxoSet().addCoin(c1);
    cp.utxoSet().addCoin(c2);

    // Pending tx spends c2 (different coin)
    TxPoW pending;
    pending.header().blockNumber = MiniNumber(int64_t(99));
    pending.header().timeMilli   = MiniNumber(int64_t(1099000));
    pending.body().txnDifficulty = MiniData(std::vector<uint8_t>(32, 0xFF));
    pending.body().txn.addInput(c2);
    pending.body().witness = returnTrueWitness();
    MiniData pendingID = pending.computeID();
    cp.mempool().add(pending);

    // Process block spending c1 (no conflict with pending)
    TxPoW blk = makeTxPoW(c1, makeOutputCoin(100.0), 1);
    auto r = cp.process(blk);
    CHECK(r.accepted);

    // Pending tx (spending c2) must still be in pool
    CHECK(cp.mempool().contains(pendingID));
}
