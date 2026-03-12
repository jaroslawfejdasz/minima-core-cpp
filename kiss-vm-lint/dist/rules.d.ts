import { Token, LintError } from './tokenizer';
export type Rule = (tokens: Token[], script: string) => LintError[];
export declare const R001_NoReturn: Rule;
export declare const R002_UnreachableCode: Rule;
export declare const R003_UseBeforeLet: Rule;
export declare const R004_InstructionLimit: Rule;
export declare const R005_UnboundedWhile: Rule;
export declare const R006_ChecksigWarning: Rule;
export declare const R007_MultisigArgs: Rule;
export declare const R008_AssertTrue: Rule;
export declare const R009_SignedByHex: Rule;
export declare const R010_StatePortRange: Rule;
export declare const ALL_RULES: Rule[];
//# sourceMappingURL=rules.d.ts.map