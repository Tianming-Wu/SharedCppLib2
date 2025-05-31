#include "eventloop.hpp"

namespace evl {

eventloop* eventloop::shared_ptr = nullptr;

eventloop::eventloop(int argc, char** argv)
    : m_args(argc, argv)
{
    if(shared_ptr != nullptr) {
        throw std::runtime_error("Cannot construct multiple eventloop instances");
    }

    shared_ptr = this;
}

eventloop::~eventloop() {
    for(object* obj : m_objects) {
        if(obj == nullptr) continue;
        delete []obj;
    }
}

eventloop& eventloop::get() {
    if(shared_ptr != nullptr)
        return *shared_ptr;
    else throw std::runtime_error("Called eventloop::get() before an instance is actually constructed");
}

void eventloop::addChildren(object* children) {
    m_objects.push_back(children);
}

bool eventloop::removeChildren(object *children) {
    std::vector<object*>::iterator it = m_objects.begin();
    while(it++ != m_objects.end()) {
        if((*it) == children) {
            m_objects.erase(it);
            return true;
        }
    }
    return false;
}

void eventloop::connect(meta::signal_store store) {
    m_signalStore.push_back(store);
}

int eventloop::exec() {
    m_flagRunning.store(true);
    m_exitcode.store(0);

    while(m_flagRunning.load()) {
        if(!m_signalExec.empty()) {
            auto signalExec = m_signalExec.front();
            m_signalExec.pop_front();

            signalExec();
        }
        std::this_thread::yield();
    }

    return m_exitcode.load();
}

void eventloop::exit(int exitcode) {
    eventloop& evlo = get();
    evlo.m_exitcode.store(exitcode);
    evlo.m_flagRunning.store(false);
}





// object functions


object::object(object* parent)
    : m_parent(parent)
{
    if(parent != nullptr) {
        parent->addChildren(this);
    } else {
        eventloop::get().addChildren(this);
    }
}

object::~object()
{
    if(m_parent != nullptr) {
        m_parent->removeChildren(this);
    } else {
        eventloop::get().removeChildren(this);
    }

    for(object* obj : m_childrens) {
        if(obj == nullptr) continue;
        delete []obj;
    }
}

void object::addChildren(object* children) {
    m_childrens.push_back(children);
}

bool object::removeChildren(object *children) {
    std::vector<object*>::iterator it = m_childrens.begin();
    while(it++ != m_childrens.end()) {
        if((*it) == children) {
            m_childrens.erase(it);
            return true;
        }
    }
    return false;
}

// void object::connect(object* sender, signal_t* signal, object* receiver, slot_t* slot) {

// }

// void object::disconnect(object* sender, signal_t* signal, object* receiver, slot_t* slot) {

// }


} // namespace evl
