import { Token, LintError, FUNCTION_KEYWORDS, STATEMENT_KEYWORDS } from './tokenizer';

// All lint rules — each takes tokens and returns LintError[]
export type Rule = (tokens: Token[], script: string) => LintError[];

// ─────────────────────────────────────────────
// R001: No RETURN statement
// ─────────────────────────────────────────────
export const R001_NoReturn: Rule = (tokens) => {
  const hasReturn = tokens.some(t => t.type === 'KEYWORD' && t.value === 'RETURN');
  if (!hasReturn) {
    return [{ severity: 'error', code: 'R001', message: 'Script has no RETURN statement — will throw at runtime' }];
  }
  return [];
};

// ─────────────────────────────────────────────
// R002: Unreachable code after RETURN
// ─────────────────────────────────────────────
export const R002_UnreachableCode: Rule = (tokens) => {
  const errors: LintError[] = [];
  // Find RETURN at top level (depth 0) — any STATEMENT keyword after at same depth is unreachable
  let depth = 0;
  let foundTopLevelReturn = false;
  for (let i = 0; i < tokens.length; i++) {
    const t = tokens[i];
    if (t.type === 'KEYWORD') {
      if (t.value === 'IF' || t.value === 'WHILE') depth++;
      if (t.value === 'ENDIF' || t.value === 'ENDWHILE') depth--;
      if (t.value === 'RETURN' && depth === 0) {
        foundTopLevelReturn = true;
        continue;
      }
    }
    // Only flag statement-level keywords as unreachable (not expressions/values)
    if (foundTopLevelReturn && depth === 0 && t.type === 'KEYWORD' && STATEMENT_KEYWORDS.has(t.value)) {
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

// ─────────────────────────────────────────────
// R003: Variable used before LET
// ─────────────────────────────────────────────
export const R003_UseBeforeLet: Rule = (tokens) => {
  const errors: LintError[] = [];
  const defined = new Set<string>();
  // Built-in globals don't need LET
  const builtinGlobals = new Set(['@BLOCK','@BLOCKDIFF','@BLOCKMILLI','@COINAGE',
    '@AMOUNT','@ADDRESS','@TOKENID','@SCRIPT','@TOTIN','@TOTOUT','@INCOUNT','@OUTCOUNT','@INPUT']);

  for (let i = 0; i < tokens.length; i++) {
    const t = tokens[i];

    // LET x = ... defines x
    if (t.type === 'KEYWORD' && t.value === 'LET') {
      const next = tokens[i+1];
      if (next && next.type === 'VARIABLE') {
        defined.add(next.value);
        i++; // skip the variable name
        continue;
      }
    }

    // VARIABLE used — check if defined
    if (t.type === 'VARIABLE' && !builtinGlobals.has(t.value)) {
      // Skip if it's followed by = (LET target) or it's a function call parameter being defined
      const prev = tokens[i-1];
      if (prev && prev.type === 'KEYWORD' && prev.value === 'LET') continue; // already handled

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

// ─────────────────────────────────────────────
// R004: Instruction count estimation
// ─────────────────────────────────────────────
export const R004_InstructionLimit: Rule = (tokens) => {
  const errors: LintError[] = [];
  // Count statement keywords as proxy for instructions
  const statementCount = tokens.filter(t =>
    t.type === 'KEYWORD' && STATEMENT_KEYWORDS.has(t.value)
  ).length;

  if (statementCount > 900) {
    errors.push({
      severity: 'error', code: 'R004',
      message: `Estimated ~${statementCount} instructions — dangerously close to or over the 1024 limit`
    });
  } else if (statementCount > 700) {
    errors.push({
      severity: 'warning', code: 'R004',
      message: `Estimated ~${statementCount} instructions — approaching the 1024 limit. Consider simplifying.`
    });
  }
  return errors;
};

// ─────────────────────────────────────────────
// R005: WHILE without upper bound (infinite loop risk)
// ─────────────────────────────────────────────
export const R005_UnboundedWhile: Rule = (tokens) => {
  const errors: LintError[] = [];
  for (let i = 0; i < tokens.length; i++) {
    const t = tokens[i];
    if (t.type === 'KEYWORD' && t.value === 'WHILE') {
      // Heuristic: check if condition contains a comparison with a literal number
      // Scan until DO
      let hasBound = false;
      let j = i + 1;
      while (j < tokens.length && !(tokens[j].type === 'KEYWORD' && tokens[j].value === 'DO')) {
        const tk = tokens[j];
        if (tk.type === 'NUMBER' || tk.type === 'GLOBAL') hasBound = true;
        if (tk.type === 'KEYWORD' && (tk.value === 'LT' || tk.value === 'GT' || tk.value === 'LTE' || tk.value === 'GTE')) hasBound = true;
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

// ─────────────────────────────────────────────
// R006: CHECKSIG always returns FALSE (mock warning)
// ─────────────────────────────────────────────
export const R006_ChecksigWarning: Rule = (tokens) => {
  const errors: LintError[] = [];
  const hasCHECKSIG = tokens.some(t => t.type === 'KEYWORD' && t.value === 'CHECKSIG');
  if (hasCHECKSIG) {
    errors.push({
      severity: 'info', code: 'R006',
      message: 'CHECKSIG performs raw EC signature verification — ensure sig/data/pubkey are in correct format (Minima uses secp256k1)'
    });
  }
  return errors;
};

// ─────────────────────────────────────────────
// R007: MULTISIG argument count check
// ─────────────────────────────────────────────
export const R007_MultisigArgs: Rule = (tokens) => {
  const errors: LintError[] = [];
  for (let i = 0; i < tokens.length; i++) {
    const t = tokens[i];
    if (t.type === 'KEYWORD' && t.value === 'MULTISIG') {
      // MULTISIG(n, key1, key2...) - count args
      if (tokens[i+1]?.type !== 'LPAREN') continue;
      let j = i + 2;
      let depth = 1;
      let argCount = 1;
      while (j < tokens.length && depth > 0) {
        if (tokens[j].type === 'LPAREN') depth++;
        else if (tokens[j].type === 'RPAREN') { depth--; if (depth === 0) break; }
        else if (tokens[j].type === 'COMMA' && depth === 1) argCount++;
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

// ─────────────────────────────────────────────
// R008: ASSERT without meaningful condition
// ─────────────────────────────────────────────
export const R008_AssertTrue: Rule = (tokens) => {
  const errors: LintError[] = [];
  for (let i = 0; i < tokens.length; i++) {
    if (tokens[i].type === 'KEYWORD' && tokens[i].value === 'ASSERT') {
      const next = tokens[i+1];
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

// ─────────────────────────────────────────────
// R009: SIGNEDBY with hardcoded public key check
// ─────────────────────────────────────────────
export const R009_SignedByHex: Rule = (tokens) => {
  const errors: LintError[] = [];
  for (let i = 0; i < tokens.length; i++) {
    if (tokens[i].type === 'KEYWORD' && tokens[i].value === 'SIGNEDBY') {
      if (tokens[i+1]?.type === 'LPAREN') {
        const arg = tokens[i+2];
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

// ─────────────────────────────────────────────
// R010: STATE port out of range (0-255)
// ─────────────────────────────────────────────
export const R010_StatePortRange: Rule = (tokens) => {
  const errors: LintError[] = [];
  for (let i = 0; i < tokens.length; i++) {
    const t = tokens[i];
    if (t.type === 'KEYWORD' && (t.value === 'STATE' || t.value === 'PREVSTATE')) {
      if (tokens[i+1]?.type === 'LPAREN') {
        const arg = tokens[i+2];
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

export const ALL_RULES: Rule[] = [
  R001_NoReturn,
  R002_UnreachableCode,
  R003_UseBeforeLet,
  R004_InstructionLimit,
  R005_UnboundedWhile,
  R006_ChecksigWarning,
  R007_MultisigArgs,
  R008_AssertTrue,
  R009_SignedByHex,
  R010_StatePortRange,
];
