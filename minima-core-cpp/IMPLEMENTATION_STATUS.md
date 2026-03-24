# Implementation Status — minima-core-cpp

Last updated: 2026-03-24

## Test suites: 19/19 pass ✅

| Suite               | Tests             | Status |
|---------------------|-------------------|--------|
| test_mini_number    | types             | ✅ |
| test_mini_data      | types             | ✅ |
| test_kissvm         | KISS VM interpreter | ✅ |
| test_txpow          | TxPoW / TxHeader / TxBody | ✅ |
| test_validation     | TxPoWValidator    | ✅ |
| test_mmr            | Merkle Mountain Range | ✅ |
| test_chain          | BlockStore / ChainProcessor | ✅ |
| test_token          | Token / Greeting / TxBlock | ✅ |
| test_mining         | TxPoWMiner        | ✅ |
| test_difficulty     | DifficultyAdjust  | ✅ |
| test_network        | NIOMessage (P2P wire) | ✅ |
| test_loopback       | real TCP loopback handshake | ✅ |
| test_ibd            | IBD (Initial Blockchain Download) | ✅ |
| test_mempool        | Mempool           | ✅ |
| test_rsa            | RSA-1024 verify   | ✅ |
| test_wots           | WOTS+ / TreeKey   | ✅ |
| test_checksig       | CHECKSIG RSA integration | ✅ |
| test_checksig_tk    | CHECKSIG TreeKey integration | ✅ |
| test_multisig       | MULTISIG RSA+TreeKey | ✅ |

## `minimanode` executable ✅

A working experimental P2P node binary.

```
Usage: minimanode [options]
  -port <n>       Listen port (default: 9001)
  -connect <host> Connect to peer on startup
  -cport <n>      Peer port when using -connect (default: 9001)
  -quiet          Suppress verbose logging
```

**Verified behaviour (local loopback):**
- Two nodes start, exchange Greeting (version=1.0.45), reach READY state
- Inbound + outbound TCP connections both work
- Graceful shutdown (Ctrl+C / SIGTERM) — no crashes

**Wire format compatibility:**
- Greeting: MiniString version + MiniNumber topBlock + MiniNumber count + MiniData[] chain IDs
- NIOMessage framing: 4-byte big-endian length + 1-byte type + payload
- Protocol version: 1.0.45 (matching Java reference)

## Implemented modules

### Core types
- [x] MiniNumber (BigDecimal-compatible, full arithmetic)
- [x] MiniData (byte array, HEX encoding)
- [x] MiniString (UTF-8)

### Objects
- [x] TxHeader (wire-exact: superParents[32] RLE, Magic, blockDifficulty)
- [x] TxBody (wire-exact: prng, txnDifficulty, burnTxn)
- [x] TxPoW (ID = SHA3(SHA3(header)), getPoWHash = SHA2(header))
- [x] Transaction (inputs/outputs/state variables)
- [x] Witness (signatures, scripts, proofs)
- [x] Coin (UTxO model)
- [x] CoinProof (MMR proof for coin)
- [x] Address (script → SHA3 hash)
- [x] StateVariable (key-value, 255 slots)
- [x] Token (custom tokens)
- [x] Magic (network parameters)
- [x] Greeting (initial handshake, version, topBlock, chain IDs)
- [x] TxBlock (block + new/spent coins + MMR peaks)
- [x] IBD (Initial Blockchain Download, serialise/deserialise)

### Cryptography
- [x] SHA2-256 (pure C++)
- [x] SHA3-256 (pure C++)
- [x] RSA-1024 SHA256withRSA (signature verify)
- [x] WOTS+ (Winternitz OTS, w=16, 67 chains, 2144-byte sig)
- [x] TreeKey (Merkle tree over 256 WOTS leaves, quantum-resistant)
- [ ] Schnorr (stub — Java reference uses RSA)

### KISS VM
- [x] Tokenizer + Parser + Interpreter
- [x] 42+ built-in functions
- [x] CHECKSIG / SIGNEDBY / MULTISIG with RSA + TreeKey dispatch
- [x] All state variable functions (STATE, PREVSTATE, SAMESTATE)
- [x] Transaction context (@BLOCK, @COINAGE, @AMOUNT, @ADDRESS, @TXPOWID)

### MMR (Merkle Mountain Range)
- [x] MMRData, MMREntry, MMRProof
- [x] MMRSet (add, get, getProof, verifyProof)

### Chain layer
- [x] BlockStore (in-memory block storage)
- [x] ChainState (UTxO set management)
- [x] UTxOSet (add/spend/query coins)
- [x] ChainProcessor (process blocks, validate+apply)
- [x] DifficultyAdjust (Java-exact retarget)
- [x] Cascade (chain cascade/pruning)

### Validation
- [x] TxPoWValidator (PoW check, script execution, signature verify)

### Mining
- [x] TxPoWMiner (nonce increment, difficulty check, cancel flag)

### Network (P2P)
- [x] NIOMessage (24 message types, encode/decode)
- [x] NetworkProtocol (state machine: handshake → READY)
- [x] NIOServer (TCP server + client, per-connection threads, detached)
- [x] IBD exchange during handshake
- [x] Real TCP loopback handshake (6 test cases)
- [x] `minimanode` binary — starts, listens, connects, handshakes

### Mempool
- [x] Mempool (add/remove/query pending TxPoW)

### Serialization
- [x] DataStream (read/write all primitives)

## TODO (next priority)

1. **TxPoW payload in NetworkEvent** — expose received TxPoW object (not just "received" string)
2. **Persistence** — LevelDB or SQLite for BlockStore (survive restart)
3. **IBD serve** — when peer is behind us, serve IBD from our BlockStore
4. **PULSE keepalive** — send PULSE every 10 minutes (Java: USER_PULSE_FREQ)
5. **P2P discovery** — handle P2P message type (peer list exchange)
6. **Connect to real Minima mainnet node** — verify wire format compatibility

## Architecture

```
L1: Minima (UTxO flood-fill P2P TxPoW)
    ↓
    Binary:   minimanode (start, listen :9001, connect to peer)
    Network:  NIOServer (TCP) → ProtocolConnection (state machine) → NIOMessage
    Objects:  TxPoW → TxBlock → IBD → Greeting
    Chain:    BlockStore + UTxOSet + ChainProcessor + DifficultyAdjust
    Crypto:   SHA2/SHA3/RSA/WOTS+/TreeKey
    KISSVM:   Tokenizer → Parser → Interpreter (42+ functions)
    Mempool:  pending TxPoWs
    Mining:   TxPoWMiner
```
