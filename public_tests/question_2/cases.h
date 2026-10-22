#pragma once
#include "catch.hpp"
#include "parallel_copy_if.h"
#include "support.h"

#if TEST_CASE_INDEX == 1
TEST_CASE("Q2 basico 1.00x - seleccion y salida") {
    std::vector<int> v{7, 2, 9, 4, 6, 1}, out(8, -1);
    auto even = [](int x) { return x % 2 == 0; };
    auto end = parallel_copy_if(v.begin(), v.end(), out.begin(), even, 1);
    REQUIRE(end == out.begin() + 3);
    REQUIRE(out == std::vector<int>{2, 4, 6, -1, -1, -1, -1, -1});
    REQUIRE(parallel_copy_if(v.end(), v.end(), out.begin(), even, 2) == out.begin());
    REQUIRE_THROWS_AS(parallel_copy_if(v.begin(), v.end(), out.begin(), even, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(parallel_copy_if(v.end(), v.end(), out.begin(), even, 0), std::invalid_argument);
    std::vector<int> one{4}, single;
    auto next = parallel_copy_if(one.begin(), one.end(), std::back_inserter(single), even, 99);
    *next++ = 8;
    REQUIRE(single == std::vector<int>{4, 8});
}
#elif TEST_CASE_INDEX == 2
TEST_CASE("Q2 intermedio 1.25x - particiones iteradores y predicado copiable") {
    std::forward_list<std::string> v{"a", "bb", "c", "dd", "ee", "f", "gg"};
    for (std::size_t w : {1u, 2u, 3u, 20u}) {
        std::list<std::string> out;
        std::atomic<int> calls{0};
        prog3_test::Visits visits;
        parallel_copy_if(v.cbegin(), v.cend(), std::back_inserter(out),
            [&](const auto& x) { ++calls; visits.record(); return x.size() == 2; }, w);
        REQUIRE(out == std::list<std::string>{"bb", "dd", "ee", "gg"});
        REQUIRE(calls.load() == 7);
        REQUIRE(visits.distinct() <= std::min<std::size_t>(w, 7));
    }
    std::vector<int> v2{3, 1, 3, 2, 3}, out;
    parallel_copy_if(v2.begin(), v2.end(), std::back_inserter(out), [](int) { return false; }, 3);
    REQUIRE(out.empty());
    parallel_copy_if(v2.begin(), v2.end(), std::back_inserter(out), [](int) { return true; }, 3);
    REQUIRE(out == v2);
    std::set<int> ordered;
    parallel_copy_if(v2.begin(), v2.end(), std::inserter(ordered, ordered.end()), [](int) { return true; }, 2);
    REQUIRE(ordered == std::set<int>{1, 2, 3});
}
#elif TEST_CASE_INDEX == 3
TEST_CASE("Q2 avanzado 1.50x - estabilidad y fallo sin escritura") {
    std::vector<prog3_test::Token> records;
    for (int i = 0; i < 29; ++i) records.emplace_back(std::to_string(i));
    std::vector<prog3_test::Token> expected;
    for (const auto& x : records) if (std::stoi(x.text) % 3 != 1) expected.push_back(x);
    for (std::size_t w : {2u, 7u, 40u}) {
        std::vector<prog3_test::Token> out;
        parallel_copy_if(records.begin(), records.end(), std::back_inserter(out),
            [](const auto& x) { return std::stoi(x.text) % 3 != 1; }, w);
        REQUIRE(out == expected);
    }
    std::vector<int> v{0, 1}, out{91};
    std::atomic<int> finished{0};
    prog3_test::pending([&](prog3_test::Gate& gate) {
        parallel_copy_if(v.begin(), v.end(), std::back_inserter(out),
            [&](int x) { if (x == 0) { gate.block(); ++finished; return true; } gate.note_failure(); throw prog3_test::Fault(); }, 2);
    }, true);
    REQUIRE(finished.load() == 1);
    REQUIRE(out == std::vector<int>{91});
}
#elif TEST_CASE_INDEX == 4
TEST_CASE("Q2 intensivo 2.00x - cargas irregulares e independencia") {
    std::vector<std::future<bool>> runs;
    for (int job = 0; job < 4; ++job) runs.push_back(std::async(std::launch::async, [job] {
        for (int rep = 0; rep < 5; ++rep) {
            std::vector<int> v(4099 + job * 193 + rep * 7), expected, out;
            for (std::size_t i = 0; i < v.size(); ++i) v[i] = int(i * 17 % 997);
            auto pred = [=](int x) { return (x * x + job) % (rep + 2) == 0; };
            std::copy_if(v.begin(), v.end(), std::back_inserter(expected), pred);
            parallel_copy_if(v.cbegin(), v.cend(), std::back_inserter(out), pred, 1 + (rep + job * 3) % 11);
            if (out != expected) return false;
        }
        return true;
    }));
    for (auto& run : runs) REQUIRE(run.get());
}
#else
#error "TEST_CASE_INDEX must be in [1,4]"
#endif
