/* (c) 2015 Michael R. Tirado -- GPLv3, GNU General Public License, version 3.
 * contact: mtirado418@gmail.com
 *
 */ 

#ifndef __NETPCMSTREAM_H__
#define __NETPCMSTREAM_H__

#include "ByteBuffer.h"
#include "TQueue.h"
#include <map>


///stream PCM audio over a network, makes no effort to request retransmission of missing packets as of now.
class NetPCMStream
{
private:
    NetPCMStream();
    ///the key will be seqNum / packets in buffer
    std::map<unsigned int, char *> audioBuffers;
    short packetSize; //size of a single packet
    short packetsInBuffer; //how many packets in a single buffer?
    unsigned int audioBufferSize;
    unsigned int lowSeqNum;  //lowest sequence number allowed in (we already read buffer it belongs to)
public:
    ///size of a full audio buffer, make sure this is big enough for at least 100MS of audio...
    ///packet size * packets per buffer  is the size of the audio buffer
    ///the size should be a number that allows for dropped packets, without screwing up the data alignment
    ///eg. if 2 bytes per sample, and 2 channels of audio, thats 4 bytes per sample, must divide by 4!
    NetPCMStream(short _packetSize, short packetsPerBuffer)
    {
        packetsInBuffer = packetsPerBuffer;
        packetSize = _packetSize;
        audioBufferSize = packetsInBuffer * packetSize;
        lowSeqNum = 0;
    }

    ///sequence number, data, and size of data is packetsize
    void WritePacket(unsigned int seqNum, char *data)
    {
        if (seqNum < lowSeqNum) //this packet belongs to a buffer that has already been processed
        {
            LogWarning("NetPCMStream::WriteBytes() - packet rejected, sequence number too low");
            return;
        }

        //get key for this packet
        unsigned int key = seqNum / packetsInBuffer;
        std::map<unsigned int, char *>::iterator iter = audioBuffers.find(key);
        if (iter != audioBuffers.end())
        {
            //final position in buffer
            unsigned int dPos = (seqNum % packetsInBuffer) * packetSize;
            if (dPos < audioBufferSize)
                memcpy(&(iter->second)[dPos], data, packetSize);
            else
                LogError("NetPCMStream::WriteBytes() attempting to write outside of audio buffer ranger");
        }
        else
        {
            char *buff = new char[audioBufferSize];
            audioBuffers[key] = buff;
            memset(buff, 0, audioBufferSize); //maybe fill with static instead of 0 to avoid pops?
            memcpy(&buff[((seqNum % packetsInBuffer) * packetSize)], data, packetSize);
        }

       // LOGOUTPUT << "pcm stream size: " <<);
        //LogInfo();
    }

    ///returns the buffer with the lowest sequence number
    std::pair<char *, unsigned int> GetAudioBuffer()
    {
        std::pair<char *, unsigned int> aBuff(0,0);
        if (audioBuffers.size() > 0)
        {
            unsigned int key;
            if (lowSeqNum == 0)
                key = 0;
            else
                key = lowSeqNum / packetsInBuffer;
            
            std::map<unsigned int, char *>::iterator iter = audioBuffers.find(key);
            if (iter == audioBuffers.end())
            {
                LogError("key not found in audio buffer list? missed first few buffers?\
                         call CheckSeqKeys to set lowest packets sequence number");
                return aBuff;
            }
            else
            {
                aBuff.first  = iter->second;
                aBuff.second = audioBufferSize;
                audioBuffers.erase(iter);
                lowSeqNum += packetsInBuffer; //advance the lowest allowed packet
            }
        }
        return aBuff;       
    }

    ///sets lowSeqNum to the lowest key available
    void CheckSeqKeys()
    {
        if (audioBuffers.size() == 0)
        {
            LogInfo("no buffers being filled, setting lowSeqNum to 0");
            lowSeqNum = 0;
        }
        else
        {
            lowSeqNum = audioBuffers.begin()->first * packetsInBuffer;
        }
        
    }

    ///resets this data structure to initial state
    void ClearStream()
    {
        lowSeqNum = 0;
        std::map<unsigned int, char *>::iterator iter = audioBuffers.begin();
        while (iter != audioBuffers.end())
        {
            delete[] iter->second;
            iter++;
        }
        audioBuffers.clear();
    }

    inline unsigned int numBuffers() { return audioBuffers.size(); }
};

#endif
