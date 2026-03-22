#!/usr/bin/env node
"use strict";
/**
 * minima-test CLI — run KISS VM test files
 *
 * Usage:
 *   minima-test [pattern]              # run tests matching glob
 *   minima-test --watch [pattern]      # watch mode
 *   minima-test --version              # print version
 */
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
const node_fs_1 = require("node:fs");
const node_path_1 = require("node:path");
const node_url_1 = require("node:url");
const test_runner_js_1 = require("./test-runner.js");
const args = process.argv.slice(2);
if (args.includes('--version') || args.includes('-v')) {
    const pkg = JSON.parse(require('fs').readFileSync((0, node_path_1.join)(__dirname, '..', 'package.json'), 'utf8'));
    console.log(pkg.version);
    process.exit(0);
}
const pattern = args.find(a => !a.startsWith('--')) ?? '**/*.test.{ts,js}';
const cwd = process.cwd();
function findTestFiles(dir, pat) {
    const files = [];
    const ext = ['.test.ts', '.test.js', '.spec.ts', '.spec.js'];
    function walk(d) {
        if (!(0, node_fs_1.existsSync)(d))
            return;
        for (const f of (0, node_fs_1.readdirSync)(d)) {
            const fp = (0, node_path_1.join)(d, f);
            const st = (0, node_fs_1.statSync)(fp);
            if (st.isDirectory() && f !== 'node_modules' && f !== 'dist') {
                walk(fp);
            }
            else if (ext.some(e => f.endsWith(e))) {
                files.push(fp);
            }
        }
    }
    walk(dir);
    return files;
}
async function main() {
    const files = findTestFiles(cwd, pattern);
    if (files.length === 0) {
        console.error(`No test files found matching: ${pattern}`);
        process.exit(1);
    }
    console.log(`\n  minima-test — found ${files.length} test file(s)\n`);
    for (const file of files) {
        const url = (0, node_url_1.pathToFileURL)(file).href;
        await Promise.resolve(`${url}`).then(s => __importStar(require(s)));
    }
    const ok = await (0, test_runner_js_1.runAll)();
    process.exit(ok ? 0 : 1);
}
main().catch(err => {
    console.error(err);
    process.exit(1);
});
//# sourceMappingURL=cli.js.map