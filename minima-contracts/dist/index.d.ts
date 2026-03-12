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
export interface BasicSignedParams {
    /** Owner's public key (hex, 0x-prefixed) */
    ownerKey: string;
}
export interface TimeLockParams {
    /** Owner's public key */
    ownerKey: string;
    /** Block number after which the coin can be spent */
    lockBlock: number;
}
export interface CoinAgeLockParams {
    /** Owner's public key */
    ownerKey: string;
    /** Minimum coin age in blocks before it can be spent */
    minAge: number;
}
export interface MultiSigParams {
    /** Minimum number of signatures required */
    required: number;
    /** List of public keys (at least `required` items) */
    keys: string[];
}
export interface TimeLockMultiSigParams {
    required: number;
    keys: string[];
    lockBlock: number;
}
export interface HTLCParams {
    /** Recipient's public key — can spend with the secret */
    recipientKey: string;
    /** Sender's public key — can reclaim after timeout */
    senderKey: string;
    /** SHA3 hash of the secret (0x-prefixed hex) */
    secretHash: string;
    /** State port where the secret is stored (0-255) */
    secretStatePort: number;
    /** Block number after which sender can reclaim */
    timeoutBlock: number;
}
export interface ExchangeParams {
    /** Output index to verify */
    outputIndex: number;
    /** Required amount in the output */
    amount: number;
    /** Required recipient address (0x-prefixed hex) */
    recipientAddress: string;
    /** Token ID (0x00 for Minima native) */
    tokenId?: string;
    /** Owner's public key (must sign the spend) */
    ownerKey: string;
}
export interface StatefulParams {
    /** Owner's public key */
    ownerKey: string;
    /** State port for the counter */
    counterPort: number;
    /** Maximum allowed counter value */
    maxCount: number;
}
export interface VestingParams {
    ownerKey: string;
    /** Block when vesting starts */
    startBlock: number;
    /** Total vesting duration in blocks */
    vestingBlocks: number;
    /** Total amount to vest */
    totalAmount: number;
    /** State port storing the already-claimed amount */
    claimedPort: number;
}
/**
 * Basic signed contract — simplest possible contract.
 * Only the owner (holder of the private key) can spend.
 *
 * Security: standard — equivalent to a regular Minima address.
 */
export declare function basicSigned(params: BasicSignedParams): string;
/**
 * Time lock — coin cannot be spent until a specific block height.
 * After the lock expires, only the owner can spend.
 *
 * Security: owner cannot prematurely unlock.
 * Use case: vesting cliffs, scheduled payments, dead man switches.
 */
export declare function timeLock(params: TimeLockParams): string;
/**
 * Coin age lock — coin cannot be spent until it has existed for `minAge` blocks.
 * Useful for HODLer incentives or anti-dust protection.
 *
 * Security: even if owner wants to spend early, @COINAGE prevents it.
 * Use case: staking, long-term savings.
 */
export declare function coinAgeLock(params: CoinAgeLockParams): string;
/**
 * Multi-signature — requires M-of-N signatures to spend.
 *
 * Security: no single key can unilaterally spend.
 * Use case: treasury management, joint custody, escrow.
 */
export declare function multiSig(params: MultiSigParams): string;
/**
 * Time-locked multi-sig — M-of-N, only after lockBlock.
 *
 * Use case: delayed treasury release, board-approved spending.
 */
export declare function timeLockMultiSig(params: TimeLockMultiSigParams): string;
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
export declare function htlc(params: HTLCParams): string;
/**
 * Exchange contract — atomic on-chain swap.
 * Spendable only if the transaction includes a specific output.
 *
 * Security: validates BOTH amount AND address to prevent partial-spend attacks.
 * Use case: DEX order books, trustless P2P trading on Minima.
 */
export declare function exchange(params: ExchangeParams): string;
/**
 * Stateful counter — coin tracks a counter in state, rejects if counter exceeds limit.
 * Each spend increments the counter (via SAMESTATE enforcement on other ports).
 *
 * Use case: rate limiting, multi-step workflows, metered access.
 */
export declare function statefulCounter(params: StatefulParams): string;
export declare const contracts: {
    readonly basicSigned: typeof basicSigned;
    readonly timeLock: typeof timeLock;
    readonly coinAgeLock: typeof coinAgeLock;
    readonly multiSig: typeof multiSig;
    readonly timeLockMultiSig: typeof timeLockMultiSig;
    readonly htlc: typeof htlc;
    readonly exchange: typeof exchange;
    readonly statefulCounter: typeof statefulCounter;
};
export type ContractName = keyof typeof contracts;
//# sourceMappingURL=index.d.ts.map