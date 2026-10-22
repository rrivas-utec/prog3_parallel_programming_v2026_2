#ifndef PROG3_PARALLEL_TEST_SUPPORT_H
#define PROG3_PARALLEL_TEST_SUPPORT_H
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <forward_list>
#include <functional>
#include <future>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace prog3_test {
using namespace std::chrono_literals;
struct Fault : std::runtime_error { Fault() : std::runtime_error("worker failure") {} };
struct Token {
    std::string text;
    Token() = delete;
    explicit Token(std::string s) : text(std::move(s)) {}
    friend bool operator==(const Token&, const Token&) = default;
};
struct Concat {
    Token operator()(const Token& a, const Token& b) const {
        return Token(a.text + b.text);
    }
};

// All mutable instrumentation is synchronized. Callbacks never invoke Catch2.
struct Gate {
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false, released = false, timed_out = false, failure = false;
    void block() {
        std::unique_lock lock(mutex);
        entered = true;
        changed.notify_all();
        if (!changed.wait_for(lock, 10s, [&] { return released; })) timed_out = true;
    }
    bool await_entry() {
        std::unique_lock lock(mutex);
        return changed.wait_for(lock, 5s, [&] { return entered; });
    }
    void release() {
        std::lock_guard lock(mutex);
        released = true;
        changed.notify_all();
    }
    void note_failure() {
        std::lock_guard lock(mutex);
        failure = true;
        changed.notify_all();
    }
    void await_failure() {
        std::unique_lock lock(mutex);
        // Another block may fail while this one is pending. A single-block
        // partition remains legal where the statement only gives an upper bound.
        changed.wait_for(lock, 2s, [&] { return failure; });
    }
};

// Runs a call with a callback held pending. Release happens before any REQUIRE.
// Exceptions are transported to the test thread by the outer future as well.
template<class Call>
void pending(Call call, bool observe_failure = false) {
    Gate gate;
    auto result = std::async(std::launch::async, [&] { call(gate); });
    const bool entered = gate.await_entry();
    if (entered && observe_failure) gate.await_failure();
    const bool waiting = result.wait_for(0ms) == std::future_status::timeout;
    gate.release();
    bool fault = false;
    try { result.get(); }
    catch (const Fault&) { fault = true; }
    REQUIRE(entered);
    REQUIRE(waiting);
    REQUIRE_FALSE(gate.timed_out);
    REQUIRE(fault);
}

struct Visits {
    std::mutex mutex;
    std::vector<std::thread::id> ids;
    void record() {
        std::lock_guard lock(mutex);
        ids.push_back(std::this_thread::get_id());
    }
    [[nodiscard]] std::size_t distinct() const { return std::set<std::thread::id>(ids.begin(), ids.end()).size(); }
};

}
#endif
