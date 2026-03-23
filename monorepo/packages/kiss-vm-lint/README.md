# kiss-vm-lint

Static analyzer and linter for [Minima](https://minima.global) KISS VM smart contracts.

## Install

```bash
npm install -g kiss-vm-lint
```

## Usage

### CLI

```bash
# Lint a file
kiss-vm-lint contract.kiss

# Lint from stdin
echo "RETURN SIGNEDBY(0xABCD)" | kiss-vm-lint --stdin

# Multiple files
kiss-vm-lint *.kiss
```

### API

```js
import { lint, formatResult } from 'kiss-vm-lint';

const result = lint(`
  LET sig = SIGNEDBY(0xABC123)
  RETURN sig
`);

console.log(formatResult(result, 'mycontract.kiss'));
console.log(result.clean);  // true/false
console.log(result.errors); // number of errors
```

## Rules

| Rule | Severity | Description |
|------|----------|-------------|
| `instruction-limit` | error/warning | Detects scripts approaching/exceeding 1024 instruction limit |
| `return-present` | warning | Script must have a RETURN statement |
| `balanced-if` | error | Every IF must have a matching ENDIF |
| `balanced-while` | error | Every WHILE must have a matching ENDWHILE |
| `address-check` | warning | Contracts should verify output addresses or require signatures |
| `no-rsa` | warning | RSA is deprecated — use CHECKSIG with WOTS keys |
| `no-dead-code` | warning | Multiple top-level RETURN statements |
| `no-shadow-globals` | warning | LET should not shadow @-variables |
| `assert-usage` | info | Many ASSERT statements detected |

## Custom Rules

```js
import { KissVMLinter, ALL_RULES } from 'kiss-vm-lint';

const myRule = {
  id: 'my-rule',
  description: 'Custom rule',
  severity: 'warning',
  check(tokens, script) {
    // return array of LintIssue objects
    return [];
  }
};

const linter = new KissVMLinter([...ALL_RULES, myRule]);
const result = linter.lint(script);
```

## Exit Codes

- `0` — no errors (warnings are OK)
- `1` — errors found, or file not readable

## License

MIT
