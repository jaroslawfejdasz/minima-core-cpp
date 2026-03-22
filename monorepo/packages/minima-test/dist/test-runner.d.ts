/**
 * Test runner — Jest/Node-style API for KISS VM contracts
 */
import { TestCase, TestSuiteResult, TransactionContext, ContractResult } from './types.js';
type TestFn = () => void | Promise<void>;
export declare function describe(name: string, fn: () => void): void;
export declare function it(name: string, fn: TestFn): void;
export declare const test: typeof it;
export declare function beforeEach(fn: TestFn): void;
export declare function afterEach(fn: TestFn): void;
export declare class Expectation {
    private script;
    private ctx;
    constructor(script: string, ctx: TransactionContext);
    toPass(): Promise<void>;
    toFail(): Promise<void>;
    toReturn(expected: ContractResult): Promise<void>;
    toThrow(errorContains?: string): Promise<void>;
}
export declare function expectContract(script: string, ctx: TransactionContext): Expectation;
export declare function runContract(script: string, ctx: TransactionContext): ContractResult;
export declare function runAll(): Promise<boolean>;
export declare function runTestSuite(name: string, cases: TestCase[]): TestSuiteResult;
export declare function printSuiteResult(result: TestSuiteResult): void;
export {};
//# sourceMappingURL=test-runner.d.ts.map