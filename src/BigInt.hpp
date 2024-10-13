#ifndef BIGINT_HPP
#define BIGINT_HPP

#include <array>
#include <cstdint>
#include <stdexcept>
#include <cstring>
#include <string>

#define N 4

class BigIntClass {
public:
    std::array<uint64_t, N> data;

    BigIntClass() : data{} {}

    BigIntClass(uint64_t value) : data{} {
        data[0] = value;
    }

    BigIntClass(const uint8_t* array, const size_t size) {
        if (size > N * sizeof(uint64_t)) {
            throw std::invalid_argument("Array size exceeds BigIntClass capacity");
        }
        std::memset(data.data(), 0, N * sizeof(uint64_t));
        std::memcpy(data.data(), array, size);
    }

    BigIntClass& operator+=(const BigIntClass& other) {
        uint64_t carry = 0;
        for (size_t i = 0; i < N; ++i) {
            uint64_t sum = data[i] + other.data[i] + carry;
            carry = (sum < data[i]) ? 1 : 0;
            data[i] = sum;
        }
        return *this;
    }

    BigIntClass& operator+=(uint64_t value) {
        return *this += BigIntClass(value);
    }

    BigIntClass operator+(const BigIntClass& other) const {
        BigIntClass result = *this;
        result += other;
        return result;
    }

    BigIntClass operator+(uint64_t value) const {
        return *this + BigIntClass(value);
    }

    BigIntClass& operator-=(const BigIntClass& other) {
        bool borrow = false;
        for (std::size_t i = 0; i < N; ++i) {
            uint64_t subtrahend = other.data[i] + borrow;
            if (data[i] < subtrahend) {
                data[i] = data[i] - subtrahend + UINT64_MAX + 1;
                borrow = true;
            } else {
                data[i] -= subtrahend;
                borrow = false;
            }
        }
        return *this;
    }

    BigIntClass& operator-=(uint64_t value) {
        return *this -= BigIntClass(value);
    }

    BigIntClass operator-(const BigIntClass& other) const {
        BigIntClass result = *this;
        result -= other;
        return result;
    }

    BigIntClass operator-(uint64_t value) const {
        return *this - BigIntClass(value);
    }

    BigIntClass& operator*=(const BigIntClass& other) {
        BigIntClass result;
        for (size_t i = 0; i < N; ++i) {
            uint64_t carry = 0;
            for (size_t j = 0; j < N - i; ++j) {
                __uint128_t product = (__uint128_t)data[i] * other.data[j] + result.data[i + j] + carry;
                result.data[i + j] = (uint64_t)product;
                carry = (uint64_t)(product >> 64);
            }
        }
        *this = result;
        return *this;
    }

    BigIntClass& operator*=(uint64_t value) {
        return *this *= BigIntClass(value);
    }

    BigIntClass operator*(const BigIntClass& other) const {
        BigIntClass result = *this;
        result *= other;
        return result;
    }

    BigIntClass operator*(uint64_t value) const {
        return *this * BigIntClass(value);
    }

    BigIntClass operator/(const BigIntClass& other) const {
        uint64_t modulus;
        return divmod(other, modulus);
    }

    BigIntClass operator/(uint64_t value) const {
        uint64_t modulus;
        return divmod(value, modulus);
    }

    BigIntClass operator%(const BigIntClass& other) const {
        uint64_t modulus;
        divmod(other, modulus);
        return BigIntClass(modulus);
    }

    BigIntClass operator%(uint64_t value) const {
        uint64_t modulus;
        divmod(value, modulus);
        return BigIntClass(modulus);
    }

    BigIntClass operator^(const BigIntClass& other) const {
        BigIntClass result;
        for (std::size_t i = 0; i < N; ++i) {
            result.data[i] = data[i] ^ other.data[i];
        }
        return result;
    }

    BigIntClass operator^(uint64_t value) const {
        return *this ^ BigIntClass(value);
    }

    BigIntClass& operator^=(const BigIntClass& other) {
        for (std::size_t i = 0; i < N; ++i) {
            data[i] ^= other.data[i];
        }
        return *this;
    }

    BigIntClass& operator^=(uint64_t value) {
        return *this ^= BigIntClass(value);
    }

    BigIntClass& operator%=(const BigIntClass& other) {
        BigIntClass modulus;
        divmod(other, modulus);
        *this = modulus;
        return *this;
    }

    BigIntClass& operator%=(uint64_t value) {
        uint64_t modulus;
        *this = divmod(value, modulus);
        return *this;
    }

    BigIntClass operator|(const BigIntClass& other) const {
        BigIntClass result;
        for (std::size_t i = 0; i < N; ++i) {
            result.data[i] = data[i] | other.data[i];
        }
        return result;
    }

    BigIntClass operator|(uint64_t value) const {
        return *this | BigIntClass(value);
    }

