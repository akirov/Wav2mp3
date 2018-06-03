/******************************************************************************
 * @author Assen Kirov                                                        *
 ******************************************************************************/

#ifndef __WAVFILE_H__
#define __WAVFILE_H__

#include <stdint.h>
#include <string>
#include <fstream>
#include <vector>

namespace wav2mp3 {


struct RIFFHeader
{
    char   riffid[4];    // "RIFF" in big endian (0x52494646). Strictly speaking must be int8_t
    uint32_t chunksz;    // The size of the rest of the chunk following this field.
                         // This is the size of the entire file in bytes minus 8 bytes */
    char   format[4];    // "WAVE" in big endian (0x57415645)
} __attribute__((__packed__));

struct FMTHeader
{
    char   fmtid[4];     // "fmt " in big endian (0x666d7420)
    uint32_t fmthsz;     // 16 for PCM - size of the rest of FMTHeader
    uint16_t fmttag;     // 1 for PCM. Other values indicate compression
    uint16_t numchan;    // Number of channels. Mono = 1, Stereo = 2, ...
    uint32_t samprate;   // Sampels per second: 8000, 44100, ...
    uint32_t byterate;   // Bytes per second == SampleRate * NumChannels * BitsPerSample/8
    uint16_t blkalign;   // Block align (frame size) == NumChannels * BitsPerSample/8
    uint16_t bitspersamp;// BitsPerSample (per channel) == 8 (unsigned), 16 (signed), 24, 32
} __attribute__((__packed__));

struct DataHeader
{
    char   dataid[4];    // "data" in big endian (0x64617461)
    uint32_t datasz;     // Size of audio data below == NumSamples * NumChannels * BitsPerSample/8
    // 44 bytes above this point (if headers are sequential)
    // Beginning of audio data - first frame, left sample, right sample, ...
} __attribute__((__packed__));


class WavFile
{
  public:
    /**
     * Constructor. Opens the file. May throws on error.
     *
     * @param[in] URI of the file
     */
    WavFile( const std::string& uri );

    /**
     * Destructor. Closes the file if open
     */
    ~WavFile();

    std::string getURI() const { return mFileUri; }

    /**
     * Read entire file in memory and close it. May throw.
     *
     * @return the size of the file in memory, 0 on error
     */
    int readEntireFile();

    /**
     * Parses the file in memory and sets header pointers.
     * Assuming little endian host architecture.
     *
     * @return true on success, false on error or WAV data not found
     */
    bool findNextWavChunk();

    // Methods below are for the current wav chunk
    uint16_t getNumChannels() const;
    uint32_t getSampleRate() const;
    uint32_t getByteRate() const;
    uint16_t getFrameSize() const;
    uint16_t getBitsPerSample() const;

    const char* getRawAudioDataPtr() const;
    uint32_t getRawAudioDataSize() const;

  private:
    WavFile( const WavFile& );  // Disable copying.
    WavFile& operator=( const WavFile& );  // Disable assignment.

    std::string mFileUri;
    std::ifstream mFile;
    std::vector<char> mFileData; // The entire file contents

    RIFFHeader* mRiffHPtr; // Should be equal to beginning of mFileData
    FMTHeader*  mFmtHPtr;  // Points to FMTHeader of the current chunk
    DataHeader* mDataHPtr; // Points to DataHeader of the current chunk
};


} // namespace

#endif // __WAVFILE_H__
