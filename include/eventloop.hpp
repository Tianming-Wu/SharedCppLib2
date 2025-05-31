/*
    Eventloop (eventloop.hpp) introduces an event loop system into the program.

    class example:

    class myObject : public evl::object
    {
    public:
        myObject() : evl::object(), sigstr(this) {
            connect(this, &sigstr, this, &myObject::slotstr);
        
            sigstr.emit("Hello world"!);
        }

        evl::evl_signal<const std::string&> sigstr;

        void slotstr(const std::string& str) { std::cout << str; }
    }
*/
#pragma once
#include <string>
#include <chrono>
#include "stringlist.hpp"
#include <vector>
#include <deque>
#include <atomic>
#include <functional>
#include <exception>
#include <thread>
#include <memory>

#include "evl_macros.hpp"

namespace evl {

// Pre definitions for definition references.
class object;
class eventloop;
class meta;
template<typename... Args> class evl_signal;

// Type definitions
using signal_t = std::shared_ptr<void>;
using slot_t   = std::shared_ptr<void>;

// Actual class definitions
class meta {
public:
    struct signal_store {
        object* sender;   signal_t signal;
        object* receiver; slot_t slot;
        inline bool operator==(const signal_store& c) const {
            return (sender == c.sender) && (signal == c.signal) && (receiver == c.receiver) && (slot == c.slot);
        }
        inline bool operator!=(const signal_store& c) const {
            return (sender != c.sender) || (signal != c.signal) || (receiver != c.receiver) || (slot != c.slot);
        }
    };
    // struct signal_action {
    //     object* sender; signal_t* signal; void* param;
    // };

    // inline static bool compatible(const signal_store& store, const signal_action& action) {
    //     return (store.receiver == action.sender) && (store.signal == action.signal);
    // }
};

class eventloop
{
public:
    eventloop(int argc, char** argv);
    ~eventloop();

    eventloop(eventloop&) = delete;
    eventloop(eventloop&&) = delete;

    static eventloop& get();
    void addChildren(object* children);
    bool removeChildren(object *children);

    void connect(meta::signal_store store);

    int exec();
    static void exit(int exitcode);

    template<typename... Args>
    void metaCall(object* sender, evl_signal<Args...>* signal, std::tuple<Args...> tuple) {
        for(const meta::signal_store& store : m_signalStore) {
            // match corrent senders and signals
            if(store.sender == sender && *std::static_pointer_cast<evl_signal<Args...>*>(store.signal) == signal) {
                auto callable = *std::static_pointer_cast<std::function<void(Args...)>>(store.slot);
                // push into the queue to execute later.
                m_signalExec.push_back([callable, tuple] {
                    std::apply(callable, tuple);
                });
            }
        }
    }

private:
    std::stringlist m_args;
    std::atomic<bool> m_flagRunning;
    std::atomic<int> m_exitcode;

    static eventloop* shared_ptr;
    std::vector<object*> m_objects;

    std::vector<meta::signal_store> m_signalStore;

    std::deque<std::function<void(void)>> m_signalExec;

};

template<typename ...Args>
class evl_signal {
public:
    
    evl_signal(object* owner)
        : owner(owner)
    {}

    bool operator==(const evl_signal& other) const {
        return this == &other;
    }

    // template<typename ...Args>
    void emit(Args... args) {
        eventloop::get().metaCall(owner, this, std::tuple<Args...>(args...));
    }

private:
    object* owner;

    friend object;
    friend eventloop;
};

class object {
public:
    object(object* parent = nullptr);
    ~object();

    inline std::string objectName() { return m_objectName; }
    inline void setObjectName(const std::string &name) { m_objectName = name; }

    // void connect(object* sender, signal_t* signal, object* receiver, slot_t* slot);
    // void disconnect(object* sender, signal_t* signal, object* receiver, slot_t* slot);

protected:
    // template<typename... Args, typename Receiver, typename... SlotArgs>
    // static void connect(object* sender, evl_signal<Args...>* signal, object* receiver, void (Receiver::*slot)(SlotArgs...)) {
    //     if(signal->owner != sender) throw std::runtime_error("connect(): Signal doesn't exist in sender");

    //     // build the template call function
    //     auto callable = std::make_shared<std::function<void(Args...)>>(
    //         [receiver, slot](Args... args) {
    //             if constexpr (sizeof...(SlotArgs) == 0) {
    //                 (reinterpret_cast<Receiver*>(receiver)->*slot)();
    //             } else {
    //                 auto args_tuple = std::forward_as_tuple(args...);
    //                 if constexpr (sizeof...(SlotArgs) > 0) {
    //                     std::apply([receiver, slot](auto&&... usedArgs) {
    //                         (reinterpret_cast<Receiver*>(receiver)->*slot)(
    //                             std::forward<decltype(usedArgs)>(usedArgs)...);
    //                     }, std::tuple<SlotArgs...>(
    //                         std::get<SlotArgs>(args_tuple)...));
    //                 }
    //             }
    //         }
    //     );

    //     // store and load it to eventloop
    //     meta::signal_store store {
    //         .sender = sender,
    //         .signal = std::static_pointer_cast<void>(std::make_shared<evl_signal<Args...>*>(signal)),
    //         .receiver = receiver,
    //         .slot = std::static_pointer_cast<void>(callable)
    //     };
    //     eventloop::get().connect(store);
    // }

    template<typename... Args, typename Receiver, typename... SlotArgs>
    static void connect(object* sender, evl_signal<Args...>* signal, 
                    object* receiver, void (Receiver::*slot)(SlotArgs...)) {
        if(signal->owner != sender) 
            throw std::runtime_error("connect(): Signal doesn't exist in sender");
        
        auto callable = std::make_shared<std::function<void(Args...)>>(
            [receiver, slot](Args... args) {
                if constexpr (sizeof...(SlotArgs) == 0) {
                    // 槽函数没有参数
                    (reinterpret_cast<Receiver*>(receiver)->*slot)();
                } else {
                    // 将参数打包成元组
                    auto args_tuple = std::make_tuple(std::forward<Args>(args)...);
                    
                    // 使用索引序列提取前N个参数
                    [&]<size_t... Is>(std::index_sequence<Is...>) {
                        (reinterpret_cast<Receiver*>(receiver)->*slot)(
                            std::get<Is>(args_tuple)...
                        );
                    }(std::make_index_sequence<sizeof...(SlotArgs)>{});
                }
            }
        );

        meta::signal_store store {
            .sender = sender,
            .signal = std::static_pointer_cast<void>(std::make_shared<evl_signal<Args...>*>(signal)),
            .receiver = receiver,
            .slot = std::static_pointer_cast<void>(callable)
        };
        eventloop::get().connect(store);
    }

    void addChildren(object* children);
    bool removeChildren(object *children);

private:
    object* m_parent;
    std::vector<object*> m_childrens;
    std::string m_objectName;
};





} // namespace evl
