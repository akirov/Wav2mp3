#ifndef __LOG_H__
#define __LOG_H__

#include <iostream>
#include <sstream>
#include "Locker.h"

extern pthread_mutex_t gLogMutex;

#define LOG( text ) \
    do { \
        Locker lock(gLogMutex); \
        std::stringstream sstr; \
        sstr /*<< __FILE__ << ":" << __FUNCTION__ << "(): "*/ << text; \
        std::cerr << sstr.str(); \
    } while( false )

#endif // __LOG_H__
