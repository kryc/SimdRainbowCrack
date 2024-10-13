//
//  biginttest.cpp
//  RainbowCrack++
//
//  Created by Kryc on 22/03/2024.
//  Copyright © 2024 Kryc. All rights reserved.
//

#include <iostream>
#include <string>

#include <gmpxx.h>

#include "BigInt.hpp"

int testBasic() {
    // Test constructor and toString
    BigIntClass a(100);
    std::cout << "a = " << a.toString() << std::endl;

    // Test addition
    BigIntClass b(50);
    BigIntClass c = a + b;
    std::cout << "a + b = " << c.toString() << std::endl;

    // Test subtraction
    BigIntClass d = a - b;
    std::cout << "a - b = " << d.toString() << std::endl;

    d = b - a;
    std::cout << "b - a = " << d.toString() << std::endl;

    // Test multiplication
    BigIntClass e = a * b;
    std::cout << "a * b = " << e.toString() << std::endl;

    // Test division and modulus
    uint64_t modulus;
    BigIntClass f = a.divmod(b, modulus);
    std::cout << "a / b = " << f.toString() << ", a % b = " << modulus << std::endl;

    // Test power
    BigIntClass g = a.pow(3);
    std::cout << "a ^ 3 = " << g.toString() << std::endl;

    // Test bitwise XOR
    BigIntClass h = a ^ b;
    std::cout << "a ^ b = " << h.toString() << std::endl;

    // Test bitwise OR
    BigIntClass i = a | b;
    std::cout << "a | b = " << i.toString() << std::endl;

    // Test increment
    ++a;
    std::cout << "++a = " << a.toString() << std::endl;

    // Test decrement
    --a;
    std::cout << "--a = " << a.toString() << std::endl;

    // Test fromArray
    uint8_t array[32] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    BigIntClass j(array, 32);
    std::cout << "j from array = " << j.toString() << std::endl;

    return 0;
}

void testBigIntClass() {
    // Test smaller numbers that fit within 256 bits
    mpz_class mpz_a("12345678901234567890");
    mpz_class mpz_b("98765432109876543210");

    BigIntClass a(reinterpret_cast<const uint8_t*>(mpz_a.get_mpz_t()->_mp_d), mpz_sizeinbase(mpz_a.get_mpz_t(), 256));
    BigIntClass b(reinterpret_cast<const uint8_t*>(mpz_b.get_mpz_t()->_mp_d), mpz_sizeinbase(mpz_b.get_mpz_t(), 256));

    // Addition
    mpz_class mpz_c = mpz_a + mpz_b;
    BigIntClass c = a + b;
    std::cout << "Addition: " << (mpz_c == mpz_class(c.toString())) << std::endl;

    // Subtraction
    mpz_class mpz_d = mpz_a - mpz_b;
    BigIntClass d = a - b;
    std::cout << "Subtraction: " << (mpz_d == mpz_class(d.toSignedString())) << std::endl;
    // std::cout << mpz_d.get_str() << std::endl;
    // std::cout << d.toSignedString() << std::endl;

    // Multiplication
    mpz_class mpz_e = mpz_a * mpz_b;
    BigIntClass e = a * b;
    std::cout << "Multiplication: " << (mpz_e == mpz_class(e.toString())) << std::endl;

    // Division and modulus
    mpz_class mpz_f = mpz_a / mpz_b;
    mpz_class mpz_mod = mpz_a % mpz_b;
    uint64_t modulus;
    BigIntClass f = a.divmod(b, modulus);
    std::cout << "Division: " << (mpz_f == mpz_class(f.toString())) << std::endl;
    std::cout << "Modulus: " << (mpz_mod == mpz_class(std::to_string(modulus))) << std::endl;

    // Power
    mpz_class mpz_g;
    mpz_pow_ui(mpz_g.get_mpz_t(), mpz_a.get_mpz_t(), 3);
    BigIntClass g = a.pow(3);
    std::cout << "Power: " << (mpz_g == mpz_class(g.toString())) << std::endl;

    // Bitwise XOR
    mpz_class mpz_h = mpz_a ^ mpz_b;
    BigIntClass h = a ^ b;
    std::cout << "Bitwise XOR: " << (mpz_h == mpz_class(h.toString())) << std::endl;

    // Bitwise OR
    mpz_class mpz_i = mpz_a | mpz_b;
    BigIntClass i = a | b;
    std::cout << "Bitwise OR: " << (mpz_i == mpz_class(i.toString())) << std::endl;

    // Increment
    ++mpz_a;
    ++a;
    std::cout << "Increment: " << (mpz_a == mpz_class(a.toString())) << std::endl;

    // Decrement
    --mpz_a;
    --a;
    std::cout << "Decrement: " << (mpz_a == mpz_class(a.toString())) << std::endl;
}

int main()
{
    testBasic();
    testBigIntClass();
}