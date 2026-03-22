"use strict";
/**
 * KISS VM lint rules
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.ALL_RULES = exports.ruleNoDeadCode = exports.ruleMagicVars = exports.ruleNoRSA = exports.ruleAddressCheck = exports.ruleBalancedWhile = exports.ruleBalancedIf = exports.ruleReturnPresent = exports.ruleInstructionLimit = void 0;
// ── Rule: instruction count ───────────────────────────────────────────────────
exports.ruleInstructionLimit = {
    id: 'instruction-limit',
    description: 'KISS VM has a hard limit of 1024 instructions',
    severity: 'error',
    check(tokens) {
        const issues = [];
        if (tokens.length > 1024) {
            issues.push({
                rule: 'instruction-limit',
                severity: 'error',
                message: `Script has ${tokens.length} tokens (max 1024). May exceed instruction limit at runtime.`,
                suggestion: 'Split logic into sub-scripts using MAST, or reduce complexity.',
            });
        }
        else if (tokens.length > 800) {
            issues.push({
                rule: 'instruction-limit',
                severity: 'warning',
                message: `Script has ${tokens.length} tokens — approaching 1024 limit.`,
            });
        }
        return issues;
    },
};
// ── Rule: RETURN missing ──────────────────────────────────────────────────────
exports.ruleReturnPresent = {
    id: 'return-present',
    description: 'Script should have a RETURN statement',
    severity: 'warning',
    check(tokens) {
        const has = tokens.some(t => t.toUpperCase() === 'RETURN');
        if (!has) {
            return [{
                    rule: 'return-present',
                    severity: 'warning',
                    message: 'Script has no RETURN statement — result depends on top of stack.',
                    suggestion: 'Add RETURN to make intent explicit.',
                }];
        }
        return [];
    },
};
// ── Rule: unbalanced IF/ENDIF ─────────────────────────────────────────────────
exports.ruleBalancedIf = {
    id: 'balanced-if',
    description: 'Every IF must have a matching ENDIF',
    severity: 'error',
    check(tokens) {
        let depth = 0;
        for (const t of tokens) {
            const up = t.toUpperCase();
            if (up === 'IF')
                depth++;
            else if (up === 'ENDIF')
                depth--;
            if (depth < 0) {
                return [{
                        rule: 'balanced-if',
                        severity: 'error',
                        message: 'ENDIF without matching IF.',
                    }];
            }
        }
        if (depth !== 0) {
            return [{
                    rule: 'balanced-if',
                    severity: 'error',
                    message: `${depth} IF block(s) without ENDIF.`,
                    suggestion: 'Add matching ENDIF for each IF.',
                }];
        }
        return [];
    },
};
// ── Rule: WHILE without ENDWHILE ─────────────────────────────────────────────
exports.ruleBalancedWhile = {
    id: 'balanced-while',
    description: 'Every WHILE must have ENDWHILE',
    severity: 'error',
    check(tokens) {
        let depth = 0;
        for (const t of tokens) {
            const up = t.toUpperCase();
            if (up === 'WHILE')
                depth++;
            else if (up === 'ENDWHILE')
                depth--;
            if (depth < 0) {
                return [{
                        rule: 'balanced-while',
                        severity: 'error',
                        message: 'ENDWHILE without matching WHILE.',
                    }];
            }
        }
        if (depth !== 0) {
            return [{
                    rule: 'balanced-while',
                    severity: 'error',
                    message: `${depth} WHILE block(s) without ENDWHILE.`,
                }];
        }
        return [];
    },
};
// ── Rule: No address verification ─────────────────────────────────────────────
exports.ruleAddressCheck = {
    id: 'address-check',
    description: 'Contracts that move funds should verify output addresses',
    severity: 'warning',
    check(tokens) {
        const hasOutputOp = tokens.some(t => ['VERIFYOUT', 'GETOUTADDR', 'GETOUTAMT'].includes(t.toUpperCase()));
        const hasSignedBy = tokens.some(t => ['SIGNEDBY', 'CHECKSIG', 'MULTISIG'].includes(t.toUpperCase()));
        if (!hasOutputOp && !hasSignedBy) {
            return [{
                    rule: 'address-check',
                    severity: 'warning',
                    message: 'No output verification or signature check found. Funds may be accessible to anyone.',
                    suggestion: 'Add SIGNEDBY or VERIFYOUT to restrict access.',
                }];
        }
        return [];
    },
};
// ── Rule: Deprecated RSA usage ────────────────────────────────────────────────
exports.ruleNoRSA = {
    id: 'no-rsa',
    description: 'RSA is deprecated in Minima — use CHECKSIG with WOTS keys',
    severity: 'warning',
    check(_tokens, script) {
        if (/\bRSA\b/i.test(script)) {
            return [{
                    rule: 'no-rsa',
                    severity: 'warning',
                    message: 'RSA usage detected. Minima uses Winternitz OTS (WOTS) — prefer CHECKSIG.',
                    suggestion: 'Replace RSA verification with CHECKSIG and WOTS public keys.',
                }];
        }
        return [];
    },
};
// ── Rule: Magic variables without null check ──────────────────────────────────
exports.ruleMagicVars = {
    id: 'magic-vars',
    description: '@-variables should be used carefully',
    severity: 'info',
    check(tokens) {
        const magicVars = tokens.filter(t => t.startsWith('@'));
        const issues = [];
        for (const v of magicVars) {
            if (v.toUpperCase() === '@BLOCK' || v.toUpperCase() === '@COINAGE') {
                // OK — commonly used
            }
        }
        return issues;
    },
};
// ── Rule: Unreachable code after RETURN ───────────────────────────────────────
exports.ruleNoDeadCode = {
    id: 'no-dead-code',
    description: 'Code after RETURN at top level is unreachable',
    severity: 'warning',
    check(tokens) {
        let depth = 0;
        for (let i = 0; i < tokens.length; i++) {
            const t = tokens[i].toUpperCase();
            if (t === 'IF' || t === 'WHILE')
                depth++;
            if (t === 'ENDIF' || t === 'ENDWHILE')
                depth--;
            if (t === 'RETURN' && depth === 0 && i < tokens.length - 1) {
                return [{
                        rule: 'no-dead-code',
                        severity: 'warning',
                        message: `RETURN at token ${i} — ${tokens.length - i - 1} token(s) after it are unreachable.`,
                        suggestion: 'Remove unreachable code after top-level RETURN.',
                    }];
            }
        }
        return [];
    },
};
exports.ALL_RULES = [
    exports.ruleInstructionLimit,
    exports.ruleReturnPresent,
    exports.ruleBalancedIf,
    exports.ruleBalancedWhile,
    exports.ruleAddressCheck,
    exports.ruleNoRSA,
    exports.ruleMagicVars,
    exports.ruleNoDeadCode,
];
//# sourceMappingURL=rules.js.map