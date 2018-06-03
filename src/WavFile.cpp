/******************************************************************************
 * @author Assen Kirov                                                        *
 ******************************************************************************/

#include <algorithm>
#include <cstring>
#include "WavFile.h"
#include "Log.h"

using namespace wav2mp3;


WavFile::WavFile( const std::string& uri ):
        mFileUri(uri),
        mFile(uri.c_str(), std::ios::in | std::ios::binary),  // Opens the file
        mFileData(),
        mRiffHPtr(NULL),
        mFmtHPtr(NULL),
        mDataHPtr(NULL)
{
}


WavFile::~WavFile()
{
    if( mFile.is_open() )
    {
        LOG("Closing wav file " << mFileUri << std::endl);
        mFile.close();
    }
}


int WavFile::readEntireFile()
{
    if( !mFileData.empty() ) return mFileData.size();  // Already read

    if( !mFile.is_open() )
    {
        mFile.clear();
        mFile.open(mFileUri.c_str(), std::ios::in | std::ios::binary);
    }

    if( mFile.is_open() )
    {
        mFile.seekg(0, std::ios::end);
        mFileData.resize(mFile.tellg());
        mFile.seekg(0, std::ios::beg);
        mFile.read(&mFileData[0], mFileData.size());
        mFile.close();
    }

    return mFileData.size();
}


bool WavFile::findNextWavChunk()
{
    if( mFileData.empty() )
    {
        this->readEntireFile();  // Try to fill mFileData
        if( mFileData.empty() )
        {
            LOG("Can't read file " << mFileUri << std::endl);
            return false;  // If still empty - give up
        }
    }

    // Start looking after the last data chunk
    std::vector<char>::iterator beg;
    if( NULL == mDataHPtr )
    {
        beg = mFileData.begin();
    }
    else
    {
        char* pos = reinterpret_cast<char*>(mDataHPtr) + 8 + mDataHPtr->datasz;
        beg = mFileData.begin() + (pos - &mFileData[0]);
    }

    // Check if we have reached the end of the file
    if( beg >= mFileData.end() ) return false;

    // Parse RIFF header (once)
    if( NULL == mRiffHPtr )
    {
        std::vector<char>::iterator it;
        const char* riffid = "RIFF";

        if( ((mFileData.end() - beg) < (int)sizeof(RIFFHeader)) ||
            ((it = std::search(beg, mFileData.end(), riffid, riffid + 4)) == mFileData.end()) )
        {
            LOG("Can't find RIFF header in file " << mFileUri << std::endl);
            mFmtHPtr  = NULL;
            mDataHPtr = NULL;
            return false;
        }
        else
        {
            RIFFHeader* riffhp = reinterpret_cast<RIFFHeader*>(&mFileData[0] + (it - mFileData.begin()));

            // Check if format is WAVE
            if( strncmp(riffhp->format, "WAVE", 4) )
            {
                LOG("Format is not WAVE in file " << mFileUri << std::endl);
                mFmtHPtr  = NULL;
                mDataHPtr = NULL;
                return false;
            }
            else
            {
                mRiffHPtr = riffhp;
                beg = it + sizeof(RIFFHeader);  // Continue after RIFF heder
            }
        }
    }

    // Parse FMT header (must have one before next data header - new or previous)
    std::vector<char>::iterator fmtit = beg;
    while( (mFileData.end() - fmtit) > (int)sizeof(FMTHeader) )
    {
        std::vector<char>::iterator it;

        // Find FMT header
        const char* fmtid = "fmt ";
        it = std::search(fmtit, mFileData.end(), fmtid, fmtid + 4);
        if( it == mFileData.end() ) break;

        FMTHeader* fmthp = reinterpret_cast<FMTHeader*>(&mFileData[0] + (it - mFileData.begin()));

        // Check if format is PCM with 1 or 2 channels, 8, 16, 24, 32 bps
        uint16_t bps = fmthp->bitspersamp;
        if( (fmthp->fmthsz == 16) && (fmthp->fmttag == 1) &&
            ((fmthp->numchan == 1) || (fmthp->numchan == 2)) &&
            ((8 == bps) || (16 == bps) || (24 == bps) || (32 == bps)) )
        {
            mFmtHPtr = fmthp;
            beg = it + sizeof(FMTHeader);  // Continue after FMT heder
            break;
        }
        else
        {
            fmtit = it + 4;  // skip fmtid and try again
            continue;
        }
    }
    if( NULL == mFmtHPtr )
    {
        LOG("Can't find PCM FMT header with 1 or 2 channels, 8, 16, 24 or 32 bps in file " <<
                mFileUri << std::endl);
        mDataHPtr = NULL;
        return false;
    }

    // Find next data header
    std::vector<char>::iterator it;
    const char* dataid = "data";
    if( ((mFileData.end() - beg) < (int)sizeof(DataHeader)) ||
        ((it = std::search(beg, mFileData.end(), dataid, dataid + 4)) == mFileData.end()) )
    {
        if( NULL == mDataHPtr )  // No previous data header
        {
            LOG("Can't find Data header in file " << mFileUri << std::endl);
        }
        mDataHPtr = NULL;
        return false;
    }
    else
    {
        mDataHPtr = reinterpret_cast<DataHeader*>(&mFileData[0] + (it - mFileData.begin()));
        return true;
    }
}


uint16_t WavFile::getNumChannels() const
{
    if( NULL != mFmtHPtr )
    {
        return mFmtHPtr->numchan;
    }
    else
    {
        return 0;
    }
}


uint32_t WavFile::getSampleRate() const
{
    if( NULL != mFmtHPtr )
    {
        return mFmtHPtr->samprate;
    }
    else
    {
        return 0;
    }
}


uint32_t WavFile::getByteRate() const
{
    if( NULL != mFmtHPtr )
    {
        return mFmtHPtr->byterate;
    }
    else
    {
        return 0;
    }
}


uint16_t WavFile::getFrameSize() const
{
    if( NULL != mFmtHPtr )
    {
        return mFmtHPtr->blkalign;
    }
    else
    {
        return 0;
    }
}


uint16_t WavFile::getBitsPerSample() const
{
    if( NULL != mFmtHPtr )
    {
        return mFmtHPtr->bitspersamp;
    }
    else
    {
        return 0;
    }
}


const char* WavFile::getRawAudioDataPtr() const
{
    if( NULL != mDataHPtr )
    {
        return (reinterpret_cast<char*>(mDataHPtr) + sizeof(DataHeader));
    }
    else
    {
        return NULL;
    }
}


/*
 * @return the smaller of: data size in the header and the remaining file size
 */
uint32_t WavFile::getRawAudioDataSize() const
{
    if( NULL != mDataHPtr )
    {
        uint32_t datasz = mDataHPtr->datasz;
        uint32_t space  = mFileData.size() - (reinterpret_cast<char*>(mDataHPtr)
                                          + sizeof(DataHeader) - &mFileData[0]);
        return std::min(datasz, space);
    }
    else
    {
        return 0;
    }
}
