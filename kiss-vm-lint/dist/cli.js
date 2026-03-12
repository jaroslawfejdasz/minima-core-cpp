#!/usr/bin/env node
"use strict";
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
const fs = __importStar(require("fs"));
const path = __importStar(require("path"));
const analyzer_1 = require("./analyzer");
const RESET = '\x1b[0m';
const RED = '\x1b[31m';
const YELLOW = '\x1b[33m';
const CYAN = '\x1b[36m';
const GRAY = '\x1b[90m';
const GREEN = '\x1b[32m';
const BOLD = '\x1b[1m';
function colorSeverity(sev) {
    if (sev === 'error')
        return RED + 'error' + RESET;
    if (sev === 'warning')
        return YELLOW + 'warning' + RESET;
    return CYAN + 'info' + RESET;
}
function run() {
    const args = process.argv.slice(2);
    const flags = { json: false, noInfo: false, strict: false };
    const files = [];
    for (const arg of args) {
        if (arg === '--json') {
            flags.json = true;
            continue;
        }
        if (arg === '--no-info') {
            flags.noInfo = true;
            continue;
        }
        if (arg === '--strict') {
            flags.strict = true;
            continue;
        }
        if (arg === '--help' || arg === '-h') {
            printHelp();
            process.exit(0);
        }
        files.push(arg);
    }
    if (files.length === 0) {
        // read from stdin
        let input = '';
        process.stdin.setEncoding('utf8');
        process.stdin.on('data', d => input += d);
        process.stdin.on('end', () => lintScript('<stdin>', input, flags));
        return;
    }
    let totalErrors = 0;
    let totalWarnings = 0;
    for (const file of files) {
        let script;
        try {
            script = fs.readFileSync(path.resolve(file), 'utf8');
        }
        catch (e) {
            console.error(RED + 'Error reading file: ' + file + RESET);
            process.exit(1);
        }
        const result = lintScript(file, script, flags);
        totalErrors += result.errors;
        totalWarnings += result.warnings;
    }
    if (files.length > 1) {
        console.log('\n' + BOLD + '─── Total ───' + RESET);
        console.log(RED + totalErrors + ' error(s)' + RESET + ', ' + YELLOW + totalWarnings + ' warning(s)' + RESET);
    }
    if (totalErrors > 0 || (flags.strict && totalWarnings > 0)) {
        process.exit(1);
    }
}
function lintScript(filename, script, flags) {
    const result = (0, analyzer_1.analyze)(script);
    if (flags.json) {
        console.log(JSON.stringify(result, null, 2));
        return result.summary;
    }
    const items = flags.noInfo ? result.all.filter(e => e.severity !== 'info') : result.all;
    if (items.length === 0) {
        console.log(GREEN + '✓' + RESET + ' ' + BOLD + filename + RESET + GRAY + '  — no issues' + RESET);
        console.log(GRAY + '  Instructions (estimated): ' + result.summary.instructionEstimate + '/1024' + RESET);
        return result.summary;
    }
    console.log('\n' + BOLD + filename + RESET);
    console.log(GRAY + '─'.repeat(Math.min(filename.length + 4, 60)) + RESET);
    for (const item of items) {
        const posStr = item.pos !== undefined ? GRAY + '@' + item.pos + RESET + ' ' : '';
        console.log('  ' + colorSeverity(item.severity) + '  ' + posStr + GRAY + '[' + item.code + ']' + RESET + '  ' + item.message);
    }
    console.log('');
    console.log(GRAY + '  Instructions (estimated): ' + result.summary.instructionEstimate + '/1024' + RESET);
    console.log('  ' +
        RED + result.summary.errors + ' error(s)' + RESET + '  ' +
        YELLOW + result.summary.warnings + ' warning(s)' + RESET + '  ' +
        CYAN + result.summary.infos + ' info(s)' + RESET);
    return result.summary;
}
function printHelp() {
    console.log(`
${BOLD}kiss-vm-lint${RESET} — Static analyzer for Minima KISS VM scripts

${BOLD}Usage:${RESET}
  kiss-vm-lint [options] <file.kvm> [...]
  cat script.kvm | kiss-vm-lint

${BOLD}Options:${RESET}
  --json      Output results as JSON
  --no-info   Hide info-level messages (show only errors + warnings)
  --strict    Exit with error code if any warnings are present
  --help      Show this help

${BOLD}Exit codes:${RESET}
  0  No errors (warnings are ok unless --strict)
  1  Errors found (or warnings with --strict)

${BOLD}Error codes:${RESET}
  E001-E003   Tokenizer errors (unterminated string, bad hex, unknown char)
  E010-E011   Instruction limit exceeded, missing RETURN
  E020-E022   Statement errors
  E030        LET errors
  E040-E042   IF/THEN/ENDIF errors
  E050-E051   WHILE/DO/ENDWHILE errors
  E060        Division by zero
  E070-E083   Expression / function call errors
  W010        Unused variable
  W020        Variable shadows keyword
  W030        Deep nesting
  W040        Dead code after RETURN
  W050        Use EQ not =
  W060        Unknown global variable
  W070        Variable used before LET
  I001-I002   EXEC/MAST dynamic code info
  I010-I012   State/output security hints
`);
}
run();
//# sourceMappingURL=cli.js.map