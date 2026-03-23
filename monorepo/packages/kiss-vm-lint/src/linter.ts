/**
 * KISS VM Linter — static analysis engine
 */

import { LintIssue, LintRule, ALL_RULES, Severity } from './rules.js';

export interface LintResult {
  script: string;
  issues: LintIssue[];
  errors: number;
  warnings: number;
  infos: number;
  clean: boolean;
}

export class KissVMLinter {
  private rules: LintRule[];

  constructor(rules: LintRule[] = ALL_RULES) {
    this.rules = rules;
  }

  lint(script: string): LintResult {
    const tokens = this.tokenize(script);
    const issues: LintIssue[] = [];

    for (const rule of this.rules) {
      issues.push(...rule.check(tokens, script));
    }

    const errors   = issues.filter(i => i.severity === 'error').length;
    const warnings = issues.filter(i => i.severity === 'warning').length;
    const infos    = issues.filter(i => i.severity === 'info').length;

    return {
      script,
      issues,
      errors,
      warnings,
      infos,
      clean: errors === 0 && warnings === 0,
    };
  }

  private tokenize(script: string): string[] {
    // Strip /* ... */ comments
    const stripped = script.replace(/\/\*[\s\S]*?\*\//g, ' ');
    // Insert spaces around parens and commas so SIGNEDBY(x) → SIGNEDBY ( x )
    const spaced = stripped
      .replace(/([()\[\],])/g, ' $1 ');
    return spaced
      .split(/\s+/)
      .filter(t => t.length > 0);
  }
}

export function lint(script: string, rules?: LintRule[]): LintResult {
  return new KissVMLinter(rules).lint(script);
}

// ── Formatter ─────────────────────────────────────────────────────────────────

const SEVERITY_COLOR: Record<Severity, string> = {
  error:   '\x1b[31m',
  warning: '\x1b[33m',
  info:    '\x1b[36m',
};
const RESET = '\x1b[0m';

export function formatResult(result: LintResult, filename = '<script>'): string {
  if (result.clean) {
    return `\x1b[32m✓ ${filename} — no issues\x1b[0m`;
  }

  const lines: string[] = [`\x1b[1m${filename}\x1b[0m`];
  for (const issue of result.issues) {
    const c = SEVERITY_COLOR[issue.severity];
    const loc = issue.line ? `:${issue.line}` : '';
    lines.push(`  ${c}${issue.severity.toUpperCase()}${RESET} [${issue.rule}] ${issue.message}`);
    if (issue.suggestion) {
      lines.push(`    \x1b[2m→ ${issue.suggestion}\x1b[0m`);
    }
  }
  const summary: string[] = [];
  if (result.errors   > 0) summary.push(`\x1b[31m${result.errors} error(s)\x1b[0m`);
  if (result.warnings > 0) summary.push(`\x1b[33m${result.warnings} warning(s)\x1b[0m`);
  if (result.infos    > 0) summary.push(`\x1b[36m${result.infos} info(s)\x1b[0m`);
  lines.push(`\n  ${summary.join(', ')}`);
  return lines.join('\n');
}
