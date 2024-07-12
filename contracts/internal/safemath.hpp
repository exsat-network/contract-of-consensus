#pragma once

namespace safemath {
using std::string;
uint64_t add(const uint64_t a, const uint64_t b) {
    uint64_t c = a + b;
    check(c >= a, "add-overflow");
    return c;
}

uint64_t sub(const uint64_t a, const uint64_t b) {
    uint64_t c = a - b;
    check(c <= a, "sub-overflow");
    return c;
}

uint64_t mul(const uint64_t a, const uint64_t b) {
    uint64_t c = a * b;
    check(b == 0 || c / b == a, "mul-overflow");
    return c;
}

uint64_t div(const uint64_t a, const uint64_t b) {
    check(b > 0, "divide by zero");
    return a / b;
}

uint64_t muldiv(const uint64_t a, const uint64_t b, const uint64_t c) { return div(mul(a, b), c); }
}  // namespace safemath

namespace safemath128 {
using std::string;
uint128_t add(const uint128_t a, const uint128_t b) {
    uint128_t c = a + b;
    check(c >= a, "add-overflow");
    return c;
}

uint128_t sub(const uint128_t a, const uint128_t b) {
    uint128_t c = a - b;
    check(c <= a, "sub-overflow");
    return c;
}

uint128_t mul(const uint128_t a, const uint128_t b) {
    uint128_t c = a * b;
    check(b == 0 || c / b == a, "mul-overflow");
    return c;
}

uint128_t div(const uint128_t a, const uint128_t b) {
    check(b > 0, "divide by zero");
    return a / b;
}

uint128_t muldiv(const uint128_t a, const uint128_t b, const uint128_t c) { return div(mul(a, b), c); }
}  // namespace safemath128
