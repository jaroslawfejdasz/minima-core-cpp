# Parity Gap Analysis — C++ vs Java Reference

**Date:** 2026-03-22  
**Current state:** 17/17 test suites pass, ale to nie znaczy 100% parity.  
**Goal:** Bit-for-bit, wire-exact 1:1 z Java referencją.

---

## Ocena obecnego stanu

| Obszar | Stan | Uwagi |
|--------|------|-------|
| Wire format (serialization) | ✅ 1:1 | DataStream zgodny z Java DataOutputStream |
| Primitive types | ✅ 1:1 | MiniNumber/MiniData/MiniString |
| Protocol objects | ✅ mostly | Brakuje Signature, SignatureProof, ScriptProof, Pulse |
| KISS VM interpreter | ✅ ~85% | Brakuje 13 funkcji built-in |
| Cryptography (WOTS/TreeKey) | ✅ 1:1 | Byte-exact z BouncyCastle |
| MMR accumulator | ✅ 1:1 | |
| Database layer | ⚠️ partial | Mamy BlockStore/UTxOStore, brak TxPowTree, MinimaDB, Wallet |
| Processing pipeline | ❌ missing | TxPoWProcessor, TxPoWGenerator, TxPoWSearcher |
| Genesis block | ❌ missing | GenesisCoin, GenesisMMR, GenesisTxPoW |
| P2P integration | ⚠️ partial | NIO layer jest, brak integracji z procesorem |
| MegaMMR | ❌ missing | Potrzebny do IBD sync |

---

## GAP 1 — Brakujące objects (KRYTYCZNE dla wire compat)

### `Signature.java` → brak w C++

`Signature` to lista `SignatureProof` — główna struktura do weryfikacji podpisów w `Witness`.  
**Witness w C++ ma inną strukturę niż Java!**

Java `Witness.java`:
```java
ArrayList<Signature> mSignatures;       // lista Signature (które zawierają SignatureProof[])
ArrayList<ScriptProof> mScriptProofs;   // lista ScriptProof (script + MMRProof)
```

C++ `Witness.hpp`:
```cpp
std::vector<SignatureProof> signatures; // PŁASKO — bez grupowania
std::vector<MiniString>    scripts;     // bez MMRProof!
```

**Efekt:** nasz wire format `Witness` jest INNY niż Java. To breaking change — węzeł C++ nie może komunikować się z węzłem Java.

### `ScriptProof.java` → brak w C++

`ScriptProof` to: skrypt + `MMRProof` (Merkle dowód że skrypt należy do adresu).  
Bez tego weryfikacja skryptów jest niekompletna.

### `SignatureProof.java` → brak w C++

Zawiera: `pubKey` + `signature` + `MMRProof` (dowód że pubKey należy do TreeKey root).  
Używany do weryfikacji WOTS w kontekście Merkle tree.

### `Pulse.java` → brak w C++

Wiadomość P2P z listą ostatnich 60 minut bloków. Używana do synchronizacji "am I on the right chain?" bez pełnego IBD.

### `MiniByte.java` → brak w C++

Trivialny — 1-byte wrapper. Ale używany w niektórych wire formats.

---

## GAP 2 — KISS VM — brakujące funkcje (13 sztuk)

| Java klasa | Funkcja | Co robi |
|---|---|---|
| `STRING` | `STRING(hex)` | Hex → UTF-8 string |
| `EXISTS` | `EXISTS(var)` | Czy zmienna istnieje w środowisku |
| `OVERWRITE` | `OVERWRITE(data,pos,val)` | Nadpisz bajty w MiniData |
| `SETLEN` | `SETLEN(data,len)` | Ustaw długość MiniData |
| `SIGDIG` | `SIGDIG(n,digits)` | Liczba cyfr znaczących |
| `PROOF` | `PROOF(data,proof,root)` | Weryfikuj MMR proof |
| `REPLACEFIRST` | `REPLACEFIRST(s,from,to)` | Replace pierwszego wystąpienia |
| `SUBSTR` | `SUBSTR(s,start,len)` | Substring |
| `GETINADDR` | `GETINADDR(i)` | Adres i-tego inputu |
| `GETINAMT` | `GETINAMT(i)` | Kwota i-tego inputu |
| `GETINID` | `GETINID(i)` | CoinID i-tego inputu |
| `GETINTOK` | `GETINTOK(i)` | TokenID i-tego inputu |
| `GETOUTKEEPSTATE` | `GETOUTKEEPSTATE(i)` | Czy output i zachowuje state |

**Krytyczne:** `PROOF` (MMR verification w skrypcie), `GETINADDR/AMT/ID/TOK` (często używane w kontraktach).

---

## GAP 3 — Database layer — brakujące struktury (KRYTYCZNE)

### `TxPowTree` + `TxPoWTreeNode`

Drzewo TxPoW — kanoniczna reprezentacja łańcucha jako drzewo (nie lista).  
Obsługuje re-orgi (chain reorganization). **BEZ TEGO NIE MA PRAWDZIWEJ KONSENSUSU.**

```java
TxPowTree {
    TxPoWTreeNode mRoot;
    TxPoWTreeNode mTip;
    
    void addTxPoW(TxPoW)      // dodaj do drzewa, znajdź longest chain
    void reorg(TxPoWTreeNode)  // reorganizacja łańcucha
    TxPoWTreeNode getTip()
}
```

