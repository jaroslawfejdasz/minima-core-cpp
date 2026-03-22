<div align="center">
  <h1>⛓️ minima-core-cpp</h1>
  <p><strong>C++20 reimplementation of the Minima blockchain core protocol</strong></p>
  <p>
    UTxO model &nbsp;·&nbsp;
    TxPoW consensus &nbsp;·&nbsp;
    KISS VM smart contracts &nbsp;·&nbsp;
    Winternitz OTS post-quantum signatures &nbsp;·&nbsp;
    MMR accumulator
  </p>

  [![CI](https://github.com/jaroslawfejdasz/-KISS-VM-interpreter/actions/workflows/ci.yml/badge.svg)](https://github.com/jaroslawfejdasz/-KISS-VM-interpreter/actions)
  [![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)
  [![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CMakeLists.txt)
  [![Tests](https://img.shields.io/badge/tests-17%20suites%20%7C%200%20failures-brightgreen.svg)](IMPLEMENTATION_STATUS.md)
  [![Status](https://img.shields.io/badge/status-active%20development-orange.svg)](IMPLEMENTATION_STATUS.md)
</div>

---

## What is this?

A clean-room C++20 port of the [Minima blockchain](https://minima.global) core protocol,
derived from the reference [Java implementation](https://github.com/minima-global/Minima).

> Minima [is migrating from Java to C++](https://www.linkedin.com/posts/minimaglobal_minima-has-nearly-completed-the-full-conversion-activity-7394416482753040384-hWOG)
> and has already run on FPGA. This repo is a community implementation aligned with that direction.

**If you know the Java codebase** — every module here maps 1:1 to the Java source.
Wire formats, class names, and algorithm behaviour are identical. See the
[Java → C++ mapping table](#java--c-module-mapping) below.

**Goals:**
- Native-level performance for edge devices (IoT, mobile, 300 MB RAM constraint)
- Post-quantum ready — Winternitz OTS signatures (same as Java BouncyCastle backend)
- Zero mandatory external dependencies (SQLite and SHA embedded as vendor headers)
- Modular — each subdirectory under `src/` can be used independently

---

## Current Status

**17 / 17 test suites pass — 0 failures.**

| Module | Tests | Java parity |
|--------|-------|-------------|
| `types/` MiniNumber · MiniData · MiniString | ✅ 19 tests | ✅ wire-exact |
| `objects/` TxPoW · Transaction · Coin · Witness · Token · IBD · Magic | ✅ 23 tests | ✅ 1:1 |
| `kissvm/` Tokenizer → Parser → Interpreter · 42 built-ins | ✅ 45 tests | ✅ 1:1 |
| `crypto/` SHA2/SHA3 · Winternitz OTS · TreeKey | ✅ 22 tests | ✅ WOTS W=8 exact |
| `mmr/` MMRSet · MMREntry · MMRProof | ✅ 20 tests | ✅ 1:1 |
| `chain/` ChainState · DifficultyAdjust · Cascade | ✅ 41 tests | ✅ 1:1 |
| `mining/` TxPoWMiner · MiningManager | ✅ 16 tests | ✅ |
| `network/` NIOMessage · NIOServer · P2PSync | ✅ 17 tests | ✅ wire-exact |
| `persistence/` BlockStore · UTxOStore (SQLite) | ✅ 21 tests | ✅ |
| `validation/` TxPoWValidator | ✅ 17 tests | ✅ |

Full detail: [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md)

---

## Build

**Requirements:** CMake 3.16+, C++20 compiler (GCC 11+, Clang 13+, MSVC 2022+)

```bash
git clone https://github.com/jaroslawfejdasz/-KISS-VM-interpreter
cd -KISS-VM-interpreter

# Configure + build + test
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DMINIMA_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Expected output:

```
100% tests passed, 0 tests failed out of 17
Total Test time (real) = ~4s
```

### Build options

| CMake flag | Default | Description |
|---|---|---|
| `MINIMA_BUILD_TESTS` | `OFF` | Build the doctest test suite |
| `MINIMA_CRYPTO_OPENSSL` | `OFF` | Use OpenSSL for SHA (otherwise bundled impl) |
| `CMAKE_BUILD_TYPE` | `Debug` | `Debug` / `Release` / `RelWithDebInfo` |

### Cross-compile for ARM

```bash
# ARM64 (Raspberry Pi 4/5, modern Android)
cmake -B build-arm64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DMINIMA_BUILD_TESTS=OFF
cmake --build build-arm64 --parallel

file build-arm64/minima_node
# → ELF 64-bit LSB executable, ARM aarch64
```

Pre-built ARM64 and ARMv7 binaries are uploaded as **GitHub Actions artifacts** on every
push to `main`.

---

## Quick Example — KISS VM

```cpp
#include "minima_core.hpp"

using namespace minima;
using namespace minima::kissvm;

// Build a minimal UTxO spend context
Transaction txn;
Witness     wit;

// Provide the signer's public key in the witness
SignatureProof sp;
sp.pubKey    = MiniData::fromHex("0xABCDEF...");
sp.signature = MiniData::fromHex("0x...");
wit.addSignature(sp);

// Execute the locking script
Contract c("RETURN SIGNEDBY(0xABCDEF...)", txn, wit, /*coinIndex=*/0);
Value result = c.execute();

if (result.isTrue()) {
    // Script passed — coin can be spent
}
```

---

## Repository Layout

```
.
├── src/
│   ├── types/          MiniNumber · MiniData · MiniString
│   ├── objects/        Coin · TxPoW · Transaction · Witness
│   │                   TxHeader · TxBody · Token · IBD · Magic …
│   ├── kissvm/         Tokenizer → Parser → Interpreter
│   │   └── functions/  42 built-in KISS VM functions
│   ├── crypto/         SHA2/SHA3 · Winternitz OTS · TreeKey
│   │   └── impl/       sha256.h · sha3.h (vendored, header-only)
│   ├── serialization/  DataStream (Minima wire format)
│   ├── mmr/            Merkle Mountain Range accumulator
│   ├── chain/          ChainState · ChainProcessor · DifficultyAdjust
│   │   └── cascade/    Cascade chain pruning
│   ├── mining/         TxPoWMiner · MiningManager
│   ├── network/        NIOServer · NIOClient · NIOMessage · P2PSync
│   ├── mempool/        Mempool
│   ├── persistence/    BlockStore · UTxOStore (SQLite3 embedded)
│   ├── validation/     TxPoWValidator
│   ├── vendor/         sqlite3.c/h (embedded, no install required)
│   ├── minima_core.hpp Umbrella header — include this for everything
│   └── main.cpp        Full-node entry point
│
├── tests/              One test file per module (doctest framework)
├── monorepo/           TypeScript packages (minima-test, kiss-vm-lint, …)
│   └── packages/
│       ├── minima-test/         KISS VM test framework
│       ├── kiss-vm-lint/        KISS VM static linter
│       ├── minima-contracts/    Standard contract library
│       └── create-minidapp/     MiniDapp scaffold CLI
├── cmake/              ARM cross-compile toolchain files
├── docs/               ARCHITECTURE.md — deep-dive per module
└── .github/workflows/  CI (5 jobs: x86-64 GCC/Clang + ARM cross)
```

---

## Java → C++ Module Mapping

For developers coming from the Java codebase.

| Java class / package | C++ equivalent |
|---|---|
| `MiniNumber` | `src/types/MiniNumber.hpp` |
| `MiniData` | `src/types/MiniData.hpp` |
| `MiniString` | `src/types/MiniString.hpp` |
| `Coin` | `src/objects/Coin.hpp` |
| `Transaction` | `src/objects/Transaction.hpp` |
| `Witness` | `src/objects/Witness.hpp` |
| `TxPoW` | `src/objects/TxPoW.hpp` |
| `TxHeader` | `src/objects/TxHeader.hpp` |
| `TxBody` | `src/objects/TxBody.hpp` |
| `Token` | `src/objects/Token.hpp` |
| `IBD` (Initial Block Download) | `src/objects/IBD.hpp` |
| `Magic` (protocol params) | `src/objects/Magic.hpp` |
| `StateVariable` | `src/objects/StateVariable.hpp` |
| `Address` | `src/objects/Address.hpp` |
| `Contract` | `src/kissvm/Contract.hpp` |
| `KISSSyntax` / tokenizer | `src/kissvm/Tokenizer.hpp` |
| `KISSParser` | `src/kissvm/Parser.hpp` |
| `KISSInterpreter` | `src/kissvm/Interpreter.hpp` |
| `KISSEnvironment` | `src/kissvm/Environment.hpp` |
| Built-in functions | `src/kissvm/functions/Functions.hpp` |
| `WinternitzOTSignature` (BouncyCastle) | `src/crypto/Winternitz.hpp` |
| `TreeKey` | `src/crypto/TreeKey.hpp` |
| `Crypto.SHA2/SHA3` | `src/crypto/Hash.hpp` |
| `DataStream` | `src/serialization/DataStream.hpp` |
| `MMRSet` | `src/mmr/MMRSet.hpp` |
| `MMREntry` | `src/mmr/MMREntry.hpp` |
| `MMRProof` | `src/mmr/MMRProof.hpp` |
| `TxPoWTree` / `ChainState` | `src/chain/ChainState.hpp` |
| `TxPoWProcessor` | `src/chain/ChainProcessor.hpp` |
| `DifficultyManager` | `src/chain/DifficultyAdjust.hpp` |
| `Cascade` | `src/chain/cascade/Cascade.hpp` |
| `TxPoWMiner` | `src/mining/TxPoWMiner.hpp` |
| `MiningManager` | `src/mining/MiningManager.hpp` |
| `NIOMessage` | `src/network/NIOMessage.hpp` |
| `NIOServer` / `NIOClient` | `src/network/NIOServer.hpp` / `NIOClient.hpp` |
| `P2PMessageProcessor` | `src/network/P2PSync.hpp` |
| `TxPoWDB` | `src/persistence/BlockStore.hpp` |
| `CoinDB` | `src/persistence/UTxOStore.hpp` |
| `TxPoWChecker` | `src/validation/TxPoWValidator.hpp` |

---

## Key Protocol Concepts

### TxPoW — Transaction Proof of Work

Every transaction in Minima **is** a potential block. When a user submits a
transaction, they spend ~1 second performing PoW. If the SHA2-256 hash of the
`TxHeader` crosses the `blockDifficulty` threshold, the TxPoW becomes a block.

This eliminates dedicated miners — every user is simultaneously a miner.

```
TxPoW
├── TxHeader   ← what gets hashed during mining (nonce here)
└── TxBody
    ├── Transaction   inputs[] + outputs[] + state[]
    ├── Witness       scripts[] + signatures[]
    └── txnList[]     cross-referenced transactions
```

### UTxO Model

Like Bitcoin: coins are unspent outputs, spending requires proving a valid
script condition. Unlike Bitcoin: scripts use KISS VM (richer language, but
still deterministic and strictly bounded).

```
Coin
├── coinID      SHA3(transactionID + outputIndex)
├── address     SHA3(lockingScript)
├── amount      MiniNumber
├── tokenID     "0x00" for native Minima
└── state[]     StateVariable[0..255]
```

### KISS VM — Smart Contracts

Every coin carries a **locking script**. To spend the coin, you provide a
**witness** (signatures + unlocking script), and the KISS VM executes the
locking script. It must return `TRUE`.

Default script for a simple wallet: `RETURN SIGNEDBY(pubKey)`

**Execution pipeline:**
```
Script string
  → Tokenizer  (lexical analysis)
  → Token[]
  → Executor   (stack machine, statement-by-statement)
  → Value      (must be TRUE)
```

**Hard limits (enforced at runtime):**
- Max **1024 instructions** — throws `ContractException` if exceeded
- Max **64 stack depth** — throws `ContractException` if exceeded

**Available types:** `BOOLEAN · NUMBER · HEX · SCRIPT`

**Selected built-in functions:**

| Function | Description |
|---|---|
| `SIGNEDBY(pubKey)` | TRUE if witness contains valid sig for pubKey |
| `MULTISIG(m, k1, k2, …)` | m-of-n multi-signature |
| `CHECKSIG(pub, data, sig)` | Verify a Winternitz OTS signature |
| `STATE(n)` | Read state variable n of the current coin |
| `PREVSTATE(n)` | Read state variable n of the previous coin |
| `VERIFYOUT(idx, addr, amt, tok)` | Assert a specific output exists |
| `@BLOCK` | Current block height |
| `@COINAGE` | How many blocks since this coin was created |
| `@AMOUNT` | Amount of the coin being spent |
| `SHA2(data)` / `SHA3(data)` | Cryptographic hashes |
| `CONCAT(a, b)` | Concatenate HEX values |

### Winternitz OTS — Post-Quantum Signatures

Minima does **not** use ECDSA or Schnorr. It uses **Winternitz One-Time
Signatures** (a post-quantum hash-based scheme), implemented in Java via
BouncyCastle's `WinternitzOTSignature`. The C++ implementation in
`src/crypto/Winternitz.hpp` is byte-for-byte compatible:

- Hash function: **SHA3-256**
- Winternitz parameter: **W = 8**
- Key size: **2880 bytes** (public and private)
- Signature size: **2880 bytes**

Because each key is one-time use, Minima uses a **Merkle tree of WOTS keys**
(`TreeKey`, depth = 12) as the wallet key. The public key is the Merkle root.

### MMR — Merkle Mountain Range

Minima uses an MMR (instead of a Merkle tree) as its UTXO commitment scheme.
New coins are appended to the MMR; when a coin is spent, its MMR entry is
updated. Each block header commits to the MMR root.

```
MMRSet      append(coin), update(spent), getRoot(), getProof(index)
MMREntry    index · data · inBlock · spent
MMRProof    index · blockTime · peaks[] · proof[]
```

### Cascade — Chain Pruning

Minima keeps only the most recent `N` blocks in full detail. Older blocks are
"cascaded" — summarised into cascade nodes and pruned. This keeps disk usage
constant regardless of chain age.

```
Cascade
├── CascadeNode[]   (summarised old blocks)
└── tip             (latest cascade point)
```

---

## CI Pipeline

Five jobs run on every push to `main`:

| Job | Platform | Compiler |
|---|---|---|
| `build-and-test` | Ubuntu 22.04 | GCC 11 |
| `build-and-test` | Ubuntu 24.04 | GCC 13 |
| `clang-build` | Ubuntu 22.04 | Clang 15 |
| `cross-aarch64` | (cross) | aarch64-linux-gnu GCC |
| `cross-armv7` | (cross) | armv7-linux-gnueabihf GCC |

ARM binaries uploaded as artifacts, not QEMU-tested (next milestone).

---

## TypeScript Packages (monorepo/)

The `monorepo/` directory contains developer tooling for Minima MiniDapp
development — separate from the C++ core.

| Package | Description | Status |
|---|---|---|
| `minima-test` | Test framework for KISS VM scripts (like Jest for contracts) | ✅ ready |
| `kiss-vm-lint` | Static linter for KISS VM scripts | ✅ ready |
| `minima-contracts` | Standard contract library (multi-sig, time-lock, HTLC, …) | ✅ ready |
| `create-minidapp` | CLI scaffold for new MiniDapp projects | ✅ ready |

```bash
cd monorepo
npm install
npm run build   # builds all 4 packages
```

---

## Known Limitations

| # | Issue | Notes |
|---|---|---|
| 1 | Schnorr signatures | Stub only; Winternitz is the real signing scheme |
| 2 | ARM binaries not QEMU-tested | Cross-compiled, but execution not verified in CI |
| 3 | Maxima / Omnia (L2) | Out of scope for core protocol |
| 4 | Full P2P peer discovery | NIO layer present; peer handshake not fully integrated |

---

## Reference

- [Minima Documentation](https://docs.minima.global)
- [Java Reference Implementation](https://github.com/minima-global/Minima)
- [KISS VM Scripting Guide](https://docs.minima.global/docs/development/contracts-basics)
- [Architecture Deep-Dive](docs/ARCHITECTURE.md)
- [Implementation Status](IMPLEMENTATION_STATUS.md)
