"use strict";
/**
 * minima-test — Testing framework for Minima KISS VM smart contracts
 *
 * @example
 * import { describe, it, expectContract, MockTransaction } from 'minima-test';
 *
 * describe('Timelock contract', () => {
 *   it('passes when block >= lockBlock', async () => {
 *     const ctx = MockTransaction.create()
 *       .withInput({ address: '0xABC', amount: 100n })
 *       .atBlock(200n)
 *       .build();
 *
 *     await expectContract('RETURN @BLOCK GTE 100', ctx).toPass();
 *   });
 * });
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.Expectation = exports.printSuiteResult = exports.runTestSuite = exports.runAll = exports.runContract = exports.expectContract = exports.afterEach = exports.beforeEach = exports.test = exports.it = exports.describe = exports.MockTransaction = exports.KissVMInterpreter = void 0;
var interpreter_js_1 = require("./interpreter.js");
Object.defineProperty(exports, "KissVMInterpreter", { enumerable: true, get: function () { return interpreter_js_1.KissVMInterpreter; } });
var mock_transaction_js_1 = require("./mock-transaction.js");
Object.defineProperty(exports, "MockTransaction", { enumerable: true, get: function () { return mock_transaction_js_1.MockTransaction; } });
var test_runner_js_1 = require("./test-runner.js");
Object.defineProperty(exports, "describe", { enumerable: true, get: function () { return test_runner_js_1.describe; } });
Object.defineProperty(exports, "it", { enumerable: true, get: function () { return test_runner_js_1.it; } });
Object.defineProperty(exports, "test", { enumerable: true, get: function () { return test_runner_js_1.test; } });
Object.defineProperty(exports, "beforeEach", { enumerable: true, get: function () { return test_runner_js_1.beforeEach; } });
Object.defineProperty(exports, "afterEach", { enumerable: true, get: function () { return test_runner_js_1.afterEach; } });
Object.defineProperty(exports, "expectContract", { enumerable: true, get: function () { return test_runner_js_1.expectContract; } });
Object.defineProperty(exports, "runContract", { enumerable: true, get: function () { return test_runner_js_1.runContract; } });
Object.defineProperty(exports, "runAll", { enumerable: true, get: function () { return test_runner_js_1.runAll; } });
Object.defineProperty(exports, "runTestSuite", { enumerable: true, get: function () { return test_runner_js_1.runTestSuite; } });
Object.defineProperty(exports, "printSuiteResult", { enumerable: true, get: function () { return test_runner_js_1.printSuiteResult; } });
Object.defineProperty(exports, "Expectation", { enumerable: true, get: function () { return test_runner_js_1.Expectation; } });
//# sourceMappingURL=index.js.map