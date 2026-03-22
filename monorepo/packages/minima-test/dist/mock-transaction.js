"use strict";
/**
 * MockTransaction — fluent builder for test transaction contexts
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.MockTransaction = void 0;
class MockTransaction {
    constructor() {
        this.ctx = {
            inputs: [],
            outputs: [],
            signers: [],
            block: 0n,
            blocktime: 0n,
        };
    }
    static create() {
        return new MockTransaction();
    }
    // ── Inputs ────────────────────────────────────────────────────────────────
    addInput(coin) {
        this.ctx.inputs.push(coin);
        return this;
    }
    withInput(opts) {
        return this.addInput({
            coinid: opts.coinid ?? '0x' + '00'.repeat(32),
            address: opts.address,
            amount: opts.amount,
            tokenid: opts.tokenid ?? '0x00',
            stateVars: opts.stateVars,
            script: opts.script,
        });
    }
    // ── Outputs ───────────────────────────────────────────────────────────────
    addOutput(coin) {
        this.ctx.outputs.push(coin);
        return this;
    }
    withOutput(opts) {
        return this.addOutput({
            coinid: opts.coinid,
            address: opts.address,
            amount: opts.amount,
            tokenid: opts.tokenid ?? '0x00',
            stateVars: opts.stateVars,
        });
    }
    // ── Signers ───────────────────────────────────────────────────────────────
    signedBy(address, publicKey) {
        this.ctx.signers.push({ address, publicKey });
        return this;
    }
    // ── Block / time ──────────────────────────────────────────────────────────
    atBlock(block) {
        this.ctx.block = block;
        return this;
    }
    atTime(blocktime) {
        this.ctx.blocktime = blocktime;
        return this;
    }
    // ── State variables ───────────────────────────────────────────────────────
    withState(stateVars) {
        this.ctx.stateVars = { ...this.ctx.stateVars, ...stateVars };
        return this;
    }
    withPrevState(prevStateVars) {
        this.ctx.prevStateVars = { ...this.ctx.prevStateVars, ...prevStateVars };
        return this;
    }
    // ── MMR ───────────────────────────────────────────────────────────────────
    withMMR(root, peaks) {
        this.ctx.mmrroot = root;
        this.ctx.mmrpeaks = peaks ?? root;
        return this;
    }
    // ── Build ─────────────────────────────────────────────────────────────────
    build() {
        return { ...this.ctx };
    }
}
exports.MockTransaction = MockTransaction;
//# sourceMappingURL=mock-transaction.js.map