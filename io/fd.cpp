#include <unistd.h>
#include <fcntl.h>

#include "fd.h"
#include "../trace/assert.h"

namespace cbricks{ namespace io{

Fd::Fd(int fd):m_fd(fd){
    CBRICKS_ASSERT(this->m_fd > 0, "nonpositive fd");
}

Fd::~Fd(){this->closeFd();}

const int Fd::get()const{
    return this->m_fd;
}

void Fd::setNonblocking(){
    int oldOpt = fcntl(this->m_fd,F_GETFL);
    int newOpt = oldOpt | O_NONBLOCK;
    fcntl(this->m_fd,F_SETFL,newOpt);
}

void Fd::closeFd(){
    this->m_once.onceDo([this](){close(this->m_fd);});
}

}}