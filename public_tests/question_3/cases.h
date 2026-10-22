#pragma once
#include "catch.hpp"
#include "parallel_inclusive_scan.h"
#include "support.h"

#if TEST_CASE_INDEX == 1
TEST_CASE("Q3 basico 1.00x - prefijos limites e iterador devuelto") {
    std::vector<int> v{100, 0, 75, 25, 300}, out(7, -1);
    auto end = parallel_inclusive_scan(v.begin(), v.end(), out.begin(), std::plus<int>{}, 1);
    REQUIRE(end == out.begin() + 5);
    REQUIRE(out == std::vector<int>{100, 100, 175, 200, 500, -1, -1});
    REQUIRE(parallel_inclusive_scan(v.end(), v.end(), out.begin(), std::plus<int>{}, 2) == out.begin());
    REQUIRE_THROWS_AS(parallel_inclusive_scan(v.begin(), v.end(), out.begin(), std::plus<int>{}, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(parallel_inclusive_scan(v.end(), v.end(), out.begin(), std::plus<int>{}, 0), std::invalid_argument);
    std::vector<int> one{7}, single;
    auto next = parallel_inclusive_scan(one.begin(), one.end(), std::back_inserter(single), std::plus<int>{}, 99);
    *next++ = 8;
    REQUIRE(single == std::vector<int>{7, 8});
}
#elif TEST_CASE_INDEX == 2
TEST_CASE("Q3 intermedio 1.25x - particiones y forward iterators") {
    std::forward_list<int> v{3, -1, 0, 8, -4, 2, 7};
    for (std::size_t t : {1u, 2u, 3u, 20u}) {
        std::list<int> out;
        parallel_inclusive_scan(v.cbegin(), v.cend(), std::back_inserter(out), std::plus<int>{}, t);
        REQUIRE(out == std::list<int>{3, 2, 2, 10, 6, 8, 15});
    }
    // In-place output is safe: the complete input is collected before writing.
    std::vector<int> in_place{2, 3, 5, 7, 11};
    auto end = parallel_inclusive_scan(in_place.begin(), in_place.end(), in_place.begin(), std::plus<int>{}, 3);
    REQUIRE(end == in_place.end());
    REQUIRE(in_place == std::vector<int>{2, 5, 10, 17, 28});
}
#elif TEST_CASE_INDEX == 3
TEST_CASE("Q3 avanzado 1.50x - no conmutatividad y fallo sin escritura") {
    std::list<prog3_test::Token> v;
    for (const auto* s : {"a", "B", "cc", "D", "e", "FF", "g"}) v.emplace_back(s);
    std::vector<prog3_test::Token> expected;
    std::string prefix;
    for (const auto& x : v) { prefix += x.text; expected.emplace_back(prefix); }
    for (std::size_t t : {1u, 2u, 3u, 10u}) {
        std::vector<prog3_test::Token> out;
        parallel_inclusive_scan(v.begin(), v.end(), std::back_inserter(out), prog3_test::Concat{}, t);
        REQUIRE(out == expected);
    }
    std::vector<int> data{1, 2, 3, 4, 5, 6, 7, 8}, out{91};
    // tasks=1 guarantees the local phase calls op. The callback is pending
    // until released by the test thread; no scheduling assumption is needed.
    prog3_test::pending([&](prog3_test::Gate& gate) {
        parallel_inclusive_scan(data.begin(), data.end(), std::back_inserter(out),
            [&](int, int) -> int { gate.block(); throw prog3_test::Fault(); }, 1);
    });
    REQUIRE(out == std::vector<int>{91});
    std::atomic<int> first{0}, finished{0};
    prog3_test::pending([&](prog3_test::Gate& gate) {
        parallel_inclusive_scan(data.begin(), data.end(), std::back_inserter(out),
            [&](int a, int b) {
                if (first.fetch_add(1) == 0) { gate.block(); ++finished; return a + b; }
                gate.note_failure();
                throw prog3_test::Fault();
            }, 2);
    }, true);
    REQUIRE(finished.load() == 1);
    REQUIRE(out == std::vector<int>{91});
    // A failure in any phase must preserve dest, including coordinator work.
    for (std::size_t t : {2u, 8u}) {
        REQUIRE_THROWS_AS(parallel_inclusive_scan(data.begin(), data.end(), std::back_inserter(out),
            [](int, int) -> int { throw prog3_test::Fault(); }, t), prog3_test::Fault);
        REQUIRE(out == std::vector<int>{91});
    }
    // A pure concatenation in phase one and the coordinator, but a controlled
    // fault on async adjustment. No particular block sizes are assumed. A
    // legal one-block strategy has no offset and therefore completes normally.
    std::vector<std::string> text{"a", "b", "c", "d", "e", "f", "g", "h"};
    std::vector<std::string> written{"sentinel"};
    const auto coordinator = std::this_thread::get_id();
    std::atomic<bool> totals_seen{false}, adjusting{false};
    bool fault = false;
    try {
        parallel_inclusive_scan(text.begin(), text.end(), std::back_inserter(written),
            [&](const std::string& a, const std::string& b) {
                if (std::this_thread::get_id() == coordinator) totals_seen.store(true);
                else if (totals_seen.load()) { adjusting.store(true); throw prog3_test::Fault(); }
                return a + b;
            }, 2);
    } catch (const prog3_test::Fault&) { fault = true; }
    if (adjusting.load()) {
        REQUIRE(fault);
        REQUIRE(written == std::vector<std::string>{"sentinel"});
    } else {
        REQUIRE_FALSE(fault);
        REQUIRE(written == std::vector<std::string>{"sentinel", "a", "ab", "abc", "abcd", "abcde", "abcdef", "abcdefg", "abcdefgh"});
    }
}
#elif TEST_CASE_INDEX == 4
TEST_CASE("Q3 intensivo 2.00x - oraculo cargas y simultaneidad") {
    std::vector<std::future<bool>> runs;
    for (int job = 0; job < 4; ++job) runs.push_back(std::async(std::launch::async, [job] {
        for (int rep = 0; rep < 5; ++rep) {
            std::vector<long long> v(3079 + job * 137 + rep * 13), expected, out;
            for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<long long>((i * 19 + job) % 101) - 50;
            std::inclusive_scan(v.begin(), v.end(), std::back_inserter(expected), std::plus<long long>{});
            parallel_inclusive_scan(v.cbegin(), v.cend(), std::back_inserter(out), std::plus<long long>{}, 1 + (job + rep * 3) % 11);
            if (out != expected) return false;
        }
        return true;
    }));
    for (auto& run : runs) REQUIRE(run.get());
    std::list<std::string> parts;
    std::vector<std::string> expected, out;
    std::string total;
    for (int i = 0; i < 257; ++i) { parts.push_back(std::to_string(i) + ";"); total += parts.back(); expected.push_back(total); }
    parallel_inclusive_scan(parts.cbegin(), parts.cend(), std::back_inserter(out), std::plus<std::string>{}, 13);
    REQUIRE(out == expected);
}
#else
#error "TEST_CASE_INDEX must be in [1,4]"
#endif
