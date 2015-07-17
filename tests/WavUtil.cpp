/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#include "WavUtil.h"

#include <iostream>
#include <string.h>
using std::ifstream;
using std::ios_base;
using std::cout;
using std::endl;

ALfloat listenerPosv[]={0.0,0.0,0.0};
ALfloat listenerVelv[]={0.0,0.0,0.0};
ALfloat listenerOriv[]={0.0,0.0,-1.0, 0.0,1.0,0.0};

bool GetWavInfo(const char *filename, ALsizei *size, Engine::WavFormat *fmt, size_t *dataPosition)
{
    std::ifstream file;
    Engine::RIFFHeader header;
    Engine::ChunkHeader chunkheader;
    ALbyte *buffer = 0;

    file.open(filename, std::ifstream::in | ios_base::binary);

    if (file.is_open())
    {
        // open the file
        // read the main RIFFHeader

        file.read((char *)&header, sizeof(Engine::RIFFHeader));

        // read first chunk ? only what we need for wav format structure & skip rest of chunk
        file.read( ( char* )&chunkheader,               sizeof( Engine::ChunkHeader ) );
        file.read( ( char* )&fmt->formatTag,            sizeof( short ) );
        file.read( ( char* )&fmt->numChannels,          sizeof( short ) );
        file.read( ( char* )&fmt->sampleRate,           sizeof( int   ) );
        file.read( ( char* )&fmt->avgBytesPerSecond,    sizeof( int   ) );
        file.read( ( char* )&fmt->blockAlign,           sizeof( short ) );
        file.read( ( char* )&fmt->bitsPerSample,        sizeof( short ) );
        file.seekg( ( int )file.tellg() + ( chunkheader.length - 16 ), ios_base::beg );


        // skip all chunks that aren?t the ?data? chunk
        char check[4];
        unsigned int CheckHaxDepth = 10000; //if we didnt hit data in 10kish bytes, somethings f'd up??
        unsigned int bytesChecked = 0;
        while (true)
        {
            if (bytesChecked > CheckHaxDepth)
            {
                std::cout << "Error loading wav file: " << filename << " -- could not locate data header";
                return false;
            }
            // read another ChunkHeader
            file.read((char *)&check, 4);
            // if type is "data" break
            if (!memcmp(check, "data", 4))
            {
                int pos = (int)file.tellg();
                file.seekg(0, ios_base::beg);
                file.seekg(pos - 4, ios_base::beg);
                break;
            }
            bytesChecked += 4; //NOTE this is HORRIBLY HACKED, could potentially miss the check if its not aligned with 4 byte checker, fix this mang
        }
        //read this chunk
        file.read((char *)&chunkheader, sizeof(Engine::ChunkHeader));
        //we are now at the actual pcm data
        *dataPosition = file.tellg();
        *size = chunkheader.length;
        file.close();
        return true;
        
    }
    else
    {
        std::cout << "Could not open sound file: " << filename;
        return false;
    }
}

ALbyte *LoadWavFile(const char *filename, ALsizei *size, Engine::WavFormat *fmt)
{
    std::ifstream file;
    Engine::RIFFHeader header;
    Engine::ChunkHeader chunkheader;
    ALbyte *buffer = 0;

    file.open(filename, std::ifstream::in | ios_base::binary);

    if (file.is_open())
    {
        // open the file
        // read the main RIFFHeader

        file.read((char *)&header, sizeof(Engine::RIFFHeader));

        // read first chunk ? only what we need for wav format structure & skip rest of chunk
        file.read( ( char* )&chunkheader,               sizeof( Engine::ChunkHeader ) );
        file.read( ( char* )&fmt->formatTag,            sizeof( short ) );
        file.read( ( char* )&fmt->numChannels,          sizeof( short ) );
        file.read( ( char* )&fmt->sampleRate,           sizeof( int   ) );
        file.read( ( char* )&fmt->avgBytesPerSecond,    sizeof( int   ) );
        file.read( ( char* )&fmt->blockAlign,           sizeof( short ) );
        file.read( ( char* )&fmt->bitsPerSample,        sizeof( short ) );
        file.seekg(( int   )file.tellg() + ( chunkheader.length - 16 ), ios_base::beg );


        // skip all chunks that aren?t the ?data? chunk
        char check[4];
        unsigned int CheckHaxDepth = 10000; //if we didnt hit data in 10kish bytes, somethings f'd up??
        unsigned int bytesChecked = 0;
        while (true)
        {
            if (bytesChecked > CheckHaxDepth)
            {
                std::cout << "Error loading wav file: " << filename << " -- could not locate data header";
                return 0;
            }
            // read another ChunkHeader
            file.read((char *)&check, 4);
            // if type is "data" break
            if (!memcmp(check, "data", 4))
            {
                int pos = (int)file.tellg();
                file.seekg(0, ios_base::beg);
                file.seekg(pos - 4, ios_base::beg);
                break;
            }
            bytesChecked += 4; //NOTE this is HORRIBLY HACKED, could potentially miss the check if its not aligned with 4 byte checker, fix this mang
        }

        //read this chunk
        file.read((char *)&chunkheader, sizeof(Engine::ChunkHeader));

        //allocate the buffer
        buffer = new ALbyte[chunkheader.length];

        //read pcm data in
        file.read((char *)buffer, chunkheader.length);
        //TODO error check incase read fails.
        //set size
        *size = chunkheader.length;

    }
    else
    {
        std::cout << "Could not open sound file: " << filename;
    }

    if (file.is_open())
        file.close();

    return buffer;
}

