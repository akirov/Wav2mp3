wav2mp3 - multithreaded WAV to MP3 file converter written in C++, using LAME lib
================================================================================

Usage:
`> wav2mp3 wav_folder_uri`
All wav files in the folder `wav_folder_uri` will be encoded to mp3.

Supports WAV files containing uncompressed audio (PCM) with no more than 2
channels for now.

It will be difficult to use multiple encoding threads on a single wav file,
because it is not clear whether LAME library structures are thread-safe. Also
stitching audio pieces together without degrading audio quality is not trivial.
This may be needed though, if we need to process just one very large file.
But for now each worker thread will encode a single wav file.
Workers themselves could read wav files, but parallel reading will be
ineffective (because we have a single hard disk) - better read them separately
and sequentially.

So, I am using multithreaded producer-consumer architecture:
Create a pool of worker (encoding) threads. Don't destroy them until there is
work to do. Number of workers is equal to the number of CPU cores.
A manager thread reads wav files into memory and pushes them in a work queue.
This thread should be idle most of the time (because reading wav files into
memory should be faster than encoding and writing them in mp3), so don't
allocate CPU core for it. Queue max size is two times the number of cores
(and the number of workers) to avoid workers waiting for a job. This limit
can be adjusted. Putting a limit to work queue size avoids memory becoming
full with wav files (eventually). Workers pick files from the queue and
encode them in parallel. Manager thread fills the queue with smart pointers to
wav file objects. The process ends when all wav files are encoded.

Tested on Ubuntu Linux 16.04 x64 with GCC 5.4.0, on Windows 10 x64 with
MinGW GCC 5.3.0 x32 (from Qt 5.9), on WindowsXP x86 with GCC 4.9.2 x32 (from
Qt 5.5.1), and with GCC 6.4.0 x32 and 7.3.0 x64 from Cygwin on Windows 10 x64.
On Ubuntu and Cygwin need to install libmp3lame-dev package. LAME library can
be linked statically in the executable. On Windows may need to build LAME
from source to get static libraries. Also may need to apply patch from
https://aur.archlinux.org/packages/mingw-w64-lame/.
The Makefile assumes no spaces in the path. You may need to set LAME lib path.


TODO:
----
- signal handling (Ctrl+C)
- add encoding options to Encoder class methods
- put manager, workers and globals in a class
- use C++11 threads instead of POSIX threads
- lock-free queue implementation?
- support more wave formats
- Qt GUI?
