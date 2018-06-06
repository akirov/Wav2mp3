/******************************************************************************
 * @author Assen Kirov                                                        *
 ******************************************************************************/
#ifndef __ENCODER_H__
#define __ENCODER_H__

#if defined(__GXX_EXPERIMENTAL_CXX0X) || __cplusplus >= 201103L
 #include <memory>
 using std::shared_ptr;
#else  // TR1
 #include <tr1/memory>
 using std::tr1::shared_ptr;
#endif  // c++11

#include <string>
#include <fstream>
#include "lame/lame.h"
#include "WavFile.h"

namespace wav2mp3 {


class Encoder
{
  public:
    /**
     * Constructor
     *
     * @param[in] wavFilePtr - shared pointer to WavFile object
     */
    Encoder( shared_ptr<WavFile> wavFilePtr );

    ~Encoder();

    /**
     * Encode the wav file, which may contain several WAV chunks. Should we have
     * separate mp3 files for each WAV chunk? Probably, because audio data parameters
     * may be different. Mp3 files after the first will have an index in the name.
     * Need to to copy and convert 8-bit unsigned data to 16-bit signed, and 24-bit
     * to 32-bit, because LAME lib works only with 16-bit and 32-bit buffers.
     */
    int encode();  // TODO Add encoding parameters?

  private:
    // Helper functions
    static std::string getBaseFileUri( const std::string& fname );
    static std::string int2str(int i);
    static int getStdBRate(int brate);

    shared_ptr<WavFile> mWavFilePtr;
    std::string         mMp3Uri;
    lame_global_flags*  mLameContext;
    std::ofstream       mMp3File;
};


} // namespace

#endif  // __ENCODER_H__