bool LoadSoundWav(const char* filename, WaveSource *waveSource)
{
    //setup variables for loading the wav
    ALsizei         alSize, alFreq;
    ALenum          alFormat;
    ALbyte          *alData;
    ALint           alError;

    //load the wav file into a buffer
    Engine::WavFormat fmt;
    alData = LoadWavFile(filename, &alSize, &fmt);
    if (alData == 0)
        return false;

    //create the AL buffer
    ALuint tempBuffer;
    alGenBuffers(1, &tempBuffer);

    if (fmt.numChannels == 1)
    {
        if (fmt.bitsPerSample == 8)
        {
            alFormat = AL_FORMAT_MONO8;
        }
        else if (fmt.bitsPerSample == 16)
        {
            alFormat = AL_FORMAT_MONO16;
        }
        else
        {
            alDeleteBuffers(1, &tempBuffer);
            delete[] alData;
            std::cout << "Unknown wav format";
            return false;
        }
    }
    else if (fmt.numChannels == 2)
    {
        if (fmt.bitsPerSample == 8)
        {
            alFormat = AL_FORMAT_STEREO8;
        }
        else if (fmt.bitsPerSample == 16)
        {
            alFormat = AL_FORMAT_STEREO16;
        }
        else
        {
            alDeleteBuffers(1, &tempBuffer);
            delete[] alData;
            std::cout << "Unknown wav format";
            return false;
        }
    }
    else
    {
        alDeleteBuffers(1, &tempBuffer);
        delete[] alData;
        std::cout << "Unknown wav format";
        return false;
    }

    alFreq = fmt.sampleRate;

    //load that buffer into an alBuffer
    alBufferData(tempBuffer, alFormat, alData, alSize, alFreq);
    alError = alGetError();
    if (alError != AL_NO_ERROR)
    {
        std::cout << "OpenAL Error - alBufferData (" << filename << ", error: " <<  alError << ")";
        alDeleteBuffers(1, &tempBuffer);
        delete[] alData;
        return false;
    }

    // unload the wav buffer
    delete[] alData;

    ALuint source;
    alGenSources(1, &source);
    alSourcef(source,  AL_PITCH, 1.0f);
    alSourcef(source,  AL_GAIN, 0.54f);
    alSourcefv(source, AL_POSITION, listenerPosv);
    alSourcefv(source, AL_VELOCITY, listenerVelv);
    alSourcei(source,  AL_BUFFER, tempBuffer);
    alSourcei(source,  AL_LOOPING, AL_FALSE);
    

    waveSource->buffer[0] = tempBuffer;
    waveSource->source = source;
    return true;
}

FileStream::FileStream(unsigned int maxChunk)
{
    reachedEnd = false;
    fileSize = 0;
    open = false;
    position = 0;
    maxChunkRead = maxChunk;
    chunkData = new char[maxChunkRead];
}

FileStream::~FileStream(){ if (open) file.close(); delete[] chunkData; }

bool FileStream::OpenFile(const char* filename)
{
    file.open(filename, std::ifstream::in | ios_base::binary);
    if (!file.is_open())
    {
        cout << "FileStream::OpenFile() - could not open file: " << filename << endl;
        return false;
    }
    reachedEnd = false;
    open = true;
    //get filesize
    file.seekg(0, ifstream::end);
    fileSize = file.tellg();
    if (fileSize == -1)
        return false;

    file.seekg(0, ifstream::beg);
    return true;
}

void FileStream::CloseFile()
{
    open = false;
    file.close();
    fileSize = 0;
    position = 0;
}

char* FileStream::StreamData(unsigned int numBytes)
{
    if (position >= fileSize-1)
    {
        //cout << "FileStream::StreamData() - atPosition is beyond EOF." << endl;
        reachedEnd = true;
        return 0;
    }
    if (position + numBytes > fileSize-1)
        numBytes = fileSize - 1 - (position);
    if (numBytes > maxChunkRead)
    {
        cout << "FileStream::StreamData() - cannot stream more than maxChunkRead" << endl;
        return 0;
    }
    file.read(chunkData, numBytes);
    position += numBytes;
    return chunkData;
}

char* FileStream::ReadData(size_t atPosition, unsigned int numBytes)
{
    if (atPosition >= fileSize-1)
    {
        cout << "FileStream::ReadData() - atPosition is beyond EOF." << endl;
        return 0;
    }
    if (atPosition + numBytes > fileSize-1)
        numBytes = fileSize - 1 - (atPosition);
    if (numBytes > maxChunkRead)
    {
        cout << "FileStream::ReadData() - cannot stream more than maxChunkRead" << endl;
        return 0;
    }
    file.seekg(atPosition); //go to desired position
    file.read(chunkData, numBytes);
    file.seekg(position); //go back to stream position
    return chunkData;    
}

void FileStream::SetPosition(size_t pos)
{
    if (pos > fileSize-1)
        file.seekg(fileSize-1);
    else
        file.seekg(pos);

    reachedEnd = false;
}



