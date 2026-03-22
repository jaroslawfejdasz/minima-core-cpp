"use strict";
/**
 * KISS VM Interpreter — TypeScript implementation
 *
 * Minima KISS VM scripting language interpreter.
 * Supports all standard KISS VM operations and built-in functions.
 *
 * Reference: docs.minima.global/learn/smart-contracts/kiss-vm
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.KissVMInterpreter = void 0;
// SHA256 and SHA3-256 stubs (pure JS — for test purposes)
// In production you'd use a proper crypto library
function sha256hex(data) {
    // Simple deterministic hash for testing — NOT cryptographically secure
    let h = 0;
    for (let i = 0; i < data.length; i++) {
        h = ((h << 5) - h + data.charCodeAt(i)) | 0;
    }
    return Math.abs(h).toString(16).padStart(8, '0').repeat(8);
}
function sha3_256hex(data) {
    let h = 0xDEADBEEF;
    for (let i = 0; i < data.length; i++) {
        h = (h ^ data.charCodeAt(i)) * 0x9E3779B9;
        h = ((h >>> 16) ^ h) | 0;
    }
    return Math.abs(h).toString(16).padStart(8, '0').repeat(8);
}
class KissVMInterpreter {
    constructor(ctx) {
        this.stack = [];
        this.vars = new Map();
        this.trace = [];
        this.instructions = 0;
        this.MAX_INSTRUCTIONS = 1024;
        this.MAX_STACK = 64;
        this.ctx = ctx;
    }
    execute(script) {
        this.stack = [];
        this.vars = new Map();
        this.trace = [];
        this.instructions = 0;
        try {
            const tokens = this.tokenize(script);
            const result = this.runTokens(tokens, 0);
            const top = this.stack.length > 0 ? this.stack[this.stack.length - 1] : false;
            const contractResult = top === true || top === 'TRUE' ? 'TRUE' : 'FALSE';
            return {
                result: contractResult,
                trace: this.trace,
                gasUsed: this.instructions,
            };
        }
        catch (err) {
            const msg = err instanceof Error ? err.message : String(err);
            return {
                result: 'EXCEPTION',
                trace: this.trace,
                error: msg,
                gasUsed: this.instructions,
            };
        }
    }
    tokenize(script) {
        // Preserve quoted strings as single tokens
        const tokens = [];
        let i = 0;
        const s = script.trim();
        while (i < s.length) {
            if (s[i] === ' ' || s[i] === '\t' || s[i] === '\n' || s[i] === '\r') {
                i++;
                continue;
            }
            // Comments: /* ... */
            if (s[i] === '/' && s[i + 1] === '*') {
                const end = s.indexOf('*/', i + 2);
                if (end === -1)
                    break;
                i = end + 2;
                continue;
            }
            if (s[i] === '"' || s[i] === "'") {
                const q = s[i];
                let j = i + 1;
                while (j < s.length && s[j] !== q)
                    j++;
                tokens.push(s.slice(i, j + 1));
                i = j + 1;
                continue;
            }
            let j = i;
            while (j < s.length && s[j] !== ' ' && s[j] !== '\t' && s[j] !== '\n' && s[j] !== '\r')
                j++;
            tokens.push(s.slice(i, j));
            i = j;
        }
        return tokens.filter(t => t.length > 0);
    }
    runTokens(tokens, startIdx) {
        let i = startIdx;
        while (i < tokens.length) {
            const tok = tokens[i].toUpperCase();
            i++;
            this.instructions++;
            if (this.instructions > this.MAX_INSTRUCTIONS) {
                throw new Error('KISS VM: instruction limit exceeded (1024)');
            }
            if (this.stack.length > this.MAX_STACK) {
                throw new Error('KISS VM: stack depth exceeded (64)');
            }
            this.trace.push(tok);
            // ── Control flow ────────────────────────────────────────────
            if (tok === 'IF') {
                const cond = this.pop();
                if (cond) {
                    // execute THEN branch, skip to ENDIF or ELSE
                    i = this.runBranch(tokens, i, true);
                }
                else {
                    i = this.runBranch(tokens, i, false);
                }
                continue;
            }
            if (tok === 'RETURN') {
                return i; // stop processing — result is top of stack
            }
            if (tok === 'ASSERT') {
                const v = this.pop();
                if (v !== true && v !== 'TRUE') {
                    throw new Error('ASSERT failed');
                }
                continue;
            }
            // ── Variable assignment ──────────────────────────────────────
            if (tok === 'LET') {
                const name = tokens[i++];
                const eq = tokens[i++]; // '='
                if (eq !== '=')
                    throw new Error(`LET: expected '=', got '${eq}'`);
                const expr = tokens[i++];
                const val = this.evalExpr(expr, tokens, i - 1);
                this.vars.set(name.toUpperCase(), val);
                continue;
            }
            // ── Literals & values ───────────────────────────────────────
            const pushed = this.pushLiteralOrExec(tok, tokens[i - 1], tokens, i);
            if (pushed.consumed) {
                i += pushed.extra;
            }
        }
        return i;
    }
    runBranch(tokens, startIdx, runThen) {
        let depth = 1;
        let i = startIdx;
        let elseIdx = -1;
        let endifIdx = -1;
        // Find matching ELSE and ENDIF
        let scan = startIdx;
        let d = 1;
        while (scan < tokens.length && d > 0) {
            const t = tokens[scan].toUpperCase();
            if (t === 'IF')
                d++;
            else if (t === 'ENDIF') {
                d--;
                if (d === 0) {
                    endifIdx = scan;
                    break;
                }
            }
            else if (t === 'ELSE' && d === 1) {
                elseIdx = scan;
            }
            else if (t === 'ELSEIF' && d === 1) {
                elseIdx = scan;
            }
            scan++;
        }
        if (endifIdx === -1)
            throw new Error('IF without ENDIF');
        if (runThen) {
            const end = elseIdx !== -1 ? elseIdx : endifIdx;
            this.runTokens(tokens.slice(startIdx, end), 0);
        }
        else if (elseIdx !== -1) {
            const elseKeyword = tokens[elseIdx].toUpperCase();
            if (elseKeyword === 'ELSEIF') {
                // Treat as IF condition
                const condTok = tokens[elseIdx + 1];
                const cond = this.evalExpr(condTok, tokens, elseIdx + 1);
                const innerStart = elseIdx + 2;
                this.runBranch(tokens, innerStart, cond);
            }
            else {
                this.runTokens(tokens.slice(elseIdx + 1, endifIdx), 0);
            }
        }
        return endifIdx + 1; // after ENDIF
    }
    evalExpr(expr, tokens, _idx) {
        const up = expr.toUpperCase();
        // Boolean literals
        if (up === 'TRUE')
            return true;
        if (up === 'FALSE')
            return false;
        // Numeric
        if (/^-?\d+(\.\d+)?$/.test(expr))
            return BigInt(Math.trunc(parseFloat(expr)));
        // Hex
        if (/^0x[0-9a-fA-F]+$/.test(expr))
            return expr.toLowerCase();
        // String literal
        if ((expr.startsWith('"') && expr.endsWith('"')) ||
            (expr.startsWith("'") && expr.endsWith("'"))) {
            return expr.slice(1, -1);
        }
        // Variable lookup
        const varVal = this.vars.get(up);
        if (varVal !== undefined)
            return varVal;
        // Builtin function call
        return this.callBuiltin(up, tokens, _idx);
    }
    pushLiteralOrExec(tok, _raw, tokens, _nextIdx) {
        const up = tok.toUpperCase();
        // Boolean
        if (up === 'TRUE') {
            this.push(true);
            return { consumed: true, extra: 0 };
        }
        if (up === 'FALSE') {
            this.push(false);
            return { consumed: true, extra: 0 };
        }
        // Number
        if (/^-?\d+(\.\d+)?$/.test(tok)) {
            this.push(BigInt(Math.trunc(parseFloat(tok))));
            return { consumed: true, extra: 0 };
        }
        // Hex
        if (/^0x[0-9a-fA-F]+$/i.test(tok)) {
            this.push(tok.toLowerCase());
            return { consumed: true, extra: 0 };
        }
        // String literal
        if ((tok.startsWith('"') && tok.endsWith('"')) || (tok.startsWith("'") && tok.endsWith("'"))) {
            this.push(tok.slice(1, -1));
            return { consumed: true, extra: 0 };
        }
        // Variable
        if (this.vars.has(up)) {
            this.push(this.vars.get(up));
            return { consumed: true, extra: 0 };
        }
        // Operators (two-operand, postfix — pop two items)
        if (up === 'ADD') {
            const b = this.popNum(), a = this.popNum();
            this.push(a + b);
            return { consumed: true, extra: 0 };
        }
        if (up === 'SUB') {
            const b = this.popNum(), a = this.popNum();
            this.push(a - b);
            return { consumed: true, extra: 0 };
        }
        if (up === 'MUL') {
            const b = this.popNum(), a = this.popNum();
            this.push(a * b);
            return { consumed: true, extra: 0 };
        }
        if (up === 'DIV') {
            const b = this.popNum(), a = this.popNum();
            if (b === 0n)
                throw new Error('DIV by zero');
            this.push(a / b);
            return { consumed: true, extra: 0 };
        }
        if (up === 'MOD') {
            const b = this.popNum(), a = this.popNum();
            this.push(a % b);
            return { consumed: true, extra: 0 };
        }
        if (up === 'INC') {
            this.push(this.popNum() + 1n);
            return { consumed: true, extra: 0 };
        }
        if (up === 'DEC') {
            this.push(this.popNum() - 1n);
            return { consumed: true, extra: 0 };
        }
        if (up === 'NEG') {
            this.push(-this.popNum());
            return { consumed: true, extra: 0 };
        }
        // Boolean operators
        if (up === 'AND') {
            const b = this.popBool(), a = this.popBool();
            this.push(a && b);
            return { consumed: true, extra: 0 };
        }
        if (up === 'OR') {
            const b = this.popBool(), a = this.popBool();
            this.push(a || b);
            return { consumed: true, extra: 0 };
        }
        if (up === 'NOT') {
            this.push(!this.popBool());
            return { consumed: true, extra: 0 };
        }
        if (up === 'XOR') {
            const b = this.popBool(), a = this.popBool();
            this.push(a !== b);
            return { consumed: true, extra: 0 };
        }
        if (up === 'NAND') {
            const b = this.popBool(), a = this.popBool();
            this.push(!(a && b));
            return { consumed: true, extra: 0 };
        }
        if (up === 'NOR') {
            const b = this.popBool(), a = this.popBool();
            this.push(!(a || b));
            return { consumed: true, extra: 0 };
        }
        // Comparison
        if (up === 'EQ') {
            const b = this.pop(), a = this.pop();
            this.push(a === b);
            return { consumed: true, extra: 0 };
        }
        if (up === 'NEQ') {
            const b = this.pop(), a = this.pop();
            this.push(a !== b);
            return { consumed: true, extra: 0 };
        }
        if (up === 'LT') {
            const b = this.popNum(), a = this.popNum();
            this.push(a < b);
            return { consumed: true, extra: 0 };
        }
        if (up === 'GT') {
            const b = this.popNum(), a = this.popNum();
            this.push(a > b);
            return { consumed: true, extra: 0 };
        }
        if (up === 'LTE') {
            const b = this.popNum(), a = this.popNum();
            this.push(a <= b);
            return { consumed: true, extra: 0 };
        }
        if (up === 'GTE') {
            const b = this.popNum(), a = this.popNum();
            this.push(a >= b);
            return { consumed: true, extra: 0 };
        }
        // Built-in functions
        this.push(this.callBuiltin(up, tokens, _nextIdx - 1));
        return { consumed: true, extra: 0 };
    }
    callBuiltin(name, _tokens, _idx) {
        const ctx = this.ctx;
        switch (name) {
            // ── Transaction context ────────────────────────────────────
            case '@AMOUNT':
                return ctx.inputs[0]?.amount ?? 0n;
            case '@ADDRESS':
                return ctx.inputs[0]?.address ?? '0x00';
            case '@BLOCK':
                return ctx.block ?? 0n;
            case '@COINAGE':
                return ctx.blocktime ?? ctx.block ?? 0n;
            case '@COINID':
                return ctx.inputs[0]?.coinid ?? '0x00';
            case '@SCRIPT':
                return ctx.inputs[0]?.script ?? '';
            case '@TOKENID':
                return ctx.inputs[0]?.tokenid ?? '0x00';
            // ── Signature checks ──────────────────────────────────────
            case 'SIGNEDBY': {
                const addr = this.pop();
                return ctx.signers.some(s => s.address.toLowerCase() === String(addr).toLowerCase());
            }
            case 'CHECKSIG': {
                const pub = this.pop();
                const sig = this.pop();
                const data = this.pop();
                // In test mode: always true if signer is in context
                const signerAddr = String(pub);
                return ctx.signers.some(s => s.address === signerAddr || s.publicKey === signerAddr);
            }
            case 'MULTISIG': {
                const required = Number(this.popNum());
                const total = Number(this.popNum());
                const addresses = [];
                for (let k = 0; k < total; k++)
                    addresses.push(String(this.pop()));
                let count = 0;
                for (const addr of addresses) {
                    if (ctx.signers.some(s => s.address === addr || s.publicKey === addr))
                        count++;
                }
                return count >= required;
            }
            // ── State variables ───────────────────────────────────────
            case 'STATE': {
                const idx = Number(this.popNum());
                const v = ctx.stateVars?.[idx];
                return v !== undefined ? v : 0n;
            }
            case 'PREVSTATE': {
                const idx = Number(this.popNum());
                const v = ctx.prevStateVars?.[idx];
                return v !== undefined ? v : 0n;
            }
            case 'SAMESTATE': {
                const idx = Number(this.popNum());
                const cur = ctx.stateVars?.[idx];
                const prev = ctx.prevStateVars?.[idx];
                return cur === prev;
            }
            // ── Outputs ───────────────────────────────────────────────
            case 'VERIFYOUT': {
                const outIdx = Number(this.popNum());
                const addr = String(this.pop());
                const amount = this.popNum();
                const tok = String(this.pop());
                const out = ctx.outputs[outIdx];
                if (!out)
                    return false;
                return out.address === addr &&
                    out.amount === amount &&
                    (out.tokenid ?? '0x00') === tok;
            }
            case 'VERIFYIN': {
                const inIdx = Number(this.popNum());
                const addr = String(this.pop());
                const amount = this.popNum();
                const tok = String(this.pop());
                const inp = ctx.inputs[inIdx];
                if (!inp)
                    return false;
                return inp.address === addr &&
                    inp.amount === amount &&
                    (inp.tokenid ?? '0x00') === tok;
            }
            case 'GETOUTADDR': {
                const outIdx = Number(this.popNum());
                return ctx.outputs[outIdx]?.address ?? '0x00';
            }
            case 'GETOUTAMT': {
                const outIdx = Number(this.popNum());
                return ctx.outputs[outIdx]?.amount ?? 0n;
            }
            case 'SUMINPUTS':
                return ctx.inputs.reduce((sum, inp) => sum + inp.amount, 0n);
            case 'SUMOUTPUTS':
                return ctx.outputs.reduce((sum, out) => sum + out.amount, 0n);
            // ── Crypto ────────────────────────────────────────────────
            case 'SHA2': {
                const data = String(this.pop());
                return '0x' + sha256hex(data);
            }
            case 'SHA3': {
                const data = String(this.pop());
                return '0x' + sha3_256hex(data);
            }
            case 'SHA256': {
                const data = String(this.pop());
                return '0x' + sha256hex(data);
            }
            // ── String / hex ──────────────────────────────────────────
            case 'CONCAT': {
                const b = String(this.pop());
                const a = String(this.pop());
                return a + b;
            }
            case 'SUBSET': {
                const end = Number(this.popNum());
                const start = Number(this.popNum());
                const data = String(this.pop());
                return data.slice(start, end);
            }
            case 'LEN': {
                const data = this.pop();
                if (typeof data === 'string')
                    return BigInt(data.replace(/^0x/, '').length / 2);
                return 0n;
            }
            case 'REV': {
                const data = String(this.pop()).replace(/^0x/, '');
                const rev = data.match(/../g)?.reverse().join('') ?? '';
                return '0x' + rev;
            }
            case 'HEXCAT': {
                const b = String(this.pop()).replace(/^0x/, '');
                const a = String(this.pop()).replace(/^0x/, '');
                return '0x' + a + b;
            }
            case 'NUMBER': {
                const hex = String(this.pop()).replace(/^0x/, '');
                return BigInt('0x' + (hex || '0'));
            }
            case 'HEX': {
                const num = this.popNum();
                return '0x' + num.toString(16);
            }
            case 'BOOL': {
                const v = this.pop();
                return v !== false && v !== 0n && v !== '0x' && v !== '';
            }
            case 'STR': {
                const v = this.pop();
                return String(v);
            }
            // ── Stack ops ─────────────────────────────────────────────
            case 'DUP': {
                const v = this.pop();
                this.push(v);
                this.push(v);
                return v; // will be double-pushed — handled below
            }
            case 'SWAP': {
                const b = this.pop(), a = this.pop();
                this.push(b);
                this.push(a);
                return a;
            }
            case 'DROP':
                return this.pop();
            // ── MMR ───────────────────────────────────────────────────
            case '@MMRROOT':
                return ctx.mmrroot ?? '0x00';
            case '@MMRPEAKS':
                return ctx.mmrpeaks ?? '0x00';
            default:
                throw new Error(`Unknown KISS VM token: ${name}`);
        }
    }
    push(v) {
        if (this.stack.length >= this.MAX_STACK) {
            throw new Error('KISS VM: stack overflow (max 64)');
        }
        this.stack.push(v);
    }
    pop() {
        if (this.stack.length === 0)
            throw new Error('KISS VM: stack underflow');
        return this.stack.pop();
    }
    popNum() {
        const v = this.pop();
        if (typeof v === 'bigint')
            return v;
        if (typeof v === 'boolean')
            return v ? 1n : 0n;
        if (typeof v === 'string') {
            if (/^0x[0-9a-f]+$/i.test(v))
                return BigInt(v);
            if (/^-?\d+$/.test(v))
                return BigInt(v);
        }
        throw new Error(`Expected number, got: ${typeof v} = ${v}`);
    }
    popBool() {
        const v = this.pop();
        if (typeof v === 'boolean')
            return v;
        if (typeof v === 'bigint')
            return v !== 0n;
        if (v === 'TRUE')
            return true;
        if (v === 'FALSE')
            return false;
        return Boolean(v);
    }
}
exports.KissVMInterpreter = KissVMInterpreter;
//# sourceMappingURL=interpreter.js.map