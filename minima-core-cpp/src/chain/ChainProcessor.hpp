#pragma once
/**
 * ChainProcessor — validates and integrates TxPoWs into the chain.
 * Java ref: src/org/minima/system/brains/TxPoWProcessor.java
 *
 * Pipeline (per TxPoW):
 *   1. Duplicate check   — reject already-seen IDs
 *   2. TxPoWValidator    — PoW, scripts, balance, coinIDs, state, size
 *   3. Store block       — BlockStore
 *   4. Apply coins       — UTxOSet: remove inputs, add outputs
 *   5. Update tip        — if blockNumber > current tip
 *
 * CoinLookup for the validator is wired to the UTxOSet automatically.
 */

#include "BlockStore.hpp"
#include "ChainState.hpp"
#include "UTxOSet.hpp"
#include "../objects/TxPoW.hpp"
#include "../objects/Transaction.hpp"
#include "../types/MiniData.hpp"
#include "../validation/TxPoWValidator.hpp"
#include <string>
#include <optional>

namespace minima::chain {

struct ProcessResult {
    bool        accepted{false};
    std::string reason;   // empty on success; error message on rejection

    static ProcessResult ok()                          { return {true, ""}; }
    static ProcessResult reject(const std::string& r)  { return {false, r}; }
};

class ChainProcessor {
public:
    ChainProcessor()
        : m_validator([this](const MiniData& coinID) -> const Coin* {
              return m_utxo.getCoin(coinID);
          })
    {}

    /**
     * Full pipeline: validate + store + apply.
     * Returns ProcessResult::ok() if accepted.
     */
    ProcessResult process(const TxPoW& txpow) {
        const MiniData id = txpow.computeID();

        // 1. Duplicate check
        if (m_store.has(id))
            return ProcessResult::reject("duplicate txpow");

        // 2. Validate (PoW + scripts + balance + coinIDs + state + size)
        auto vr = m_validator.validate(txpow);
        if (!vr.valid)
            return ProcessResult::reject("validation failed: " + vr.error);

        // 3. Store
        m_store.add(id, txpow);

        // 4. Apply coins
        applyCoins(txpow);

        // 5. Update tip
        int64_t bn = txpow.header().blockNumber.getAsLong();
        if (bn >= m_state.getHeight())
            m_state.setTip(id, bn);

        return ProcessResult::ok();
    }

    /**
     * processBlock() — legacy alias, skips validation (for bootstrap/genesis).
     * Use process() for normal operation.
     */
    bool processBlock(const TxPoW& txpow) {
        const MiniData id = txpow.computeID();
        if (m_store.has(id)) return false;
        m_store.add(id, txpow);
        applyCoins(txpow);
        int64_t bn = txpow.header().blockNumber.getAsLong();
        if (bn >= m_state.getHeight())
            m_state.setTip(id, bn);
        return true;
    }

    // Accessors
    int64_t       getHeight()   const { return m_state.getHeight(); }
    MiniData      getTip()      const { return m_state.getTip();    }
    BlockStore&   blockStore()        { return m_store;  }
    UTxOSet&      utxoSet()           { return m_utxo;   }

    /**
     * reorg() — given a fork tip, find the common ancestor and
     * reorganise the UTxO set to the new chain.
     * Returns false if the fork tip is not in BlockStore.
     */
    bool reorg(const MiniData& forkTip, size_t maxDepth = 100) {
        // Collect the fork chain
        std::vector<TxPoW> forkChain;
        if (!m_store.getAncestors(forkTip, maxDepth, forkChain))
            return false;

        // Collect the main chain from current tip
        std::vector<TxPoW> mainChain;
        if (!m_state.getTip().bytes().empty())
            m_store.getAncestors(m_state.getTip(), maxDepth, mainChain);

        // Find common ancestor (by block number / ID)
        MiniData commonAncestorID;
        int64_t  forkHeight = forkChain.empty() ? 0
                              : forkChain.front().header().blockNumber.getAsLong();

        // Undo main chain back to common ancestor
        for (const auto& blk : mainChain) {
            MiniData blkID = blk.computeID();
            // Check if this block exists in fork ancestry
            bool isCommon = false;
            for (const auto& fb : forkChain) {
                if (fb.computeID().bytes() == blkID.bytes()) {
                    isCommon = true;
                    break;
                }
            }
            if (isCommon) {
                commonAncestorID = blkID;
                break;
            }
            // Undo: re-add inputs, remove outputs
            for (const auto& c : blk.body().txn.outputs())
                m_utxo.spendCoin(c.coinID());
            for (const auto& c : blk.body().txn.inputs())
                m_utxo.addCoin(c);
        }

        // Apply fork chain forward (skip up to common ancestor)
        bool pastCommon = commonAncestorID.bytes().empty();
        for (int i = (int)forkChain.size() - 1; i >= 0; --i) {
            const auto& blk = forkChain[i];
            if (!pastCommon) {
                if (blk.computeID().bytes() == commonAncestorID.bytes())
                    pastCommon = true;
                continue;
            }
            applyCoins(blk);
        }

        // Update tip
        m_state.setTip(forkTip, forkHeight);
        return true;
    }

private:
    BlockStore                     m_store;
    ChainState                     m_state;
    UTxOSet                        m_utxo;
    minima::validation::TxPoWValidator m_validator;

    void applyCoins(const TxPoW& txpow) {
        // Spend inputs
        for (const auto& c : txpow.body().txn.inputs())
            m_utxo.spendCoin(c.coinID());

        // Add outputs — assign canonical coinID before storing
        const MiniData txID = txpow.computeID();
        const auto& outputs = txpow.body().txn.outputs();
        for (size_t i = 0; i < outputs.size(); ++i) {
            Coin coin = outputs[i];
            // Always derive coinID from txID + output index (Java: Coin.getDerivedCoinID)
            MiniData coinID = Transaction::computeCoinID(txID, static_cast<uint32_t>(i));
            coin.setCoinID(coinID);
            m_utxo.addCoin(coin);
        }
    }
};

} // namespace minima::chain
