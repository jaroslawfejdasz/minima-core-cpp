"use strict";
/**
 * minima-contracts
 * Audited KISS VM smart contract templates for Minima blockchain.
 *
 * Usage:
 *   const { basicSigned, timeLock, multiSig, htlc, exchange } = require('minima-contracts');
 *
 *   const script = timeLock({ ownerKey: '0xYOUR_KEY', lockBlock: 1000 });
 *   // => "LET lockBlock = 1000\nIF @BLOCK LT lockBlock THEN..."
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.contracts = void 0;
exports.basicSigned = basicSigned;
exports.timeLock = timeLock;
exports.coinAgeLock = coinAgeLock;
exports.multiSig = multiSig;
exports.timeLockMultiSig = timeLockMultiSig;
exports.htlc = htlc;
exports.exchange = exchange;
exports.statefulCounter = statefulCounter;
// =====================================================================
// Validation helpers
// =====================================================================
function assertHex(val, name) {
    if (!val || !/^0x[0-9a-fA-F]+$/.test(val)) {
        throw new Error(`${name} must be a valid hex value (0x-prefixed), got: ${val}`);
    }
}
function assertPositive(val, name) {
    if (!Number.isInteger(val) || val < 0) {
        throw new Error(`${name} must be a non-negative integer, got: ${val}`);
    }
}
function assertKeys(keys) {
    if (!keys || keys.length === 0)
        throw new Error('keys array must not be empty');
    keys.forEach((k, i) => assertHex(k, `keys[${i}]`));
}
// =====================================================================
// Contract templates
// =====================================================================
/**
 * Basic signed contract — simplest possible contract.
 * Only the owner (holder of the private key) can spend.
 *
 * Security: standard — equivalent to a regular Minima address.
 */
function basicSigned(params) {
    assertHex(params.ownerKey, 'ownerKey');
    return `RETURN SIGNEDBY(${params.ownerKey})`;
}
/**
 * Time lock — coin cannot be spent until a specific block height.
 * After the lock expires, only the owner can spend.
 *
 * Security: owner cannot prematurely unlock.
 * Use case: vesting cliffs, scheduled payments, dead man switches.
 */
function timeLock(params) {
    assertHex(params.ownerKey, 'ownerKey');
    assertPositive(params.lockBlock, 'lockBlock');
    return [
        `/* Time Lock: cannot spend until block ${params.lockBlock} */`,
        `LET lockBlock = ${params.lockBlock}`,
        `IF @BLOCK LT lockBlock THEN`,
        `  RETURN FALSE`,
        `ENDIF`,
        `RETURN SIGNEDBY(${params.ownerKey})`,
    ].join('\n');
}
/**
 * Coin age lock — coin cannot be spent until it has existed for `minAge` blocks.
 * Useful for HODLer incentives or anti-dust protection.
 *
 * Security: even if owner wants to spend early, @COINAGE prevents it.
 * Use case: staking, long-term savings.
 */
function coinAgeLock(params) {
    assertHex(params.ownerKey, 'ownerKey');
    assertPositive(params.minAge, 'minAge');
    return [
        `/* Coin Age Lock: must hold for ${params.minAge} blocks */`,
        `LET minAge = ${params.minAge}`,
        `IF @COINAGE LT minAge THEN`,
        `  RETURN FALSE`,
        `ENDIF`,
        `RETURN SIGNEDBY(${params.ownerKey})`,
    ].join('\n');
}
/**
 * Multi-signature — requires M-of-N signatures to spend.
 *
 * Security: no single key can unilaterally spend.
 * Use case: treasury management, joint custody, escrow.
 */
function multiSig(params) {
    assertPositive(params.required, 'required');
    assertKeys(params.keys);
    if (params.required > params.keys.length) {
        throw new Error(`required (${params.required}) cannot exceed number of keys (${params.keys.length})`);
    }
    if (params.required < 1)
        throw new Error('required must be >= 1');
    const keyList = params.keys.join(', ');
    return [
        `/* Multi-sig: ${params.required}-of-${params.keys.length} */`,
        `RETURN MULTISIG(${params.required}, ${keyList})`,
    ].join('\n');
}
/**
 * Time-locked multi-sig — M-of-N, only after lockBlock.
 *
 * Use case: delayed treasury release, board-approved spending.
 */
