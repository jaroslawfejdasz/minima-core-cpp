import { LintError } from './tokenizer';
import { Rule } from './rules';
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
export declare function lint(script: string, options?: LintOptions): LintResult;
export declare function lintOrThrow(script: string, options?: LintOptions): LintResult;
export { ALL_RULES } from './rules';
export * as rules from './rules';
//# sourceMappingURL=index.d.ts.map