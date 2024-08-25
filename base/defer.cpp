#include "defer.h"

namespace cbricks{namespace base{

Defer::Defer(std::function<void()> cb):m_cb(cb){}

Defer::~Defer(){
    if (this->m_cb){
        this->m_cb();
    }
}

void Defer::Clear(){
    this->m_cb = nullptr;
}

}}