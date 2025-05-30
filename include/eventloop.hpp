/*
    Eventloop (eventloop.hpp) introduces an event loop system into the program.

*/

#include <string>
#include <chrono>
#include "stringlist.hpp"
#include <vector>
#include <atomic>
#include <exception>

namespace evl {

// Pre definitions for definition references.
class object;
class eventloop;

// Actual class definitions
class object {
public:
    inline std::string objectName() { return m_objectName; }
    inline void setObjectName(const std::string &name) { m_objectName = name; }


private:
    std::string m_objectName;
};

class eventloop
{
public:
    eventloop(int argc, char** argv);
    ~eventloop();

    eventloop(eventloop&) = delete;
    eventloop(eventloop&&) = delete;


    static eventloop& get();

    int exec();

private:
    std::atomic<bool> m_flagRunning;

    static eventloop* shared_ptr;
};




} // namespace evl