### `MinimaDB`

Centralny "God object" — trzyma referencje do wszystkich sub-baz:
- `TxPoWDB` (raw storage)
- `TxPowTree` (canonical chain)  
- `Cascade` (pruning)
- `MMRSet` (UTXO set)
- `Wallet`
- `UserDB`, `TxnDB`

Bez `MinimaDB` poszczególne komponenty nie są ze sobą powiązane.

### `MegaMMR`

Specjalny tryb MMR dla nowych węzłów — sync bez pełnego IBD.  
Węzeł może zsynchronizować się poprzez pobranie tylko aktualnego stanu UTXO (nie całej historii).

### `Wallet`

Zarządzanie kluczami WOTS (generowanie, przechowywanie, podpisywanie).

---

## GAP 4 — Processing pipeline (KRYTYCZNE dla działającego węzła)

### `TxPoWProcessor`

**Najważniejszy brakujący komponent.** Asynchroniczny procesor wiadomości obsługujący:
- `TXP_PROCESSTXPOW` — walidacja i dodanie nowego TxPoW do drzewa
- `TXP_PROCESSTXBLOCK` — przetworzenie nowego bloku
- `TXP_PROCESS_IBD` — Initial Block Download
- `TXP_PROCESS_SYNCIBD` — Sync IBD

W Java rozszerza `MessageProcessor` (asynchroniczna kolejka zadań).

### `TxPoWGenerator`

Tworzy nowe TxPoW z transakcji w mempoolu — "assembles" blok przed miningiem.

### `TxPoWSearcher`

Przeszukuje `TxPowTree` — np. "czy ta transakcja jest już w łańcuchu?".

### `TxPoWChecker`

Pełna walidacja (odpowiednik naszego `TxPoWValidator`) — ale operuje na `MinimaDB`, nie izolowanym kontekście.

---

## GAP 5 — Genesis block

Bez genesis nie da się zainicjalizować węzła "od zera".

| Java | Funkcja |
|---|---|
| `GenesisCoin` | Pierwsza moneta — 1 miliard Minima |
| `GenesisMMR` | Stan MMR po genesis bloku |
| `GenesisTxPoW` | Genesis TxPoW (blok #0) |

---

## GAP 6 — Utility infrastructure

| Brakuje | Ważność | Opis |
|---|---|---|
| `Streamable` interface | WYSOKA | Interface `readDataStream/writeDataStream` — wszystkie obiekty Java go implementują. C++ nie ma odpowiednika jako wymuszanego interfejsu |
| `MessageProcessor` | WYSOKA | Asynchroniczna kolejka zdarzeń — fundament dla TxPoWProcessor, NIOManager itp. |
| `Stack<T>` | ŚREDNIA | Thread-safe stack z timeoutem — używany w message passing |
| `BIP39` | ŚREDNIA | Mnemonics dla seed phrase (wallet recovery) |
| `Maths` | NISKA | Utility math functions — większość już w MiniNumber |

---

## Priorytetowana lista zadań (do pełnej parity)

### 🔴 BLOKUJĄCE (bez tego węzeł nie działa z siecią Java)

1. **Napraw `Witness` wire format** — dostosuj do Java struktury `Signature[]` + `ScriptProof[]`
2. **Zaimplementuj `Signature`, `SignatureProof`, `ScriptProof`** — kluczowe objects
3. **Zaimplementuj `TxPowTree` + `TxPoWTreeNode`** — prawdziwy konsensus
4. **Zaimplementuj `TxPoWProcessor`** — pipeline przetwarzania
5. **Genesis block** — `GenesisCoin`, `GenesisMMR`, `GenesisTxPoW`

### 🟡 WAŻNE (potrzebne do pełnej funkcjonalności)

6. **`MinimaDB`** — centralny koordynator wszystkich sub-systemów
7. **Brakujące 13 funkcji KISS VM** — w tym `PROOF`, `GETINADDR/AMT/ID/TOK`
8. **`MessageProcessor`** — infrastruktura async message passing
9. **`TxPoWGenerator`** — assembling bloków do miningu
10. **`Wallet`** — zarządzanie kluczami

### 🟢 UZUPEŁNIAJĄCE (po pełnej funkcjonalności)

11. `Pulse` — keepalive P2P message
12. `MegaMMR` — fast sync dla nowych węzłów
13. `BIP39` — mnemonics dla wallet recovery
14. `TxPoWSearcher` — query po drzewie
15. `MiniByte` — trivial wrapper

---

## Szacunek: ile pracy do 100% parity?

| Etap | Szacunek | Blokuje co |
|---|---|---|
| Fix Witness wire format | 1-2h | sieć |
| Signature + SignatureProof + ScriptProof | 3-4h | walidacja |
| TxPowTree + TxPoWTreeNode | 4-6h | konsensus |
| Genesis | 2h | bootstrap |
| TxPoWProcessor + MessageProcessor | 6-8h | processing pipeline |
| MinimaDB | 3-4h | integracja |
| 13 KISS VM funkcji | 2-3h | kontrakty |
| Wallet | 3-4h | podpisywanie |
| **RAZEM** | **~25-35h pracy** | pełna parity |
