#include <iostream>
#include <gmpxx.h>
#include <cstdint>

int main()
{
    mpz_class c;
    uint8_t bytes[4] = {0x7f, 0xff, 0xff, 0xff};
    uint16_t words[2] = {0x7fff, 0xffff};

    mpz_import(c.get_mpz_t(), 4, 1, sizeof(uint8_t), 0, 0, &bytes[0]);

    mpz_import(c.get_mpz_t(), 2, 1, sizeof(uint16_t), 0, 0, &words[0]);

    auto x = c.get_ui();
    printf("%lX\n", x);
}