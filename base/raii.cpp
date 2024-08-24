#include "raii.h"

namespace cbricks{namespace base{

RAII::RAII(std::function<void()> now, std::function<void()> defer):m_now(now),m_defer(defer){
    if (this->m_now){
        this->m_now();
    }
}

RAII::~RAII(){
    if (this->m_defer){
        this->m_defer();
    }
}

}}