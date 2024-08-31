#include "time.h"

namespace cbricks{namespace base{

tm& getLocalTime(time_t* t){
    time_t _t = time(t);
    return *(localtime(&_t));
}

timeval getTimeOfDay(){
    timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    return now;
}

}} 