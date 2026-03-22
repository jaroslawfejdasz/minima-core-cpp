/**
 * MockTransaction — fluent builder for test transaction contexts
 */
import { TransactionContext, InputCoin, OutputCoin, StateVar } from './types.js';
export declare class MockTransaction {
    private ctx;
    static create(): MockTransaction;
    addInput(coin: InputCoin): this;
    withInput(opts: Partial<InputCoin> & {
        address: string;
        amount: bigint;
    }): this;
    addOutput(coin: OutputCoin): this;
    withOutput(opts: Partial<OutputCoin> & {
        address: string;
        amount: bigint;
    }): this;
    signedBy(address: string, publicKey?: string): this;
    atBlock(block: bigint): this;
    atTime(blocktime: bigint): this;
    withState(stateVars: StateVar): this;
    withPrevState(prevStateVars: StateVar): this;
    withMMR(root: string, peaks?: string): this;
    build(): TransactionContext;
}
//# sourceMappingURL=mock-transaction.d.ts.map