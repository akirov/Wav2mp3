/******************************************************************************
 * @author Assen Kirov                                                        *
 ******************************************************************************/
#include <sstream>
#include <cstdlib>
#include <pthread.h>
#include "Encoder.h"
#include "Log.h"

using namespace wav2mp3;


Encoder::Encoder( shared_ptr<WavFile> wavFilePtr ):
        mWavFilePtr(wavFilePtr),
        mMp3Uri(),
        mLameContext(NULL),
        mMp3Buffer(NULL),
        mCopiedDataL(NULL),
        mCopiedDataR(NULL),
        mMp3File()
{
}


Encoder::~Encoder()
{
    // Close mp3 file if open
    if( mMp3File.is_open() ) mMp3File.close();

    // Free memory resources if still allocated
    if( mCopiedDataR ) delete[] mCopiedDataR;
    if( mCopiedDataL ) delete[] mCopiedDataL;
    if( mMp3Buffer ) delete[] mMp3Buffer;
    if( mLameContext ) lame_close(mLameContext);
}


/**
 * @param[in] fname full file URI
 * @return file URI without (last) extension, if any
 */
std::string Encoder::getBaseFileUri( const std::string& fname )
{
    size_t fslashp = fname.rfind('/');
    size_t bslashp = fname.rfind('\\');
    size_t slashp = 0;

    if( fslashp != std::string::npos  &&  bslashp != std::string::npos )
        slashp = std::max(fslashp, bslashp);
    else if( fslashp != std::string::npos )
        slashp = fslashp;
    else if( bslashp != std::string::npos )
        slashp = bslashp;

    return fname.substr(0, slashp) + fname.substr(slashp, fname.rfind('.')-slashp);
}


std::string Encoder::int2str(int i)
{
#if defined(__GXX_EXPERIMENTAL_CXX0X) || __cplusplus >= 201103L
    return std::to_string(i);
#else
    std::ostringstream ss;
    ss << i;
    return ss.str();
#endif  // c++11
}


/**
 * Returns the closest bigger standard rate to the argument (or exact value)
 *
 * @param[in] brate calculated Kbrate
 * @return the closest bigger standard rate (if no exact match)
 */
int Encoder::getStdBRate(int brate)
{
    static const int rates[]={8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128,
                              144, 160, 192, 224, 256, 320};
    static const size_t len = sizeof(rates)/sizeof(int);

    if( brate <= rates[0] ) return rates[0];
    if( brate >= rates[len-1] ) return rates[len-1];

    // Binary search
    int beg=0, end=len-1, i;
    while( end > beg )
    {
        i = beg + (end-beg)/2;

        if( brate == rates[i] )
            break;
        else if( brate > rates[i] )
            beg = i+1;
        else
            end = i;
    }

    if( end > beg )
        return rates[i];
    else
        return rates[end];
}


