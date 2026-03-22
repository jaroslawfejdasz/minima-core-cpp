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
export declare const ruleInstructionLimit: LintRule;
export declare const ruleReturnPresent: LintRule;
export declare const ruleBalancedIf: LintRule;
export declare const ruleBalancedWhile: LintRule;
export declare const ruleAddressCheck: LintRule;
export declare const ruleNoRSA: LintRule;
export declare const ruleMagicVars: LintRule;
export declare const ruleNoDeadCode: LintRule;
export declare const ALL_RULES: LintRule[];
//# sourceMappingURL=rules.d.ts.map