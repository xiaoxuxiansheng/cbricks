#pragma once 

#include <time.h>
#include <sys/time.h>

namespace cbricks{namespace base{

tm& getLocalTime(time_t* t = nullptr);

timeval getTimeOfDay();

}} 
