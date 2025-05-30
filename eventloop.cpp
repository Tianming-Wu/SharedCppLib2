#include "eventloop.hpp"

namespace evl {

eventloop* eventloop::shared_ptr = nullptr;

eventloop::eventloop(int argc, char** argv)
{
    if(shared_ptr != nullptr) {
        throw std::runtime_error("Cannot construct multiple eventloop instances");
    }

    shared_ptr = this;
}

eventloop::~eventloop() {

}

eventloop& eventloop::get() {
    if(shared_ptr != nullptr)
        return *shared_ptr;
    else throw std::runtime_error("Called eventloop::get() before an instance is actually constructed");
}

int eventloop::exec() {
    m_flagRunning.store(true);

    while(m_flagRunning.load()) {


    }
}

} // namespace evl
