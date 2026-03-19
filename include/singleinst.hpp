/*
    Single instance template for creating singleton classes.

    This is built inside a class as member.

    Usage Example:

    // in .h / .hpp file:
    class myClass {
        SINGLE_INSTANCE(myClass)
    };

    // in .cpp file:
    SINGLE_INSTANCE_IMPL(myClass)


*/

#pragma once
#include <type_traits>

template <class TClass>
requires std::is_class_v<TClass>
class SingleInstance {
    friend TClass; // to allow the class to access protected members
protected:
    TClass* m_instance = nullptr;
    inline void onCreate(TClass* inst) { m_instance = inst; }
    inline void onDestroy() { m_instance = nullptr; }
public:
    inline bool hasInstance() const { return m_instance != nullptr; }
    inline TClass* instance() { return m_instance; }
    inline const TClass* instance() const { return m_instance; }
};

#define SINGLE_INSTANCE(CLASS) \
protected: \
    static SingleInstance<CLASS> s_single_instance; \
public: \
    inline static bool hasInstance() { return s_single_instance.hasInstance(); } \
    inline static CLASS* instance() { return s_single_instance.instance(); }


#define SINGLE_INSTANCE_IMPL(CLASS) \
    SingleInstance<CLASS> CLASS::s_single_instance;