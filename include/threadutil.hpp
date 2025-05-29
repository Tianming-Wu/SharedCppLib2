#pragma once

#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
// #include <string.h>
#include <SDL2/SDL_thread.h>

#include "threadtick.hpp"
#include "logd.hpp"

// typedef struct SDL_Thread SDL_Thread;
// typedef struct SDL_mutex SDL_mutex;
// typedef struct SDL_cond SDL_cond;

#ifndef NDEBUG
#define NDEBUG
#endif

typedef int thread_fn(void *);
typedef unsigned thread_id;
#ifndef NDEBUG
typedef atomic_uint atomic_thread_id;
#endif

namespace threadutil {

struct uthread {
    SDL_Thread *thread;
};

enum thread_priority {
    THREAD_PRIORITY_LOW,
    THREAD_PRIORITY_NORMAL,
    THREAD_PRIORITY_HIGH,
    THREAD_PRIORITY_TIME_CRITICAL,
};

struct umutex {
    SDL_mutex *mutex;
#ifndef NDEBUG
    atomic_thread_id locker;
#endif
};

struct ucond {
    SDL_cond *cond;
};

bool thread_create(uthread *thread, thread_fn fn, const char *name, void *userdata);
void thread_join(uthread *thread, int *status);
bool thread_set_priority(enum thread_priority priority);
bool mutex_init(umutex *mutex);
void mutex_destroy(umutex *mutex);
void mutex_lock(umutex *mutex);
void mutex_unlock(umutex *mutex);
thread_id thread_get_id(void);

#ifndef NDEBUG
bool mutex_held(struct mutex *mutex);
# define mutex_assert(mutex) assert(mutex_held(mutex))
#else
# define mutex_assert(mutex)
#endif

bool cond_init(ucond *cond);
void cond_destroy(ucond *cond);
void cond_wait(ucond *cond, umutex *mutex);

// return true on signaled, false on timeout
bool cond_timedwait(ucond *cond, umutex *mutex, utick deadline);

void cond_signal(ucond *cond);
void cond_broadcast(ucond *cond);

};