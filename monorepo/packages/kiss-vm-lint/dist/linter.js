"use strict";
/**
 * KISS VM Linter — static analysis engine
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.KissVMLinter = void 0;
exports.lint = lint;
exports.formatResult = formatResult;
const rules_js_1 = require("./rules.js");
class KissVMLinter {
    constructor(rules = rules_js_1.ALL_RULES) {
        this.rules = rules;
    }
    lint(script) {
        const tokens = this.tokenize(script);
        const issues = [];
        for (const rule of this.rules) {
            issues.push(...rule.check(tokens, script));
        }
        const errors = issues.filter(i => i.severity === 'error').length;
        const warnings = issues.filter(i => i.severity === 'warning').length;
        const infos = issues.filter(i => i.severity === 'info').length;
        return {
            script,
            issues,
            errors,
            warnings,
            infos,
            clean: errors === 0 && warnings === 0,
        };
    }
    tokenize(script) {
        return script
            .replace(/\/\*[\s\S]*?\*\//g, ' ') // strip comments
            .split(/\s+/)
            .filter(t => t.length > 0);
    }
}
exports.KissVMLinter = KissVMLinter;
function lint(script, rules) {
    return new KissVMLinter(rules).lint(script);
}
// ── Formatter ─────────────────────────────────────────────────────────────────
const SEVERITY_COLOR = {
    error: '\x1b[31m',
    warning: '\x1b[33m',
    info: '\x1b[36m',
};
const RESET = '\x1b[0m';
function formatResult(result, filename = '<script>') {
    if (result.clean) {
        return `\x1b[32m✓ ${filename} — no issues\x1b[0m`;
    }
    const lines = [`\x1b[1m${filename}\x1b[0m`];
    for (const issue of result.issues) {
        const c = SEVERITY_COLOR[issue.severity];
        const loc = issue.line ? `:${issue.line}` : '';
        lines.push(`  ${c}${issue.severity.toUpperCase()}${RESET} [${issue.rule}] ${issue.message}`);
        if (issue.suggestion) {
            lines.push(`    \x1b[2m→ ${issue.suggestion}\x1b[0m`);
        }
    }
    const summary = [];
    if (result.errors > 0)
        summary.push(`\x1b[31m${result.errors} error(s)\x1b[0m`);
    if (result.warnings > 0)
        summary.push(`\x1b[33m${result.warnings} warning(s)\x1b[0m`);
    if (result.infos > 0)
        summary.push(`\x1b[36m${result.infos} info(s)\x1b[0m`);
    lines.push(`\n  ${summary.join(', ')}`);
    return lines.join('\n');
}
//# sourceMappingURL=linter.js.map