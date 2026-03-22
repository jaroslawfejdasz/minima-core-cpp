/**
 * Test runner — Jest/Node-style API for KISS VM contracts
 */

import { KissVMInterpreter } from './interpreter.js';
import { MockTransaction } from './mock-transaction.js';
import {
  TestCase,
  TestResult,
  TestSuiteResult,
  TransactionContext,
  ContractResult,
} from './types.js';

// ANSI colors
const C = {
  green:  (s: string) => `\x1b[32m${s}\x1b[0m`,
  red:    (s: string) => `\x1b[31m${s}\x1b[0m`,
  yellow: (s: string) => `\x1b[33m${s}\x1b[0m`,
  cyan:   (s: string) => `\x1b[36m${s}\x1b[0m`,
  bold:   (s: string) => `\x1b[1m${s}\x1b[0m`,
  dim:    (s: string) => `\x1b[2m${s}\x1b[0m`,
};

type TestFn = () => void | Promise<void>;

interface SuiteEntry {
  name: string;
  tests: Array<{ name: string; fn: TestFn }>;
  beforeEachFns: TestFn[];
  afterEachFns: TestFn[];
}

let currentSuite: SuiteEntry | null = null;
const suites: SuiteEntry[] = [];

// ── Public test API ───────────────────────────────────────────────────────────

export function describe(name: string, fn: () => void): void {
  const suite: SuiteEntry = { name, tests: [], beforeEachFns: [], afterEachFns: [] };
  suites.push(suite);
  const prev = currentSuite;
  currentSuite = suite;
  fn();
  currentSuite = prev;
}

export function it(name: string, fn: TestFn): void {
  if (!currentSuite) throw new Error('it() must be inside describe()');
  currentSuite.tests.push({ name, fn });
}

export const test = it;

export function beforeEach(fn: TestFn): void {
  if (!currentSuite) throw new Error('beforeEach() must be inside describe()');
  currentSuite.beforeEachFns.push(fn);
}

export function afterEach(fn: TestFn): void {
  if (!currentSuite) throw new Error('afterEach() must be inside describe()');
  currentSuite.afterEachFns.push(fn);
}

// ── Expectations ──────────────────────────────────────────────────────────────

export class Expectation {
  constructor(
    private script: string,
    private ctx: TransactionContext,
  ) {}

  async toPass(): Promise<void> {
    const vm = new KissVMInterpreter(this.ctx);
    const res = vm.execute(this.script);
    if (res.result !== 'TRUE') {
      throw new Error(
        `Expected contract to pass (TRUE), got ${res.result}\n` +
        (res.error ? `Error: ${res.error}\n` : '') +
        `Trace: ${res.trace.slice(-10).join(' ')}`
      );
    }
  }

  async toFail(): Promise<void> {
    const vm = new KissVMInterpreter(this.ctx);
    const res = vm.execute(this.script);
    if (res.result === 'TRUE') {
      throw new Error(
        `Expected contract to fail (FALSE/EXCEPTION), but it returned TRUE\n` +
        `Trace: ${res.trace.slice(-10).join(' ')}`
      );
    }
  }

  async toReturn(expected: ContractResult): Promise<void> {
    const vm = new KissVMInterpreter(this.ctx);
    const res = vm.execute(this.script);
    if (res.result !== expected) {
      throw new Error(
        `Expected ${expected}, got ${res.result}\n` +
        (res.error ? `Error: ${res.error}` : '')
      );
    }
  }

  async toThrow(errorContains?: string): Promise<void> {
    const vm = new KissVMInterpreter(this.ctx);
    const res = vm.execute(this.script);
    if (res.result !== 'EXCEPTION') {
      throw new Error(`Expected EXCEPTION, got ${res.result}`);
    }
    if (errorContains && !res.error?.includes(errorContains)) {
      throw new Error(
        `Expected error containing "${errorContains}", got: ${res.error}`
      );
    }
  }
}

export function expectContract(script: string, ctx: TransactionContext): Expectation {
  return new Expectation(script, ctx);
}

// ── Simple assertion helper ───────────────────────────────────────────────────

export function runContract(script: string, ctx: TransactionContext): ContractResult {
  const vm = new KissVMInterpreter(ctx);
  return vm.execute(script).result;
}

// ── Test runner ───────────────────────────────────────────────────────────────

export async function runAll(): Promise<boolean> {
  let totalPassed = 0;
  let totalFailed = 0;

  for (const suite of suites) {
    console.log(`\n  ${C.bold(C.cyan(suite.name))}`);
    for (const testCase of suite.tests) {
      const start = Date.now();
      try {
        for (const fn of suite.beforeEachFns) await fn();
        await testCase.fn();
        for (const fn of suite.afterEachFns) await fn();
        const ms = Date.now() - start;
        console.log(`    ${C.green('✓')} ${testCase.name} ${C.dim(`(${ms}ms)`)}`);
        totalPassed++;
      } catch (err: unknown) {
        const ms = Date.now() - start;
        const msg = err instanceof Error ? err.message : String(err);
        console.log(`    ${C.red('✗')} ${testCase.name} ${C.dim(`(${ms}ms)`)}`);
        console.log(`      ${C.red(msg.split('\n')[0])}`);
        totalFailed++;
      }
    }
  }

  const color = totalFailed === 0 ? C.green : C.red;
  console.log(`\n  ${color(C.bold(`${totalPassed} passed`))}` +
              (totalFailed > 0 ? `, ${C.red(C.bold(`${totalFailed} failed`))}` : '') +
              `\n`);

  return totalFailed === 0;
}

// ── Legacy batch runner ───────────────────────────────────────────────────────

export function runTestSuite(name: string, cases: TestCase[]): TestSuiteResult {
  const start = Date.now();
  const results: TestResult[] = [];

  for (const tc of cases) {
    const t0 = Date.now();
    const vm = new KissVMInterpreter(tc.context);
    const exec = vm.execute(tc.script);

    const got: ContractResult = exec.result;
    const expectedResult: ContractResult = tc.expected ? 'TRUE' : 'FALSE';
    const passed = got === expectedResult;

    results.push({
      name: tc.name,
      passed,
      expected: tc.expected,
      got,
      error: exec.error,
      durationMs: Date.now() - t0,
    });
  }

  const passed = results.filter(r => r.passed).length;
  const failed = results.length - passed;

  return {
    name,
    passed,
    failed,
    total: results.length,
    results,
    durationMs: Date.now() - start,
  };
}

export function printSuiteResult(result: TestSuiteResult): void {
  const color = result.failed === 0 ? C.green : C.red;
  console.log(`\n${C.bold(result.name)}: ${color(`${result.passed}/${result.total} passed`)} (${result.durationMs}ms)`);
  for (const r of result.results) {
    if (r.passed) {
      console.log(`  ${C.green('✓')} ${r.name}`);
    } else {
      console.log(`  ${C.red('✗')} ${r.name} — expected ${r.expected ? 'TRUE' : 'FALSE'}, got ${r.got}${r.error ? ` (${r.error})` : ''}`);
    }
  }
}
