"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ALL_RULES = exports.R010_StatePortRange = exports.R009_SignedByHex = exports.R008_AssertTrue = exports.R007_MultisigArgs = exports.R006_ChecksigWarning = exports.R005_UnboundedWhile = exports.R004_InstructionLimit = exports.R003_UseBeforeLet = exports.R002_UnreachableCode = exports.R001_NoReturn = void 0;
const tokenizer_1 = require("./tokenizer");
// ─────────────────────────────────────────────
// R001: No RETURN statement
// ─────────────────────────────────────────────
const R001_NoReturn = (tokens) => {
    const hasReturn = tokens.some(t => t.type === 'KEYWORD' && t.value === 'RETURN');
    if (!hasReturn) {
        return [{ severity: 'error', code: 'R001', message: 'Script has no RETURN statement — will throw at runtime' }];
    }
    return [];
};
exports.R001_NoReturn = R001_NoReturn;
// ─────────────────────────────────────────────
// R002: Unreachable code after RETURN
// ─────────────────────────────────────────────
const R002_UnreachableCode = (tokens) => {
    const errors = [];
    // Find RETURN at top level (depth 0) — any STATEMENT keyword after at same depth is unreachable
    let depth = 0;
    let foundTopLevelReturn = false;
    for (let i = 0; i < tokens.length; i++) {
        const t = tokens[i];
        if (t.type === 'KEYWORD') {
            if (t.value === 'IF' || t.value === 'WHILE')
                depth++;
            if (t.value === 'ENDIF' || t.value === 'ENDWHILE')
                depth--;
            if (t.value === 'RETURN' && depth === 0) {
                foundTopLevelReturn = true;
                continue;
            }
        }
        // Only flag statement-level keywords as unreachable (not expressions/values)
        if (foundTopLevelReturn && depth === 0 && t.type === 'KEYWORD' && tokenizer_1.STATEMENT_KEYWORDS.has(t.value)) {
            errors.push({
                severity: 'warning', code: 'R002',
                message: `Unreachable statement '${t.value}' after top-level RETURN`,
                pos: t.pos
            });
            break; // report once
        }
    }
    return errors;
};
exports.R002_UnreachableCode = R002_UnreachableCode;
// ─────────────────────────────────────────────
// R003: Variable used before LET
// ─────────────────────────────────────────────
const R003_UseBeforeLet = (tokens) => {
    const errors = [];
    const defined = new Set();
    // Built-in globals don't need LET
    const builtinGlobals = new Set(['@BLOCK', '@BLOCKDIFF', '@BLOCKMILLI', '@COINAGE',
        '@AMOUNT', '@ADDRESS', '@TOKENID', '@SCRIPT', '@TOTIN', '@TOTOUT', '@INCOUNT', '@OUTCOUNT', '@INPUT']);
    for (let i = 0; i < tokens.length; i++) {
        const t = tokens[i];
        // LET x = ... defines x
        if (t.type === 'KEYWORD' && t.value === 'LET') {
            const next = tokens[i + 1];
            if (next && next.type === 'VARIABLE') {
                defined.add(next.value);
                i++; // skip the variable name
                continue;
            }
        }
        // VARIABLE used — check if defined
        if (t.type === 'VARIABLE' && !builtinGlobals.has(t.value)) {
            // Skip if it's followed by = (LET target) or it's a function call parameter being defined
            const prev = tokens[i - 1];
            if (prev && prev.type === 'KEYWORD' && prev.value === 'LET')
                continue; // already handled
            if (!defined.has(t.value)) {
                errors.push({
                    severity: 'warning', code: 'R003',
                    message: `Variable '${t.value}' used before being defined with LET`,
                    pos: t.pos
                });
                defined.add(t.value); // only warn once per variable
            }
        }
    }
    return errors;
};
exports.R003_UseBeforeLet = R003_UseBeforeLet;
// ─────────────────────────────────────────────
// R004: Instruction count estimation
// ─────────────────────────────────────────────
const R004_InstructionLimit = (tokens) => {
    const errors = [];
    // Count statement keywords as proxy for instructions
    const statementCount = tokens.filter(t => t.type === 'KEYWORD' && tokenizer_1.STATEMENT_KEYWORDS.has(t.value)).length;
    if (statementCount > 900) {
        errors.push({
            severity: 'error', code: 'R004',
            message: `Estimated ~${statementCount} instructions — dangerously close to or over the 1024 limit`
        });
    }
    else if (statementCount > 700) {
        errors.push({
            severity: 'warning', code: 'R004',
            message: `Estimated ~${statementCount} instructions — approaching the 1024 limit. Consider simplifying.`
        });
    }
    return errors;
};
exports.R004_InstructionLimit = R004_InstructionLimit;
// ─────────────────────────────────────────────
// R005: WHILE without upper bound (infinite loop risk)
// ─────────────────────────────────────────────
const R005_UnboundedWhile = (tokens) => {
    const errors = [];
    for (let i = 0; i < tokens.length; i++) {
        const t = tokens[i];
        if (t.type === 'KEYWORD' && t.value === 'WHILE') {
            // Heuristic: check if condition contains a comparison with a literal number
            // Scan until DO
            let hasBound = false;
            let j = i + 1;
            while (j < tokens.length && !(tokens[j].type === 'KEYWORD' && tokens[j].value === 'DO')) {
                const tk = tokens[j];
                if (tk.type === 'NUMBER' || tk.type === 'GLOBAL')
                    hasBound = true;
                if (tk.type === 'KEYWORD' && (tk.value === 'LT' || tk.value === 'GT' || tk.value === 'LTE' || tk.value === 'GTE'))
                    hasBound = true;
                j++;
            }
            if (!hasBound) {
                errors.push({
                    severity: 'warning', code: 'R005',
                    message: 'WHILE loop may have no upper bound — risk of hitting 512 iteration limit',
                    pos: t.pos
                });
            }
        }
    }
    return errors;
};
exports.R005_UnboundedWhile = R005_UnboundedWhile;
// ─────────────────────────────────────────────
// R006: CHECKSIG always returns FALSE (mock warning)
// ─────────────────────────────────────────────
const R006_ChecksigWarning = (tokens) => {
    const errors = [];
    const hasCHECKSIG = tokens.some(t => t.type === 'KEYWORD' && t.value === 'CHECKSIG');
    if (hasCHECKSIG) {
        errors.push({
            severity: 'info', code: 'R006',
            message: 'CHECKSIG performs raw EC signature verification — ensure sig/data/pubkey are in correct format (Minima uses secp256k1)'
        });
    }
    return errors;
};
exports.R006_ChecksigWarning = R006_ChecksigWarning;
// ─────────────────────────────────────────────
// R007: MULTISIG argument count check
// ─────────────────────────────────────────────
const R007_MultisigArgs = (tokens) => {
    const errors = [];
    for (let i = 0; i < tokens.length; i++) {
        const t = tokens[i];
        if (t.type === 'KEYWORD' && t.value === 'MULTISIG') {
            // MULTISIG(n, key1, key2...) - count args
            if (tokens[i + 1]?.type !== 'LPAREN')
                continue;
            let j = i + 2;
            let depth = 1;
            let argCount = 1;
            while (j < tokens.length && depth > 0) {
                if (tokens[j].type === 'LPAREN')
                    depth++;
                else if (tokens[j].type === 'RPAREN') {
                    depth--;
                    if (depth === 0)
                        break;
                }
                else if (tokens[j].type === 'COMMA' && depth === 1)
                    argCount++;
                j++;
            }
            // argCount = total args including n
            // first arg is n (required), rest are keys — need at least n+1 args total
            // We can't know n statically always, but check minimum (at least 2 args: n + 1 key)
            if (argCount < 2) {
                errors.push({
                    severity: 'error', code: 'R007',
                    message: `MULTISIG requires at least 2 arguments: MULTISIG(n, key1, ...) — got ${argCount}`,
                    pos: t.pos
                });
            }
        }
    }
    return errors;
};
exports.R007_MultisigArgs = R007_MultisigArgs;
// ─────────────────────────────────────────────
// R008: ASSERT without meaningful condition
// ─────────────────────────────────────────────
const R008_AssertTrue = (tokens) => {
    const errors = [];
    for (let i = 0; i < tokens.length; i++) {
        if (tokens[i].type === 'KEYWORD' && tokens[i].value === 'ASSERT') {
            const next = tokens[i + 1];
            if (next && next.type === 'BOOLEAN' && next.value === 'TRUE') {
                errors.push({
                    severity: 'warning', code: 'R008',
                    message: 'ASSERT TRUE is a no-op — remove it or use a real condition',
                    pos: tokens[i].pos
                });
            }
        }
    }
    return errors;
};
exports.R008_AssertTrue = R008_AssertTrue;
// ─────────────────────────────────────────────
// R009: SIGNEDBY with hardcoded public key check
// ─────────────────────────────────────────────
const R009_SignedByHex = (tokens) => {
    const errors = [];
    for (let i = 0; i < tokens.length; i++) {
        if (tokens[i].type === 'KEYWORD' && tokens[i].value === 'SIGNEDBY') {
            if (tokens[i + 1]?.type === 'LPAREN') {
                const arg = tokens[i + 2];
                if (arg?.type === 'HEX') {
                    const hexLen = (arg.value.length - 2); // remove 0x
                    if (hexLen !== 64) {
                        errors.push({
                            severity: 'warning', code: 'R009',
                            message: `SIGNEDBY expects a 32-byte (64 hex char) public key — got ${hexLen} hex chars`,
                            pos: arg.pos
                        });
                    }
                }
            }
        }
    }
    return errors;
};
exports.R009_SignedByHex = R009_SignedByHex;
// ─────────────────────────────────────────────
// R010: STATE port out of range (0-255)
// ─────────────────────────────────────────────
const R010_StatePortRange = (tokens) => {
    const errors = [];
    for (let i = 0; i < tokens.length; i++) {
        const t = tokens[i];
        if (t.type === 'KEYWORD' && (t.value === 'STATE' || t.value === 'PREVSTATE')) {
            if (tokens[i + 1]?.type === 'LPAREN') {
                const arg = tokens[i + 2];
                if (arg?.type === 'NUMBER') {
                    const port = parseInt(arg.value);
                    if (port < 0 || port > 255) {
                        errors.push({
                            severity: 'error', code: 'R010',
                            message: `${t.value} port must be 0–255, got ${port}`,
                            pos: arg.pos
                        });
                    }
                }
            }
        }
    }
    return errors;
};
exports.R010_StatePortRange = R010_StatePortRange;
exports.ALL_RULES = [
    exports.R001_NoReturn,
    exports.R002_UnreachableCode,
    exports.R003_UseBeforeLet,
    exports.R004_InstructionLimit,
    exports.R005_UnboundedWhile,
    exports.R006_ChecksigWarning,
    exports.R007_MultisigArgs,
    exports.R008_AssertTrue,
    exports.R009_SignedByHex,
    exports.R010_StatePortRange,
];
//# sourceMappingURL=rules.js.map