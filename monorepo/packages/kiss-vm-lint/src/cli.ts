#!/usr/bin/env node
/**
 * kiss-vm-lint CLI
 * 
 * Usage:
 *   kiss-vm-lint <file.kiss>
 *   echo "RETURN TRUE" | kiss-vm-lint --stdin
 *   kiss-vm-lint --version
 */

import { readFileSync } from 'node:fs';
import { KissVMLinter, formatResult } from './linter.js';

const args = process.argv.slice(2);

if (args.includes('--version') || args.includes('-v')) {
  console.log('0.1.0');
  process.exit(0);
}

if (args.includes('--stdin')) {
  let script = '';
  process.stdin.setEncoding('utf8');
  process.stdin.on('data', (chunk: string) => { script += chunk; });
  process.stdin.on('end', () => {
    const linter = new KissVMLinter();
    const result = linter.lint(script);
    console.log(formatResult(result, '<stdin>'));
    process.exit(result.errors > 0 ? 1 : 0);
  });
} else if (args.length > 0) {
  const linter = new KissVMLinter();
  let hasErrors = false;
  for (const file of args) {
    try {
      const script = readFileSync(file, 'utf8');
      const result = linter.lint(script);
      console.log(formatResult(result, file));
      if (result.errors > 0) hasErrors = true;
    } catch (err: unknown) {
      const msg = err instanceof Error ? err.message : String(err);
      console.error(`Cannot read ${file}: ${msg}`);
      hasErrors = true;
    }
  }
  process.exit(hasErrors ? 1 : 0);
} else {
  console.error('Usage: kiss-vm-lint <file.kiss> [...]');
  console.error('       echo "RETURN TRUE" | kiss-vm-lint --stdin');
  process.exit(1);
}
