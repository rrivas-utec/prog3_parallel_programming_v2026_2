#pragma once
#include "catch.hpp"
#include "parallel_transform_reduce.h"
#include "support.h"

#if TEST_CASE_INDEX == 1
TEST_CASE("Q1 basico 1.00x - contrato y limites") {
    std::vector<int> v{2, 1, 3, 0};
    auto price = [](int x) { return x * 125; };
    REQUIRE(parallel_transform_reduce(v.begin(), v.end(), 10, price, std::plus<int>{}, 1) == 760);
    REQUIRE(parallel_transform_reduce(v.end(), v.end(), 71, price, std::plus<int>{}, 3) == 71);
    REQUIRE_THROWS_AS(parallel_transform_reduce(v.begin(), v.end(), 0, price, std::plus<int>{}, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(parallel_transform_reduce(v.end(), v.end(), 0, price, std::plus<int>{}, 0), std::invalid_argument);
    std::vector<int> one{9};
    REQUIRE(parallel_transform_reduce(one.begin(), one.end(), 7, price, std::plus<int>{}, 8) == 1132);
}
#elif TEST_CASE_INDEX == 2
TEST_CASE("Q1 intermedio 1.25x - forward iterators y trabajadores") {
    std::forward_list<int> v{1, 2, 3, 4, 5, 6, 7};
    for (std::size_t w : {1u, 2u, 3u, 20u}) {
        prog3_test::Visits visits;
        const auto caller = std::this_thread::get_id();
        const auto answer = parallel_transform_reduce(v.cbegin(), v.cend(), 13LL,
            [&](int x) { visits.record(); return 1LL * x * x; }, std::plus<long long>{}, w);
        REQUIRE(answer == 153);
        REQUIRE(visits.ids.size() == 7);
        REQUIRE(visits.distinct() <= std::min<std::size_t>(w, 7));
        REQUIRE(std::find(visits.ids.begin(), visits.ids.end(), caller) == visits.ids.end());
    }
    // With one element per worker, a bounded rendezvous keeps all IDs live.
    std::vector<int> items{1, 2, 3, 4};
    std::mutex m; std::condition_variable cv;
    int arrived = 0; bool timeout = false;
    prog3_test::Visits visits;
    auto value = parallel_transform_reduce(items.begin(), items.end(), 0,
        [&](int x) {
            visits.record();
            std::unique_lock lock(m); ++arrived; cv.notify_all();
            if (!cv.wait_for(lock, std::chrono::seconds(5), [&] { return arrived == 4; })) timeout = true;
            return x;
        }, std::plus<int>{}, 99);
    REQUIRE(value == 10);
    REQUIRE_FALSE(timeout);
    REQUIRE(visits.distinct() == 4);
}
#elif TEST_CASE_INDEX == 3
TEST_CASE("Q1 avanzado 1.50x - orden generico y excepciones pendientes") {
    std::list<std::string> v{"a", "B", "cc", "D", "e"};
    for (std::size_t w : {1u, 2u, 3u, 9u}) {
        auto result = parallel_transform_reduce(v.begin(), v.end(), prog3_test::Token("!"),
            [](const auto& x) { return prog3_test::Token(x); }, prog3_test::Concat{}, w);
        REQUIRE(result.text == "!aBccDe");
    }
    std::vector<int> data{0, 1};
    std::atomic<int> finished{0};
    prog3_test::pending([&](prog3_test::Gate& gate) {
        (void)parallel_transform_reduce(data.begin(), data.end(), 0,
            [&](int x) { if (x == 0) { gate.block(); ++finished; return 1; } gate.note_failure(); throw prog3_test::Fault(); },
            std::plus<int>{}, 2);
    }, true);
    REQUIRE(finished.load() == 1);
    std::atomic<int> calls{0};
    REQUIRE_THROWS_AS(parallel_transform_reduce(data.begin(), data.end(), 0,
        [&](int x) { ++calls; return x; },
        [](int, int) -> int { throw prog3_test::Fault(); }, 2), prog3_test::Fault);
    REQUIRE(calls.load() == 2);
    std::vector<int> local{1, 2, 3, 4};
    REQUIRE_THROWS_AS(parallel_transform_reduce(local.begin(), local.end(), 0,
        [](int x) { return x; }, [](int, int) -> int { throw prog3_test::Fault(); }, 2), prog3_test::Fault);
}
#elif TEST_CASE_INDEX == 4
TEST_CASE("Q1 intensivo 2.00x - oraculo y llamadas independientes") {
    std::vector<std::future<bool>> runs;
    for (int job = 0; job < 4; ++job) runs.push_back(std::async(std::launch::async, [job] {
        for (int rep = 0; rep < 5; ++rep) {
            std::vector<int> v(2053 + job * 173 + rep * 11);
            for (std::size_t i = 0; i < v.size(); ++i) v[i] = int((i * 37 + job) % 101) - 50;
            long long expected = 83;
            for (int x : v) expected += 1LL * x * x - 3;
            if (parallel_transform_reduce(v.begin(), v.end(), 83LL,
                [](int x) { return 1LL * x * x - 3; }, std::plus<long long>{}, 1 + (rep * 3 + job) % 13) != expected) return false;
        }
        return true;
    }));
    for (auto& run : runs) REQUIRE(run.get());
}
#else
#error "TEST_CASE_INDEX must be in [1,4]"
#endif
