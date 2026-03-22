# Minima Core C++ — Implementation Status

**Last updated:** 2026-03-22  
**Build:** ✅ 0 errors, 0 warnings (non-vendor)  
**Tests:** ✅ 19/19 suites — 0 failures

---

## Test Suites

| # | Suite | Module | Status |
|---|-------|--------|--------|
| 1 | test_mini_number | types/ | ✅ |
| 2 | test_mini_data | types/ | ✅ |
| 3 | test_kissvm | kissvm/ | ✅ |
| 4 | test_txpow | objects/ | ✅ |
| 5 | test_validation | validation/ | ✅ |
| 6 | test_mmr | mmr/ | ✅ |
| 7 | test_chain | chain/ | ✅ |
| 8 | test_token | objects/ | ✅ |
| 9 | test_mining | mining/ | ✅ |
| 10 | test_difficulty | chain/ | ✅ |
| 11 | test_network | network/ | ✅ |
| 12 | test_p2p_sync | network/ | ✅ |
| 13 | test_ibd | objects/ | ✅ |
| 14 | test_persistence | persistence/ | ✅ |
| 15 | test_cascade | chain/cascade/ | ✅ |
| 16 | test_wots | crypto/ | ✅ |
| 17 | test_treekey | crypto/ | ✅ |
| 18 | test_database | database/ | ✅ |
| 19 | test_processor | system/ | ✅ |
| **TOTAL** | **19 suites** | | **✅ 100%** |

---

## Module Status

| Module | Files | Java parity | Notes |
|--------|-------|-------------|-------|
| **Types** | MiniNumber · MiniData · MiniString | ✅ 1:1 | Wire-exact serialisation |
| **Objects — core** | TxPoW · TxHeader · TxBody · Coin · Witness · Transaction · Address · StateVariable | ✅ 1:1 | Wire-exact |
| **Objects — ext** | Token · TxBlock · Greeting · CoinProof · IBD · Magic | ✅ 1:1 | |
| **KISS VM** | Tokenizer · Parser · Interpreter · Environment · Contract · Functions (42+) | ✅ 1:1 | 1024 instr / 64 stack enforced |
| **Crypto — Hash** | SHA2-256 · SHA3-256 | ✅ | Bundled vendor headers |
| **Crypto — WOTS** | Winternitz.hpp (W=8, SHA3-256, 2880-byte keys/sigs) | ✅ 1:1 | Matches BouncyCastle pqc |
| **Crypto — TreeKey** | Merkle tree of WOTS keys, depth=12 | ✅ 1:1 | |
| **Serialization** | DataStream (Minima wire format, big-endian) | ✅ 1:1 | |
| **MMR** | MMRSet · MMREntry · MMRProof · MMRData | ✅ 1:1 | Peaks, proof verify |
| **Chain** | ChainState · ChainProcessor · DifficultyAdjust (256-block window) | ✅ 1:1 | |
| **Chain Tree** | TxPowTree · TxPoWTreeNode | ✅ 1:1 | Fork support, heaviest chain, reorg |
| **Cascade** | CascadeNode · Cascade | ✅ 1:1 | cascadeChain, serialise |
| **Mining** | TxPoWMiner · MiningManager | ✅ | Continuous loop, interruptible |
| **Network** | NIOMessage (24 types) · NIOServer · NIOClient · P2PSync | ✅ 1:1 | Wire-exact |
| **Persistence** | BlockStore (SQLite3) · UTxOStore | ✅ | SQLite3 embedded |
| **Validation** | TxPoWValidator | ✅ | PoW + scripts + MMR proofs + sigs |
| **Database** | MinimaDB (God object) · Wallet | ✅ | Central coordinator |
| **System** | MessageProcessor · TxPoWProcessor · TxPoWGenerator · TxPoWSearcher | ✅ | Full processing pipeline |
| **Node** | main.cpp | ✅ | Full-node entry point |
| **ARM toolchains** | cmake/toolchain-aarch64.cmake · toolchain-armv7.cmake | ✅ | CI cross-compile artifacts |

---

## CI Matrix

| Job | Platform | Compiler | Status |
|-----|----------|----------|--------|
| build-linux-x64 | Ubuntu 22.04 | GCC 11 | ✅ |
| build-linux-arm64 | Ubuntu 22.04 | aarch64-linux-gnu-g++ | ✅ |
| build-linux-armv7 | Ubuntu 22.04 | arm-linux-gnueabihf-g++ | ✅ |

---

## GAP Analysis (remaining)

| GAP | Items | Status |
|-----|-------|--------|
| GAP 1 — Wire compat | Signature · SignatureProof · ScriptProof · Pulse · MiniByte | ⚠️ Partial |
| GAP 5 — Genesis | GenesisCoin · GenesisMMR · GenesisTxPoW | ❌ TODO |
| GAP 6 — Utils | BIP39 mnemonics · MegaMMR | ❌ TODO |

