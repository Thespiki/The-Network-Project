#include "simulation/EventQueue.h"
#include "simulation/NetworkScheduler.h"

#include <catch2/catch_test_macros.hpp>

using namespace tnp;
using namespace tnp::sim;

namespace {

DeviceTimerExpiry timer(core::TimerId id) {
    return DeviceTimerExpiry{core::DeviceId::generate(), id};
}

} // namespace

TEST_CASE("Events come out in time order", "[simulation][queue]") {
    EventQueue queue;

    queue.push(SimTime{milliseconds(30)}, timer(3));
    queue.push(SimTime{milliseconds(10)}, timer(1));
    queue.push(SimTime{milliseconds(20)}, timer(2));

    CHECK(queue.size() == 3);

    for (const core::TimerId expected : {1u, 2u, 3u}) {
        const auto event = queue.pop();
        REQUIRE(event.has_value());
        CHECK(std::get<DeviceTimerExpiry>(event->payload).timer == expected);
    }
    CHECK(queue.empty());
    CHECK_FALSE(queue.pop().has_value());
}

TEST_CASE("Events at the same instant keep insertion order", "[simulation][queue]") {
    EventQueue queue;
    const SimTime instant{milliseconds(5)};

    for (core::TimerId id = 1; id <= 5; ++id) queue.push(instant, timer(id));

    // This tie-break is what makes a run reproducible.
    for (core::TimerId expected = 1; expected <= 5; ++expected) {
        const auto event = queue.pop();
        REQUIRE(event.has_value());
        CHECK(std::get<DeviceTimerExpiry>(event->payload).timer == expected);
    }
}

TEST_CASE("Cancelled timers never surface", "[simulation][queue]") {
    EventQueue queue;

    queue.push(SimTime{milliseconds(10)}, timer(1));
    queue.push(SimTime{milliseconds(20)}, timer(2));
    queue.push(SimTime{milliseconds(30)}, timer(3));

    queue.cancelTimer(2);

    auto first = queue.pop();
    REQUIRE(first.has_value());
    CHECK(std::get<DeviceTimerExpiry>(first->payload).timer == 1);

    auto second = queue.pop();
    REQUIRE(second.has_value());
    CHECK(std::get<DeviceTimerExpiry>(second->payload).timer == 3);

    CHECK(queue.empty());
}

TEST_CASE("Cancelling the earliest event keeps the reported next time honest",
          "[simulation][queue]") {
    EventQueue queue;
    queue.push(SimTime{milliseconds(10)}, timer(1));
    queue.push(SimTime{milliseconds(50)}, timer(2));

    queue.cancelTimer(1);
    CHECK(queue.nextEventTime() == SimTime{milliseconds(50)});
    CHECK_FALSE(queue.empty());
}

TEST_CASE("The scheduler only advances when work is due", "[simulation][scheduler]") {
    NetworkScheduler scheduler;
    CHECK(scheduler.now() == simTimeZero());

    scheduler.schedule(milliseconds(10), timer(1));
    scheduler.schedule(milliseconds(50), timer(2));

    CHECK_FALSE(scheduler.nextDueEvent(SimTime{milliseconds(5)}).has_value());
    CHECK(scheduler.now() == simTimeZero()); // nothing was due, so time did not move

    const auto due = scheduler.nextDueEvent(SimTime{milliseconds(20)});
    REQUIRE(due.has_value());
    CHECK(scheduler.now() == SimTime{milliseconds(10)});

    CHECK_FALSE(scheduler.nextDueEvent(SimTime{milliseconds(20)}).has_value());
    CHECK(scheduler.hasPendingEvents());
}

TEST_CASE("The scheduler clock never runs backwards", "[simulation][scheduler]") {
    NetworkScheduler scheduler;

    scheduler.advanceClockTo(SimTime{milliseconds(100)});
    CHECK(scheduler.now() == SimTime{milliseconds(100)});

    scheduler.advanceClockTo(SimTime{milliseconds(50)});
    CHECK(scheduler.now() == SimTime{milliseconds(100)});

    // An event scheduled with a negative delay lands now, not in the past.
    scheduler.schedule(milliseconds(-10), timer(1));
    CHECK(scheduler.nextEventTime() >= SimTime{milliseconds(100)});

    scheduler.reset();
    CHECK(scheduler.now() == simTimeZero());
    CHECK_FALSE(scheduler.hasPendingEvents());
}
