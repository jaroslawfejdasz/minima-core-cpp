/**
 * KISS VM Linter — static analysis engine
 */
import { LintIssue, LintRule } from './rules.js';
export interface LintResult {
    script: string;
    issues: LintIssue[];
    errors: number;
    warnings: number;
    infos: number;
    clean: boolean;
}
export declare class KissVMLinter {
    private rules;
    constructor(rules?: LintRule[]);
    lint(script: string): LintResult;
    private tokenize;
}
export declare function lint(script: string, rules?: LintRule[]): LintResult;
export declare function formatResult(result: LintResult, filename?: string): string;
//# sourceMappingURL=linter.d.ts.map