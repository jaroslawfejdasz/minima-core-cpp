#include "MiniNumber.hpp"
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <cmath>
#include <cassert>

namespace minima {

// ─── helpers ──────────────────────────────────────────────────────────────────

static bool isDigit(char c) { return c >= '0' && c <= '9'; }

// Remove leading zeros but keep at least "0"
static std::string stripLeadingZeros(const std::string& s) {
    bool neg = (!s.empty() && s[0] == '-');
    size_t start = neg ? 1 : 0;
    size_t i = start;
    while (i + 1 < s.size() && s[i] == '0') ++i;
    return (neg ? "-" : "") + s.substr(i);
}

// Compare two non-negative decimal strings (no sign, no decimal point)
static int cmpUnsignedInt(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    return a.compare(b);  // lexicographic == numeric for equal lengths
}

// Add two non-negative decimal integer strings
static std::string addUnsigned(const std::string& a, const std::string& b) {
    std::string res;
    int carry = 0;
    int i = (int)a.size() - 1;
    int j = (int)b.size() - 1;
    while (i >= 0 || j >= 0 || carry) {
        int sum = carry;
        if (i >= 0) { sum += (a[i--] - '0'); }
        if (j >= 0) { sum += (b[j--] - '0'); }
        carry = sum / 10;
        res.push_back('0' + sum % 10);
    }
    std::reverse(res.begin(), res.end());
    return res.empty() ? "0" : res;
}

// Subtract b from a where a >= b, both non-negative decimal strings
static std::string subUnsigned(const std::string& a, const std::string& b) {
    std::string res;
    int borrow = 0;
    int i = (int)a.size() - 1;
    int j = (int)b.size() - 1;
    while (i >= 0) {
        int diff = (a[i--] - '0') - borrow - (j >= 0 ? (b[j--] - '0') : 0);
        if (diff < 0) { diff += 10; borrow = 1; } else { borrow = 0; }
        res.push_back('0' + diff);
    }
    std::reverse(res.begin(), res.end());
    return stripLeadingZeros(res);
}

// Multiply two non-negative decimal integer strings
static std::string mulUnsigned(const std::string& a, const std::string& b) {
    size_t n = a.size(), m = b.size();
    std::vector<int> digits(n + m, 0);
    for (int i = (int)n - 1; i >= 0; --i)
        for (int j = (int)m - 1; j >= 0; --j)
            digits[i + j + 1] += (a[i]-'0') * (b[j]-'0');
    for (int i = (int)digits.size()-1; i > 0; --i) {
        digits[i-1] += digits[i] / 10;
        digits[i] %= 10;
    }
    std::string res;
    bool leading = true;
    for (int d : digits) {
        if (leading && d == 0) continue;
        leading = false;
        res.push_back('0' + d);
    }
    return res.empty() ? "0" : res;
}

// Divide a by b (integer division), both non-negative strings. Also returns remainder.
static std::pair<std::string,std::string> divmodUnsigned(const std::string& a, const std::string& b) {
    if (b == "0") throw std::runtime_error("MiniNumber: division by zero");
    if (cmpUnsignedInt(a, b) < 0) return {"0", a};

    std::string quotient, remainder = "0";
    for (char ch : a) {
        // remainder = remainder * 10 + digit
        if (remainder == "0") remainder = std::string(1, ch);
        else remainder.push_back(ch);
        remainder = stripLeadingZeros(remainder);

        // find how many times b fits in remainder (0..9)
        int q = 0;
        while (cmpUnsignedInt(remainder, b) >= 0) {
            remainder = subUnsigned(remainder, b);
            ++q;
        }
        quotient.push_back('0' + q);
    }
    return {stripLeadingZeros(quotient), stripLeadingZeros(remainder)};
}

// ─── normalise ────────────────────────────────────────────────────────────────

std::string MiniNumber::normalise(const std::string& s) {
    if (s.empty()) return "0";
    std::string t = s;
    // Trim whitespace
    while (!t.empty() && (t.front()==' '||t.front()=='\t')) t.erase(t.begin());
    while (!t.empty() && (t.back()==' '||t.back()=='\t')) t.pop_back();
    if (t.empty() || t == "-") return "0";

    bool neg = (t[0] == '-');
    std::string abs = neg ? t.substr(1) : t;

    // Validate: only digits and at most one dot
    size_t dots = std::count(abs.begin(), abs.end(), '.');
    if (dots > 1) throw std::invalid_argument("MiniNumber: invalid: " + s);
    for (char c : abs) if (!isDigit(c) && c != '.') throw std::invalid_argument("MiniNumber: invalid char in: " + s);

    // Strip trailing zeros after decimal point
    if (dots == 1) {
        while (abs.back() == '0') abs.pop_back();
        if (abs.back() == '.') abs.pop_back();
    }

    abs = stripLeadingZeros(abs);
    if (abs == "0" || abs.empty()) return "0";
    return (neg ? "-" : "") + abs;
}

// ─── construction ─────────────────────────────────────────────────────────────

MiniNumber::MiniNumber() : m_value("0") {}
MiniNumber::MiniNumber(const std::string& s) : m_value(normalise(s)) {}
MiniNumber::MiniNumber(long long v) : m_value(std::to_string(v)) {}

MiniNumber MiniNumber::zero()      { return MiniNumber("0"); }
MiniNumber MiniNumber::one()       { return MiniNumber("1"); }
MiniNumber MiniNumber::two()       { return MiniNumber("2"); }
MiniNumber MiniNumber::maxAmount() { return MiniNumber("1000000000"); }

// ─── queries ──────────────────────────────────────────────────────────────────

bool MiniNumber::isZero()     const { return m_value == "0"; }
bool MiniNumber::isNegative() const { return !m_value.empty() && m_value[0] == '-'; }
bool MiniNumber::isPositive() const { return !isZero() && !isNegative(); }
bool MiniNumber::isInteger()  const { return m_value.find('.') == std::string::npos; }
std::string MiniNumber::toString() const { return m_value; }

long long MiniNumber::toLong() const {
    try { return std::stoll(m_value); }
    catch (...) { throw std::overflow_error("MiniNumber::toLong overflow: " + m_value); }
}

// ─── comparison ───────────────────────────────────────────────────────────────

int MiniNumber::compareDecimal(const std::string& a, const std::string& b) {
    bool aNeg = (!a.empty() && a[0] == '-');
    bool bNeg = (!b.empty() && b[0] == '-');
    if (aNeg != bNeg) return aNeg ? -1 : 1;

    std::string absA = aNeg ? a.substr(1) : a;
    std::string absB = bNeg ? b.substr(1) : b;

    // Split integer and fractional parts
    auto split = [](const std::string& s) -> std::pair<std::string,std::string> {
        auto pos = s.find('.');
        if (pos == std::string::npos) return {s, ""};
        return {s.substr(0, pos), s.substr(pos+1)};
    };
    auto [intA, fracA] = split(absA);
    auto [intB, fracB] = split(absB);

    int cmp = cmpUnsignedInt(intA, intB);
    if (cmp != 0) return aNeg ? -cmp : cmp;

    // Pad fractional parts to same length
    size_t maxLen = std::max(fracA.size(), fracB.size());
    fracA.resize(maxLen, '0');
    fracB.resize(maxLen, '0');
    cmp = fracA.compare(fracB);
    return aNeg ? -cmp : cmp;
}

bool MiniNumber::isEqual(const MiniNumber& rhs)       const { return compareDecimal(m_value, rhs.m_value) == 0; }
bool MiniNumber::isLessThan(const MiniNumber& rhs)    const { return compareDecimal(m_value, rhs.m_value) < 0; }
bool MiniNumber::isGreaterThan(const MiniNumber& rhs) const { return compareDecimal(m_value, rhs.m_value) > 0; }
bool MiniNumber::isLessOrEqual(const MiniNumber& rhs) const { return compareDecimal(m_value, rhs.m_value) <= 0; }
bool MiniNumber::isGreaterOrEqual(const MiniNumber& rhs) const { return compareDecimal(m_value, rhs.m_value) >= 0; }

// ─── arithmetic ───────────────────────────────────────────────────────────────

MiniNumber MiniNumber::add(const MiniNumber& rhs) const {
    bool aNeg = isNegative();
    bool bNeg = rhs.isNegative();
    std::string absA = aNeg ? m_value.substr(1) : m_value;
    std::string absB = bNeg ? rhs.m_value.substr(1) : rhs.m_value;

    // Remove decimal handling: work with integer strings only for now
    // (Minima amounts are integer in base units)
    auto intPart = [](const std::string& s) {
        auto p = s.find('.'); return p == std::string::npos ? s : s.substr(0, p);
    };
    absA = intPart(absA); absB = intPart(absB);

    if (aNeg == bNeg) {
        std::string res = addUnsigned(absA, absB);
        return MiniNumber((aNeg ? "-" : "") + res);
    }
    // Different signs → subtract
    int c = cmpUnsignedInt(absA, absB);
    if (c == 0) return MiniNumber::zero();
    if (c > 0) return MiniNumber((aNeg ? "-" : "") + subUnsigned(absA, absB));
    return MiniNumber((bNeg ? "-" : "") + subUnsigned(absB, absA));
}

MiniNumber MiniNumber::sub(const MiniNumber& rhs) const {
    // a - b = a + (-b)
    std::string negB = rhs.isNegative() ? rhs.m_value.substr(1) : "-" + rhs.m_value;
    if (rhs.isZero()) negB = "0";
    return add(MiniNumber(negB));
}

MiniNumber MiniNumber::mul(const MiniNumber& rhs) const {
    bool neg = isNegative() != rhs.isNegative();
    std::string absA = isNegative() ? m_value.substr(1) : m_value;
    std::string absB = rhs.isNegative() ? rhs.m_value.substr(1) : rhs.m_value;
    auto intPart = [](const std::string& s) {
        auto p = s.find('.'); return p == std::string::npos ? s : s.substr(0, p);
    };
    absA = intPart(absA); absB = intPart(absB);
    std::string res = mulUnsigned(absA, absB);
    if (res == "0") return MiniNumber::zero();
    return MiniNumber((neg ? "-" : "") + res);
}

MiniNumber MiniNumber::div(const MiniNumber& rhs) const {
    if (rhs.isZero()) throw std::runtime_error("MiniNumber: division by zero");
    bool neg = isNegative() != rhs.isNegative();
    std::string absA = isNegative() ? m_value.substr(1) : m_value;
    std::string absB = rhs.isNegative() ? rhs.m_value.substr(1) : rhs.m_value;
    auto intPart = [](const std::string& s) {
        auto p = s.find('.'); return p == std::string::npos ? s : s.substr(0, p);
    };
    absA = intPart(absA); absB = intPart(absB);
    auto [q, r] = divmodUnsigned(absA, absB);
    if (q == "0") return MiniNumber::zero();
    return MiniNumber((neg ? "-" : "") + q);
}

MiniNumber MiniNumber::mod(const MiniNumber& rhs) const {
    if (rhs.isZero()) throw std::runtime_error("MiniNumber: modulo by zero");
    std::string absA = isNegative() ? m_value.substr(1) : m_value;
    std::string absB = rhs.isNegative() ? rhs.m_value.substr(1) : rhs.m_value;
    auto intPart = [](const std::string& s) {
        auto p = s.find('.'); return p == std::string::npos ? s : s.substr(0, p);
    };
    absA = intPart(absA); absB = intPart(absB);
    auto [q, rem] = divmodUnsigned(absA, absB);
    return MiniNumber(rem);
}

// ─── bitwise ──────────────────────────────────────────────────────────────────

MiniNumber MiniNumber::bitwiseAnd(const MiniNumber& rhs) const {
    return MiniNumber((long long)(toLong() & rhs.toLong()));
}
MiniNumber MiniNumber::bitwiseOr(const MiniNumber& rhs) const {
    return MiniNumber((long long)(toLong() | rhs.toLong()));
}
MiniNumber MiniNumber::bitwiseXor(const MiniNumber& rhs) const {
    return MiniNumber((long long)(toLong() ^ rhs.toLong()));
}
MiniNumber MiniNumber::shiftLeft(int bits) const {
    return MiniNumber((long long)(toLong() << bits));
}
MiniNumber MiniNumber::shiftRight(int bits) const {
    return MiniNumber((long long)(toLong() >> bits));
}
MiniNumber MiniNumber::bitwiseNot() const {
    return MiniNumber((long long)(~toLong()));
}

// ─── serialisation ────────────────────────────────────────────────────────────

std::vector<uint8_t> MiniNumber::serialise() const {
    // Format: 1-byte length + ASCII decimal string
    std::vector<uint8_t> out;
    if (m_value.size() > 255) throw std::runtime_error("MiniNumber too large to serialise");
    out.push_back((uint8_t)m_value.size());
    for (char c : m_value) out.push_back((uint8_t)c);
    return out;
}

MiniNumber MiniNumber::deserialise(const uint8_t* data, size_t& offset) {
    uint8_t len = data[offset++];
    std::string s(reinterpret_cast<const char*>(data + offset), len);
    offset += len;
    return MiniNumber(s);
}

} // namespace minima
