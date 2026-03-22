#!/usr/bin/env node
"use strict";
/**
 * kiss-vm-lint CLI
 *
 * Usage:
 *   kiss-vm-lint <file.kiss>
 *   echo "RETURN TRUE" | kiss-vm-lint --stdin
 *   kiss-vm-lint --version
 */
Object.defineProperty(exports, "__esModule", { value: true });
const node_fs_1 = require("node:fs");
const linter_js_1 = require("./linter.js");
const args = process.argv.slice(2);
if (args.includes('--version') || args.includes('-v')) {
    console.log('0.1.0');
    process.exit(0);
}
if (args.includes('--stdin')) {
    let script = '';
    process.stdin.setEncoding('utf8');
    process.stdin.on('data', (chunk) => { script += chunk; });
    process.stdin.on('end', () => {
        const linter = new linter_js_1.KissVMLinter();
        const result = linter.lint(script);
        console.log((0, linter_js_1.formatResult)(result, '<stdin>'));
        process.exit(result.errors > 0 ? 1 : 0);
    });
}
else if (args.length > 0) {
    const linter = new linter_js_1.KissVMLinter();
    let hasErrors = false;
    for (const file of args) {
        try {
            const script = (0, node_fs_1.readFileSync)(file, 'utf8');
            const result = linter.lint(script);
            console.log((0, linter_js_1.formatResult)(result, file));
            if (result.errors > 0)
                hasErrors = true;
        }
        catch (err) {
            const msg = err instanceof Error ? err.message : String(err);
            console.error(`Cannot read ${file}: ${msg}`);
            hasErrors = true;
        }
    }
    process.exit(hasErrors ? 1 : 0);
}
else {
    console.error('Usage: kiss-vm-lint <file.kiss> [...]');
    console.error('       echo "RETURN TRUE" | kiss-vm-lint --stdin');
    process.exit(1);
}
//# sourceMappingURL=cli.js.map