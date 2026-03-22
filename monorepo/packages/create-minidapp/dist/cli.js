#!/usr/bin/env node
"use strict";
/**
 * create-minidapp CLI
 *
 * Usage:
 *   npx create-minidapp my-app
 *   npx create-minidapp my-app --description "My awesome app"
 *   npx create-minidapp my-app --with-contract --with-maxima
 */
Object.defineProperty(exports, "__esModule", { value: true });
const scaffold_js_1 = require("./scaffold.js");
const node_path_1 = require("node:path");
const args = process.argv.slice(2);
if (args.includes('--version') || args.includes('-v')) {
    console.log('0.1.0');
    process.exit(0);
}
if (args.includes('--help') || args.includes('-h') || args.length === 0) {
    console.log(`
  create-minidapp — Scaffold a new Minima MiniDapp

  Usage:
    npx create-minidapp <name> [options]

  Options:
    --description <text>    App description
    --author <name>         Author name
    --category <cat>        Business|Utility|DeFi|NFT|Social (default: Utility)
    --with-contract         Include KISS VM contract scaffold
    --with-maxima           Include Maxima messaging service
    --version               Show version
    --help                  Show this help

  Example:
    npx create-minidapp my-wallet --description "My Minima wallet" --with-contract
`);
    process.exit(0);
}
// Parse args
const name = args[0];
if (!name || name.startsWith('--')) {
    console.error('Error: Please provide a project name\n  create-minidapp <name>');
    process.exit(1);
}
const get = (flag) => {
    const idx = args.indexOf(flag);
    return idx !== -1 && args[idx + 1] && !args[idx + 1].startsWith('--')
        ? args[idx + 1]
        : undefined;
};
const opts = {
    name,
    description: get('--description'),
    author: get('--author'),
    category: get('--category') ?? 'Utility',
    withContract: args.includes('--with-contract'),
    withMaxima: args.includes('--with-maxima'),
};
const targetDir = (0, node_path_1.join)(process.cwd(), name.toLowerCase().replace(/\s+/g, '-'));
console.log(`\n  Creating MiniDapp: ${name}`);
console.log(`  Directory: ${targetDir}\n`);
try {
    const files = (0, scaffold_js_1.scaffold)(targetDir, opts);
    console.log(`  \x1b[32m✓\x1b[0m Created ${files.length} files:\n`);
    for (const f of files)
        console.log(`    \x1b[2m${f}\x1b[0m`);
    console.log(`
  \x1b[1mNext steps:\x1b[0m
  
    cd ${name.toLowerCase().replace(/\s+/g, '-')}
    # Open index.html in browser to develop offline
    # Or install as MiniDapp on your Minima node

  \x1b[2mDocs: https://docs.minima.global/buildondeminima/minidapps\x1b[0m
`);
}
catch (err) {
    const msg = err instanceof Error ? err.message : String(err);
    console.error(`\n  \x1b[31mError: ${msg}\x1b[0m\n`);
    process.exit(1);
}
//# sourceMappingURL=cli.js.map