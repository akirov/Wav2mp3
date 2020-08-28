/******************************************************************************
 * @author Assen Kirov                                                        *
 ******************************************************************************/

#ifdef USE_CPP11_THREADS
 #include <thread>
#else  // POSIX
 #include <pthread.h>
#endif  // USE_CPP11_THREADS

#if defined(__GXX_EXPERIMENTAL_CXX0X) || __cplusplus >= 201103L
 #include <memory>
 using std::shared_ptr;
#else  // TR1
 #include <tr1/memory>
 using std::tr1::shared_ptr;
#endif  // c++11

#ifdef _WIN32
 #include <windows.h>
#else  // POSIX
 #include <unistd.h>
#endif  // _WIN32

#include <vector>
#include <string>
#include <climits>
#include <cstring>
#include <dirent.h>

#include "SyncQueue.h"
#include "WavFile.h"
#include "Encoder.h"
#include "Log.h"

using namespace wav2mp3;


namespace {

/**
 * Use a global variable to track the number of remaining files to be processed.
 * (Re)Initialize it in the manager thread with total number of wav files.
 * Decrease it after a file is encoded. When it becomes 0 all files are processed.
 * Signal this with a condition variable.
 * We can set it to 0 manually to flag a stop request.
 */
int             gNFilesToProcess = INT_MAX;
pthread_cond_t  gNFilesCVar;
pthread_mutex_t gNFilesMutex;


void decNFilesToProcess()
{
    pthread_mutex_lock(&gNFilesMutex);
    if( --gNFilesToProcess <= 0 ) pthread_cond_signal(&gNFilesCVar);
    pthread_mutex_unlock(&gNFilesMutex);
}


int getNFilesToProcess()
{
    Locker lock(gNFilesMutex);
    return gNFilesToProcess;
}


std::vector<std::string> gWavFileURIs;

} // anonymous namespace


pthread_mutex_t gLogMutex;  // TODO put it in wav2mp3 namespace


void* work_manager(void* arg)
{
    // Get the work queue pointer
    SyncQueue< shared_ptr<WavFile> >* wavFileQueue =
                                  (SyncQueue< shared_ptr<WavFile> >*) arg;
    if( NULL == wavFileQueue ) pthread_exit((void*) 0);  // Signal CV before exit?

    int numWavFiles = gWavFileURIs.size();

    // Set gNFilesToProcess. Workers are not active yet, so this is safe.
    gNFilesToProcess = numWavFiles;

    // Enqueue wav files in the work queue until the list is empty (or stop is requested)
    int i=0;
    for( ; i<numWavFiles && getNFilesToProcess()>0; i++ )
    {
        shared_ptr<WavFile> wavFile;
        std::string uri = gWavFileURIs[i];
        try {
            wavFile.reset(new WavFile(uri));  // Will be freed automatically
            wavFile->readEntireFile();  // Should be more effective here than in workers
        } catch(...) {
            LOG("Error opening wav file " << uri << std::endl);
            decNFilesToProcess();
            continue;
        }
        wavFileQueue->enqueue(wavFile);
    }
    LOG("Work manager is done" << std::endl);

    return ((void*) (numWavFiles-i));  // The remaining files (should be 0)
}


void* worker(void* arg)
{
    // Get the work queue pointer
    SyncQueue< shared_ptr<WavFile> >* wavFileQueue =
                                  (SyncQueue< shared_ptr<WavFile> >*) arg;
    if( NULL == wavFileQueue ) pthread_exit((void*) 0);

    unsigned int numProcFiles=0;  // Number of files processed by this thread

    while( true )
    {
        // Dequeue wav file from work queue and encode it. Block if the queue is empty
        shared_ptr<WavFile> wavFile = wavFileQueue->dequeue();

        if( wavFile )
        {
            // Encode wav file
            Encoder encoder(wavFile);
            encoder.encode();

            numProcFiles++;
        }  // Resources will be freed here
        else
        {
            LOG("Error in dequeue()!" << std::endl);
        }

        decNFilesToProcess();
    }

    pthread_exit((void*) numProcFiles);
}


