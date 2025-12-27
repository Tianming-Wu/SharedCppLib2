/*
    Function cache library.

    Allows caching of function results based on their input parameters.


example:
    int expensive_function(int x, std::string y) {
        // some expensive computation
    }

    general_cache<int,int,std::string> cached_expensive(expensive_function);

    int result = cached_expensive(42, "hello"); // computes and caches result
    int cached_result = cached_expensive(42, "hello"); // retrieves cached result

*/

#pragma once
#include <functional>
#include <unordered_map>
#include <tuple>

enum cache_mode {
    cache_construct, cache_call
};


// This class supports parameters, and assumes that the function always returns the same output for the same inputs.

template<typename ReturnType, typename... Args>
class general_cache
{
public:
    explicit general_cache(std::function<ReturnType(Args...)> func)
        : cached_function(func) {}

    ReturnType operator()(Args... args)
    {
        auto args_tuple = std::make_tuple(args...);
        auto it = cache.find(args_tuple);
        if (it != cache.end()) {
            return it->second;
        } else {
            ReturnType result = cached_function(args...);
            cache[args_tuple] = result;
            return result;
        }
    }

    void count() const {
        return cache.size();
    }

    void release() {
        cache.clear();
    }

private:
    std::function<ReturnType(Args...)> cached_function;
    std::unordered_map<std::tuple<Args...>, ReturnType> cache;
};



// These classes does not support parameters.

template<typename ReturnType>
class context_cache
{
public:
    explicit context_cache(std::function<ReturnType()> func, cache_mode mode = cache_call)
        : cached_function(func), mode(mode), is_cached(false) 
    {
        if (mode == cache_construct) {
            cached_value = cached_function();
            is_cached = true;
        }
    }

    ReturnType operator()()
    {
        if (mode == cache_call && !is_cached) {
            cached_value = cached_function();
            is_cached = true;
        }
        return cached_value;
    }

    bool available() const {
        return is_cached;
    }

    void cache() {
        if(mode != cache_call) throw std::runtime_error("Trying to cache a non-call-time cached function");
        if(!is_cached) {
            cached_value = cached_function();
            is_cached = true;
        }
    }

    void release() {
        if(mode != cache_call) throw std::runtime_error("Trying to release a non-call-time cached function");
        is_cached = false;
    }

private:
    std::function<ReturnType()> cached_function;
    cache_mode mode;
    bool is_cached;
    ReturnType cached_value;
};

template<typename ReturnType>
class global_cache
{
public:
    explicit global_cache(std::function<ReturnType()> func)
        : cached_function(func), is_cached(false) {}

    ReturnType operator()()
    {
        if (!is_cached) {
            cached_value = cached_function();
            is_cached = true;
        }
        return cached_value;
    }

    bool available() const {
        return is_cached;
    }

    void cache() {
        if(!is_cached) {
            cached_value = cached_function();
            is_cached = true;
        }
    }

    void release() {
        is_cached = false;
    }

private:
    std::function<ReturnType()> cached_function;
    bool is_cached;
    ReturnType cached_value;
};