function timeLockMultiSig(params) {
    assertPositive(params.required, 'required');
    assertKeys(params.keys);
    assertPositive(params.lockBlock, 'lockBlock');
    if (params.required > params.keys.length) {
        throw new Error(`required (${params.required}) cannot exceed number of keys (${params.keys.length})`);
    }
    const keyList = params.keys.join(', ');
    return [
        `/* Time-Locked Multi-sig: ${params.required}-of-${params.keys.length} after block ${params.lockBlock} */`,
        `LET lockBlock = ${params.lockBlock}`,
        `IF @BLOCK LT lockBlock THEN`,
        `  RETURN FALSE`,
        `ENDIF`,
        `RETURN MULTISIG(${params.required}, ${keyList})`,
    ].join('\n');
}
/**
 * HTLC (Hash Time Lock Contract) — atomic swap building block.
 * Recipient can spend by revealing a secret whose SHA3 hash matches.
 * Sender can reclaim after timeout.
 *
 * Security:
 * - Secret must be stored in state port `secretStatePort` of the spending transaction
 * - Timeout gives sender a safety net; choose large enough for recipient to respond
 *
 * Use case: cross-chain atomic swaps, lightning-style payment channels.
 */
function htlc(params) {
    assertHex(params.recipientKey, 'recipientKey');
    assertHex(params.senderKey, 'senderKey');
    assertHex(params.secretHash, 'secretHash');
    assertPositive(params.secretStatePort, 'secretStatePort');
    assertPositive(params.timeoutBlock, 'timeoutBlock');
    return [
        `/* HTLC: recipient reveals secret, sender reclaims after block ${params.timeoutBlock} */`,
        `LET secret = STATE(${params.secretStatePort})`,
        `LET hash = SHA3(secret)`,
        `IF hash EQ ${params.secretHash} THEN`,
        `  RETURN SIGNEDBY(${params.recipientKey})`,
        `ENDIF`,
        `IF @BLOCK GT ${params.timeoutBlock} THEN`,
        `  RETURN SIGNEDBY(${params.senderKey})`,
        `ENDIF`,
        `RETURN FALSE`,
    ].join('\n');
}
/**
 * Exchange contract — atomic on-chain swap.
 * Spendable only if the transaction includes a specific output.
 *
 * Security: validates BOTH amount AND address to prevent partial-spend attacks.
 * Use case: DEX order books, trustless P2P trading on Minima.
 */
function exchange(params) {
    assertPositive(params.outputIndex, 'outputIndex');
    assertPositive(params.amount, 'amount');
    assertHex(params.recipientAddress, 'recipientAddress');
    assertHex(params.ownerKey, 'ownerKey');
    const tokenId = params.tokenId ?? '0x00';
    assertHex(tokenId, 'tokenId');
    return [
        `/* Exchange: spendable only if output ${params.outputIndex} pays ${params.amount} to ${params.recipientAddress} */`,
        `LET validOut = VERIFYOUT(${params.outputIndex}, ${params.amount}, ${params.recipientAddress}, ${tokenId})`,
        `IF validOut THEN`,
        `  RETURN SIGNEDBY(${params.ownerKey})`,
        `ENDIF`,
        `RETURN FALSE`,
    ].join('\n');
}
/**
 * Stateful counter — coin tracks a counter in state, rejects if counter exceeds limit.
 * Each spend increments the counter (via SAMESTATE enforcement on other ports).
 *
 * Use case: rate limiting, multi-step workflows, metered access.
 */
function statefulCounter(params) {
    assertHex(params.ownerKey, 'ownerKey');
    assertPositive(params.counterPort, 'counterPort');
    assertPositive(params.maxCount, 'maxCount');
    return [
        `/* Stateful counter: max ${params.maxCount} spends, counter at port ${params.counterPort} */`,
        `LET count = STATE(${params.counterPort})`,
        `LET prevCount = PREVSTATE(${params.counterPort})`,
        `IF count NEQ prevCount + 1 THEN`,
        `  RETURN FALSE`,
        `ENDIF`,
        `IF count GT ${params.maxCount} THEN`,
        `  RETURN FALSE`,
        `ENDIF`,
        `RETURN SIGNEDBY(${params.ownerKey})`,
    ].join('\n');
}
// =====================================================================
// Contract registry — list all available templates
// =====================================================================
exports.contracts = {
    basicSigned,
    timeLock,
    coinAgeLock,
    multiSig,
    timeLockMultiSig,
    htlc,
    exchange,
    statefulCounter,
};
//# sourceMappingURL=index.js.map