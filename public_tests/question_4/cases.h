#pragma once
#include "catch.hpp"
#include "parallel_find_if.h"
#include "support.h"

#if TEST_CASE_INDEX == 1
TEST_CASE("Q4 basico 1.00x - primera coincidencia y limites") {
    std::vector<int> v{18, 25, 21, 31};
    auto above = [](int x) { return x > 24; };
    REQUIRE(parallel_find_if(v.begin(), v.end(), above, 1) == v.begin() + 1);
    REQUIRE(parallel_find_if(v.end(), v.end(), above, 2) == v.end());
    REQUIRE(parallel_find_if(v.begin(), v.end(), [](int) { return false; }, 3) == v.end());
    REQUIRE(parallel_find_if(v.begin(), v.end(), [](int) { return true; }, 3) == v.begin());
    REQUIRE_THROWS_AS(parallel_find_if(v.begin(), v.end(), above, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(parallel_find_if(v.end(), v.end(), above, 0), std::invalid_argument);
    std::vector<int> one{9};
    REQUIRE(parallel_find_if(one.begin(), one.end(), [](int x) { return x == 9; }, 99) == one.begin());
    REQUIRE(parallel_find_if(one.begin(), one.end(), [](int x) { return x == 0; }, 99) == one.end());
}
#elif TEST_CASE_INDEX == 2
TEST_CASE("Q4 intermedio 1.25x - repartos forward iterators y extremos") {
    const std::forward_list<std::string> v{"ok", "ok", "error", "ok", "error", "ok", "end"};
    for (std::size_t t : {1u, 2u, 3u, 20u}) {
        REQUIRE(parallel_find_if(v.begin(), v.end(), [](const auto& x) { return x == "error"; }, t) == std::next(v.begin(), 2));
        REQUIRE(parallel_find_if(v.begin(), v.end(), [](const auto& x) { return x == "end"; }, t) == std::next(v.begin(), 6));
        std::atomic<int> calls{0}; prog3_test::Visits visits;
        auto end = parallel_find_if(v.begin(), v.end(), [&](const auto&) { ++calls; visits.record(); return false; }, t);
        REQUIRE(end == v.end());
        REQUIRE(calls.load() == 7);
        REQUIRE(visits.distinct() <= std::min<std::size_t>(t, 7));
    }
}
#elif TEST_CASE_INDEX == 3
TEST_CASE("Q4 avanzado 1.50x - orden de rango y excepciones pendientes") {
    std::vector<int> v{0, 1, 2, 3, 4, 5};
    prog3_test::Gate gate;
    auto result = std::async(std::launch::async, [&] {
        return parallel_find_if(v.begin(), v.end(), [&](int x) {
            if (x == 0) gate.block();
            return x == 0 || x >= 3;
        }, 3);
    });
    const bool entered = gate.await_entry();
    const bool waiting = result.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout;
    gate.release();
    auto answer = result.get();
    REQUIRE(entered);
    REQUIRE(waiting);
    REQUIRE_FALSE(gate.timed_out);
    REQUIRE(answer == v.begin());
    // No matches: every element must be searched, independently of partition.
    std::atomic<int> finished{0};
    std::vector<int> bad{0, 1};
    prog3_test::pending([&](prog3_test::Gate& pending) {
        (void)parallel_find_if(bad.begin(), bad.end(), [&](int x) {
            if (x == 0) { pending.block(); ++finished; return false; }
            pending.note_failure();
            throw prog3_test::Fault();
        }, 2);
    }, true);
    REQUIRE(finished.load() == 1);
    // Every started task must be collected, even after a candidate match.
    std::atomic<bool> threw{false};
    bool propagated = false;
    try {
        auto found = parallel_find_if(bad.begin(), bad.end(), [&](int x) {
            if (x == 0) return true;
            threw.store(true); throw prog3_test::Fault();
        }, 2);
        REQUIRE(found == bad.begin());
    } catch (const prog3_test::Fault&) { propagated = true; }
    REQUIRE(propagated == threw.load());
    REQUIRE_THROWS_AS(parallel_find_if(bad.begin(), bad.end(),
        [](int) -> bool { throw prog3_test::Fault(); }, 2), prog3_test::Fault);
}
#elif TEST_CASE_INDEX == 4
TEST_CASE("Q4 intensivo 2.00x - oraculo irregular y llamadas independientes") {
    std::vector<std::future<bool>> runs;
    for (int job = 0; job < 4; ++job) runs.push_back(std::async(std::launch::async, [job] {
        for (int rep = 0; rep < 6; ++rep) {
            std::vector<int> v(5003 + job * 179 + rep * 17);
            for (std::size_t i = 0; i < v.size(); ++i) v[i] = int((i * 31 + job) % 1009);
            auto pred = [=](int x) { return rep % 3 == 0 ? x == -1 : x % (97 + job) == rep; };
            auto expected = std::find_if(v.cbegin(), v.cend(), pred);
            if (parallel_find_if(v.cbegin(), v.cend(), pred, 1 + (job * 3 + rep) % 13) != expected) return false;
        }
        return true;
    }));
    for (auto& run : runs) REQUIRE(run.get());
    std::list<int> v(701, 0); v.back() = 1;
    REQUIRE(parallel_find_if(v.begin(), v.end(), [](int x) { return x == 1; }, 19) == std::prev(v.end()));
}
#else
#error "TEST_CASE_INDEX must be in [1,4]"
#endif
