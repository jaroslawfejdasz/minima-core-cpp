# Minima Core C++ — Implementation Status

**Last updated:** 2026-03-22  
**Build:** ✅ 0 errors, 0 warnings (non-vendor)  
**Tests:** ✅ 24/24 suites — 0 failures

---

## Test Suites

| # | Suite | Module | Tests | Status |
|---|-------|--------|-------|--------|
| 1 | test_mini_number | types/ | — | ✅ |
| 2 | test_mini_data | types/ | — | ✅ |
| 3 | test_kissvm | kissvm/ | — | ✅ |
| 4 | test_txpow | objects/ | — | ✅ |
| 5 | test_validation | validation/ | — | ✅ |
| 6 | test_mmr | mmr/ | — | ✅ |
| 7 | test_chain | chain/ | — | ✅ |
| 8 | test_token | objects/ | — | ✅ |
| 9 | test_mining | mining/ | — | ✅ |
| 10 | test_difficulty | chain/ | — | ✅ |
| 11 | test_network | network/ | — | ✅ |
| 12 | test_p2p_sync | network/ | — | ✅ |
| 13 | test_ibd | objects/ | — | ✅ |
| 14 | test_persistence | persistence/ | — | ✅ |
| 15 | test_cascade | chain/cascade/ | — | ✅ |
| 16 | test_wots | crypto/ | — | ✅ |
| 17 | test_treekey | crypto/ | — | ✅ |
| 18 | test_database | database/ | — | ✅ |
| 19 | test_processor | system/ | 20 | ✅ |
| 20 | test_megammr | mmr/ | — | ✅ |
| 21 | test_genesis | objects/ | — | ✅ |
| 22 | test_bip39 | crypto/ | 16 | ✅ |
| 23 | test_witness_wire | objects/ | — | ✅ |
| 24 | **test_integration** | **integration** | **12** | ✅ |
| **TOTAL** | **24 suites** | | **39+ assertions** | **✅ 100%** |

---

## Module Status

| Module | Files | Java parity | Notes |
|--------|-------|-------------|-------|
| **Types** | MiniNumber · MiniData · MiniString | ✅ 1:1 | Wire-exact |
| **Objects** | TxPoW · TxHeader · TxBody · Coin · Witness · Transaction · Address · StateVariable · Token · TxBlock · Greeting · CoinProof · IBD · Magic · MiniByte · Pulse | ✅ 1:1 | Wire-exact |
| **KISS VM** | Tokenizer · Parser · Interpreter · Environment · Contract · Functions (42+) | ✅ 1:1 | 1024 instr / 64 stack |
| **Crypto** | SHA2-256 · SHA3-256 · Winternitz OTS (W=8) · TreeKey · BIP39 | ✅ 1:1 | Quantum-resistant |
| **Serialization** | DataStream | ✅ 1:1 | Minima wire format |
| **MMR** | MMRSet · MMREntry · MMRProof · MMRData · MegaMMR | ✅ 1:1 | |
| **Chain** | ChainState · ChainProcessor · DifficultyAdjust · TxPowTree · TxPoWTreeNode · Cascade | ✅ 1:1 | Fork support, reorg |
| **Mining** | TxPoWMiner · MiningManager | ✅ | setNextBlock() integration |
| **Network** | NIOMessage (24 types) · NIOServer · NIOClient · P2PSync | ✅ 1:1 | Wire-exact |
| **Persistence** | BlockStore (SQLite3) · UTxOStore | ✅ | |
| **Validation** | TxPoWValidator | ✅ | PoW + scripts + MMR + sigs |
| **Database** | MinimaDB (God Object) · Wallet | ✅ | Central coordinator |
| **System** | MessageProcessor · TxPoWProcessor · TxPoWGenerator · TxPoWSearcher | ✅ | Full async pipeline |
| **Genesis** | makeGenesisCoin · makeGenesisMMR · makeGenesisTxPoW · isGenesisBlock | ✅ 1:1 | Deterministic |
| **Node** | main.cpp | ✅ | Full-node entry point w/ integration |

---

## Integration Architecture

```
              ┌─────────────┐
              │   main.cpp   │  CLI args → NodeConfig
              └──────┬──────┘
                     │
         ┌───────────┼───────────────┐
         │           │               │
    ┌────▼────┐ ┌────▼────┐  ┌──────▼──────┐
    │MinimaDB │ │ SQLite  │  │  P2PSync    │
    │God Obj  │ │Persist  │  │  (network)  │
    └────┬────┘ └────┬────┘  └──────┬──────┘
         │           │               │
    ┌────▼────────────────────┐      │
    │   TxPoWProcessor        │◄─────┘
    │   (async message queue) │
    └────┬────────────────────┘
         │ ACCEPTED
    ┌────▼────┐    ┌──────────────┐
    │TxPowTree│    │TxPoWGenerator│──► MiningManager
    │BlockStore│   │(next template)│
    └─────────┘    └──────────────┘
```

## CI Matrix

| Job | Platform | Compiler | Status |
|-----|----------|----------|--------|
| build-linux-x64 | Ubuntu 22.04 | GCC 11 | ✅ |
| build-linux-arm64 | Ubuntu 22.04 | aarch64-linux-gnu-g++ | ✅ |
| build-linux-armv7 | Ubuntu 22.04 | arm-linux-gnueabihf-g++ | ✅ |

---

## TODO (next priorities)

| # | Task | Priority |
|---|------|----------|
| 1 | Persistence replay — pełny DB restore z SQLite w bootstrapGenesis | HIGH |
| 2 | MMR rebuild po reorg — TxPoWProcessor.updateMMRIfTip() | HIGH |
| 3 | npm publish — monorepo packages (minima-test, kiss-vm-lint) | MEDIUM |
| 4 | P2P Greeting — wysyłanie własnego Greeting do seed node | MEDIUM |
| 5 | Cascade integration — MinimaDB.addBlock() → cascade trim | LOW |
