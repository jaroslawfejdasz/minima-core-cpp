const {
  BasicSigned, MultiSig,
  TimeLock, CoinAgeLock, TimeLockOrRefund,
  HTLC,
  Exchange, LimitOrder,
  Vault, TwoOfThreeRecovery
} = require('../dist/index');

const KEY1 = '0x' + 'aa'.repeat(32);
const KEY2 = '0x' + 'bb'.repeat(32);
const KEY3 = '0x' + 'cc'.repeat(32);
const HASH = '0x' + 'de'.repeat(32);
const ADDR = '0x' + 'ff'.repeat(32);
const TOKEN = '0x00';

let passed = 0, failed = 0;

function test(name, fn) {
  try { fn(); console.log(`  ✓ ${name}`); passed++; }
  catch(e) { console.log(`  ✗ ${name}\n      → ${e.message}`); failed++; }
}
function throws(fn, msg) {
  let threw = false;
  try { fn(); } catch { threw = true; }
  if (!threw) throw new Error(msg || 'Expected to throw');
}
function assert(c, m) { if (!c) throw new Error(m || 'Assertion failed'); }
function includes(str, sub) { assert(str.includes(sub), `Expected "${sub}" in:\n${str}`); }

// ── BasicSigned ──
console.log('\nBasicSigned');
test('generates valid script', () => {
  const s = BasicSigned(KEY1);
  includes(s, 'RETURN SIGNEDBY(');
  includes(s, KEY1);
});
test('throws on short key', () => throws(() => BasicSigned('0xaabb'), 'Expected throw for short key'));
test('throws on missing 0x', () => throws(() => BasicSigned('aabbccdd'), 'Expected throw for no 0x'));

// ── MultiSig ──
console.log('\nMultiSig');
test('2-of-3 generates valid script', () => {
  const s = MultiSig(2, [KEY1, KEY2, KEY3]);
  includes(s, 'RETURN MULTISIG(2,');
  includes(s, KEY1); includes(s, KEY2); includes(s, KEY3);
});
test('throws when required > keys', () => throws(() => MultiSig(3, [KEY1, KEY2])));
test('throws on empty keys', () => throws(() => MultiSig(1, [])));
test('throws on required < 1', () => throws(() => MultiSig(0, [KEY1])));

// ── TimeLock ──
console.log('\nTimeLock');
test('generates script with block number', () => {
  const s = TimeLock(KEY1, 1000000);
  includes(s, 'ASSERT @BLOCK GTE 1000000');
  includes(s, `RETURN SIGNEDBY(${KEY1})`);
});
test('throws on negative block', () => throws(() => TimeLock(KEY1, -1)));
test('throws on float block', () => throws(() => TimeLock(KEY1, 1.5)));

// ── CoinAgeLock ──
console.log('\nCoinAgeLock');
test('generates coinage script', () => {
  const s = CoinAgeLock(KEY1, 1000);
  includes(s, 'ASSERT @COINAGE GTE 1000');
  includes(s, `RETURN SIGNEDBY(${KEY1})`);
});
test('throws on minAge < 1', () => throws(() => CoinAgeLock(KEY1, 0)));

// ── TimeLockOrRefund ──
console.log('\nTimeLockOrRefund');
test('generates dual-path script', () => {
  const s = TimeLockOrRefund(KEY1, KEY2, 5000);
  includes(s, 'IF @BLOCK LTE 5000');
  includes(s, `RETURN SIGNEDBY(${KEY1})`);
  includes(s, `RETURN SIGNEDBY(${KEY2})`);
});

// ── HTLC ──
console.log('\nHTLC');
test('generates HTLC with SHA2', () => {
  const s = HTLC(KEY1, KEY2, HASH, 100000, 'SHA2');
  includes(s, 'SHA2(preimage)');
  includes(s, `EQ ${HASH}`);
  includes(s, 'IF @BLOCK LT 100000');
  includes(s, `RETURN SIGNEDBY(${KEY1})`);
  includes(s, `RETURN SIGNEDBY(${KEY2})`);
});
test('generates HTLC with SHA3', () => {
  const s = HTLC(KEY1, KEY2, HASH, 100000, 'SHA3');
  includes(s, 'SHA3(preimage)');
});
test('throws on wrong hashFn', () => throws(() => HTLC(KEY1, KEY2, HASH, 100000, 'MD5')));
test('throws on short hashlock', () => throws(() => HTLC(KEY1, KEY2, '0xaabb', 100000)));

