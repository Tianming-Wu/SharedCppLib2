#include "threadutil.hpp"

namespace threadutil {

bool thread_create(uthread *thread, thread_fn fn, const char *name, void *userdata) {
    // The thread name length is limited on some systems. Never use a name
    // longer than 16 bytes (including the final '\0')
    assert(strlen(name) <= 15);

    SDL_Thread *sdl_thread = SDL_CreateThread(fn, name, userdata);
    if (!sdl_thread) {
        return false;
    }

    thread->thread = sdl_thread;
    return true;
}

static SDL_ThreadPriority to_sdl_thread_priority(enum thread_priority priority) {
    switch (priority) {
        case THREAD_PRIORITY_TIME_CRITICAL:
#ifdef UTIL_SDL_HAS_THREAD_PRIORITY_TIME_CRITICAL
            return SDL_THREAD_PRIORITY_TIME_CRITICAL;
#else
            // fall through
#endif
        case THREAD_PRIORITY_HIGH:
            return SDL_THREAD_PRIORITY_HIGH;
        case THREAD_PRIORITY_NORMAL:
            return SDL_THREAD_PRIORITY_NORMAL;
        case THREAD_PRIORITY_LOW:
            return SDL_THREAD_PRIORITY_LOW;
        default:
            assert(!"Unknown thread priority");
            return SDL_ThreadPriority(0);
    }
}

bool thread_set_priority(enum thread_priority priority) {
    SDL_ThreadPriority sdl_priority = to_sdl_thread_priority(priority);
    int r = SDL_SetThreadPriority(sdl_priority);
    if (r) {
        logd::logd("Could not set thread priority: $1", { SDL_GetError() });
        return false;
    }

    return true;
}

void hread_join(uthread *thread, int *status) {
    SDL_WaitThread(thread->thread, status);
}

bool umutex_init(umutex *mutex) {
    SDL_mutex *sdl_mutex = SDL_CreateMutex();
    if (!sdl_mutex) {
        logd::logd_dbg();
        return false;
    }

    mutex->mutex = sdl_mutex;
#ifndef NDEBUG
    atomic_init(&mutex->locker, 0);
#endif
    return true;
}

void mutex_destroy(umutex *mutex) {
    SDL_DestroyMutex(mutex->mutex);
}

void mutex_lock(umutex *mutex) {
    // SDL mutexes are recursive, but we don't want to use recursive mutexes
    assert(!umutex_held(mutex));
    int r = SDL_LockMutex(mutex->mutex);
#ifndef NDEBUG
    if (r) {
        LOGE("Could not lock mutex: %s", SDL_GetError());
        abort();
    }

    atomic_store_explicit(&mutex->locker, thread_get_id(),
                          memory_order_relaxed);
#else
    (void) r;
#endif
}

void mutex_unlock(umutex *mutex) {
#ifndef NDEBUG
    assert(umutex_held(mutex));
    atomic_store_explicit(&mutex->locker, 0, memory_order_relaxed);
#endif
    int r = SDL_UnlockMutex(mutex->mutex);
#ifndef NDEBUG
    if (r) {
        LOGE("Could not lock mutex: %s", SDL_GetError());
        abort();
    }
#else
    (void) r;
#endif
}

thread_id thread_get_id(void) {
    return SDL_ThreadID();
}

#ifndef NDEBUG
bool
umutex_held(struct umutex *mutex) {
    thread_id locker_id =
        atomic_load_explicit(&mutex->locker, memory_order_relaxed);
    return locker_id == thread_get_id();
}
#endif

bool cond_init(ucond *cond) {
    SDL_cond *sdl_cond = SDL_CreateCond();
    if (!sdl_cond) {
        logd::logd_dbg();
        return false;
    }

    cond->cond = sdl_cond;
    return true;
}

void cond_destroy(ucond *cond) {
    SDL_DestroyCond(cond->cond);
}

void cond_wait(ucond *cond, umutex *mutex) {
    int r = SDL_CondWait(cond->cond, mutex->mutex);
#ifndef NDEBUG
    if (r) {
        LOGE("Could not wait on condition: %s", SDL_GetError());
        abort();
    }

    atomic_store_explicit(&mutex->locker, thread_get_id(),
                          memory_order_relaxed);
#else
    (void) r;
#endif
}

bool cond_timedwait(ucond *cond, umutex *mutex, utick deadline) {
    utick now = tick_now();
    if (deadline <= now) {
        return false; // timeout
    }

    // Round up to the next millisecond to guarantee that the deadline is
    // reached when returning due to timeout
    uint32_t ms = TICK_TO_MS(deadline - now + TICK_FROM_MS(1) - 1);
    int r = SDL_CondWaitTimeout(cond->cond, mutex->mutex, ms);
#ifndef NDEBUG
    if (r < 0) {
        LOGE("Could not wait on condition with timeout: %s", SDL_GetError());
        abort();
    }

    atomic_store_explicit(&mutex->locker, thread_get_id(),
                          memory_order_relaxed);
#endif
    assert(r == 0 || r == SDL_MUTEX_TIMEDOUT);
    // The deadline is reached on timeout
    assert(r != SDL_MUTEX_TIMEDOUT || tick_now() >= deadline);
    return r == 0;
}

void cond_signal(ucond *cond) {
    int r = SDL_CondSignal(cond->cond);
#ifndef NDEBUG
    if (r) {
        LOGE("Could not signal a condition: %s", SDL_GetError());
        abort();
    }
#else
    (void) r;
#endif
}

void cond_broadcast(ucond *cond) {
    int r = SDL_CondBroadcast(cond->cond);
#ifndef NDEBUG
    if (r) {
        LOGE("Could not broadcast a condition: %s", SDL_GetError());
        abort();
    }
#else
    (void) r;
#endif
}

};