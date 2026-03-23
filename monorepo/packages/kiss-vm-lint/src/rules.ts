/**
 * KISS VM lint rules
 */

export type Severity = 'error' | 'warning' | 'info';

export interface LintIssue {
  rule: string;
  severity: Severity;
  message: string;
  line?: number;
  col?: number;
  suggestion?: string;
}

export interface LintRule {
  id: string;
  description: string;
  severity: Severity;
  check(tokens: string[], script: string): LintIssue[];
}

// ── Rule: instruction count ───────────────────────────────────────────────────
export const ruleInstructionLimit: LintRule = {
  id: 'instruction-limit',
  description: 'KISS VM has a hard limit of 1024 instructions',
  severity: 'error',
  check(tokens) {
    const issues: LintIssue[] = [];
    if (tokens.length > 1024) {
      issues.push({
        rule: 'instruction-limit',
        severity: 'error',
        message: `Script has ${tokens.length} tokens (max 1024). May exceed instruction limit at runtime.`,
        suggestion: 'Split logic into sub-scripts using MAST, or reduce complexity.',
      });
    } else if (tokens.length > 800) {
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
export const ruleReturnPresent: LintRule = {
  id: 'return-present',
  description: 'Script should have a RETURN statement',
  severity: 'warning',
  check(tokens) {
    const has = tokens.some(t => t.toUpperCase() === 'RETURN');
    if (!has) {
      return [{
        rule: 'return-present',
        severity: 'warning',
        message: 'Script has no RETURN statement — script will not produce a result.',
        suggestion: 'Add RETURN TRUE or RETURN FALSE to explicitly return a value.',
      }];
    }
    return [];
  },
};

// ── Rule: unbalanced IF/ENDIF ─────────────────────────────────────────────────
export const ruleBalancedIf: LintRule = {
  id: 'balanced-if',
  description: 'Every IF must have a matching ENDIF',
  severity: 'error',
  check(tokens) {
    let depth = 0;
    for (const t of tokens) {
      const up = t.toUpperCase();
      if (up === 'IF') depth++;
      else if (up === 'ENDIF') depth--;
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
export const ruleBalancedWhile: LintRule = {
  id: 'balanced-while',
  description: 'Every WHILE must have ENDWHILE',
  severity: 'error',
  check(tokens) {
    let depth = 0;
    for (const t of tokens) {
      const up = t.toUpperCase();
      if (up === 'WHILE') depth++;
      else if (up === 'ENDWHILE') depth--;
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
        suggestion: 'Add ENDWHILE for each WHILE loop.',
      }];
    }
    return [];
  },
};

// ── Rule: No address verification (only for non-trivial scripts) ──────────────
export const ruleAddressCheck: LintRule = {
  id: 'address-check',
  description: 'Contracts that move funds should verify output addresses',
  severity: 'warning',
  check(tokens) {
    // Skip trivial scripts (< 3 tokens) — they're likely test/template scripts
    if (tokens.length < 3) return [];

    const up = tokens.map(t => t.toUpperCase());
    const hasOutputOp = up.some(t =>
      ['VERIFYOUT', 'GETOUTADDR', 'GETOUTAMT', 'SUMINPUTS', 'SUMOUTPUTS'].includes(t)
    );
    const hasSignedBy = up.some(t =>
      ['SIGNEDBY', 'CHECKSIG', 'MULTISIG'].includes(t)
    );
    const hasStateCheck = up.some(t => ['STATE', 'PREVSTATE', 'SAMESTATE'].includes(t));
    const hasTimeLock = up.some(t =>
      ['@BLOCK', '@COINAGE', '@BLOCKMILLI'].includes(t)
    );
    // If script has only a time lock without output check — warn
    if (!hasOutputOp && !hasSignedBy && !hasStateCheck && !hasTimeLock) {
      return [{
        rule: 'address-check',
        severity: 'warning',
        message: 'No output verification or signature check found. Funds may be accessible to anyone.',
        suggestion: 'Add SIGNEDBY, CHECKSIG, MULTISIG, or VERIFYOUT to restrict access.',
      }];
    }
    return [];
  },
};

// ── Rule: Deprecated RSA usage ────────────────────────────────────────────────
export const ruleNoRSA: LintRule = {
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

// ── Rule: Multiple top-level RETURNs (dead code indicator) ───────────────────
// Detecting unreachable code precisely requires a full parser.
// We conservatively flag only the clear case: multiple RETURN at top-level depth.
export const ruleNoDeadCode: LintRule = {
  id: 'no-dead-code',
  description: 'Multiple top-level RETURN statements — earlier ones are unreachable',
  severity: 'warning',
  check(tokens) {
    let depth = 0;
    let topLevelReturns = 0;
    for (const t of tokens) {
      const up = t.toUpperCase();
      if (up === 'IF' || up === 'WHILE') depth++;
      if (up === 'ENDIF' || up === 'ENDWHILE') depth--;
      if (up === 'RETURN' && depth === 0) {
        topLevelReturns++;
        if (topLevelReturns > 1) {
          return [{
            rule: 'no-dead-code',
            severity: 'warning',
            message: `Multiple top-level RETURN statements found (${topLevelReturns}). Earlier ones may be unreachable.`,
            suggestion: 'Use IF/ELSE/ENDIF to conditionally return, or remove redundant RETURN.',
          }];
        }
      }
    }
    return [];
  },
};

// ── Rule: LET variable shadowing global ──────────────────────────────────────
export const ruleNoShadowGlobals: LintRule = {
  id: 'no-shadow-globals',
  description: 'LET variables should not shadow KISS VM global @-variables',
  severity: 'warning',
  check(tokens) {
    const issues: LintIssue[] = [];
    for (let i = 0; i < tokens.length - 1; i++) {
      if (tokens[i].toUpperCase() === 'LET') {
        const varName = tokens[i + 1];
        if (varName && varName.startsWith('@')) {
          issues.push({
            rule: 'no-shadow-globals',
            severity: 'warning',
            message: `LET ${varName} — cannot reassign KISS VM global variable.`,
            suggestion: 'Use a different variable name that does not start with @.',
          });
        }
      }
    }
    return issues;
  },
};

// ── Rule: ASSERT without explanation ──────────────────────────────────────────
export const ruleAssertInfo: LintRule = {
  id: 'assert-usage',
  description: 'ASSERT immediately fails the script — use it intentionally',
  severity: 'info',
  check(tokens) {
    const assertCount = tokens.filter(t => t.toUpperCase() === 'ASSERT').length;
    if (assertCount > 3) {
      return [{
        rule: 'assert-usage',
        severity: 'info',
        message: `Script contains ${assertCount} ASSERT statements. Consider consolidating checks.`,
      }];
    }
    return [];
  },
};

export const ALL_RULES: LintRule[] = [
  ruleInstructionLimit,
  ruleReturnPresent,
  ruleBalancedIf,
  ruleBalancedWhile,
  ruleAddressCheck,
  ruleNoRSA,
  ruleNoDeadCode,
  ruleNoShadowGlobals,
  ruleAssertInfo,
];