int Encoder::encode()
{
    LOG("Thread " << pthread_self() << " is encoding '" << mWavFilePtr->getURI() <<
            "'" << std::endl);

    int chunkNum=0;
    while( mWavFilePtr->findNextWavChunk() )
    {
        // Set mMp3Uri
        std::string wavUriNoExt = getBaseFileUri(mWavFilePtr->getURI());
        if( chunkNum )
            mMp3Uri = wavUriNoExt + int2str(chunkNum) + ".mp3";
        else
            mMp3Uri = wavUriNoExt + ".mp3";
        //LOG("Encoding '" << mMp3Uri << "'" << std::endl);

        // Initialize LAME lib
        mLameContext = lame_init();
        if( NULL == mLameContext )
        {
            LOG("ERROR in lame_init()" << std::endl);
            break;
        }

        // Set encoding parameters
        lame_set_num_channels(mLameContext, mWavFilePtr->getNumChannels());
        lame_set_in_samplerate(mLameContext, mWavFilePtr->getSampleRate());
        lame_set_brate(mLameContext, getStdBRate(mWavFilePtr->getByteRate()/1000));
        lame_set_quality(mLameContext, 5);  // "good quality, fast"
        if( mWavFilePtr->getNumChannels() == 1 )
            lame_set_mode(mLameContext, MONO);
        else
            lame_set_mode(mLameContext, STEREO);
        lame_set_bWriteVbrTag(mLameContext, 0);

        if( lame_init_params(mLameContext) != 0 )
        {
            LOG("ERROR in lame_init_params()" << std::endl);
            lame_close(mLameContext); mLameContext = NULL;
            continue;
        }

        // Allocate mp3 buffer
        int numSamples = mWavFilePtr->getRawAudioDataSize() / mWavFilePtr->getFrameSize();
        size_t mp3bufsz = numSamples*5/4 + 7200;  // According to LAME lib
        mMp3Buffer = new unsigned char[mp3bufsz];
        if( NULL == mMp3Buffer )
        {
            LOG("ERROR allocating mp3 buffer" << std::endl);
            lame_close(mLameContext); mLameContext = NULL;
            continue;
        }

        // Encode PCM data in mp3
        uint16_t bps = mWavFilePtr->getBitsPerSample();
        int encoded=-5;
        if( mWavFilePtr->getNumChannels() == 1 )  // mono
        {
            if( (8 == bps) && (1 == mWavFilePtr->getFrameSize()) )
            {
                // Allocate conversion buffer
                uint32_t datasz = mWavFilePtr->getRawAudioDataSize();
                mCopiedDataL = new uint8_t[datasz * sizeof(short int)];
                if( NULL == mCopiedDataL )
                {
                    LOG("ERROR allocating mCopiedDataL buffer" << std::endl);
                    lame_close(mLameContext); mLameContext = NULL;
                    continue;
                }

                // Copy data in 16-bit buffer mCopiedDataL, converting to signed
                short int* sip = (short int *) mCopiedDataL;
                const char* datap = mWavFilePtr->getRawAudioDataPtr();
                for( uint32_t i=0; i<datasz; i++ ) sip[i] = ((short)(datap[i] - 0x80)) << 8;

                encoded = lame_encode_buffer(mLameContext, (short int *) mCopiedDataL,
                        NULL, numSamples, mMp3Buffer, mp3bufsz);
            }
            else if( (16 == bps) || ((8 == bps) && (2 == mWavFilePtr->getFrameSize())) )
            {
                encoded = lame_encode_buffer(mLameContext,
                        (short int *) mWavFilePtr->getRawAudioDataPtr(),
                        NULL, numSamples, mMp3Buffer, mp3bufsz);
            }
            else if( (24 == bps) && (3 == mWavFilePtr->getFrameSize()) )
            {
                // Allocate conversion buffer
                uint32_t datasz = mWavFilePtr->getRawAudioDataSize();
                mCopiedDataL = new uint8_t[(datasz/3) * sizeof(int)];
                if( NULL == mCopiedDataL )
                {
                    LOG("ERROR allocating mCopiedDataL buffer" << std::endl);
                    lame_close(mLameContext); mLameContext = NULL;
                    continue;
                }

                // Copy data in 32-bit buffer mCopiedDataL
                const char* datap = mWavFilePtr->getRawAudioDataPtr();
                for( uint32_t i=0, j=0; i<datasz; i += 3, j += 4 )
                {
                    mCopiedDataL[j]   = 0;
                    mCopiedDataL[j+1] = datap[i];
                    mCopiedDataL[j+2] = datap[i+1];
                    mCopiedDataL[j+3] = datap[i+2];
                }

                encoded = lame_encode_buffer_int(mLameContext, (int *) mCopiedDataL,
                        NULL, numSamples, mMp3Buffer, mp3bufsz);
            }
            else if( (32 == bps) || ((24 == bps) && (4 == mWavFilePtr->getFrameSize())) )
            {
                encoded = lame_encode_buffer_int(mLameContext,
                        (int *) mWavFilePtr->getRawAudioDataPtr(),
                        NULL, numSamples, mMp3Buffer, mp3bufsz);
            }
        }
        else  // 2-channel stereo
        {
            if( (8 == bps) && (2 == mWavFilePtr->getFrameSize()) )
            {
                // Allocate conversion buffers
                uint32_t datasz = mWavFilePtr->getRawAudioDataSize();
                mCopiedDataL = new uint8_t[(datasz/2) * sizeof(short int)];
                mCopiedDataR = new uint8_t[(datasz/2) * sizeof(short int)];
                if( NULL == mCopiedDataL || NULL == mCopiedDataR )
                {
                    LOG("ERROR allocating conversion buffers" << std::endl);
                    lame_close(mLameContext); mLameContext = NULL;
                    continue;
                }

                // Copy data in 16-bit buffers, converting to signed
                short int* sipl = (short int *) mCopiedDataL;
                short int* sipr = (short int *) mCopiedDataR;
                const char* datap = mWavFilePtr->getRawAudioDataPtr();
                for( uint32_t i=0, l=0, r=0; i<datasz; i++ )
                {
                    sipl[l++] = (short)(datap[i] - 0x80) << 8;
                    sipr[r++] = (short)(datap[++i] - 0x80) << 8;
                }

                encoded = lame_encode_buffer(mLameContext, (short int *) mCopiedDataL,
                        (short int *)mCopiedDataR, numSamples, mMp3Buffer, mp3bufsz);
            }
            else if( (16 == bps) || ((8 == bps) && (4 == mWavFilePtr->getFrameSize())) )
            {
                encoded = lame_encode_buffer_interleaved(mLameContext,
                        (short int *) mWavFilePtr->getRawAudioDataPtr(),
                        numSamples, mMp3Buffer, mp3bufsz);
            }
            else if( (24 == bps) && (6 == mWavFilePtr->getFrameSize()) )
            {
                // TODO Copy data in two 32-bit buffers mCopiedDataL and mCopiedDataR
                LOG("24-bps stereo is not implemented yet" << std::endl);
                lame_close(mLameContext); mLameContext = NULL;
                continue;

                encoded = lame_encode_buffer_int(mLameContext, (int *) mCopiedDataL,
                        (int *) mCopiedDataR, numSamples, mMp3Buffer, mp3bufsz);
            }
            else if( (32 == bps) || ((24 == bps) && (8 == mWavFilePtr->getFrameSize())) )
            {
#if 0  // This is available only in LAME 3.100
                encoded = lame_encode_buffer_interleaved_int(mLameContext,
                        (int *) mWavFilePtr->getRawAudioDataPtr(), numSamples,
                        mMp3Buffer, mp3bufsz);
#else
                // Allocate channel buffers
                uint32_t datasz = mWavFilePtr->getRawAudioDataSize();
                mCopiedDataL = new uint8_t[datasz/2];
                mCopiedDataR = new uint8_t[datasz/2];
                if( NULL == mCopiedDataL || NULL == mCopiedDataR )
                {
                    LOG("ERROR allocating channel buffers" << std::endl);
                    lame_close(mLameContext); mLameContext = NULL;
                    continue;
                }

                // Copy data in separate channel buffers
                int* lchan = (int *) mCopiedDataL;
                int* rchan = (int *) mCopiedDataR;
                int* ichan = (int *) mWavFilePtr->getRawAudioDataPtr();
                for( uint32_t i=0, l=0, r=0; i<datasz/sizeof(int); i++ )
                {
                    lchan[l++] = ichan[i];
                    rchan[r++] = ichan[++i];
                }

                encoded = lame_encode_buffer_int(mLameContext, (int *) mCopiedDataL,
                        (int *)mCopiedDataR, numSamples, mMp3Buffer, mp3bufsz);
#endif // LAME ver
            }
        }

        if( encoded <= 0 )
        {
            LOG("ERROR in lame_encode_buffer : " << encoded << std::endl);
        }
        else
        {
            // Create/open output mp3 file
            mMp3File.open(mMp3Uri.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
            if( ! mMp3File.is_open() )
            {
                LOG("ERROR opening mp3 file " << mMp3Uri << " for writing" << std::endl);
            }
            else
            {
                // Write mp3 buffer in the file
                mMp3File.write(reinterpret_cast<char*>(mMp3Buffer), encoded);
                // Flush the buffer in the file
                int flushed = lame_encode_flush(mLameContext, mMp3Buffer, mp3bufsz);
                if( flushed > 0 )
                    mMp3File.write(reinterpret_cast<char*>(mMp3Buffer), flushed);
                else
                    LOG("ERROR in lame_encode_flush : " << flushed << std::endl);
            }
        }

        if( mMp3File.is_open() )
        {
            mMp3File.close();
            mMp3File.clear();
        }
        if( mCopiedDataR )
        {
            delete[] mCopiedDataR;
            mCopiedDataR = NULL;
        }
        if( mCopiedDataL )
        {
            delete[] mCopiedDataL;
            mCopiedDataL = NULL;
        }
        if( mMp3Buffer )
        {
            delete[] mMp3Buffer;
            mMp3Buffer = NULL;
        }
        lame_close(mLameContext); mLameContext = NULL;
        chunkNum++;
    }
    LOG("Thread " << pthread_self() << " encoded " << chunkNum <<
            " wav chunk(s) from '" << mWavFilePtr->getURI() << "'" << std::endl);
    return chunkNum;
}