    BigIntClass operator~() const {
        BigIntClass result;
        for (std::size_t i = 0; i < N; ++i) {
            result.data[i] = ~data[i];
        }
        return result;
    }

    BigIntClass& operator|=(const BigIntClass& other) {
        for (std::size_t i = 0; i < N; ++i) {
            data[i] |= other.data[i];
        }
        return *this;
    }

    BigIntClass& operator|=(uint64_t value) {
        return *this |= BigIntClass(value);
    }

    bool operator>(const BigIntClass& other) const {
        for (std::size_t i = N; i-- > 0;) {
            if (data[i] > other.data[i]) return true;
            if (data[i] < other.data[i]) return false;
        }
        return false;
    }

    BigIntClass& operator++() {
        *this += 1;
        return *this;
    }

    BigIntClass operator++(int) {
        BigIntClass temp = *this;
        ++(*this);
        return temp;
    }

    BigIntClass& operator--() {
        *this -= 1;
        return *this;
    }

    BigIntClass operator--(int) {
        BigIntClass temp = *this;
        --(*this);
        return temp;
    }

    BigIntClass divmod(const BigIntClass& divisor, BigIntClass& modulus) const {
        if (divisor == BigIntClass(0)) {
            throw std::invalid_argument("Division by zero");
        }

        BigIntClass quotient(0);
        BigIntClass remainder(0);

        for (int i = N * 64 - 1; i >= 0; --i) {
            remainder <<= 1;
            remainder.data[0] |= (data[i / 64] >> (i % 64)) & 1;
            if (remainder >= divisor) {
                remainder -= divisor;
                quotient.data[i / 64] |= (uint64_t)1 << (i % 64);
            }
        }
        modulus = remainder;
        return quotient;
    }

    BigIntClass divmod(const BigIntClass& divisor, uint64_t& modulus) const {
        BigIntClass mod;
        BigIntClass quotient = divmod(divisor, mod);
        modulus = mod.data[0];
        return quotient;
    }

    BigIntClass divmod(uint64_t value, uint64_t& modulus) const {
        return divmod(BigIntClass(value), modulus);
    }

    bool operator==(const BigIntClass& other) const {
        return data == other.data;
    }

    bool operator!=(const BigIntClass& other) const {
        return !(*this == other);
    }

    bool operator<(const BigIntClass& other) const {
        for (size_t i = N; i-- > 0;) {
            if (data[i] < other.data[i]) return true;
            if (data[i] > other.data[i]) return false;
        }
        return false;
    }

    bool operator>=(const BigIntClass& other) const {
        return !(*this < other);
    }

    BigIntClass& operator<<=(size_t shift) {
        if (shift >= N * 64) {
            *this = BigIntClass(0);
            return *this;
        }

        size_t whole_shifts = shift / 64;
        size_t bit_shifts = shift % 64;

        if (bit_shifts == 0) {
            for (size_t i = N - 1; i >= whole_shifts; --i) {
                data[i] = data[i - whole_shifts];
            }
        } else {
            for (size_t i = N - 1; i > whole_shifts; --i) {
                data[i] = (data[i - whole_shifts] << bit_shifts) | (data[i - whole_shifts - 1] >> (64 - bit_shifts));
            }
            data[whole_shifts] = data[0] << bit_shifts;
        }

        for (size_t i = 0; i < whole_shifts; ++i) {
            data[i] = 0;
        }

        return *this;
    }

    BigIntClass pow(uint64_t exponent) const {
        BigIntClass result(1);
        BigIntClass base = *this;
        while (exponent > 0) {
            if (exponent & 1) {
                result *= base;
            }
            base *= base;
            exponent >>= 1;
        }
        return result;
    }

    const uint64_t get(const size_t Index) const
    {
        return data[Index];
    }

    std::string toString() const {
        BigIntClass temp = *this;
        std::string result;
        BigIntClass zero(0);
        BigIntClass ten(10);
        uint64_t remainder;

        if (temp == zero) {
            return "0";
        }

        while (temp > zero) {
            BigIntClass quotient;
            quotient = temp.divmod(ten, remainder);
            result.insert(result.begin(), '0' + remainder);
            // std::cout << result << std::endl;
            temp = quotient;
        }

        return result;
    }

    std::string toSignedString() const {
        BigIntClass temp = *this;
        std::string result;
        bool isNegative = false;

        // Check if the number is negative
        if (temp.data[N - 1] & (1ULL << 63)) {
            isNegative = true;
            // Convert to positive by taking two's complement
            temp = ~temp;
            ++temp;
        }

        BigIntClass zero(0);
        BigIntClass ten(10);
        uint64_t remainder;

        if (temp == zero) {
            return "0";
        }

        while (temp > zero) {
            BigIntClass quotient = temp.divmod(ten, remainder);
            result.insert(result.begin(), '0' + remainder);
            temp = quotient;
        }

        if (isNegative) {
            result.insert(result.begin(), '-');
        }

        return result;
    }
};

#endif // BIGINTClass_HPP