// ── Exchange ──
console.log('\nExchange');
test('generates exchange script', () => {
  const s = Exchange(TOKEN, 100, ADDR, KEY1);
  includes(s, 'VERIFYOUT');
  includes(s, '100');
  includes(s, ADDR);
  includes(s, `SIGNEDBY(${KEY1})`);
});
test('throws on amount <= 0', () => throws(() => Exchange(TOKEN, 0, ADDR, KEY1)));
test('throws on bad tokenId', () => throws(() => Exchange('noprefix', 100, ADDR, KEY1)));

// ── LimitOrder ──
console.log('\nLimitOrder');
test('generates limit order script', () => {
  const s = LimitOrder(TOKEN, 2.5, ADDR, KEY1);
  includes(s, 'SAMESTATE');
  includes(s, '2.5');
});

// ── Vault ──
console.log('\nVault');
test('generates vault with spending limit', () => {
  const s = Vault(KEY1, 100, 10);
  includes(s, 'ASSERT SIGNEDBY(');
  includes(s, 'LTE 1000'); // 100 * 10
  includes(s, 'SAMESTATE');
});
test('throws on maxSpend <= 0', () => throws(() => Vault(KEY1, 0, 10)));
test('throws on windowBlocks < 1', () => throws(() => Vault(KEY1, 100, 0)));

// ── TwoOfThreeRecovery ──
console.log('\nTwoOfThreeRecovery');
test('generates recovery script', () => {
  const s = TwoOfThreeRecovery(KEY1, [KEY2, KEY3, '0x' + 'dd'.repeat(32)], 5000);
  includes(s, `SIGNEDBY(${KEY1})`);
  includes(s, 'ASSERT @COINAGE GTE 5000');
  includes(s, 'MULTISIG(2,');
});
test('throws on delay < 1', () => throws(() => TwoOfThreeRecovery(KEY1, [KEY2, KEY3, KEY2], 0)));

// ── Script validity: no syntax errors ──
console.log('\nScript linting (all contracts must pass kiss-vm-lint)');
const { lint } = require('../../kiss-vm-lint/dist/index');

function lintCheck(name, script) {
  test(`${name}: no errors`, () => {
    const r = lint(script, { ignoreRules: ['R003', 'R009', 'R006'] }); // R003/R009 expected in templates
    if (r.errors.length > 0) {
      throw new Error(r.errors.map(e => `[${e.code}] ${e.message}`).join('; '));
    }
  });
}

lintCheck('BasicSigned', BasicSigned(KEY1));
lintCheck('MultiSig', MultiSig(2, [KEY1, KEY2, KEY3]));
lintCheck('TimeLock', TimeLock(KEY1, 1000000));
lintCheck('CoinAgeLock', CoinAgeLock(KEY1, 1000));
lintCheck('TimeLockOrRefund', TimeLockOrRefund(KEY1, KEY2, 5000));
lintCheck('HTLC', HTLC(KEY1, KEY2, HASH, 100000));
lintCheck('Exchange', Exchange(TOKEN, 100, ADDR, KEY1));
lintCheck('LimitOrder', LimitOrder(TOKEN, 2.5, ADDR, KEY1));
lintCheck('Vault', Vault(KEY1, 100, 10));
lintCheck('TwoOfThreeRecovery', TwoOfThreeRecovery(KEY1, [KEY2, KEY3, '0x'+'dd'.repeat(32)], 5000));

// Summary
console.log('');
console.log('─'.repeat(50));
if (failed === 0) {
  console.log(`\n  ✓ All ${passed} tests passed!\n`);
  process.exit(0);
} else {
  console.log(`\n  ✗ ${failed} failed, ${passed} passed\n`);
  process.exit(1);
}
