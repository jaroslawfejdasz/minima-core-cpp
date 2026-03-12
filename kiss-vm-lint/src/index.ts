import { tokenize, LintError } from './tokenizer';
import { ALL_RULES, Rule } from './rules';

export { LintError } from './tokenizer';

export interface LintResult {
  errors: LintError[];
  warnings: LintError[];
  infos: LintError[];
  all: LintError[];
  hasErrors: boolean;
  script: string;
}

export interface LintOptions {
  rules?: Rule[];
  ignoreRules?: string[];
}

export function lint(script: string, options: LintOptions = {}): LintResult {
  const rules = options.rules ?? ALL_RULES;
  const ignore = new Set(options.ignoreRules ?? []);

  const { tokens, errors: tokenErrors } = tokenize(script);

  const all: LintError[] = [...tokenErrors];

  for (const rule of rules) {
    const ruleErrors = rule(tokens, script);
    all.push(...ruleErrors);
  }

  const filtered = all.filter(e => !ignore.has(e.code));

  return {
    errors:   filtered.filter(e => e.severity === 'error'),
    warnings: filtered.filter(e => e.severity === 'warning'),
    infos:    filtered.filter(e => e.severity === 'info'),
    all:      filtered,
    hasErrors: filtered.some(e => e.severity === 'error'),
    script
  };
}

export function lintOrThrow(script: string, options?: LintOptions): LintResult {
  const result = lint(script, options);
  if (result.hasErrors) {
    const msgs = result.errors.map(e => `[${e.code}] ${e.message}`).join('\n');
    throw new Error(`KISS VM lint errors:\n${msgs}`);
  }
  return result;
}

export { ALL_RULES } from './rules';
export * as rules from './rules';
