"use strict";
/**
 * Test runner — Jest/Node-style API for KISS VM contracts
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.Expectation = exports.test = void 0;
exports.describe = describe;
exports.it = it;
exports.beforeEach = beforeEach;
exports.afterEach = afterEach;
exports.expectContract = expectContract;
exports.runContract = runContract;
exports.runAll = runAll;
exports.runTestSuite = runTestSuite;
exports.printSuiteResult = printSuiteResult;
const interpreter_js_1 = require("./interpreter.js");
// ANSI colors
const C = {
    green: (s) => `\x1b[32m${s}\x1b[0m`,
    red: (s) => `\x1b[31m${s}\x1b[0m`,
    yellow: (s) => `\x1b[33m${s}\x1b[0m`,
    cyan: (s) => `\x1b[36m${s}\x1b[0m`,
    bold: (s) => `\x1b[1m${s}\x1b[0m`,
    dim: (s) => `\x1b[2m${s}\x1b[0m`,
};
let currentSuite = null;
const suites = [];
// ── Public test API ───────────────────────────────────────────────────────────
function describe(name, fn) {
    const suite = { name, tests: [], beforeEachFns: [], afterEachFns: [] };
    suites.push(suite);
    const prev = currentSuite;
    currentSuite = suite;
    fn();
    currentSuite = prev;
}
function it(name, fn) {
    if (!currentSuite)
        throw new Error('it() must be inside describe()');
    currentSuite.tests.push({ name, fn });
}
exports.test = it;
function beforeEach(fn) {
    if (!currentSuite)
        throw new Error('beforeEach() must be inside describe()');
    currentSuite.beforeEachFns.push(fn);
}
function afterEach(fn) {
    if (!currentSuite)
        throw new Error('afterEach() must be inside describe()');
    currentSuite.afterEachFns.push(fn);
}
// ── Expectations ──────────────────────────────────────────────────────────────
class Expectation {
    constructor(script, ctx) {
        this.script = script;
        this.ctx = ctx;
    }
    async toPass() {
        const vm = new interpreter_js_1.KissVMInterpreter(this.ctx);
        const res = vm.execute(this.script);
        if (res.result !== 'TRUE') {
            throw new Error(`Expected contract to pass (TRUE), got ${res.result}\n` +
                (res.error ? `Error: ${res.error}\n` : '') +
                `Trace: ${res.trace.slice(-10).join(' ')}`);
        }
    }
    async toFail() {
        const vm = new interpreter_js_1.KissVMInterpreter(this.ctx);
        const res = vm.execute(this.script);
        if (res.result === 'TRUE') {
            throw new Error(`Expected contract to fail (FALSE/EXCEPTION), but it returned TRUE\n` +
                `Trace: ${res.trace.slice(-10).join(' ')}`);
        }
    }
    async toReturn(expected) {
        const vm = new interpreter_js_1.KissVMInterpreter(this.ctx);
        const res = vm.execute(this.script);
        if (res.result !== expected) {
            throw new Error(`Expected ${expected}, got ${res.result}\n` +
                (res.error ? `Error: ${res.error}` : ''));
        }
    }
    async toThrow(errorContains) {
        const vm = new interpreter_js_1.KissVMInterpreter(this.ctx);
        const res = vm.execute(this.script);
        if (res.result !== 'EXCEPTION') {
            throw new Error(`Expected EXCEPTION, got ${res.result}`);
        }
        if (errorContains && !res.error?.includes(errorContains)) {
            throw new Error(`Expected error containing "${errorContains}", got: ${res.error}`);
        }
    }
}
exports.Expectation = Expectation;
function expectContract(script, ctx) {
    return new Expectation(script, ctx);
}
// ── Simple assertion helper ───────────────────────────────────────────────────
function runContract(script, ctx) {
    const vm = new interpreter_js_1.KissVMInterpreter(ctx);
    return vm.execute(script).result;
}
// ── Test runner ───────────────────────────────────────────────────────────────
async function runAll() {
    let totalPassed = 0;
    let totalFailed = 0;
    for (const suite of suites) {
        console.log(`\n  ${C.bold(C.cyan(suite.name))}`);
        for (const testCase of suite.tests) {
            const start = Date.now();
            try {
                for (const fn of suite.beforeEachFns)
                    await fn();
                await testCase.fn();
                for (const fn of suite.afterEachFns)
                    await fn();
                const ms = Date.now() - start;
                console.log(`    ${C.green('✓')} ${testCase.name} ${C.dim(`(${ms}ms)`)}`);
                totalPassed++;
            }
            catch (err) {
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
function runTestSuite(name, cases) {
    const start = Date.now();
    const results = [];
    for (const tc of cases) {
        const t0 = Date.now();
        const vm = new interpreter_js_1.KissVMInterpreter(tc.context);
        const exec = vm.execute(tc.script);
        const got = exec.result;
        const expectedResult = tc.expected ? 'TRUE' : 'FALSE';
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
function printSuiteResult(result) {
    const color = result.failed === 0 ? C.green : C.red;
    console.log(`\n${C.bold(result.name)}: ${color(`${result.passed}/${result.total} passed`)} (${result.durationMs}ms)`);
    for (const r of result.results) {
        if (r.passed) {
            console.log(`  ${C.green('✓')} ${r.name}`);
        }
        else {
            console.log(`  ${C.red('✗')} ${r.name} — expected ${r.expected ? 'TRUE' : 'FALSE'}, got ${r.got}${r.error ? ` (${r.error})` : ''}`);
        }
    }
}
//# sourceMappingURL=test-runner.js.map