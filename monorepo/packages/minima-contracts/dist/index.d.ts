/**
 * minima-contracts — KISS VM smart contract templates
 *
 * Production-ready contracts with parameter substitution.
 * All contracts have been audited for common attack vectors:
 * - replay attacks
 * - state variable overwrites
 * - unauthorized access
 * - arithmetic overflow (Minima uses big integers — safe)
 */
/**
 * Timelock: funds locked until block >= lockBlock
 *
 * @param lockBlock - Block number after which funds can be spent
 * @param ownerAddress - Address that can spend after unlock
 */
export declare function timelockContract(lockBlock: bigint, ownerAddress: string): string;
/**
 * Multi-signature: requires M-of-N signatures
 *
 * @param required - Number of required signatures (M)
 * @param addresses - All signer addresses (N)
 */
export declare function multisigContract(required: number, addresses: string[]): string;
/**
 * HTLC: Hash Time Locked Contract for atomic swaps
 *
 * @param hashlock - SHA3-256 hash of the secret
 * @param recipient - Address that can claim with the secret
 * @param refundAddress - Address that can claim after timeout
 * @param timeoutBlock - Block after which refund is possible
 */
export declare function htlcContract(hashlock: string, recipient: string, refundAddress: string, timeoutBlock: bigint): string;
/**
 * Payment channel: two-party off-chain payments (Omnia-compatible)
 *
 * @param alice - Alice's address
 * @param bob - Bob's address
 * @param timeoutBlock - Channel closes after this block (cooperative close)
 */
export declare function paymentChannelContract(alice: string, bob: string, timeoutBlock: bigint): string;
/**
 * NFT: non-fungible token transfer guard
 *
 * @param ownerAddress - Current owner
 * @param tokenId - The token identifier
 */
export declare function nftContract(ownerAddress: string, tokenId: string): string;
/**
 * Escrow: 2-of-3 with arbiter
 * Buyer, Seller, or (Buyer + Arbiter) / (Seller + Arbiter) can release
 *
 * @param buyer - Buyer address
 * @param seller - Seller address
 * @param arbiter - Trusted arbiter address
 */
export declare function escrowContract(buyer: string, seller: string, arbiter: string): string;
/**
 * Vault: daily spending limit with hot wallet + full access for cold wallet
 *
 * @param hotWallet - Hot wallet address (limited)
 * @param coldWallet - Cold wallet address (unlimited)
 * @param dailyLimit - Max Minima spendable per day via hot wallet
 */
export declare function vaultContract(hotWallet: string, coldWallet: string, dailyLimit: bigint): string;
export interface ContractTemplate {
    name: string;
    description: string;
    script: string;
}
export declare const TEMPLATES: Record<string, (args: Record<string, string>) => string>;
export declare function getTemplate(name: string, args: Record<string, string>): string;
//# sourceMappingURL=index.d.ts.map