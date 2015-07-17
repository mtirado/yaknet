/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#ifndef __WAV_UTIL_H__
#define __WAV_UTIL_H__
#include <AL/al.h>
#include <fstream>
namespace Engine
{
struct RIFFHeader
{
    char           id[4];        // "RIFF"
    unsigned int   length;       // size of chunk ( may include extra bytes )
    char           fmtId[4];     // "WAVE", "AVI", etc.
};

struct ChunkHeader
{
    char           type[4];      // chunk type
    unsigned int   length;       // size of chunk
};

struct WavFormat
{
    short formatTag;
    short numChannels;
    int   sampleRate;
    int   avgBytesPerSecond;
    short blockAlign;
    short bitsPerSample;
};
}

struct WaveSource
{
    ALuint source;
    ALuint buffer[2]; //double bufered, use [0] if you arent swapping buffers.

    WaveSource()
    {
        source = 0;
        buffer[0] = 0;
        buffer[1] = 0;
    }
};

///attempts to read a wav header, and returns that info + the position of the pcm data in the file
bool GetWavInfo(const char *filename, ALsizei *size, Engine::WavFormat *fmt, size_t *dataPosition);

///returns pcm wav data in memory, size: size of data, fmt: wav format parameters
ALbyte *LoadWavFile(const char *filename, ALsizei *size, Engine::WavFormat *fmt);

///creates a WaveSource for openal to use from a given wav file, returns false on failure.
bool LoadSoundWav(const char* filename, WaveSource *waveSource);

///re-uses a read buffer so data may be overwritten if not used immediately
///read buffer is chunkData, which is allocated in the constructor.

///io wrapper for streaming files over network
class FileStream  //TODO seeking around in the file is going to be painfully slow, figure out an alternative method.
{
private:
    std::ifstream file;
    size_t fileSize;
    bool open;
    size_t position; //current stream position
    //for debugging, it may be helpful to memset 0 before reading into it
    char *chunkData; //data buffer, returned pointers will point here
    unsigned int maxChunkRead;
    bool reachedEnd;
    FileStream();
public:
    ///maximum chunk size for a read
    FileStream(unsigned int maxChunk);
    ~FileStream();
    bool OpenFile(const char *filename);
    ///stream data from the current position, updates position
    char *StreamData(unsigned int numBytes);
    ///read numBytes, at position, does not update internal position
    ///use for retransmission requests
    char *ReadData(size_t atPosition, unsigned int numBytes);
    void SetPosition(size_t pos);
    void CloseFile();

    inline unsigned int isOpen() { return open;       }
    inline size_t  getFileSize() { return fileSize;   }
    inline bool    atEnd()       { return reachedEnd; }
};

#endif
