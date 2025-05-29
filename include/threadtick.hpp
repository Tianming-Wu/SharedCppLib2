#pragma once

#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <stdexcept>
#include <string>

typedef int64_t utick;
#define PRItick PRIi64
#define TICK_FREQ 1000000

#define TICK_TO_NS(tick) ((utick) (tick) * 1000)
#define TICK_TO_US(tick) ((utick) tick)
#define TICK_TO_MS(tick) ((utick) (tick) / 1000)
#define TICK_TO_SEC(tick) ((utick) (tick) / 1000000)
#define TICK_FROM_NS(ns) ((utick) (ns) / 1000)
#define TICK_FROM_US(us) ((utick) us)
#define TICK_FROM_MS(ms) ((utick) (ms) * 1000)
#define TICK_FROM_SEC(sec) ((utick) (sec) * 1000000)

utick tick_now() {
    struct timespec ts;
    int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (ret) {
        throw std::runtime_error("clock_gettime() failed to execute.");
    }

    return TICK_FROM_SEC(ts.tv_sec) + TICK_FROM_NS(ts.tv_nsec);
}