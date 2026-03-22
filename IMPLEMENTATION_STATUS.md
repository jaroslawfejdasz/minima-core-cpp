# Minima Core C++ — Implementation Status

**Last updated:** 2026-03-22  
**Build:** ✅ 0 errors, 0 warnings (non-vendor)  
**Tests:** ✅ 17/17 suites — 0 failures

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
| **TOTAL** | **17 suites** | | **✅ 100%** |

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
| **Cascade** | CascadeNode · Cascade | ✅ 1:1 | cascadeChain, serialise |
| **Mining** | TxPoWMiner · MiningManager | ✅ | Continuous loop, interruptible |
| **Network** | NIOMessage (24 types) · NIOServer · NIOClient · P2PSync | ✅ 1:1 | Wire-exact |
| **Persistence** | BlockStore (SQLite3) · UTxOStore | ✅ | SQLite3 embedded |
| **Validation** | TxPoWValidator | ✅ | PoW + scripts + MMR proofs + sigs |
| **Node** | main.cpp | ✅ | Full-node entry point |
| **ARM toolchains** | cmake/toolchain-aarch64.cmake · toolchain-armv7.cmake | ✅ | CI cross-compile artifacts |

---

## CI Matrix

| Job | Platform | Compiler | Status |
|-----|----------|----------|--------|
| build-and-test | Ubuntu 22.04 | GCC 11 | ✅ |
| build-and-test | Ubuntu 24.04 | GCC 13 | ✅ |
| clang-build | Ubuntu 22.04 | Clang 15 | ✅ |
| cross-aarch64 | — (cross) | aarch64-linux-gnu GCC | ✅ artifact |
| cross-armv7 | — (cross) | armv7-linux-gnueabihf GCC | ✅ artifact |

---

## Known Limitations

| # | Issue | Notes |
|---|-------|-------|
| 1 | Schnorr/secp256k1 | Stub only — Winternitz is the real signing scheme |
| 2 | ARM binaries not QEMU-tested | Cross-compiled; execution not verified in CI |
| 3 | Maxima / Omnia (L2) | Out of scope for core protocol |
| 4 | P2P peer handshake | NIO layer present; full integration pending |

---

## TODO — Next Milestones

| Priority | Task | Notes |
|---|---|---|
| 🔥 1 | npm publish — monorepo packages | minima-test, kiss-vm-lint, minima-contracts, create-minidapp |
| 🔥 2 | Persistence integration | Wire BlockStore + UTxOStore into ChainProcessor |
| 3 | Network integration | Complete NIOServer peer handshake + P2P loop |
| 4 | IBD (Initial Block Download) | Request/response, flood-fill propagation |
| 5 | ARM QEMU in CI | Execute cross-compiled binaries in CI with QEMU |