int main(int argc, char* argv[])
{
    // Parse arguments and set gWavFileURIs
    if( argc < 2 )
    {
        std::cerr << "Usage: " << argv[0] << " wav_folder_uri" << std::endl;
        return 1;
    }

    // Fill the list of wav file URIs
    std::string wavFolder(argv[1]);
    size_t len = wavFolder.length();
    if( wavFolder[len-1] != '/' && wavFolder[len-1] != '\\' ) wavFolder += "/";
    DIR* dir;
    if( (dir = opendir (wavFolder.c_str())) == NULL )
    {
        std::cerr << "Error opening folder '" << wavFolder << "'" << std::endl;
        return 1;
    }
    struct dirent* dent;
    while( (dent = readdir (dir)) != NULL )
    {
        const char* fname = dent->d_name;
        if( !strcmp(".", fname) || !strcmp("..", fname) ) continue;
        len = strlen(fname);
        if( (len > 4) && (!strcasecmp(".wav", fname+len-4)) )
            gWavFileURIs.push_back(wavFolder + std::string(fname));
    }
    closedir (dir);
    if( 0 == gWavFileURIs.size() )
    {
        std::cerr << "There are no wav files in '" << wavFolder << "' folder" << std::endl;
        return 1;
    }

    // Initialize global condition variable and mutexes
    pthread_mutex_init(&gLogMutex, NULL);
    pthread_mutex_init(&gNFilesMutex, NULL);
    pthread_cond_init(&gNFilesCVar, NULL);

    // TODO Add signal handler for CTRL+C to set gNFilesToProcess=0 and signal gNFilesCVar

    // Get the number of CPU cores
    long numCores;
#ifdef USE_CPP11_THREADS
    numCores = std::thread::hardware_concurrency();
#elif defined(_WIN32)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    numCores = sysinfo.dwNumberOfProcessors;
#else  // POSIX
    numCores = sysconf(_SC_NPROCESSORS_ONLN);
#endif
    LOG("Number of CPU cores: " << numCores << std::endl);

    // Create work queue for wav files.
#if defined(__GXX_EXPERIMENTAL_CXX0X) || __cplusplus >= 201103L
    std::unique_ptr<SyncQueue<shared_ptr<WavFile> > >
        wavFileQueuePtr(new SyncQueue<shared_ptr<WavFile> >(2*numCores));
#else
    std::auto_ptr<SyncQueue<shared_ptr<WavFile> > >
        wavFileQueuePtr(new SyncQueue<shared_ptr<WavFile> >(2*numCores));
#endif  // c++11

    // Create a manager thread to read wav files and fill the work queue
    pthread_t managerThread;
    pthread_create(&managerThread, 0, work_manager, wavFileQueuePtr.get());

    // Wait until work queue is at least half full? Use a barier?

    // Create pool of encoding threads (workers) and point them to the work queue
    pthread_t encoderThreads[numCores];
    for( int i=0; i<numCores; i++ )
    {
        pthread_create(&(encoderThreads[i]), 0, worker, wavFileQueuePtr.get());
    }

    // Wait for all files to be processed
    pthread_mutex_lock(&gNFilesMutex);
    while( gNFilesToProcess > 0 ) pthread_cond_wait(&gNFilesCVar, &gNFilesMutex);
    pthread_mutex_unlock(&gNFilesMutex);
    LOG("Work end signaled" << std::endl);

    // The manager theread should be done by now - join it
    pthread_join(managerThread, NULL);
    LOG("Work manager joined" << std::endl);

    // Cancel and join worker threads
    for( int i=0; i<numCores; i++ )
    {
        pthread_cancel(encoderThreads[i]);
        unsigned int res=0;
        pthread_join(encoderThreads[i], (void**)&res);
        //LOG("Worker thread " << i << " joined with result " << res << std::endl);
    }
    LOG("Workers joined" << std::endl);

    // Destroy work queue and globals
    wavFileQueuePtr.reset();

    pthread_cond_destroy(&gNFilesCVar);
    pthread_mutex_destroy(&gNFilesMutex);
    pthread_mutex_destroy(&gLogMutex);  // Logs in destructors may crash. Leave this to the OS. Or don't use logs in destructors. Or destroy objects before tis point.

    return 0;
}
