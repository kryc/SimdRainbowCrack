//
//  reductionperf.cpp
//  RainbowCrack-
//
//  Created by Kryc on 21/02/2024.
//  Copyright © 2024 Kryc. All rights reserved.
//

#include <iostream>
#include <gmpxx.h>
#include <chrono>

#include "Reduce.hpp"
#include "simdhash.h"
#include "WordGenerator.hpp"

#define MIN 12
#define MAX 12

void TestReducer(
    FILE* Random,
    Reducer* Reducer
)
{
    using std::chrono::high_resolution_clock;
    using std::chrono::duration_cast;
    using std::chrono::duration;
    using std::chrono::milliseconds;

    uint8_t hash[SHA1_SIZE];
    char word[MAX + 1];
    
    const size_t num_iterations = 5000000;
    std::vector<double> execution_times; // To store execution times

    for (size_t i = 0; i < num_iterations; i++)
    {
        auto t1 = high_resolution_clock::now();
        fread(hash, sizeof(hash), sizeof(*hash), Random);
        (void)Reducer->Reduce(word, MAX, hash, 0);
        // word[length] = '\0';
        // std::cout << word << " (" << length << ")" << std::endl;
        auto t2 = high_resolution_clock::now();
        // Calculate execution time in milliseconds as a double.
        duration<double, std::milli> ms_double = t2 - t1;
        execution_times.push_back(ms_double.count());
    }
    
    // Calculate rolling average.
    double total_time = 0.0;
    for (double time : execution_times) {
        total_time += time;
    }
    double rolling_average = total_time / num_iterations;

    // Find min and max execution times.
    double min_time = *std::min_element(execution_times.begin(), execution_times.end());
    double max_time = *std::max_element(execution_times.begin(), execution_times.end());

    std::cout << "  Avg execution time: " << rolling_average << "ms\n";
    std::cout << "  Min execution time: " << min_time << "ms\n";
    std::cout << "  Max execution time: " << max_time << "ms\n";
}


int main(
    int argc,
    char* argv[]
)
{
    // Open a handle to /dev/urandom
    FILE* fh = fopen("/dev/urandom", "r");
    if (fh == nullptr)
    {
        std::cerr << "Unable to open handle to random" << std::endl;
        return 1;
    }

    std::cout << "BasicModuloReducer" << std::endl;
    BasicModuloReducer bmr(MIN, MAX, SHA1_SIZE, ASCII);
    TestReducer(
        fh,
        &bmr
    );

    std::cout << "ModuloReducer" << std::endl;
    ModuloReducer mr(MIN, MAX, SHA1_SIZE, ASCII);
    TestReducer(
        fh,
        &mr
    );

    std::cout << "BytewiseReducer" << std::endl;
    BytewiseReducer bwr(MIN, MAX, SHA1_SIZE, ASCII);
    TestReducer(
        fh,
        &bwr
    );

    fclose(fh);
}