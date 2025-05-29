#pragma once

#include <atomic>
#include <thread>
#include <vector>
#include <functional>

template<typename TF>
class Tasker
{
public:
    typedef std::function<TF> func_type;
    typedef int prog_type;

    enum Type {
        SingleShot, Loop
    };
    
protected:
    std::atomic<prog_type> progress {0};
    std::atomic<bool> complete {false};
    std::atomic<bool> stop_flag {false};
    std::thread worker;
    
    func_type &task;
    Type type;
    
public:
    Tasker() = delete;
    Tasker(const Tasker&) = delete;
    Tasker(Tasker&&) = delete;
    Tasker& operator=(const Tasker&) = delete;
    Tasker& operator=(Tasker&&) = delete;

    Tasker(func_type &task, Type type = Singleshot)
        :task(task), type(type)
    {}
    
    ~Tasker() {
        stop();
        if (worker.joinable()) {
            worker.join();
        }
    }

    void start() {
        stop_flag.store(false);
        worker = std::thread([this]() {
            if(type == SingleShot) {
                task();
            } else if(type == Loop) {
                while(!stop_flag.load(std::memory_order_acquire)) {
                    task();
                }
            }

            progress.store(100, std::memory_order_release);
            complete.store(true, std::memory_order_release);
        });
    }
    
    prog_type progress() const {
        return progress.load(std::memory_order_relaxed);
    }

    bool complete() const {
        return complete.load(std::memory_order_acquire);
    }
    
    void stop() {
        stop_flag.store(true, std::memory_order_release);
    }
    
    static void exec(package pack) {
        
    }
};

using Taskerm = Tasker<int(void*)>;
