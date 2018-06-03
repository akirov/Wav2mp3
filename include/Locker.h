/******************************************************************************
 * @author Assen Kirov                                                        *
 ******************************************************************************/

#ifndef __LOCKER_H__
#define __LOCKER_H__

#include <pthread.h>

namespace wav2mp3 {


/// Exception safe locker
class Locker
{
  public:
    explicit Locker( pthread_mutex_t& mutex ): mMutex(mutex)
    {
        pthread_mutex_lock(&mMutex);
    }

    ~Locker()
    {
        pthread_mutex_unlock(&mMutex);
    }

  private:
    Locker( const Locker& );  // Disable copying.
    Locker& operator=( const Locker& );  // Disable assignment.

    //  Disable creation on the heap
    void *operator new( size_t );
    void operator delete( void * );
    void *operator new[]( size_t );
    void operator delete[]( void * );

    pthread_mutex_t& mMutex;
};


} // namespace

#endif // __LOCKER_H__
