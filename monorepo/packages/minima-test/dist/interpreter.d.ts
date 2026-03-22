/**
 * KISS VM Interpreter — TypeScript implementation
 *
 * Minima KISS VM scripting language interpreter.
 * Supports all standard KISS VM operations and built-in functions.
 *
 * Reference: docs.minima.global/learn/smart-contracts/kiss-vm
 */
import { TransactionContext, ContractExecution } from './types.js';
export declare class KissVMInterpreter {
    private stack;
    private vars;
    private trace;
    private ctx;
    private instructions;
    private readonly MAX_INSTRUCTIONS;
    private readonly MAX_STACK;
    constructor(ctx: TransactionContext);
    execute(script: string): ContractExecution;
    private tokenize;
    private runTokens;
    private runBranch;
    private evalExpr;
    private pushLiteralOrExec;
    private callBuiltin;
    private push;
    private pop;
    private popNum;
    private popBool;
}
//# sourceMappingURL=interpreter.d.ts.map